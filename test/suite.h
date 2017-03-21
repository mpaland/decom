#ifndef _DECOM_TEST_SUITE_H_
#define _DECOM_TEST_SUITE_H_


#include <iostream>
#include <fstream>

///////////////////////////////////////////////////////////
// INCLUDE AVAILABLE UNIT TESTS HERE
#include "test_msg.h"
#include "test_prot_intel_hex.h"
#include "test_prot_iso15765.h"
//#include "test_prot_zvt.h"
//#include "test_prot_scheduler.h"
#include "test_com_inet.h"
///////////////////////////////////////////////////////////

namespace decom {
namespace test {


class suite
{
private:
  /*
   * Test list - list all modules to test here
   */
  void test_modules()
  {
    msg(*result_stream_, format_);
    //prot_intel_hex(*result_stream_, format_);
    //prot_iso15765(*result_stream_, format_);
    //prot_zvt(*result_stream_, format_);
    //prot_scheduler(*result_stream_, format_);
    //com_inet(*result_stream_, format_);
  }

public:
  /*
   * Filename ctor
   */
  suite(const char* filename, format_type format = text)
    : filename_(filename)
    , format_(format)
  {
    // init result file
    std::ofstream of;
    result_stream_ = &of;
    of.open(filename_, std::ios::out | std::ios::trunc);

    result_file_head();   // init result file
    test_modules();       // run tests
    result_file_tail();   // write tail

    of.close();
  }


  /*
   * Stream ctor
   */
  suite(std::ostream& result_stream, format_type format = text)
   : result_stream_(&result_stream)
   , format_(format)
  {
    result_file_head();   // init result file
    test_modules();       // run tests
    result_file_tail();   // write tail
  }


  /*
   * dtor
   */
  ~suite()
  {
  }

private:
  // write test suite report head
  void result_file_head()
  {
    switch (format_) {
      case xml :
        // create head
        *result_stream_ <<
        "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>" << std::endl <<
        "<?xml-stylesheet type=\"text/xsl\" href=\"test.xsl\"?>" << std::endl <<
        "<tests>" << std::endl;
        break;
      default :
        break;
    }
  }

  // write test suite report tail
  void result_file_tail()
  {
    switch (format_) {
      case xml :
        *result_stream_ <<
        "</tests>" << std::endl;
        break;
      default :
        break;
    }
  }

private:
  suite& operator=(const suite&) { }  // no assignment

  const char* filename_;
  const format_type format_;
  std::ostream* result_stream_;
};

} // namespace test
} // namespace decom

#endif  // _DECOM_TEST_SUITE_H_
