#ifndef _DECOM_TEST_SKELETON_H_
#define _DECOM_TEST_SKELETON_H_

#include "decom_cfg.h"
#include "test.h"

//#include "...."   // include your test module here

//#include "...."   // include further modules

namespace decom {
namespace test {

class skeleton : public test
{
  // TEST CASES
public:
  skeleton(std::ostream& result_file, format_type format)
   : test("skeleton", result_file, format)
  {
    test_case1();
    test_case2();
    test_case3();
    test_case4();
  }

protected:

  void test_case1()
  {
    // ALWAYS start test case with TEST_BEGIN macro
    TEST_BEGIN("test case 1");

    // construct your test case here using TEST_CHECK macro
    TEST_CHECK(true);     // check passes, test case continues
    TEST_CHECK(false);    // check fails, test case is aborted here

    // more checks

    // use TEST_END macro to end the test case. If we are here, test case is passed
    TEST_END;
  }

  void test_case2()
  {
    TEST_BEGIN("test case 2");

    TEST_SKIP;  // skip this test - can be used instead of TEST_END
  }

  void test_case3()
  {
    TEST_BEGIN("test case 2");

    TEST_INFO("This is a diagnostig info");   // info output - can be used instead of TEST_END
  }


  void test_case4()
  {
    TEST_BEGIN("test case 4");

    // construct your test case here
    TEST_CHECK(true);     // test case passes
    TEST_CHECK(false);    // test case fails

    // more checks

    TEST_END;
  }
};

} // namespace test
} // namespace decom

#endif  // _DECOM_TEST_SKELETON_H_
