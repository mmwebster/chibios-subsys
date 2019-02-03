#include <utest/utest.hpp>
#include "../Uart.h"

static unsigned int Trivial(unsigned int x) {
  return x*2;
}

static std::string MyStr() {
  return std::string("Some String");
}

/**
 * @TODO fix the assertion error reporting in the lib -- utest breaks when an
 *       assertion fails and it tries to describe expected vs. received. The
 *       issue relates to some sort of file TestString in the TestReporter's
 *       (GoogleTest) failure(...) member
 * @TODO write actual unit tests for this UART subsystem w/ the interface in
 *       loop-back mode. Requires adding support for an additional driver
 */
static utest::TestRunner g([] (utest::TestSuite& test_suite) {
  test_suite.name("Simple Suite 1").run([] (utest::TestCase& test_case) {
    test_case.name("trivial_case_0").run([] (utest::TestParams& p) {
      utest::TestAssert{p}.equal(Trivial(1), 2u);
      utest::TestAssert{p}.equal(MyStr(), std::string("Some String"));
      // utest::TestAssert{p}.fail(); // note assertion reporting broken
    });
    test_case.name("trivial_case_2").run([] (utest::TestParams& p) {
      utest::TestAssert{p}.equal(Trivial(1), 2u);
      utest::TestAssert{p}.equal(MyStr(), std::string("Some String"));
      if (MyStr() != std::string("Some String")) {
        utest::TestAssert{p}.fail();
      }
    });
  });

  test_suite.name("Simple Suite 2").run([] (utest::TestCase& test_case) {
    test_case.name("trivial_case_0").run([] (utest::TestParams& p) {
      utest::TestAssert{p}.equal(Trivial(1), 2u);
      utest::TestAssert{p}.equal(MyStr(), std::string("Some String"));
    });
  });
});

