#include <functional>
#include <string>

#include "../../cal.h"
#include "../uart/Uart.h"
#include "EventSim.h"

// @TODO fix include dirs w/ local Makefile
#include "../../common/Gpio.h"
#include "../../common/Event.h"
#include "../../common/EventQueue.h"
#include "ch.h"
#include "hal.h"
// @TODO Define pinconfs for every supported board type so that user code
//       indicates board and then chibios-subsys knows what pin confs to use
//       (that facilitates compile-time checks that such pins exist)
#include "pinconf.h"

cal::EventSim::EventSim(cal::Uart& uart, EventQueue& testConsumer) : m_uart(uart),
  m_testConsumer(testConsumer) {}

/**
 * @brief For now, perform the test itself by calling the function, notifying of
 *        test progress and success/failure over provided UART interface
 */
void cal::EventSim::registerTest(std::string name,
    std::function<bool(EventQueue&, EventQueue&)> test) {
  // log test start
  m_uart.send(std::string("EventSim: Beginning test [") + name
      + std::string("].\n"));

  // run the test
  bool result = test(m_testConsumer, m_receiveQueue);

  std::string resultStr = result == true ? "SUCCESS" : "FAILURE";

  // log test end
  m_uart.send(std::string("EventSim: Ending test    [") + name
      + std::string("] with ") + resultStr + std::string(".\n"));
}
