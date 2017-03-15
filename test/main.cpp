#include "suite.h"

int main(int /* argc */, char /* *argv[] */, char /* *envp[] */)
{
  // run the test suite
  decom::test::suite suite("test results.xml",  decom::test::xml);
  return 0;
}
