///////////////////////////////////////////////////////////////////////////////
// \author (c) Marco Paland (info@paland.com)
//             2013-2017, PALANDesign Hannover, Germany
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
// \brief Base class for devices
//
// This class is used for devices (layer 7). Devices have no upper layer.
// All devices are derived from this class.
//
// \brief Internet communication class
//
// This class is used for client or server TCP or UDP connections over IP
// The Winsock2 API is used.
//
// TCP server (multi connections):
// com_inet (true, true)
// open("'localhost' or local ip : listen_port", eid_any)  --> indication: eid is (client addr/port)
// receive(data, eid (client addr/port))
// send(data, eid (client addr/port))
//
// UDP server (multi connections):
// com_inet (false, true)
// open("'localhost' or local ip : listen_port", eid_any)  --> indication: eid is (client addr/port)
// receive(data, eid (client addr/port))
// send(data, eid(client addr/port))
//
// TCP client (one connection):
// open("host:port", eid_any)                              --> indication: eid is eid_any 
// receive(data, eid_any)
// send(data, eid_any)
//
// UDP client (one connection)
// open("host:port", eid_any)                              --> indication: eid is eid_any
// receive(data, eid_any)
// send(data, eid_any)
//
// eid IP addr is stored in network format
// eid IP port is stored in host format
//
// \todo - Implementation of ::AcceptEx to be more robust against DOS and high server load
//
///////////////////////////////////////////////////////////////////////////////

#ifndef _DECOM_COM_INET_H_
#define _DECOM_COM_INET_H_


#define _WINSOCKAPI_    // stops windows.h including old winsock.h
#include <Windows.h>
#include <WinSock2.h>
#include <vector>
#include <map>
#include <sstream>

#include "com.h"


// defines the size of the receive buffer
#define COM_INET_RX_BUFFER_SIZE         8192U

// defines the number of threads per processor in server mode, 2 is a good default
#define COM_INET_THREADS_PER_PROCESSOR  2U

/////////////////////////////////////////////////////////////////////

namespace decom {
namespace com {

class inet : public communicator
{
public:
  /**
   * Normal ctor
   * \param tcp true for TCP protocol, false for UDP protocol
   * \param server TCP: true for listening server, false for connecting client. UDP: true for send/revc, false for send only
   * \param ipv6 true for IPv6 protocol, false for IPv4, default is IPv4
   * \param name Layer name
   */
  inet(bool tcp, bool server = false, bool ipv6 = false, const char* name = "com_inet");

  /**
   * dtor
   */
  virtual ~inet();

  /**
   * Called by upper layer to open this layer
   * \param address The address to open in the form of "host:port", "IP:port" (IPv4) or "[IP]:port" (IPv6)
   *                Server mode: Defines the local interface and listen port
   *                Client mode: Defines the client host and port, the local interface may be set via set_source_address
   * \param id Unused
   * \return true if open is successful
   */
  virtual bool open(const char* address = "", decom::eid const& id = eid_any);

  /**
   * Called by upper layer to close this layer
   * \param id Unused (all open eids are closed)
   */
  virtual void close(decom::eid const& id = eid_any);

  /**
   * Called by upper layer to transmit data to the internet
   * \param data The message to send
   * \param id The endpoint identifier, use eid_any for default socket
   * \param more true if message is a fragment - mostly unused on this layer
   * \return true if Send is successful
   */
  virtual bool send(decom::msg& data, decom::eid const& id = eid_any, bool more = false);

  ////////////////////////////////////////////////////////////////////////
  // C O M M U N I C A T O R   A P I

  /**
   * TCP/UDP CLIENT mode: Set the client (source) address/port in and define other address/port than default.
   * Set this BEFORE calling open()!
   * \param address The source address in the form of "host:port", "IP:port" (IPv4) or "[IP]:port" (IPv6)
   */
  void set_source_address(const char* address = "");

  ////////////////////////////////////////////////////////////////////////
public:
  static void accept_thread(void* arg);
  static void worker_thread(void* arg);

private:
  std::vector<std::thread*> worker_threads_;    // pool of worker threads
  std::thread* accept_thread_;                  // accept thread

  bool   use_tcp_;
  bool   use_ipv6_;
  bool   server_;
  SOCKET socket_;             // socket
  HANDLE completion_port_;    // completion port
  std::string source_addr_;   // source addr

  typedef struct struct_io_data_type {
    OVERLAPPED        overlapped;
    WSABUF            wsa_buffer;
    char              buffer[COM_INET_RX_BUFFER_SIZE];
    SOCKADDR_STORAGE  from_addr;
    int               from_len;
    bool              send;
  } io_data_type;

  typedef struct struct_client_context_type {
    io_data_type recv;
    io_data_type send;
    SOCKET       socket;    // associated accept socket
    decom::eid   id;        // eid of the socket, here: address = IP, port = port
  } client_context_type;

  typedef std::map<decom::eid, client_context_type*> client_contexts_type;

  client_contexts_type client_contexts_;
  std::mutex client_contexts_mutex_;

  // helper
  std::stringstream format_eid(decom::eid const& id) const;
  std::stringstream address_to_string(SOCKADDR_STORAGE* addr) const;
  PADDRINFOA resolve_address(const char* address) const;
};

} // namespace com
} // namespace decom

#endif    // _DECOM_COM_INET_H_
