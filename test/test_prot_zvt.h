#ifndef _DECOM_TEST_PROT_ZVT_H_
#define _DECOM_TEST_PROT_ZVT_H_

#include "../src/ext/ZVT/prot_zvt.h"
#include "../src/com/com_generic.h"
#include "../src/dev/dev_generic.h"
#include "test.h"


namespace decom {
namespace test {

const std::uint8_t cmd1_req[] = { 0x06, 0x93, 0x03, 0x12, 0x34, 0x56 };
const std::uint8_t cmd1_raw[] = { 0x10, 0x02, 0x06, 0x93, 0x03, 0x12, 0x34, 0x56, 0x10, 0x03, 0xCA, 0xA4 };
const std::uint8_t res1_raw[] = { 0x10, 0x02, 0x80, 0x00, 0x00, 0x10, 0x03, 0xF5, 0x1F };
const std::uint8_t cmd2_req[] = { 0x06, 0x01, 0x0A, 0x04, 0x00, 0x00, 0x00, 0x01, 0x10, 0x00, 0x49, 0x09, 0x78 };
const std::uint8_t cmd2_raw[] = { 0x10, 0x02, 0x06, 0x01, 0x0A, 0x04, 0x00, 0x00, 0x00, 0x01, 0x10, 0x10, 0x00, 0x49, 0x09, 0x78, 0x10, 0x03, 0xF2, 0xFF };

class prot_zvt : public test
{
  // TEST CASES
public:
  prot_zvt(std::ostream& result_file, format_type format)
   : test("prot_intel_hex", result_file, format)
  {
    test1();
  }

protected:

  static void test1_com_callback(void* arg, decom::msg& data, decom::eid const& id, bool)
  {
    decom::com::generic* c = static_cast<decom::com::generic*>(arg);

    // check if cmd1
    decom::msg c1;
    c1.put((std::uint8_t*)cmd1_raw, sizeof(cmd1_raw));
    if (c1 == data) {
      decom::log(DECOM_LOG_LEVEL_INFO, "cmd1 received, sending answer");
      decom::msg m;
      m.push_back(0x06U);
      c->communicator::receive(m, id);
      decom::util::timer::sleep(std::chrono::milliseconds(100));

      m.put((std::uint8_t*)res1_raw, sizeof(res1_raw));
      c->communicator::receive(m, id);
      decom::util::timer::sleep(std::chrono::milliseconds(100));
    }
  }


  void test1()
  {
    TEST_BEGIN("test1");

    decom::com::generic com_gen;
    decom::prot::debug dbg(&com_gen);
    decom::prot::zvt zvt(&dbg);
    decom::dev::generic dev_gen(&zvt);

    com_gen.set_receive_callback(&com_gen, test1_com_callback);

    decom::msg m;
    m.put((std::uint8_t*)cmd1_req, sizeof(cmd1_req));
    TEST_CHECK(dev_gen.open());
    TEST_CHECK(dev_gen.write(m));
    decom::util::timer::sleep(std::chrono::seconds(20));

    TEST_CHECK(rx_msg_.size() == 12);
    decom::msg::value_type buf[12];
    rx_msg_.get(buf, 12);
    rx_msg_.clear();
    TEST_CHECK(memcmp(buf, cmd1_raw, 12) == 0);
    dev_gen.close();

    m.put((std::uint8_t*)cmd2_req, sizeof(cmd2_req));
    TEST_CHECK(dev_gen.open());
    TEST_CHECK(dev_gen.write(m));
    TEST_CHECK(rx_msg_.size() == 20);
    decom::msg::value_type buf2[20];
    rx_msg_.get(buf2, 20);
    TEST_CHECK(memcmp(buf2, cmd2_raw, 20) == 0);

    TEST_END;
  }

private:
  decom::msg rx_msg_;
};

} // namespace test
} // namespace decom

#endif  // _DECOM_TEST_PROT_ZVT_H_
