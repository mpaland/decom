#ifndef _DECOM_TEST_SCHEDULER_H_
#define _DECOM_TEST_SCHEDULER_H_

#include "decom_cfg.h"
#include "test.h"

#include "../prot/prot_scheduler.h"
#include "../prot/prot_debug.h"
#include "../com/com_null.h"
#include "../dev/dev_generic.h"


namespace decom {
namespace test {

class prot_scheduler : public test
{
  // TEST CASES
public:
  prot_scheduler(std::ostream& result_file, format_type format)
   : test("prot_scheduler", result_file, format)
  {
    test1();
  }

protected:

  void test1()
  {
    TEST_BEGIN("test case 1");

    decom::com::null        com_null;
    decom::prot::debug      prot_dbg(&com_null);
    decom::prot::scheduler  prot_sched(&prot_dbg);
    decom::dev::generic     dev_gen(&prot_sched);

    TEST_CHECK(prot_sched.set_scheduler_period(std::chrono::milliseconds(100)));
/*
    // construction
    TEST_CHECK( prot_sched.add_message(10, 0));   // add to table 0 valid
    TEST_CHECK(!prot_sched.add_message(10, 2));   // add to table 2 invalid
    TEST_CHECK(!prot_sched.add_message(10, 3));   // add to table 3 invalid
    TEST_CHECK( prot_sched.add_message(20, 0));   // add to table 0 valid
    TEST_CHECK( prot_sched.add_message(30, 0));   // add to table 0 valid

    TEST_CHECK( prot_sched.add_message(110, 1));  // add to table 1 valid
    TEST_CHECK( prot_sched.add_message(120, 1));  // add to table 1 valid
    TEST_CHECK(!prot_sched.add_message(130, 3));  // add to table 3 invalid

    TEST_CHECK(!prot_sched.add_table(2, 0));      // add table 2 to list 0 invalid
    TEST_CHECK(!prot_sched.add_table(1, 2));      // add table 1 to list 2 invalid
    TEST_CHECK( prot_sched.add_table(0, 0));      // add table 0 to list 0 valid

    TEST_CHECK(prot_sched.activate_list(0))
 */
    dev_gen.open();

    prot_sched.set_periodic_message(decom::eid(10), std::chrono::milliseconds(10));

    decom::msg data;
    data.push_back(0x55);
    dev_gen.write(data, decom::eid(10));

    decom::msg data2;
    data2.push_back(0xAA);
    dev_gen.write(data2, decom::eid(20));


    TEST_CHECK(prot_sched.start());

    util::timer::sleep(std::chrono::seconds(10));
    dev_gen.close();

    TEST_END;
  }

  void test_case2()
  {
    TEST_BEGIN("test case 2");

    TEST_SKIP;  // skip this test - can be used instead of TEST_END
  }

  void test_case3()
  {
    TEST_BEGIN("test case 3");

    // construct your test case here

    TEST_SKIP;
  }
};

} // namespace test
} // namespace decom

#endif  // _DECOM_TEST_SKELETON_H_
