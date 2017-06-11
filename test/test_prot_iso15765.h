#ifndef _DECOM_TEST_ISO15765_H_
#define _DECOM_TEST_ISO15765_H_

#include "../src/prot/automotive/prot_iso15765.h"
#include "../src/prot/prot_debug.h"
#include "../src/com/com_generic.h"
#include "../src/com/com_loopback.h"
#include "../src/dev/dev_generic.h"
#include "test.h"


namespace decom {
namespace test {


class prot_iso15765 : public test
{
  // TEST CASES
public:
  prot_iso15765(std::ostream& result_file, format_type format)
    : test("iso15765", result_file, format)
  {
    SFtest();
    CFtest();
    looptest();
  }


protected:

  void looptest()
  {
    TEST_BEGIN("looptest");
    DECOM_LOG_NOTICE2("looptest", "iso15765 test");

    decom::com::loopback loop1;
    loop1.name_ = "L1";
    decom::com::loopback loop2;
    loop2.name_ = "L2";
    loop1.register_loopback(&loop2);
    loop2.register_loopback(&loop1);

    decom::prot::debug dbg1(&loop1);
    decom::prot::debug dbg2(&loop2);

    decom::prot::iso15765 tp1(&dbg1, 50, 3, 4095);
    decom::prot::iso15765 tp2(&dbg2, 50, 3, 4095);
/*
    decom::prot::iso15765 tp1(&loop1, 50, 3);
    decom::prot::iso15765 tp2(&loop2, 50, 3);
*/
    decom::dev::generic gen1(&tp1);
    decom::dev::generic gen2(&tp2);

    gen1.open();
    gen2.open();

    decom::msg tx, rx;
    decom::eid id = 0;
    for (int i = 0; i < 1000U; i++) {
      tx.push_back((std::uint8_t)i);
    }

    gen1.write(tx, id);
    decom::util::timer::sleep(std::chrono::milliseconds(100));
    gen2.read(rx, id, std::chrono::seconds(100));

    TEST_END;
  }


  void SFtest()
  {
    TEST_BEGIN("SF Test");
    DECOM_LOG_NOTICE2("SF test", "iso15765 test");

    decom::com::generic gen1;
    decom::prot::debug dbg1(&gen1);
    decom::prot::iso15765 tp(&dbg1, 50, 3, 4095);
    decom::dev::generic gen2(&tp);

    gen2.open();

    decom::msg tx, rx;
    decom::eid id;
    tx.push_back(1);
    tx.push_back(5);
    tx.push_back(9);
    gen2.write(tx, 10, false, false);
    bool more;
    gen1.read(rx, id, more);

    TEST_CHECK(id == 10);
    TEST_CHECK(more == false);
    TEST_CHECK(rx[0] == 3);
    TEST_CHECK(rx[1] == 1);
    TEST_CHECK(rx[2] == 5);
    TEST_CHECK(rx[3] == 9);
    TEST_CHECK(rx.size() == 4);
    TEST_END;
  }


  void CFtest()
  {
    TEST_BEGIN("CF Test");
    DECOM_LOG_NOTICE2("CF test", "iso15765 test");

    decom::com::generic gen_com;
    decom::prot::debug dbg1(&gen_com);
    decom::prot::iso15765 tp(&dbg1, 50, 3, 4095);
    decom::dev::generic gen_dev(&tp);
    tp.set_zero_padding(false);

    gen_dev.open();

    decom::msg tx, rx;
    decom::eid id = 0;
    tx.push_back(1);
    tx.push_back(2);
    tx.push_back(3);
    tx.push_back(4);
    tx.push_back(5);
    tx.push_back(6);
    tx.push_back(7);
    tx.push_back(8);
    gen_dev.write(tx, 10, false, false);  // write 8 bytes to channel 10

    bool more;
    gen_com.read(rx, id, more);
    TEST_CHECK(id == 10);
    TEST_CHECK(more == false);
    TEST_CHECK(rx[0] == 0x10);
    TEST_CHECK(rx[1] == 0x08);
    TEST_CHECK(rx[2] == 0x01);
    TEST_CHECK(rx[3] == 0x02);
    TEST_CHECK(rx[4] == 0x03);
    TEST_CHECK(rx[5] == 0x04);
    TEST_CHECK(rx[6] == 0x05);
    TEST_CHECK(rx[7] == 0x06);
    TEST_CHECK(rx.size() == 8);

    // tx still in use, can't be changed (this generates an ERROR in log, don't worry)
    tx.push_back(9);
    TEST_CHECK(tx.size() == 8);

    // send FC frame manually
    decom::msg fc;
    fc.push_back(0x30);
    fc.push_back(0x0);
    fc.push_back(0x0);
    gen_com.write(fc, 1);

    decom::util::timer::sleep(std::chrono::milliseconds(100));

    gen_com.read(rx, id, more);
    TEST_CHECK(id == 10);
    TEST_CHECK(more == false);
    TEST_CHECK(rx[0] == 0x21);
    TEST_CHECK(rx[1] == 0x07);
    TEST_CHECK(rx[2] == 0x08);
    TEST_CHECK(rx.size() == 3);

    TEST_END;
  }

};

} // namespace test
} // namespace decom

#endif  // _DECOM_TEST_ISO15765_H_
