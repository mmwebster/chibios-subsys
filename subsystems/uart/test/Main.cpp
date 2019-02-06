#include <array>
#include <mutex>
#include <string>
#include <cstring>
#include <utest/utest.hpp>
#include <utest/test_reporter/google_test.hpp>

// UART TestWriter adapter
#include "UartWriter.h"

// module under test
#include "Uart.h"

// library common includes
#include "Event.h"
#include "EventQueue.h"

// chibios and target-specific includes
#include "ch.hpp"
#include "hal.h"
#include "pinconf.h"

// defined test, included here just to keep main short
#include "tests.cpp"

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

  // TODO: Change to #def'd vars and move to UART subsystem
  // USART3_TX, PAL_MODE_OUTPUT_PUSHPULL
  palSetPadMode(GPIOC, 10, PAL_MODE_ALTERNATE(7)); 
  // USART3_RX, PAL_MODE_OUTPUT_PUSHPULL
  palSetPadMode(GPIOC, 11, PAL_MODE_ALTERNATE(7)); 

  // Digital outputs
  palSetPadMode(STARTUP_LED_PORT, STARTUP_LED_PIN,
                PAL_MODE_OUTPUT_PUSHPULL);

  // Init LED states to LOW
  palClearPad(STARTUP_LED_PORT, STARTUP_LED_PIN);

  EventQueue fsmEventQueue = EventQueue();

  // setup a UART interface to immediately begin transmitting and receiving
  cal::Uart uart = cal::Uart(UartInterface::kD3, fsmEventQueue);

  // test async UART transmit
  // @TODO move these to UART loop-back mode tests with uTest framwork
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
  chThdSleepMilliseconds(100);

  /**
   * New uTest Framework code
   */
  UartWriter writer(&uart);

  utest::TestWriterReference writers[]{
    writer
  };

  utest::test_reporter::GoogleTest google_test{writers};

  utest::TestReporterReference reporters[]{
    google_test
  };

  if (utest::TestStatus::PASS == utest::Test(reporters).run().status()) {
    uart.send("\r\nAll tests PASSED\r\n");
  } else {
    uart.send("\r\nTests FAILED\r\n");
  }

  // transmit back any bytes received over UART
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
