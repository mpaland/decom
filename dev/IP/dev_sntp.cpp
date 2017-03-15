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
///////////////////////////////////////////////////////////////////////////////

#include "dev_sntp.h"

#include <string.h>
#include <sstream>

/////////////////////////////////////////////////////////////////////

namespace decom {
namespace dev {

/////////////////////////////////////////////////////////////////////
// NTP Constants

// LI
#define NTP_LI_NO_WARN      0U
#define NTP_LI_61_SEC       1U
#define NTP_LI_59_SEC       2U
#define NTP_LI_ALARM        3U

// mode
#define NTP_MODE_SYM_ACT    1U
#define NTP_MODE_SYM_PAS    2U
#define NTP_MODE_CLIENT     3U
#define NTP_MODE_SERVER     4U
#define NTP_MODE_BROADCAST  5U

#define NTP_PORT            123U
#define NTP_VERSION         4U
#define NTP_HEADER_LEN      48U
#define NTP_HEADER_EXT_LEN  68U


// defines the maximum retries before failure
#define MAX_RETRIES         3U

// defines the maximum server response time before timeout im [ms]
#define RESPONSE_TIMEOUT    2000U

// period from 1900 - 1970 in [µs]
#define JAN_1970  (std::uint64_t)2208988800000000ULL


sntp::sntp(layer* lower)
 : device(lower, "dev_sntp")    // it's VERY IMPORTANT to call the base class ctor HERE!!!
{
  retries_ = 0U;
}


sntp::~sntp()
{
  // call close() here to make sure the stack is closed before deleting device
  close();
}


bool sntp::open(const char* address, eid const&)
{
  // assemble host address
  std::stringstream host;
  host << address << ":" << static_cast<int>(NTP_PORT);

  // eid is unused, using eid ANY
  return device::open(host.str().c_str());
}


// receive function for data from lower layer
void sntp::receive(msg& data, eid const&, bool)
{
  // stop the timer
  timer_.stop();

  // get destination (local time)
  dest_timestamp_.set(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()) + std::chrono::microseconds(JAN_1970));

  // check header size
  if ((data.size() != NTP_HEADER_LEN) && (data.size() != NTP_HEADER_EXT_LEN)) {
    // wrong header length - discard
    DECOM_LOG_ERROR("Invalid header length: << " << (int)data.size() << ", resonse discarded");
    timeout(this);  // restart
    return;
  }

  // get header
  data.get(reinterpret_cast<std::uint8_t*>(&ntp_header_), sizeof(ntp_header_));

  // basic checks
  if ((ntp_header_.ref_timestamp.fixpt  == 0U) || (ntp_header_.orig_timestamp.fixpt == 0U) ||
      (ntp_header_.recv_timestamp.fixpt == 0U) || (ntp_header_.send_timestamp.fixpt == 0U) ||
      ((ntp_header_.mode & 0x07U) != NTP_MODE_SERVER)) {
    // invalid data in header
    DECOM_LOG_ERROR("Invalid data in header, response discarded");
    timeout(this);  // restart
    return;
  }

  // set event
  rx_ev_.set();
}


// error indication from lower layer
void sntp::indication(status_type, eid const&)
{
  // TBD: connected indication must be evaluated
}


///////// A P P L I C A T I O N   I N T E R F A C E /////////////////

bool sntp::get_time(std::time_t& time)
{
  // send request
  retries_ = 0;
  if (!request()) {
    return false;
  }

  // wait for answer, in case of timeout event is also set
  rx_ev_.wait();

  if (retries_ >= MAX_RETRIES) {
    // no time received
    time = 0U;
    return false;
  }

  // convert timestamps from net to host order
  ntp_header_.ref_timestamp.ntoh();
  ntp_header_.orig_timestamp.ntoh();
  ntp_header_.recv_timestamp.ntoh();
  ntp_header_.send_timestamp.ntoh();

  DECOM_LOG_DEBUG("Leap Indicator: " << static_cast<int>(ntp_header_.mode >> 6U));
  DECOM_LOG_DEBUG("NTP Stratum   : " << static_cast<int>(ntp_header_.stratum));
  DECOM_LOG_DEBUG("Reference ID  : " << std::hex << ntp_header_.ref_id);

  // round trip delay = (dest - orig) - (send - recv)
  ntp_timestamp_type round_trip = (dest_timestamp_ - ntp_header_.orig_timestamp) -
                                  (ntp_header_.send_timestamp - ntp_header_.recv_timestamp);
  // offset = ((recv - orig) + (send - dest)) / 2
  ntp_timestamp_type offset_ts  = (ntp_header_.recv_timestamp - ntp_header_.orig_timestamp) +
                                  (ntp_header_.send_timestamp - dest_timestamp_);
  struct fixpt_t<std::uint64_t, std::int32_t, std::uint32_t> offset;
  offset.fixpt = offset_ts.fixpt;

  DECOM_LOG_DEBUG("Round trip    : " << round_trip.get().count() / 1000U << " ms");
  DECOM_LOG_DEBUG("Clock offset  : " << offset.get().count()     / 2000U << " ms");

  std::chrono::system_clock::time_point tp = std::chrono::system_clock::now() + offset.get() / 2;
  time = std::chrono::system_clock::to_time_t(tp);
  DECOM_LOG_INFO("Actual time   : " << std::ctime(&time));

  return true;
}


bool sntp::request()
{
  // assemble header
  ntp_header_.mode                  = (NTP_LI_NO_WARN << 6U) | (NTP_VERSION << 3U) | NTP_MODE_CLIENT;
  ntp_header_.stratum               = 0U; // all other fields are unused in client mode
  ntp_header_.poll                  = 0U;
  ntp_header_.precision             = 0U;
  ntp_header_.root_delay.fixpt      = 0U;
  ntp_header_.root_dispersion.fixpt = 0U;
  ntp_header_.ref_id                = 0U;
  ntp_header_.ref_timestamp.fixpt   = 0U;
  ntp_header_.orig_timestamp.fixpt  = 0U;
  ntp_header_.recv_timestamp.fixpt  = 0U;
  ntp_header_.send_timestamp.set(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()) + std::chrono::microseconds(JAN_1970));
  ntp_header_.send_timestamp.hton();

  decom::msg buf;
  buf.put(reinterpret_cast<std::uint8_t*>(&ntp_header_), sizeof(ntp_header_));

  // start timer
  timer_.start(std::chrono::milliseconds(RESPONSE_TIMEOUT), false, timeout, this);

  // reset receive event
  rx_ev_.reset();

  DECOM_LOG_INFO("Sending request");
  return device::send(buf);
}


// response timeout handler
void sntp::timeout(void* arg)
{
  // response timeout
  sntp* s = static_cast<sntp*>(arg);

  if (s->retries_++ < MAX_RETRIES) {
    // restart
    DECOM_LOG2(DECOM_LOG_LEVEL_WARN, s->name_, "No answer from NTP server received, " << s->retries_ << ". retry now");
    s->request();
  }
  else {
    // max retries reached
    DECOM_LOG2(DECOM_LOG_LEVEL_WARN, s->name_, "Still no answer from NTP server, giving up now");
    s->rx_ev_.set();
  }
}


} // namespace dev
} // namespace decom
