///////////////////////////////////////////////////////////////////////////////
// \author (c) Marco Paland (info@paland.com)
//             2011-2017, PALANDesign Hannover, Germany
//
// \license The MIT License (MIT)
//
// This file is part of the decom library.
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// \brief Serial COM port communication class
//
// This class abstracts the Windows serial COM port.
// All kind of serial ports (RS232, USB, virtual etc.) are supported
// A worker thread for overlapped/asynchronous communication is used.
//
// \todo Add functions for interbyte timing etc.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef _DECOM_COM_SERIAL_H_
#define _DECOM_COM_SERIAL_H_

#include <windows.h>
#include <process.h>
#include <vector>
#include <sstream>

#include "com.h"

/////////////////////////////////////////////////////////////////////

namespace decom {
namespace com {


class serial : public communicator
{
public:
  // params
  typedef enum enum_parity_type {
    no_parity = 0,
    odd_parity,
    even_parity,
    mark_parity,
    space_parity
  } parity_type;

  typedef enum enum_stopbits_type {
    one_stopbit = 0,
    one5_stopbits,
    two_stopbits
  } stopbits_type;

  typedef enum enum_handshake_type {
    no_handshake = 0,
    cts_rts_handshake,
    dsr_dtr_handshake,
    xon_xoff_handshake
  } handshake_type;


public:
  /**
   * Param ctor, sets the default port parameters
   * \param baudrate The baudrate given in [baud]
   * \param databits Number of databits, normally and default is 8
   * \param parity Parity, see defines
   * \param stopbits Number of stopbits, see defines
   * \param handshake Handshake options, see defines
   * \param name Layer name
   */
  serial(std::uint32_t baudrate,
         std::uint8_t databits    = 8U,
         parity_type parity       = no_parity,
         stopbits_type stopbits   = one_stopbit,
         handshake_type handshake = no_handshake,
         const char* name         = "com_serial"
        )
    : communicator(name)   // it's VERY IMPORTANT to call the base class ctor HERE!!!
  {
    events_[ev_terminate] = ::CreateEvent(NULL, TRUE,  FALSE, NULL);
    events_[ev_transmit]  = ::CreateEvent(NULL, FALSE, FALSE, NULL);    // auto reset event
    events_[ev_receive]   = ::CreateEvent(NULL, FALSE, FALSE, NULL);    // auto reset event

    thread_handle_ = INVALID_HANDLE_VALUE;
    com_handle_    = INVALID_HANDLE_VALUE;

    memset(&tx_overlapped_, 0, sizeof(tx_overlapped_));
    tx_overlapped_.hEvent = events_[ev_transmit];
    tx_busy_ = false;

    // params
    port_      = 0U;
    baudrate_  = baudrate;
    databits_  = databits;
    parity_    = parity;
    stopbits_  = stopbits;
    handshake_ = handshake;
  }


  /**
   * default dtor
   */
  ~serial()
  {
    close();

    // close handles
    (void)::CloseHandle(events_[ev_terminate]);
    (void)::CloseHandle(events_[ev_transmit]);
    (void)::CloseHandle(events_[ev_receive]);
  }


public:
  /**
   * Called by upper layer to open this layer
   * \param address The port to open, allowed are 'COM1', 'COM2' etc.
   * \param id Unused
   * \return true if open is successful
   */
  virtual bool open(const char* address = "", eid const& id = eid_any)
  {
    (void)id;   // unused

    // check that upper protocol/device exists
    if (!upper_) {
      return false;
    }

    // assemble port address
    if (!address || !(*address)) {
      // invalid address
      return false;
    }
    std::stringstream port;
    port << "\\\\.\\" << address;   // extend to complete port address (like '\\.\COM1')

    close();  // just in case layer was already open

    // call CreateFile to open the COM port
    com_handle_ = ::CreateFileA(
      port.str().c_str(),
      GENERIC_READ | GENERIC_WRITE,
      0U,
      NULL,
      OPEN_EXISTING,
      FILE_FLAG_OVERLAPPED,
      NULL
    );
    if (com_handle_ == INVALID_HANDLE_VALUE) {
      DECOM_LOG_ERROR("Error opening port " << port.str().c_str());
      return false;
    }

    DECOM_LOG_INFO("Opened serial port ") << address;

    // set params
    if (!set_param(baudrate_, databits_, parity_, stopbits_, handshake_)) {
      // error in set params
      DECOM_LOG_ERROR("Error setting params");
      close();
      return false;
    }

    // set timeouts
    COMMTIMEOUTS cto;
    cto.ReadIntervalTimeout = MAXDWORD;
    cto.ReadTotalTimeoutMultiplier = MAXDWORD;
    cto.ReadTotalTimeoutConstant = MAXDWORD - 1;
    cto.WriteTotalTimeoutMultiplier = 0U;
    cto.WriteTotalTimeoutConstant = 0U;
    if (!::SetCommTimeouts(com_handle_, &cto)) {
      DECOM_LOG_ERROR("Error setting timeouts");
      // error in set timeouts
      close();
      return false;
    }

    tx_busy_ = false;

    ///////////////////// create receive thread ////////////////////////////

    thread_handle_ = reinterpret_cast<HANDLE>(::_beginthreadex(
      NULL,
      0U,
      reinterpret_cast<unsigned(__stdcall *)(void*)>(&serial::worker_thread),
      reinterpret_cast<void*>(this),
      CREATE_SUSPENDED,
      NULL
    ));
    if (!thread_handle_)
    {
      // error to create thread - close all
      DECOM_LOG_ERROR("Error creating thread");
      close();
      return false;
    }

    // run at higher priority for short latency time
    (void)::SetThreadPriority(thread_handle_, THREAD_PRIORITY_HIGHEST);
    (void)::ResumeThread(thread_handle_);

    // send port open indication
    communicator::indication(connected);

    return true;
  }


  /**
   * Called by upper layer to close this layer
   * \param id Unused
   */
  virtual void close(eid const& id = eid_any)
  {
    (void)id;   // unused

    // stop the receive thread
    if (thread_handle_ != INVALID_HANDLE_VALUE)
    {
      (void)::SetEvent(events_[ev_terminate]);
      (void)::WaitForSingleObject(thread_handle_, 5000U);  // wait 3 sec for thread termination
      (void)::CloseHandle(thread_handle_);
      thread_handle_ = INVALID_HANDLE_VALUE;
    }

    // close the comm port
    if (com_handle_ != INVALID_HANDLE_VALUE) {
      (void)purge();
      // final close
      (void)::CloseHandle(com_handle_);
      com_handle_ = INVALID_HANDLE_VALUE;

      DECOM_LOG_INFO("Closed serial port");

      // port closed indication
      communicator::indication(disconnected);
    }
  }


  /**
   * Called by upper layer to send data to COM port
   * \param data The message to send
   * \param id The endpoint identifier, unused here
   * \param more unused
   * \return true if Send is successful
   */
  virtual bool send(msg& data, eid const& id = eid_any, bool more = false)
  {
    (void)id; (void)more;   // unused

    // validate port
    if (!is_open()) {
      DECOM_LOG_ERROR("Sending failed: port is not open");
      return false;
    }

    // return false if an overlapped transfer is in progress, means no new data can be accepted
    // this is mostly the case when the upper layer didn't wait for tx_done indication
    if (tx_busy_) {
      DECOM_LOG_WARN("Transmission already in progress");
      return false;
    }

    // convert msg to vector - linear array is needed by WriteFile
    tx_buf_.clear();
    tx_buf_.reserve(data.size());
    tx_buf_.insert(tx_buf_.begin(), data.begin(), data.end());

    DWORD bytes_written = 0;
    tx_busy_ = true;
    if (::WriteFile(com_handle_, static_cast<LPCVOID>(&tx_buf_[0]), static_cast<DWORD>(tx_buf_.size()), &bytes_written, &tx_overlapped_)) {
      // okay - byte written
      if (bytes_written != tx_buf_.size()) {
        // error - byte not written
        tx_busy_ = false;
        return false;
      }
    }
    else {
      if (::GetLastError() != ERROR_IO_PENDING) {
        // sending error - should never happen - check it!
        tx_busy_ = false;
        DECOM_LOG_CRIT("Sending error - should not happen, check it!");
        return false;
      }
    }

    // sending in progress now, indication is handled in worker thread
    return true;
  }


  ////////////// additional functions ///////////
public:
  /**
   * Set communication parameter
   * \param baudrate The baudrate given in [baud]
   * \param databits Number of databits, normally 8
   * \param parity Parity, see defines
   * \param stopbits Number of stopbits, see defines
   * \param handshake Handshake protocol, see defines
   * \return true if successful
   */
  bool set_param(std::uint32_t baudrate,
                 std::uint8_t databits    = 8U,
                 parity_type parity       = no_parity,
                 stopbits_type stopbits   = one_stopbit,
                 handshake_type handshake = no_handshake)
  {
    // validate port
    if (!is_open()) {
      return false;
    }

    DCB dcb;
    memset(&dcb, 0, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);

    dcb.fBinary  = TRUE;                    // only binary mode
    dcb.fParity  = (parity != no_parity);   // parity
    dcb.BaudRate = (DWORD)baudrate;         // setup the baud rate
    dcb.fOutxCtsFlow = (handshake == cts_rts_handshake);
    dcb.fOutxDsrFlow = (handshake == dsr_dtr_handshake);
    dcb.fDtrControl  = (handshake == dsr_dtr_handshake ? DTR_CONTROL_HANDSHAKE : DTR_CONTROL_ENABLE);
    dcb.fDsrSensitivity = FALSE;
    dcb.fTXContinueOnXoff = FALSE;
    dcb.fOutX = (handshake == xon_xoff_handshake);
    dcb.fInX  = (handshake == xon_xoff_handshake);
    dcb.fErrorChar = FALSE;
    dcb.fNull = FALSE;
    dcb.fRtsControl = (handshake == cts_rts_handshake ? RTS_CONTROL_HANDSHAKE : RTS_CONTROL_ENABLE);
    dcb.fAbortOnError = FALSE;
    dcb.XonLim = 1024U;           // Xon limit
    dcb.XoffLim = 1024U;           // Xoff limit
    dcb.ByteSize = (BYTE)databits;  // setup the data bits
    dcb.Parity = (BYTE)parity;    // setup the parity
    dcb.StopBits = (BYTE)stopbits;  // setup the stop bits
    dcb.XonChar = 0x11U;
    dcb.XoffChar = 0x13U;
    dcb.ErrorChar = 0x00U;
    dcb.EofChar = 0x04U;
    dcb.EvtChar = 0x00U;

    // apply settings
    return ::SetCommState(com_handle_, &dcb) != FALSE;
  }


  /**
   * Flush all buffers, means all data in buffers is send/get to/from hardware
   * \return true if successful
   */
  bool flush(void)
  {
    // validate port
    if (!is_open()) {
      return false;
    }

    DECOM_LOG_INFO("Flushing COM port");

    return ::FlushFileBuffers(com_handle_) != FALSE;
  }


  /**
   * Purge all buffers
   * \param rx Clears the receive buffer (if the device has one)
   * \param tx Clears the transmit buffer (if the device has one)
   * \return true if successful
   */
  bool purge(bool rx = true, bool tx = true)
  {
    // validate port
    if (!is_open()) {
      return false;
    }

    DECOM_LOG_INFO("Purging COM port");

    return ::PurgeComm(com_handle_, (rx ? (PURGE_RXABORT | PURGE_RXCLEAR) : 0U) |
                                    (tx ? (PURGE_TXABORT | PURGE_TXCLEAR) : 0U)
                      ) != FALSE;
  }

/////////////////////////////////////////////////////////////////////////////////////

private:
  /**
   * Check if the port is open
   * \return true if the COM port is open
   */
  inline bool is_open() const
  { return com_handle_ != INVALID_HANDLE_VALUE; }


  // worker thread
  static void worker_thread(void* arg)
  {
    // see http://support.microsoft.com/kb/156932 !!
    // and http://www.daniweb.com/software-development/cpp/threads/8020

    serial* s = static_cast<serial*>(arg);

    // defines the receive buffer size
    #define DECOM_COM_SERIAL_RX_BUFSIZE 32768

    BYTE buf[DECOM_COM_SERIAL_RX_BUFSIZE];  // receive buffer
    DWORD bytes_read, bytes_written;

    // validate port - MUST be open and active
    if (!s->is_open()) {
      DECOM_LOG2(DECOM_LOG_LEVEL_CRIT, s->name_, "COM port is not open - please check!");
      return;
    }

    OVERLAPPED rx_overlapped;
    memset(&rx_overlapped, 0, sizeof(rx_overlapped));
    rx_overlapped.hEvent = s->events_[ev_receive];

    for (;;)
    {
      // issue read operation
      if (::ReadFile(s->com_handle_, buf, DECOM_COM_SERIAL_RX_BUFSIZE, &bytes_read, &rx_overlapped)) {
        // data is available, dwBytesRead is valid
        if (bytes_read) {
          msg data(buf, buf + bytes_read);
          s->communicator::receive(data);
        }
      }
      else {
        if (::GetLastError() != ERROR_IO_PENDING) {
          // com error, port closed etc.
          DECOM_LOG2(DECOM_LOG_LEVEL_CRIT, s->name_, "COM port error - please check!");
        }

        switch (::WaitForMultipleObjects(serial::ev_max, reinterpret_cast<HANDLE*>(s->events_), FALSE, INFINITE))
        {
        case WAIT_OBJECT_0:
          // kill event - terminate worker thread
          DECOM_LOG2(DECOM_LOG_LEVEL_DEBUG, s->name_, "Terminating worker-thread gracefully");
          return;

        case WAIT_OBJECT_0 + serial::ev_receive:
          // rx event
          if (::GetOverlappedResult(s->com_handle_, &rx_overlapped, &bytes_read, TRUE) && bytes_read) {
            // bytes received, pass data to upper layer
            msg data(buf, buf + bytes_read);
            s->communicator::receive(data);
          }
          (void)::ResetEvent(s->events_[ev_receive]);
          break;
 
        case WAIT_OBJECT_0 + serial::ev_transmit:
          // tx event
          if (::GetOverlappedResult(s->com_handle_, &s->tx_overlapped_, &bytes_written, TRUE) && bytes_written) {
            // okay - bytes written, inform upper layer
            s->communicator::indication(tx_done);
          }
          else {
            // error - bytes not written
            s->communicator::indication(tx_error);
          }
          s->tx_busy_ = false;
          break;

        case WAIT_TIMEOUT:
          // should never happen
          DECOM_LOG2(DECOM_LOG_LEVEL_CRIT, s->name_, "WAIT TIMEOUT - should not happen!");
          break;

        default:
          break;
        }
      }
    }
  }


  // COM parameters
  std::uint32_t   baudrate_;
  std::uint8_t    port_;
  std::uint8_t    databits_;
  parity_type     parity_;
  stopbits_type   stopbits_;
  handshake_type  handshake_;

  HANDLE com_handle_;                   // COM port handle
  HANDLE thread_handle_;                // worker thread handle
  OVERLAPPED tx_overlapped_;            // tx overlapped structure
  std::vector<std::uint8_t> tx_buf_;    // static linear tx buffer
  volatile bool tx_busy_;               // tx transfer running

  enum {
    ev_terminate = 0,
    ev_transmit  = 1,
    ev_receive   = 2,
    ev_max       = 3
  };

  HANDLE events_[ev_max];     // terminate, xmit and receive event handles
};

} // namespace com
} // namespace decom

#endif // _DECOM_COM_SERIAL_H_
