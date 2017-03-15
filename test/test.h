#ifndef _DECOM_TEST_TEST_H_
#define _DECOM_TEST_TEST_H_

#include <fstream>
#include <iomanip>

namespace decom {
namespace test {

// params
typedef enum enum_format_type {
  text = 0,
  xml
} format_type;

// test results
typedef enum enum_test_result_type {
  test_result_okay = 0,
  test_result_fail,
  test_result_info,
  test_result_skip
} test_result_type;


class test
{
public:
  test(const char* name, std::ostream& result_file, format_type format)
   : result_file_(result_file)
   , format_(format)
  {
    switch (format_) {
      case xml :
        result_file_ << "<module name=\"" << name << "\">" << std::endl;
        break;
      default :
        result_file_ << "Test of: " << name << std::endl;
        break;
    }
  }


  ~test()
  {
    switch (format_) {
      case xml :
        result_file_ << "</module>" << std::endl;
        break;
      default :
        result_file_ << std::endl;
        break;
    }
  }


  void error(const char* condition, const char* in_file, int in_line)
  {
    switch (format_) {
      case xml :
        result_file_ << "    <error line=\"" << in_line << "\" file=\"" << in_file << "\">error: ";
        xml_escape(condition);
        result_file_<< " in line " << in_line << "</error>" << std::endl;
        test_end(test_result_fail);
        break;
      default :
        test_end(test_result_fail);
        result_file_ << "error: " << condition << " in line " << in_line << std::endl;
        break;
    }
  }


  void test_begin(const char* test_case)
  {
    switch (format_) {
      case xml :
        result_file_ << "  <test>" << std::endl << "    <case>" << test_case << "</case>" << std::endl;
        break;
      case text :
      default :
        result_file_ << std::setw(60) << std::left << test_case;
        break;
    }
  }


  void test_end(test_result_type res, const char* info = "")
  {
    switch (res) {
      case test_result_okay :
        switch (format_) {
          case xml:
            result_file_ << "    <result type=\"okay\">okay</result>" << std::endl << "  </test>" << std::endl;
            break;
          default:
            result_file_ << std::setw(4) << "okay" << std::endl;
            break;
        }
        break;
      case test_result_fail :
        switch (format_) {
          case xml :
            result_file_ << "    <result type=\"fail\">fail</result>" << std::endl << "  </test>" << std::endl;
            break;
          default :
            result_file_ << std::setw(4) << "fail" << std::endl;
            break;
        }
        break;
      case test_result_info :
        switch (format_) {
        case xml :
          result_file_ << "    <result type=\"info\">" << info << "</result>" << std::endl << "  </test>" << std::endl;
          break;
        default :
          result_file_ << std::setw(4) << info << std::endl;
          break;
        }
        break;
      case test_result_skip :
      default:
        switch (format_) {
          case xml :
            result_file_ << "    <result type=\"skip\">skip</result>" << std::endl << "  </test>" << std::endl;
            break;
          default :
            result_file_ << std::setw(4) << "skip" << std::endl;
            break;
        }
        break;
    }
  }

private:
  void xml_escape(const char* data) {
    for (std::size_t pos = 0; data[pos] != 0; ++pos) {
      switch(data[pos]) {
        case '&' : result_file_ << "&amp;";    break;
        case '\"': result_file_ << "&quot;";   break;
        case '\'': result_file_ << "&apos;";   break;
        case '<' : result_file_ << "&lt;";     break;
        case '>' : result_file_ << "&gt;";     break;
        default  : result_file_ << data[pos];  break;
      }
    }
  }

  test& operator=(const test&) { }  // no assignment

  const format_type  format_;       // output format
  std::ostream& result_file_;       // result stream/file
};

#define TEST_CHECK(X) \
  if (!(X)) { \
    error(#X, __FILE__, __LINE__); \
    return; \
  }

#define TEST_BEGIN(X) { \
    test_begin(X); \
  }

#define TEST_INFO(X) { \
    test_end(test_result_info, X); \
  }

#define TEST_END { \
    test_end(test_result_okay); \
  }

#define TEST_SKIP { \
    test_end(test_result_skip); \
  }


} // namespace test
} // namespace decom

#endif  // _DECOM_TEST_TEST_H_
