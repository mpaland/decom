#ifndef _DECOM_TEST_MSG_H_
#define _DECOM_TEST_MSG_H_

#include "decom_cfg.h"

#include "test.h"
#include "../msg.h"

namespace decom {
namespace test {

class msg : public test
{
  // TEST CASES
public:
  msg(std::ostream& result_file, format_type format)
   : test("msg", result_file, format)
  {
    generic();
    capacity();
    push_back();
    push_front();
    insert();
    erase();
    resize();
    copy();
    access();
    iterators();
    get();
    dummy();
  }

protected:

  void generic()
  {
    TEST_BEGIN("generic");

    decom::msg m1;
    TEST_CHECK(m1.empty() == true);
    TEST_CHECK(m1.size() == 0U);

    m1.push_back(42U);    // add a byte
    TEST_CHECK(m1.size() == 1U);
    TEST_CHECK(m1[0] == 42U);
    TEST_CHECK(m1.at(0) == 42U);

    decom::msg msg(10, 0x55U);
    decom::msg::iterator it(msg.begin()), end(msg.end());
    for (; it != end; ++it) {
      TEST_CHECK((*it) == 0x55U);
    }
    TEST_END;
  }


  void push_back()
  {
    TEST_BEGIN("push_back");

    decom::msg::get_msg_pool().clear_used_pages_max();

    decom::msg m;
    for (std::uint16_t i = 0; i < DECOM_MSG_POOL_PAGE_SIZE * 4; i++) {
      TEST_CHECK(m.size() == i);
      TEST_CHECK(m.push_back(static_cast<std::uint8_t>(i)));
    }
    for (std::uint16_t i = 0; i < DECOM_MSG_POOL_PAGE_SIZE * 4; i++) {
      TEST_CHECK(m[i] == (static_cast<std::uint8_t>(i)));
    }
    for (std::uint16_t i =  DECOM_MSG_POOL_PAGE_SIZE * 4; i; i--) {
      TEST_CHECK(m.size() == i);
      m.pop_back();
    }
    TEST_CHECK(decom::msg::get_msg_pool().used_pages() == 1U);
    TEST_CHECK(decom::msg::get_msg_pool().used_pages_max() == 5U);
    TEST_END;
  }


  void push_front()
  {
    TEST_BEGIN("push_front");

    decom::msg::get_msg_pool().clear_used_pages_max();

    decom::msg m;
    for (std::uint16_t i = 0; i < DECOM_MSG_POOL_PAGE_SIZE * 4; i++) {
      TEST_CHECK(m.size() == i);
      TEST_CHECK(m.push_front(static_cast<std::uint8_t>(i)));
    }
    for (std::uint16_t i = 0; i < DECOM_MSG_POOL_PAGE_SIZE * 4; i++) {
      TEST_CHECK(m[i] == (static_cast<std::uint8_t>(m.size() - 1U - i)));
    }
    for (std::uint16_t i =  DECOM_MSG_POOL_PAGE_SIZE * 4; i; i--) {
      TEST_CHECK(m.size() == i);
      m.pop_front();
    }
    TEST_CHECK(decom::msg::get_msg_pool().used_pages() == 1U);
    TEST_CHECK(decom::msg::get_msg_pool().used_pages_max() == 5U);
    TEST_END;
  }


  void insert()
  {
    TEST_BEGIN("insert");

    std::uint8_t array1[] = { 1, 4, 25 };
    std::uint8_t array2[] = { 9, 16 };

    decom::msg m(array1, array1 + 3);
    decom::msg::iterator it = m.insert(m.begin(), 0);
    TEST_CHECK(*it == 0);

    it = m.insert(m.end(), 36);
    TEST_CHECK(*it == 36);

    TEST_CHECK(m.size() == 5);
    TEST_CHECK(m[0] == 0);
    TEST_CHECK(m[1] == 1);
    TEST_CHECK(m[2] == 4);
    TEST_CHECK(m[3] == 25);
    TEST_CHECK(m[4] == 36);

    m.insert(m.begin() + 3, array2, array2 + 2);
    TEST_CHECK(m.size() == 7);

    TEST_CHECK(m[0] == 0);
    TEST_CHECK(m[1] == 1);
    TEST_CHECK(m[2] == 4);
    TEST_CHECK(m[3] == 9);
    TEST_CHECK(m[4] == 16);
    TEST_CHECK(m[5] == 25);
    TEST_CHECK(m[6] == 36);

    m.insert(m.begin(), 3, (decom::msg::value_type)0x55);
    TEST_CHECK(m[0] == 0x55);
    TEST_CHECK(m[1] == 0x55);
    TEST_CHECK(m[2] == 0x55);
    TEST_CHECK(m[3] == 0);
    TEST_CHECK(m[4] == 1);
    TEST_CHECK(m[5] == 4);
    TEST_CHECK(m[6] == 9);
    TEST_CHECK(m[7] == 16);
    TEST_CHECK(m[8] == 25);
    TEST_CHECK(m[9] == 36);
    TEST_CHECK(m.size() == 10);

    m.clear();
    TEST_CHECK(m.empty());

    m.insert(m.begin(), 5, (decom::msg::value_type)10);
    TEST_CHECK(m.size() == 5);
    TEST_CHECK(m[0] == 10);
    TEST_CHECK(m[1] == 10);
    TEST_CHECK(m[2] == 10);
    TEST_CHECK(m[3] == 10);
    TEST_CHECK(m[4] == 10);
    TEST_END;
  }


  void erase()
  {
    TEST_BEGIN("erase");

    decom::msg m;
    m.push_back(1);
    m.push_back(4);
    m.push_back(9);
    m.push_back(16);
    m.push_back(19);
    TEST_CHECK(m.size() == 5);

    decom::msg::iterator it = m.erase(m.begin() + 2, m.begin() + 4);
    TEST_CHECK(m[0] == 1);
    TEST_CHECK(m[1] == 4);
    TEST_CHECK(m[2] == 19);
    TEST_CHECK(m.size() == 3);

    m.erase(m.begin());
    TEST_CHECK(m[0] == 4);
    TEST_CHECK(m[1] == 19);
    TEST_CHECK(m.size() == 2);

    m.erase(m.end() - 1);
    TEST_CHECK(m[0] == 4);
    TEST_CHECK(m.size() == 1);
    TEST_END;
  }

  void resize()
  {
    TEST_BEGIN("resize");
    DECOM_LOG_NOTICE2("resize", "msg test");

    decom::msg m(16U, 0);
    m[0] = 1;
    m[1] = 4;
    m[2] = 9;
    m[3] = 16;
    TEST_CHECK(m.size() == 16);

    m.resize(4);
    TEST_CHECK(m.size() == 4);

    for (std::uint16_t i = 4; i < DECOM_MSG_POOL_PAGE_SIZE * 4; i++) {
      TEST_CHECK(m.size() == i);
      TEST_CHECK(m.push_back(static_cast<std::uint8_t>(i)));
    }
    m.resize(3);
    TEST_CHECK(m.size() == 3);
    TEST_CHECK(m[0] == 1);
    TEST_CHECK(m[1] == 4);
    TEST_CHECK(m[2] == 9);

    TEST_END;
  }

  void copy()
  {
    TEST_BEGIN("copy/assignment");
    DECOM_LOG_NOTICE2("copy/assignment", "msg test");

    decom::msg m(4U, 0);
    m[0] = 1;
    m[1] = 4;
    m[2] = 9;
    m[3] = 16;
    TEST_CHECK(m.size() == 4);

    // cheap copy 1
    decom::msg* cc = (decom::msg*)new decom::msg;
    cc->ref_copy(m);
    TEST_CHECK(cc->size() == 4);
    TEST_CHECK(cc->at(0) == 1);
    TEST_CHECK(cc->at(1) == 4);
    TEST_CHECK(cc->at(2) == 9);
    TEST_CHECK(cc->at(3) == 16);

    // prohibit msg change
    TEST_CHECK(!m.push_back(1U));
    TEST_CHECK(!m.push_front(1U));
    TEST_CHECK(m.size() == 4);
    TEST_CHECK(!cc->push_back(1U));
    TEST_CHECK(!cc->push_front(1U));
    TEST_CHECK(cc->size() == 4);
    TEST_CHECK(m.size() == 4);

    cc->clear();
    TEST_CHECK(cc->empty());
    TEST_CHECK(m.size() == 4);
    delete cc;

    // cheap copy 2
    decom::msg cc2;
    cc2.ref_copy(m);
    TEST_CHECK(cc2.size() == 4);
    TEST_CHECK(cc2[0] == 1);
    TEST_CHECK(cc2[1] == 4);
    TEST_CHECK(cc2[2] == 9);
    TEST_CHECK(cc2[3] == 16);

    // prohibit msg change
    m.push_back(1U);
    m.push_front(1U);
    TEST_CHECK(m.size() == 4);
    cc2.push_back(1U);
    cc2.push_front(1U);
    TEST_CHECK(cc2.size() == 4);
    TEST_CHECK(m.size() == 4);

    cc2.clear();
    TEST_CHECK(cc2.empty());
    m.push_back(25U);
    TEST_CHECK(m[4] == 25);
    m.pop_back();

    // real copy
    decom::msg rc;
    rc = m;
    TEST_CHECK(rc.size() == 4);
    TEST_CHECK(rc[0] == 1);
    TEST_CHECK(rc[1] == 4);
    TEST_CHECK(rc[2] == 9);
    TEST_CHECK(rc[3] == 16);

    // rc change possible
    rc.clear();
    TEST_CHECK(rc.empty());
    m.clear();
    TEST_CHECK(m.empty());

    rc.push_back(1U);
    rc.push_front(1U);
    TEST_CHECK(rc.size() == 2);
    m.push_back(1U);
    m.push_front(1U);
    TEST_CHECK(m.size() == 2);
    TEST_END;
  }

  void access()
  {
    TEST_BEGIN("element access");

    decom::msg m(4U, 0);

    m[0] = 1;
    m[1] = 4;
    m[2] = 9;
    m[3] = 10;

    TEST_CHECK(m[0] == 1);
    TEST_CHECK(m[1] == 4);
    TEST_CHECK(m[2] == 9);
    TEST_CHECK(m[3] == 10);

    m.at(3) = 16;
    TEST_CHECK(m[3] == 16);
    TEST_CHECK(m[4] == 0xCCU);   // illegal ref

    TEST_CHECK(m.size() == 4);
    TEST_CHECK(m.front() == 1);
    TEST_CHECK(m.back() == 16);

    m.push_back(25);
    TEST_CHECK(m.back() == 25);
    TEST_CHECK(m.size() == 5);

    m.pop_back();
    TEST_CHECK(m.back() == 16);
    TEST_CHECK(m.size() == 4);

    m.pop_front();
    TEST_CHECK(m.front() == 4);
    TEST_CHECK(m.size() == 3);
    TEST_END;
  }


  void iterators()
  {
    TEST_BEGIN("iterators");

    decom::msg m(10U, 0x55);
    decom::msg const& crm = m;

    TEST_CHECK(m.begin()   == m.begin());
    TEST_CHECK(m.begin()   == crm.begin());
    TEST_CHECK(crm.begin() == m.begin());
    TEST_CHECK(crm.begin() == crm.begin());

    TEST_CHECK(m.begin()   != m.end());
    TEST_CHECK(m.begin()   != crm.end());
    TEST_CHECK(crm.begin() != m.end());
    TEST_CHECK(crm.begin() != crm.end());

    for (decom::msg::const_iterator cit = m.begin(); cit != m.end(); ++cit) {
      TEST_CHECK(*cit == 0x55);
    }
    TEST_END;
  }


  void capacity()
  {
    TEST_BEGIN("capacity");

    decom::msg::get_msg_pool().clear_used_pages_max();

    decom::msg m;
    for (msg_pool::size_type i = 0; i < DECOM_MSG_POOL_PAGE_SIZE * (decom::msg::get_msg_pool().max_size() - 1); i++) {
      TEST_CHECK(m.size() == i);
      TEST_CHECK(m.push_back(static_cast<std::uint8_t>(i * 3)));
    }
    for (msg_pool::size_type i = 0; i < DECOM_MSG_POOL_PAGE_SIZE * (decom::msg::get_msg_pool().max_size() - 1); i++) {
      TEST_CHECK(m[i] == (static_cast<std::uint8_t>(i * 3)));
    }
    for (msg_pool::size_type i = 0; i < DECOM_MSG_POOL_PAGE_SIZE * (decom::msg::get_msg_pool().max_size() - 1); i++) {
      TEST_CHECK(m.size() == DECOM_MSG_POOL_PAGE_SIZE * (decom::msg::get_msg_pool().max_size() - 1) - i);
      TEST_CHECK(m[0] == (static_cast<std::uint8_t>(i * 3)));
      m.pop_front();
    }
    TEST_CHECK(decom::msg::get_msg_pool().max_size() == DECOM_MSG_POOL_PAGES); 
    TEST_CHECK(decom::msg::get_msg_pool().used_pages() == 1U);
    TEST_CHECK(decom::msg::get_msg_pool().used_pages_max() == decom::msg::get_msg_pool().max_size());
    TEST_END;
  }


  void get()
  {
    TEST_BEGIN("get");

    decom::msg m1( 5, 2U);
    decom::msg m2(10, 5U);
    decom::msg m3( 8, 7U);

    m1[0] = 1;
    m1[1] = 4;
    m1[2] = 9;
    m1[3] = 10;

    m2[0] = 11;
    m2[1] = 14;
    m2[2] = 19;
    m2[3] = 20;

    m3[0] = 21;
    m3[1] = 24;
    m3[2] = 29;
    m3[3] = 30;

    m1.append(m2);
    m1.append(m3);
    TEST_CHECK(m1.size() == 23);

    decom::msg::value_type buf[23];
    decom::msg::value_type buf_ref[23] = { 1, 4, 9, 10, 2, 11, 14, 19, 20, 5, 5, 5, 5, 5, 5, 21, 24, 29, 30, 7, 7, 7, 7 };

    memset(buf, 0, 23);
    m1.get(buf, 23, 0);
    TEST_CHECK(memcmp(buf, buf_ref, 23) == 0);

    memset(buf, 0, 23);
    m1.get(buf,  6, 2);
    TEST_CHECK(memcmp(buf, buf_ref + 2, 6) == 0);

    memset(buf, 0, 23);
    m1.get(buf,  4, 15);
    TEST_CHECK(memcmp(buf, buf_ref + 15, 4) == 0);

    TEST_END;
  }

  void dummy()
  {
    TEST_BEGIN("dummy skipped");
    TEST_SKIP;
  }

};

} // namespace test
} // namespace decom

#endif  // _DECOM_TEST_MSG_H_
