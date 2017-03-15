///////////////////////////////////////////////////////////////////////////////
// \author (c) Marco Paland (info@paland.com)
//             2012-2017, PALANDesign Hannover, Germany
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
// \brief ISO15765-2 (CAN TP) transport protocol including extended addressing
//
///////////////////////////////////////////////////////////////////////////////

#ifndef _DECOM_PROT_ISO15765_H_
#define _DECOM_PROT_ISO15765_H_

#include "prot.h"
#include "util/timer.h"


/////////////////////////////////////////////////////////////////////

namespace decom {
namespace prot {

// protocol definitions
#define NPCI_SINGLE_FRAME           0x00U
#define NPCI_FIRST_FRAME            0x10U
#define NPCI_CONSECUTIVE_FRAME      0x20U
#define NPCI_FLOW_CONTROL           0x30U
#define NPCI_ERROR_FRAME            0x40U

// data frame length - normal addressing
#define SF_DATALENGTH               7U
#define FF_DATALENGTH               6U
#define CF_DATALENGTH               7U
#define FC_DATALENGTH               3U

// data frame length - extended addressing
#define SF_DATALENGTH_EXT           6U
#define FF_DATALENGTH_EXT           5U
#define CF_DATALENGTH_EXT           6U

// complete frame length
#define FRAME_LENGTH                8U

// flow control codes
#define FC_CTS                      0U
#define FC_WAIT                     1U
#define FC_OVERFLOW                 2U

// timing
#define N_As                        1000U
#define N_Ar                        1000U
#define N_Bs                        1000U
#define N_Cr                        1000U

// defines the largest frame size to accept by protocol
#define CF_MAX_FRAME_SIZE           4095U

// defines the largest (consecutive) frame size to accept due to buffer limit
#define CF_MAX_BUFFER_SIZE          CF_MAX_FRAME_SIZE

// define (uncomment) this if Flow Control overflow frame should be sent, but it's uncommon
//#define FC_SEND_OVERFLOW


class iso15765 : public protocol
{
private:
  // disable normal layer ctor
  iso15765(decom::layer* lower);

public:
  /**
   * Protocol ctor
   * \param lower Lower layer
   * \param STmin TP STmin parameter (time between CF frames in [ms])
   * \param BS TP block size parameter (max number of CF frames between FC frames)
   * \param MAX_DL Maximum acceptable data length, 4095 is maximum by protocol
   */
  iso15765(decom::layer* lower, std::uint8_t STmin, std::uint8_t BS, std::uint16_t MAX_DL = 4095U)
    : protocol(lower, "prot_ISO15765")   // it's VERY IMPORTANT to call the base class ctor HERE!!!
    , CF_STmin_(STmin)
    , CF_BS_(BS)
    , CF_MAX_DL_(MAX_DL)
  {
    CF_DL_            = 0U;
    CF_SN_            = 0U;
    CF_size_          = 0U;
    CF_BScnt_         = 0U;

    FC_STmin_         = 0U;
    FC_BS_            = 0U;
    FC_FS_            = 0U;

    ext_source_adr_   = 0U;
    ext_target_adr_   = 0U;

    use_ext_adr_      = false;
    use_zero_padding_ = false;
  }


  /**
   * dtor
   */
  virtual ~iso15765()
  { }


  /**
   * Called by upper layer to open this layer
   * \param address The address to open
   * \param channel The channel to open
   * \return true if open is successful
   */
  virtual bool open(const char* address = "", decom::eid const& id = eid_any)
  {
    // for security: check that upper protocol/device exists
    if (!upper_) {
      return false;
    }

    // open the lower layer NOW - opening is done DOWN-TOP in layer stack
    // the lowest layer is opened first, the highest layer last
    // if opening the lower layer fails, report it to upper layer
    bool result = protocol::open(address, id);

    // open/init THIS layer HERE
    CF_DL_ = 0U;

    return result;
  }


  /**
   * Called by upper layer to close this layer
   * \param channel The channel to close
   */
  virtual void close(decom::eid const& id = eid_any)
  {
    // FIRST close THIS layer HERE
    timer_TX_CF_.stop();
    timer_TX_FC_.stop();
    timer_RX_CF_.stop();

    // Close the lower layer after closing THIS layer - closing is done TOP-DOWN
    // in layer stack
    // the highest layer is closed first, the lowest layer last
    protocol::close(id);
  }


  /**
   * Called by upper layer to transmit data (message) to this protocol
   * \param data The message to send
   * \param id The endpoint identifier
   * \param more true if message is a fragment which is followed by another msg. False if no/last fragment
   * \return true if Send is successful
   */
  virtual bool send(decom::msg& data, decom::eid const& id = eid_any, bool more = false)
  {
    (void)more;

    if (data.size() > CF_MAX_FRAME_SIZE) {    // TP protocol can handle max 4095 bytes
      // data size too big
      DECOM_LOG_ERROR("msg too big (> 4095 bytes)");
      return false;
    }

    // is a msg transmission already in progress?
    if (CF_DL_ != 0U) {
      // should not happen - did you wait for tx_done ?
      DECOM_LOG_ERROR("TX already in progress");
      return false;
    }

    if (data.size() <= (use_ext_adr_ ? SF_DATALENGTH_EXT : SF_DATALENGTH)) {
      // send SF
      data.push_front(NPCI_SINGLE_FRAME | (data.size() & 0x0FU));
      if (use_ext_adr_) {
        data.push_front(ext_target_adr_);
      }
      if (use_zero_padding_) {
        data.insert(data.end(), data.size() < FRAME_LENGTH ? FRAME_LENGTH - data.size() : 0, (std::uint8_t)0);
      }
      return protocol::send(data, id);
    }
    else {
      // send FF

      CF_frame_ = data;   // store a cheap copy

      decom::msg ff;
      CF_SN_    = 1U;                                                   // init sequence number
      CF_DL_    = (use_ext_adr_ ? FF_DATALENGTH_EXT : FF_DATALENGTH);   // init FF data length
      CF_size_  = (std::uint16_t)data.size();                           // set frame size
      CF_BScnt_ = 0U;                                                   // init block counter
      CF_eid_   = id;                                                   // tx eid

      ff.push_back(NPCI_FIRST_FRAME | ((CF_size_ >> 8U) & 0x0FU));
      ff.push_back((std::uint8_t)(CF_size_));
      ff.insert(ff.end(), data.begin(), data.begin() + CF_DL_);
      if (use_ext_adr_) {
        ff.push_front(ext_target_adr_);
      }

      if (protocol::send(ff, id)) {
        // sending to lower lower layer was successful
        // start timer for FC reception check
        timer_TX_FC_.start(std::chrono::milliseconds(N_Bs), false, &iso15765::timer_func_TX_FC, this);
        return true;
      }
      else {
        // FF could not be send - abort
        CF_DL_ = 0U;
        CF_frame_.clear();
        return false;
      }
    }
  }


  /**
   * Receive function for data from lower layer
   * \param data The message to receive
   * \param id The endpoint identifier
   * \param more true if message is a fragment which is followed by another msg. False if no/last fragment
   */
  virtual void receive(decom::msg& data, decom::eid const& id = eid_any, bool more = false)
  {
    (void)more;

    if (use_ext_adr_) {
      if (data[0] != ext_source_adr_) {
        // source address mismatch, discard frame - this is no error
        return;
      }
      data.pop_front();   // strip address
    }

    // check the NPCI type
    switch(data[0] & 0xF0U) {

      case NPCI_SINGLE_FRAME :
      {
        // single frame received - just check length and pass to upper layer
        CF_DL_ = 0;
        std::uint8_t SF_DL = data[0] & 0x0FU;
        if ((SF_DL > (use_ext_adr_ ? SF_DATALENGTH_EXT : SF_DATALENGTH)) || (data.size() <= SF_DL)) {
          // error - frame length wrong, discard frame
          protocol::indication(rx_error, id);
          break;
        }

        // frame is okay
        data.pop_front();     // strip NPCI
        data.resize(SF_DL);   // resize to actual data length
        protocol::receive(data, id);
        break;
      }

      case NPCI_FIRST_FRAME :
      {
        // first frame received
        CF_DL_ = decom::util::make_large<std::uint8_t, std::uint16_t>(data[1], data[0] & 0x0FU);
        if ((CF_DL_ < (use_ext_adr_ ? FF_DATALENGTH_EXT : FF_DATALENGTH) + 2U)) {
          // error - frame length too small, discard frame
          CF_DL_ = 0U;
          CF_frame_.clear();
          protocol::indication(rx_error, id);
          break;
        }
        if (CF_DL_ > CF_MAX_DL_ || CF_DL_ > CF_MAX_BUFFER_SIZE) {
          // error - frame too big, discard frame
          CF_DL_ = 0U;
          CF_frame_.clear();
          #if defined(FC_SEND_OVERFLOW)
            // send FC
            send_FC(FC_OVERFLOW, id);
          #endif
          protocol::indication(rx_error, id);
          break;
        }

        // frame is okay
        data.pop_front();     // strip NPCI
        data.pop_front();
        data.resize(use_ext_adr_ ? FF_DATALENGTH_EXT : FF_DATALENGTH);  // resize to actual data length
        CF_frame_.copy(data); // init buffer
        CF_SN_    = 1U;       // init SN (next expected seq number)
        CF_BScnt_ = 0U;       // init block counter

        // send FC
        send_FC(FC_CTS, id);

        // trigger timeout for next CF frame reception
        timer_RX_CF_.start(std::chrono::milliseconds(N_Cr), false, &iso15765::timer_func_RX_CF, this);
        break;
      }

      case NPCI_CONSECUTIVE_FRAME :
      {
        // consecutive frame received

        // kill timer
        timer_RX_CF_.stop();

        if (CF_DL_ == 0U) {
          // no CF expected
          protocol::indication(rx_error, id);
          break;
        }

        // check sequence number
        std::uint8_t SN = data[0] & 0x0FU;
        if (SN != CF_SN_) {
          // error - wrong sequence number, discard frame and cancel reception
          CF_DL_ = 0U;
          CF_frame_.clear();
          protocol::indication(rx_error, id);
          break;
        }
        else {
          // generate next SN
          CF_SN_ = ++CF_SN_ & 0x0FU;
        }

        // frame is okay
        data.pop_front();     // strip NPCI

        // check buffer space
        if (CF_frame_.size() + data.size() <= CF_MAX_BUFFER_SIZE) {
          // append new data to buffer, don't use CF_frame_.append() here, because data is really small
          CF_frame_.insert(CF_frame_.end(), data.begin(), data.end());
        }
        else {
          // the CF is accepted, but silently discarded but the upper layer gets informed
          protocol::indication(rx_overrun, id);
        }

        // frame done?
        if (CF_frame_.size() >= CF_DL_) {
          // frame complete - send frame to upper layer
          CF_frame_.resize(CF_DL_);
          protocol::receive(CF_frame_, id);
          CF_DL_ = 0U;
          CF_frame_.clear();
          break;
        }

        if (CF_BS_ && (++CF_BScnt_ >= CF_BS_)) {
          // complete block received, send FC
          CF_BScnt_ = 0U;
          send_FC(FC_CTS, id);
        }

        // restart timer
        timer_RX_CF_.start(std::chrono::milliseconds(N_Cr), false, &iso15765::timer_func_RX_CF, this);

        break;
      }

      case NPCI_FLOW_CONTROL :
      {
        // flow control frame received

        // kill surveillance timer
        timer_TX_FC_.stop();

        if (data.size() < FC_DATALENGTH) {
          // FC frame is too short - abort
          protocol::indication(rx_error, id);
          break;
        }
        if ((data[0] & 0x0FU) > 1U) {
          // FS format error - abort
          protocol::indication(rx_error, id);
          break;
        }

        // frame is okay - store values
        FC_FS_    = data[0] & 0x01U;    // 0 = CTS (ContinueToSend), 1 = WT (Wait)
        FC_BS_    = data[1];
        FC_STmin_ = data[2];
        CF_BScnt_ = 0U;                 // reset block counter

        if (FC_FS_ == 0U) {
          // CTS set - send next consecutive frame
          timer_TX_CF_.start(std::chrono::milliseconds(FC_STmin_), false, &iso15765::timer_func_TX_CF, this);
        }

        break;
      }

      default :
        // unknown N_PCI type
        protocol::indication(rx_error, id);
        break;
    }
  }

  /**
   * Status/Error indication from lower layer
   * \param code The error code which occurred on lower layer
   * \param id The endpoint identifier
   */
  virtual void indication(status_type code, decom::eid const& id = eid_any)
  {
    if (code == tx_done) {
      // message sent, process next segment
      tx_ev_.set();
    }

    protocol::indication(code, id);
  }

////////////////////////////////////////////////////////////////////////

  /**
   * Extended addressing setup
   * \param use_extended true to activate extended addressing, false to deactivate it
   * \param source_adr Source address, the first byte of a received msg is checked against this byte
   * \param target_adr Target address, used as first byte in a transmitted message
   */
  void extended_addressing(bool use_extended, std::uint8_t source_adr, std::uint8_t target_adr)
  {
    use_ext_adr_    = use_extended;
    ext_source_adr_ = source_adr;
    ext_target_adr_ = target_adr;
  }


  /**
   * Zero padding usage
   * set this to true to use zero padding. All messages with length < 8 are padded with zeros to length of 8
   */
  bool use_zero_padding_;


private:
  bool send_CF()
  {
    decom::msg cf;

    cf.push_back(NPCI_CONSECUTIVE_FRAME | (CF_SN_ & 0x0FU));
    cf.insert(cf.end(), CF_frame_.begin() + CF_DL_, (CF_DL_ + (use_ext_adr_ ? CF_DATALENGTH_EXT : CF_DATALENGTH) < CF_frame_.size()) ? CF_frame_.begin() + CF_DL_ + (use_ext_adr_ ? CF_DATALENGTH_EXT : CF_DATALENGTH) : CF_frame_.end());
    if (use_ext_adr_) {
      cf.push_front(ext_target_adr_);
    }
    if (use_zero_padding_) {
      cf.insert(cf.end(), cf.size() < FRAME_LENGTH ? FRAME_LENGTH - cf.size() : 0,  (std::uint8_t)0);
    }

    // wait for tx_done
    if (tx_ev_.wait_for(std::chrono::milliseconds(N_As)) != std::cv_status::no_timeout) {
      // timeout - abort frame transmission
      CF_DL_ = 0U;
      CF_frame_.clear();
      DECOM_LOG_ERROR("Frame tx aborted");
      protocol::indication(tx_timeout);   // inform upper layer
      return false;
    }
    tx_ev_.reset();

    if (protocol::send(cf, CF_eid_)) {
      // sending to lower layer was successful

      CF_SN_++;
      CF_DL_ += (use_ext_adr_ ? CF_DATALENGTH_EXT : CF_DATALENGTH);

      // check if frame is complete
      if (CF_DL_ >= CF_size_) {
        // frame completely sent
        CF_DL_ = 0U;
        CF_frame_.clear();
        return true;
      }

      // check BS
      if (FC_BS_ && (CF_BScnt_++ >= FC_BS_)) {
        // block completely sent - wait for FC from receiver

        // trigger timer for next FC reception
        timer_TX_FC_.start(std::chrono::milliseconds(N_Bs), false, &iso15765::timer_func_TX_FC, this);
      }
      else {
        // trigger timer for next CF frame
        timer_TX_CF_.start(std::chrono::milliseconds(FC_STmin_), false, &iso15765::timer_func_TX_CF, this);
      }

      return true;
    }
    else {
      // transmission error on lower layer - two strategies here:
      // 1. restart STmin timer to try again (lower layer e.g. CAN may be busy)
      // 2. abort frame transmission
    #if(1)
      // retrigger timer for next CF frame
      timer_TX_CF_.start(std::chrono::milliseconds(FC_STmin_), false, &iso15765::timer_func_TX_CF, this);
    #else
      // abort frame transmission
      CF_DL_ = 0U;
      CF_frame_.clear();
      protocol::indication(tx_error);   // inform upper layer
    #endif
      return false;
    }
  }


  void iso15765::send_CF_abort()
  {
    CF_DL_ = 0U;
    CF_frame_.clear();
    DECOM_LOG_NOTICE("CF frame abort");

    // inform upper layer
    protocol::indication(rx_timeout);
  }


  bool iso15765::send_FC(std::uint8_t FS, decom::eid const& id)
  {
    decom::msg fc;

    fc.push_back(NPCI_FLOW_CONTROL | (FS & 0x0FU));
    fc.push_back(CF_BS_);
    fc.push_back(CF_STmin_);
    if (use_ext_adr_) {
      fc.push_front(ext_target_adr_);
    }
    if (use_zero_padding_) {
      fc.insert(fc.end(), fc.size() < FRAME_LENGTH ? FRAME_LENGTH - fc.size() : 0, (std::uint8_t)0);
    }

    return protocol::send(fc, id);
  }


  decom::msg    CF_frame_;                  // buffer for consecutive frames
  decom::eid    CF_eid_;                    // CF tx eid
  std::uint8_t  CF_SN_;                     // frame sequence number
  std::uint16_t CF_DL_;                     // actual sent data length
  std::uint16_t CF_size_;                   // complete frame size
  std::uint16_t CF_MAX_DL_;                 // maximum data length

  std::uint8_t CF_STmin_;                   // own STmin parameter, send to peer
  std::uint8_t CF_BS_;                      // own BS parameter, send to peer
  std::uint8_t CF_BScnt_;                   // own BS counter, internal

  std::uint8_t FC_STmin_;                   // STmin parameter, received by FC frame
  std::uint8_t FC_BS_;                      // BS parameter, received by FC frame
  std::uint8_t FC_FS_;                      // FS parameter, received by FC frame

  bool         use_ext_adr_;                // use extended addressing
  std::uint8_t ext_source_adr_;             // extended addressing source address
  std::uint8_t ext_target_adr_;             // Extended addressing target address


  // timer

  static void timer_func_TX_CF(void* arg)
  {
    // STmin expired, send next consecutive frame
    ((iso15765*)arg)->send_CF();
  }


  static void timer_func_TX_FC(void* arg)
  {
    // waiting for FC frame expired, this is an error condition
    // the upper layer needs to be informed that the receiver didn't answer
    ((iso15765*)arg)->send_CF_abort();
  }


  static void timer_func_RX_CF(void* arg)
  {
    // waiting for next CF frame from sender expired, this is an error condition
    // the upper layer needs to be informed that the sender has a timeout
    ((iso15765*)arg)->send_CF_abort();
  }

  util::timer timer_TX_CF_;                 // TX CF timer
  util::timer timer_TX_FC_;                 // TX FC timer
  util::timer timer_RX_CF_;                 // RX CF timer
  util::event tx_ev_;                       // transmit done indication
};

} // namespace prot
} // namespace decom

#endif // _DECOM_PROT_ISO15765_H_
