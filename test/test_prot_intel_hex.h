#ifndef _DECOM_TEST_PROT_INTEL_HEX_H_
#define _DECOM_TEST_PROT_INTEL_HEX_H_

#include "decom_cfg.h"
#include "test.h"

#include "../prot/prot_intel_hex.h"
#include "../com/com_generic.h"
#include "../dev/dev_generic.h"

namespace decom {
namespace test {

class prot_intel_hex : public test
{
  // TEST CASES
public:
  prot_intel_hex(std::ostream& result_file, format_type format)
   : test("prot_intel_hex", result_file, format)
  {
    test1();
  }

protected:

  static void test1_callback(void* arg, decom::msg& data, decom::eid const& id, bool)
  {
    (void)data; (void)arg;
    //prot_intel_hex* i = static_cast<prot_intel_hex*>(arg);
    DECOM_LOG_INFO2((int)id.port(), "test");
  }


  void test1()
  {
    TEST_BEGIN("test1");

    decom::com::generic com_gen;
    decom::prot::intel_hex ihex(&com_gen);
    decom::dev::generic dev_gen(&ihex);

    com_gen.set_receive_callback(this, test1_callback);

    const char szHex[] =
    ":020000040000FA\n"
    ":2000A00000E280FF04001FE80848074006384036000006360000B20580FF0400610200525E\n"
    ":2000C0002036140080FFBC0E202E0000991523065C4CDFFE003A0042004A1C0A4119240653\n"
    ":048060008207D2D7EA\n"
    ":208080008207B2D70000000000000000000000008207A2D7000000000000000000000000CC\n"
    ":20FF6000000000000000000000000000000000000000000000000000000000000000000081\n"
    ":0CFF800000000000000000000000000075\n"
    ":20FFD000000000000000000000000000000000005A5A00000000000000000000000000005D\n"
    ":10FFF00001000000010000000000000000000000FF\n"
    ":0400000500000000F7\n"
    ":00000001FF\n";

    decom::msg buf;
    buf.put((std::uint8_t*)szHex, sizeof(szHex));

    TEST_CHECK(dev_gen.open());
    TEST_CHECK(dev_gen.write(buf));

    TEST_END;
  }


};

} // namespace test
} // namespace decom

#endif  // _DECOM_TEST_PROT_INTEL_HEX_H_
