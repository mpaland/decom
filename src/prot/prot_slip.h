///////////////////////////////////////////////////////////////////////////////
// \author (c) Marco Paland (info@paland.com)
//             2011-2021, PALANDesign Hannover, Germany
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
// \brief SLIP protocol
//
// This class implements the SLIP protocol after rfc1055
// It's used to transfer discrete packets over byte streams and serial lines.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef _DECOM_PROT_SLIP_H_
#define _DECOM_PROT_SLIP_H_

#include "../prot.h"


/////////////////////////////////////////////////////////////////////

namespace decom {
namespace prot {


class slip : public protocol
{
private:
  // SLIP special character codes
  static const std::uint8_t END     = 0xC0U;    // indicates start/end of packet
  static const std::uint8_t ESC     = 0xDBU;    // indicates byte stuffing
  static const std::uint8_t ESC_END = 0xDCU;    // ESC ESC_END means "END" data byte
  static const std::uint8_t ESC_ESC = 0xDDU;    // ESC ESC_ESC means "ESC" data byte

  // receive state
  typedef enum enum_rx_state_type {
    RCV_IDLE = 0,     // idle, wait for starting END delimiter
    RCV_DATA,         // receive data
    RCV_ESC           // ESC delimiter received
  } rx_state_type;

  rx_state_type rx_state_;      // receive message state
  bool          is_open_;       // true if layer is open

  decom::msg    rx_msg_;        // receive message buffer
  decom::msg    tx_msg_;        // transmit message buffer


public:

  /**
   * Protocol ctor
   * \param lower Lower layer
   * \param name Layer name
   */
  slip(decom::layer* lower, const char* name = "prot_slip")
    : protocol(lower, name)
    , rx_state_(RCV_IDLE)
    , is_open_(false)
  {
    // SLIP has no defined MTU, limit is the maximum message buffer size
    mtu() = 0U;
  }


  /**
   * dtor
   */
  virtual ~slip()
  { }


  /**
   * Called by upper layer to open this layer
   * \param address The address to open
   * \param id The endpoint identifier, normally not used in open()
   * \return true if open is successful
   */
  virtual bool open(const char* address = "", decom::eid const& id = eid_any)
  {
    // for security: check that upper protocol/device exists
    if (!upper_) {
      return false;
    }

    // open the lower layer - opening is done top-down in layer stack
    is_open_ = protocol::open(address, id);

    // reset rx state
    rx_state_ = RCV_IDLE;

    return is_open_;
  }


  /**
   * Called by upper layer to close this layer
   * \param id The endpoint identifier to close
   */
  virtual void close(decom::eid const& id)
  {
    // this layer is closed
    is_open_ = false;

    // clear buffer
    rx_msg_.clear();
    tx_msg_.clear();

    // reset rx state
    rx_state_ = RCV_IDLE;

    // close lower layer
    protocol::close(id);
  }


  /**
   * Called by upper layer to transmit a packet (message) to this protocol
   * \param packet The packet to send
   * \param id The endpoint identifier
   * \param more true if the packet is a fragment which is followed by another fragment. False if no/last fragment
   * \return true if Send is successful
   */
  virtual bool send(decom::msg& packet, decom::eid const& id = eid_any, bool more = false)
  {
    if (!is_open_) {
      // layer is not open, don't send anything
      return false;
    }

    // a packet has always this form:
    // END DATA END
    // if the END byte occurs in the data to be sent, the two byte sequence ESC, ESC_END is sent instead,
    // if the ESC byte occurs in the data, the two byte sequence ESC, ESC_ESC is sent.
    if (tx_msg_.empty()) {
      // send an initial END character to flush out any data that may have accumulated in the receiver due to line noise
      tx_msg_.push_front(END);
    }

    // insert ESC_END or ESC_ESC
    for (auto it : packet) {
      switch (it) {
        // if it's the same code as an END character, we send a special two character code so as not to make the receiver think we sent an END
        case END :
          tx_msg_.push_back(ESC);
          tx_msg_.push_back(ESC_END);
          break;
        // if it's the same code as an ESC character, we send a special two character code so as not to make the receiver think we sent an ESC
        case ESC :
          tx_msg_.push_back(ESC);
          tx_msg_.push_back(ESC_ESC);
          break;
        default :
          tx_msg_.push_back(it);
          break;
      }
    }

    if (!more) {
      // tell the receiver that we're done sending the packet
      tx_msg_.push_back(END);

      // send the packet to lower layer
      const bool res = protocol::send(tx_msg_, id);

      // clear buffer
      tx_msg_.clear();

      return res;
    }
    else {
      // signal upper layer that the fragment is processed
      protocol::indication(tx_done, id);
      return true;
    }
  }


  /**
   * Receive function for data from lower layer
   * \param data The byte stream to receive
   */
  virtual void receive(decom::msg& data, decom::eid const& id = eid_any, bool more = false)
  {
    (void)id; (void)more;

    if (!is_open_) {
      // layer is not open, ignore reception
      return;
    }

    // scan and process incoming byte stream
    for (const auto& it : data) {
      switch (rx_state_)
      {
        case RCV_IDLE :
          // wait for starting END delimiter
          if (it == END) {
            rx_state_ = RCV_DATA;           // packet data start
          }
          break;
        case RCV_DATA :
          // packet receive
          if (it == ESC) {
            // ESC detected
            rx_state_ = RCV_ESC;
            break;
          }
          if (it == END) {
            // END delimiter detected
            if (rx_msg_.size()) {           // discard empty packets
              protocol::receive(rx_msg_);   // pass packet to upper layer
              rx_msg_.clear();              // reset receiver buffer
             }
             rx_state_ = RCV_IDLE;          // reset state
            break;
          }
          // append data
          rx_msg_.push_back(it);
          break;
        case RCV_ESC :
          if (it == ESC_END) {
            rx_msg_.push_back(END);         // "END" data byte received
            rx_state_ = RCV_DATA;           // continue packet data
            break;
          }
          if (it == ESC_ESC) {
            rx_msg_.push_back(ESC);         // "ESC" data byte received
            rx_state_ = RCV_DATA;           // continue packet data
            break;
          }
          // error, char not expected, discard packet
          DECOM_LOG_ERROR("Unexpected char in message, discarding packet");
          rx_msg_.clear();                  // reset receiver buffer
          rx_state_ = RCV_IDLE;             // reset state
          break;
        default :
          // unknown state. should not happen, reset anyway
          DECOM_LOG_ERROR("Unknown rx_state_ ") << rx_state_;
          rx_msg_.clear();                  // reset receiver buffer
          rx_state_ = RCV_IDLE;             // reset state
          break;
      }
    }
  }

};

} // namespace prot
} // namespace decom

#endif // _DECOM_PROT_SLIP_H_
