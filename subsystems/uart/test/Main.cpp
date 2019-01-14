#include <array>
#include <mutex>
#include <string>
#include <cstring>

// @TODO fix include dirs w/ local Makefile
#include "../UartChSubsys.h"
#include "../../../common/Event.h"
#include "../../../common/EventQueue.h"
#include "ch.hpp"
#include "hal.h"
#include "pinconf.h"

/**
 * UART RX subsystem thread
 * @TODO Remove the UART's subsystem thread entirely and see if anything breaks.
 *       Seems as though UART RX could run exclusively on chibios callbacks.
 *       Although, only its workspace is unnecessary overhead b/c the thread
 *       itself goes into a permanent slumber after UART RX start receive is
 *       called
 */
static THD_WORKING_AREA(uartRxThreadFuncWa, 128);
static THD_FUNCTION(uartRxThreadFunc, uart) {
  chRegSetThreadName("UART RX");
  static_cast<UartChSubsys*>(uart)->runRxThread();
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

  UartChSubsys uart = UartChSubsys(fsmEventQueue);

  // create threads to drive subsystems
  chThdCreateStatic(uartRxThreadFuncWa, sizeof(uartRxThreadFuncWa), NORMALPRIO,
      uartRxThreadFunc, &uart);

  uart.addInterface(UartInterface::kD3);

  // test async UART transmit
  uart.send("Valid message\n");
  uart.send("IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII"
            "IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIInvalid message!!!\n");

  // try sending a float
  constexpr float my_float = 314.5594;
  uart.send("Float value is: " + UartChSubsys::to_string(my_float)
      + "\n\n");

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
}
