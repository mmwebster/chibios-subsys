#include <array>
#include <mutex>
#include <string>
#include <cstring>

// @TODO fix include dirs w/ local Makefile
//#include "../../../cal.h"
#include "../Uart.h"
#include "../../../common/Event.h"
#include "../../../common/EventQueue.h"
#include "ch.hpp"
#include "hal.h"
#include "pinconf.h"

static THD_WORKING_AREA(timer1Wa, 128);
static THD_FUNCTION(timer1Func, arg) {
  chRegSetThreadName("Timer 1");

  cal::Uart* uart = static_cast<cal::Uart*>(arg);

  while (true) {
    uart->send("Timer 1 still alive...\n");
    chThdSleepMilliseconds(500);
  }
}

static THD_WORKING_AREA(timer2Wa, 128);
static THD_FUNCTION(timer2Func, arg) {
  chRegSetThreadName("Timer 2");

  cal::Uart* uart = static_cast<cal::Uart*>(arg);

  while (true) {
    uart->send("Timer 2 still alive...\n");
    chThdSleepMilliseconds(2000);
  }
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

  // create some timer threads to test mult-thread access (note that at
  // high-frequency and serial throughput these should produce race conditions)
  chThdCreateStatic(timer1Wa, sizeof(timer1Wa), NORMALPRIO,
      timer1Func, &uart);
  chThdCreateStatic(timer2Wa, sizeof(timer2Wa), NORMALPRIO,
      timer2Func, &uart);

  // test async UART transmit
  uart.send("Valid message\n");
  uart.send("IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII"
            "IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIInvalid message!!!\n");

  // try sending a float
  constexpr float my_float = 314.5594;
  uart.send("Float value is: " + cal::Uart::to_string(my_float)
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
