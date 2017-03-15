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
// \brief SNTP device class
//
// This is the implementation of the SNTPv4 protocol to retrieve the actual
// time of a NTP/SNTP server.
// 
///////////////////////////////////////////////////////////////////////////////

#ifndef _DECOM_DEV_SNTP_H_
#define _DECOM_DEV_SNTP_H_

#include "dev.h"

#include <ctime>    // for time structure

/////////////////////////////////////////////////////////////////////

namespace decom {
namespace dev {


class sntp : public device
{
public:
  /**
   * Device ctor
   * \param lower Lower layer
   */
  sntp(decom::layer* lower);

  /**
   * dtor
   */
  virtual ~sntp();

  /**
   * Called by the application to open this stack. Device is the highest layer
   * \param address The STNP server address
   * \param id unused
   * \return true if open is successful
   */
  virtual bool open(const char* address = "", eid const& id = eid_any);

  /**
   * Receive function for data from lower layer
   * \param data The message to receive
   * \param id The endpoint identifier
   * \param more true if message is a fragment which is followed by another msg. False if no/last fragment
   */
  virtual void receive(msg& data, eid const& id = eid_any, bool more = false);

  /**
   * Status/Error indication from lower layer
   * \param code The status code which occurred on lower layer
   * \param id The endpoint identifier
   */
  virtual void indication(status_type code, eid const& id = eid_any);

  ////////////////////////////////////////////////////////////////////////
  // D E V I C E   A P I

  /**
   * Read the actual time from the opened time server.
   * This is a blocking function
   * \param time UTC date/time in seconds since epoch (1.1.1970)
   * \return true if successful
   */
  bool get_time(std::time_t& time);

private:
  // POD fixed-point struct
  template<typename U, typename I, typename F>
  struct fixpt_t {
    union {
      U fixpt;
      struct {
        I integer;
        F fraction;
      } part;
    };

    fixpt_t operator-(const fixpt_t& rhs)
    {
      fixpt_t tmp;
      tmp.part.fraction = part.fraction - rhs.part.fraction;
      tmp.part.integer  = part.integer  - rhs.part.integer - (part.fraction < rhs.part.fraction ? 1U : 0U);
      return tmp;
    }
    fixpt_t operator+(const fixpt_t& rhs)
    {
      fixpt_t tmp;
      tmp.part.fraction = part.fraction + rhs.part.fraction;
      tmp.part.integer  = part.integer  + rhs.part.integer + ((tmp.part.fraction < part.fraction) && (tmp.part.fraction <rhs.part.fraction) ? 1U : 0U);
      return tmp;
    }

    // return value in microseconds
    std::chrono::microseconds get() const {
      std::chrono::microseconds time(static_cast<std::uint64_t>(part.integer * 1000000ULL) + static_cast<std::uint64_t>(part.fraction * 500000ULL) / static_cast<std::uint64_t>((static_cast<F>(-1) >> 1U) + 1U));
      return time;
    }

    // set value in microseconds
    void set(std::chrono::microseconds time) {
      part.integer  = static_cast<I>(std::chrono::duration_cast<std::chrono::seconds>(time).count());
      part.fraction = static_cast<F>(std::chrono::duration_cast<std::chrono::microseconds>(time).count() % 1000000ULL);
      part.fraction = static_cast<F>(static_cast<std::uint64_t>(part.fraction) * static_cast<std::uint64_t>((static_cast<F>(-1) >> 1U) + 1U) / 500000ULL);
    }

    // convert to and from network/host order
    void ntoh() {
      part.integer  = decom::util::net::ntoh<I>(part.integer);
      part.fraction = decom::util::net::ntoh<F>(part.fraction);
    }
    void hton() {
      ntoh();
    }
  };

  typedef struct fixpt_t<std::uint64_t, std::uint32_t, std::uint32_t> ntp_timestamp_type;

  // NTP header
  typedef struct struct_ntp_header_type
  {
    std::uint8_t  mode;
    std::uint8_t  stratum;
    std::uint8_t  poll;
    std::uint8_t  precision;
    struct fixpt_t<std::int32_t, std::int16_t, std::uint16_t> root_delay;
    struct fixpt_t<std::int32_t, std::int16_t, std::uint16_t> root_dispersion;
    std::uint32_t ref_id;
    ntp_timestamp_type ref_timestamp;
    ntp_timestamp_type orig_timestamp;
    ntp_timestamp_type recv_timestamp;
    ntp_timestamp_type send_timestamp;
  } ntp_header_type;

  ntp_header_type    ntp_header_;         // send/receive header
  decom::util::timer timer_;              // timeout for response
  decom::util::event rx_ev_;              // receive event
  std::uint8_t       retries_;            // request retries
  ntp_timestamp_type dest_timestamp_;     // destination time

  static void timeout(void* arg);         // response timeout handler
  bool request();
};


} // namespace dev
} // namespace decom

#endif //_DECOM_DEV_SNTP_H_
