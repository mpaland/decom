#ifndef _DECOM_TEST_STL_CHRONO_H_
#define _DECOM_TEST_STL_CHRONO_H_

#include "test.h"

// chrono should be included via decom_cfg


namespace decom {
namespace test {


class stl_chrono : public test
{
  // TEST CASES
public:
  stl_chrono(std::ostream& result_file, format_type format)
   : test("stl_chrono", result_file, format)
  {
    system_clock();
    highres_clock();
    looptest();
  }

protected:

  void system_clock()
  {
    TEST_BEGIN("System clock");

    TEST_END;
  }


  void SFtest()
  {
    TEST_BEGIN("SF Test");

    TEST_END;
  }


  void CFtest()
  {
    TEST_BEGIN("CF Test");

    TEST_END;
  }

};

} // namespace test
} // namespace decom

#endif  // _DECOM_TEST_STL_CHRONO_H_
