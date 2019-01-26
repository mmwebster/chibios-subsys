#include <array>
#include <mutex>
#include <string>
#include <cstring>

// @TODO fix include dirs w/ local Makefile
//#include "../../../cal.h"
#include "../../uart/Uart.h"
#include "../EventSim.h"
#include "../../../common/Event.h"
#include "../../../common/EventQueue.h"
#include "ch.hpp"
#include "hal.h"
#include "pinconf.h"

//static THD_WORKING_AREA(timer1Wa, 128);
//static THD_FUNCTION(timer1Func, arg) {
//  chRegSetThreadName("Timer 1");
//
//  cal::Uart* uart = static_cast<cal::Uart*>(arg);
//
//  while (true) {
//    uart->send("Timer 1 still alive...\n");
//    chThdSleepMilliseconds(500);
//  }
//}
//
//static THD_WORKING_AREA(timer2Wa, 128);
//static THD_FUNCTION(timer2Func, arg) {
//  chRegSetThreadName("Timer 2");
//
//  cal::Uart* uart = static_cast<cal::Uart*>(arg);
//
//  while (true) {
//    uart->send("Timer 2 still alive...\n");
//    chThdSleepMilliseconds(2000);
//  }
//}

/**
 *
 *
 *
 *
 *
 *
 *
 *
 * MAIN TODO
 *  Setup the FSM as another module receiving and sending events like any other.
 *  This will facilitate testing and improve robustness of other processes. May
 *  require incorporation of threading abstactions -- leave these for later
 *
 *
 *
 *
 *
 *
 */

bool simpleTest(EventQueue& testConsumer, EventQueue& testMonitor) {
  // basic synchronous test
  palSetPad(STARTUP_LED_PORT, STARTUP_LED_PIN);
  chThdSleepMilliseconds(50);
  palSetPad(STARTUP_LED_PORT, STARTUP_LED_PIN);
  chThdSleepMilliseconds(50);

  // TODO send an event to the FSM then wait to hear back and report the test
  //      result

  return true;
}

static THD_WORKING_AREA(testerWa, 128);
static THD_FUNCTION(testerFunc, arg) {
  chRegSetThreadName("Tester");

  cal::EventSim* eventSimulator = static_cast<cal::EventSim*>(arg);

  // register tests
  // @note Currently these must be run synchronously
  eventSimulator->registerTest("Simple Test", simpleTest);
}

int main() {
  /*
   * System initializations.
   * - HAL initialization, this also initializes the configured device drivers
   *   and performs the board-specific initializations.
   * - Kernel initialization, the main() function becomes a thread and the
   *   RTOS is active.
   */
  halInit();
  chSysInit();

  // Pin initialization
  // UART TX/RX
  // TODO: Change to #def'd vars
  // TODO: Move to UART subsystem
  palSetPadMode(GPIOC, 10, PAL_MODE_ALTERNATE(7)); // USART3_TX, PAL_MODE_OUTPUT_PUSHPULL
  palSetPadMode(GPIOC, 11, PAL_MODE_ALTERNATE(7)); // USART3_RX, PAL_MODE_OUTPUT_PUSHPULL

  // Digital outputs
  palSetPadMode(STARTUP_LED_PORT, STARTUP_LED_PIN,
                PAL_MODE_OUTPUT_PUSHPULL);

  // Init LED states to LOW
  palClearPad(STARTUP_LED_PORT, STARTUP_LED_PIN);

  EventQueue fsmEventQueue = EventQueue();

  // setup a UART interface to immediately begin transmitting and receiving
  cal::Uart uart = cal::Uart(UartInterface::kD3, fsmEventQueue);

  // test async UART transmit
  uart.send("Starting event simulator\n");

  // @note The garbled logging is a result of no hysteresis on the power button.
  //       Not a race condition in the UART abstraction. Currently its only
  //       known issue is dropping transmit bytes when its transmit event queue
  //       is busy. On inspection, the UART output simply reset afte spurious
  //       restarts causing incomplete messages and garbled bytes.

  // create some timer threads to test mult-thread access (note that at
  // high-frequency and serial throughput these should produce race conditions)
  //chThdCreateStatic(timer1Wa, sizeof(timer1Wa), NORMALPRIO,
  //    timer1Func, &uart);
  //chThdCreateStatic(timer2Wa, sizeof(timer2Wa), NORMALPRIO,
  //    timer2Func, &uart);

  // create the tester
  cal::EventSim eventSimulator = cal::EventSim(uart, fsmEventQueue);

  // @note Test thread until implementing thread abstraction for things that
  //       can't be implemented with ChibiOS callbacks
  chThdCreateStatic(testerWa, sizeof(testerWa), NORMALPRIO,
      testerFunc, &eventSimulator);

  // Indicate startup - blink then stay on
  for (uint8_t i = 0; i < 5; i++) {
    palSetPad(STARTUP_LED_PORT, STARTUP_LED_PIN);
    chThdSleepMilliseconds(50);
    palClearPad(STARTUP_LED_PORT, STARTUP_LED_PIN);
    chThdSleepMilliseconds(50);
  }

  palClearPad(STARTUP_LED_PORT, STARTUP_LED_PIN);
  chThdSleepMilliseconds(300);

  while (1) {
    // always deplete the queue to help ensure that events are
    // processed faster than they're generated
    while (fsmEventQueue.size() > 0) {
      Event e = fsmEventQueue.pop();

      if (e.type() == Event::Type::kUartRx) {
        // send received byte back to source (test throughput)
        uart.send((char)e.getByte());
      }
    }

    // TODO: use condition var to signal that events are present in the queue
    chThdSleepMilliseconds(
        1);  // must be fast enough to deplete event queue quickly enough
  }

  // sleep forever
  chThdSleepMilliseconds(1000*60*60*24);
}
