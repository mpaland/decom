#ifndef _DECOM_TEST_COM_INET_H_
#define _DECOM_TEST_COM_INET_H_

#include "decom_cfg.h"
#include "test.h"

#include "com/com_inet.h"
#include "dev/dev_generic.h"
#include "dev/dev_echo.h"


namespace decom {
namespace test {

class com_inet : public test
{
  // TEST CASES
public:
  com_inet(std::ostream& result_file, format_type format)
   : test("inet", result_file, format)
  {
    udp_server();
 //   test_case1();
  }

protected:

  void udp_server()
  {
    // ALWAYS start test case with TEST_BEGIN macro
    TEST_BEGIN("UDP server");

    // create one UDP server
    decom::com::inet    inet_server(true, true, false, "com_inet_server");
    decom::prot::debug  dbg_server(&inet_server);
    decom::dev::echo    echo_server(&dbg_server);
    echo_server.open("localhost:7081");

    // create clients
    #define MAX_CLIENTS 10U
    decom::com::inet*    inet_client[MAX_CLIENTS];
    decom::prot::debug*  dbg_client[MAX_CLIENTS];
    decom::dev::generic* gen_client[MAX_CLIENTS];
    std::string          name[MAX_CLIENTS];
    for (std::uint16_t n = 0U; n < MAX_CLIENTS; ++n) {
      std::stringstream str; str << "com_inet_client" << (int)n;
      name[n] = str.str();
      inet_client[n] = new decom::com::inet(true, false, false, name[n].c_str());
      dbg_client[n]  = new decom::prot::debug(inet_client[n]);
      gen_client[n]  = new decom::dev::generic(dbg_client[n]);
      //inet_client[n]->set_source_address("localhost:3000");
      gen_client[n]->open("localhost:7081");
      TEST_CHECK(gen_client[n]->is_connected(std::chrono::milliseconds(100)));
    }

    for (std::uint8_t n = 0U; n < MAX_CLIENTS; ++n) {
      TEST_CHECK(gen_client[n]->write(n));
    }

    decom::util::timer::sleep(std::chrono::seconds(1));
    echo_server.close();
    decom::util::timer::sleep(std::chrono::seconds(1));
    for (std::uint8_t n = 0U; n < MAX_CLIENTS; ++n) {
      gen_client[n]->close();
    }
    decom::util::timer::sleep(std::chrono::seconds(1));

    // delete clients
    for (std::uint16_t n = 0U; n < MAX_CLIENTS; ++n) {
      delete gen_client[n];
      delete dbg_client[n];
      delete inet_client[n];
    }

    // use TEST_END macro to end the test case. If we are here, test case is passed
    TEST_END;
  }


  void test_case1()
  {
    // ALWAYS start test case with TEST_BEGIN macro
    TEST_BEGIN("host resolution");

    decom::com::inet inet(true, true, false);
    decom::prot::debug dbg(&inet);
    decom::dev::generic gen(&dbg);

    inet.set_source_address("localhost:6081");
    inet.open("localhost:7081");

    // wait until open/connected
    gen.write((std::uint8_t)77U);

    decom::util::timer::sleep(std::chrono::seconds(30));
    inet.close();

    // use TEST_END macro to end the test case. If we are here, test case is passed
    TEST_END;
  }

};

} // namespace test
} // namespace decom

#endif  // _DECOM_TEST_COM_INET_H_
