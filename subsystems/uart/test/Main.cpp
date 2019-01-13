#include <array>
#include <mutex>

// @TODO fix include dirs w/ local Makefile
#include "../UartChSubsys.h"
#include "../../../common/Event.h"
#include "../../../common/EventQueue.h"
#include "ch.hpp"
#include "hal.h"
#include "pinconf.h"

static virtual_timer_t vtLedBspd, vtLedStartup, vtLedCan2Status,
                       vtLedCan1Status;

static void ledBspdOff(void* p) {
  (void)p;
  palClearPad(BSPD_FAULT_INDICATOR_PORT, BSPD_FAULT_INDICATOR_PIN);
}

static void ledStartupOff(void* p) {
  (void)p;
  palClearPad(STARTUP_LED_PORT, STARTUP_LED_PIN);
}

static void ledCan1StatusOff(void* p) {
  (void)p;
  palClearPad(CAN1_STATUS_LED_PORT, CAN1_STATUS_LED_PIN);
}

static void ledCan2StatusOff(void* p) {
  (void)p;
  palClearPad(CAN2_STATUS_LED_PORT, CAN2_STATUS_LED_PIN);
}

/**
 *********************************************************
 * START Uart Stuff
 *********************************************************
 */
constexpr char kStartByte = '1';
constexpr char kStopByte = '0';
constexpr uint8_t kUartOkMask = EVENT_MASK(1);
constexpr uint8_t kUartChMask = EVENT_MASK(4); // app not ready for char
static thread_t *uart1RxThread;
static char lostCharUart1;
static bool packetBufferRead = false; // do this the right way with signals or something

/*
 * This callback is invoked when a character is received but the application
 * was not ready to receive it, the character is passed as parameter.
 */
static void rxchar(UARTDriver *uartp, uint16_t c) {
  (void)uartp;
  eventmask_t events = kUartChMask;

  chSysLockFromISR();
  // queue this character for ingestion
  lostCharUart1 = c;
  chEvtSignalI(uart1RxThread, events);
  chSysUnlockFromISR();
}

/*
 * This callback is invoked when a receive buffer has been completely written.
 */
static void rxend(UARTDriver *uartp) {
  (void)uartp;
  eventmask_t events = kUartOkMask;

  chSysLockFromISR();
  // signal UART 1 RX to read
  chEvtSignalI(uart1RxThread, events);
  chSysUnlockFromISR();
}

/*
 * This callback is invoked when a transmission buffer has been completely
 * read by the driver.
 */
static void txend1(UARTDriver *uartp) {
  (void)uartp;
  chSysLockFromISR();
  packetBufferRead = true;
  chSysUnlockFromISR();
}

/*
 * This callback is invoked when a transmission has physically completed.
 */
static void txend2(UARTDriver *uartp) {
  (void)uartp;
}

// UART 1 driver configuration structure.
static UARTConfig uart_cfg_1 = {
  txend1,          // callback: transmission buffer completely read by the driver
  txend2,          // callback: a transmission has physically completed
  rxend,           // callback: a receive buffer has been completely written
  rxchar,          // callback: a character is received but the
                   //           application was not ready to receive it (param)
  NULL,            // callback: receive error w/ errors mask as param
  115200,          // Bit rate.
  0,               // Initialization value for the CR1 register.
  USART_CR2_LINEN, // Initialization value for the CR2 register.
  0                // Initialization value for the CR3 register.
};

/*
 * UART 1 RX Thread
 */
static THD_WORKING_AREA(uart1RxThreadFuncWa, 128);
static THD_FUNCTION(uart1RxThreadFunc, eventQueue) {
  uint16_t rxBuffer[16];

  while (true) {
    // start the first receive
    uartStopReceive(&UARTD3);
    uartStartReceive(&UARTD3, 1, rxBuffer);

    // waiting for any and all events (generated from callback)
    eventmask_t event = chEvtWaitAny(ALL_EVENTS);

    if (event) {

      if (event & kUartOkMask) {
        // handle regular byte
        uartStopReceive(&UARTD3);
        uartStartReceive(&UARTD3, 1, rxBuffer);
      }
      if (event & kUartChMask) {
        // handle lost char byte
        rxBuffer[0] = lostCharUart1;
      }

      // Push byte to FSM's event queue
      Event e = Event(Event::Type::kUartRx, rxBuffer[0]);

      // Todo: can't push directly from the callback, need
      //       to check if the lock is already acquired or
      //       temporarily stored data internal to the abstraction
      static_cast<EventQueue*>(eventQueue)->push(e);
    }
  }
}

/**
 *********************************************************
 * END Uart Stuff
 *********************************************************
 */

/**
 * UART RX subsystem thread
 */
static THD_WORKING_AREA(uartRxThreadFuncWa, 128);
static THD_FUNCTION(uartRxThreadFunc, uartChSubsys) {
  chRegSetThreadName("UART RX");
  static_cast<UartChSubsys*>(uartChSubsys)->runRxThread();
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
  palSetPadMode(GPIOC, 10, PAL_MODE_ALTERNATE(7)); // USART3_TX, PAL_MODE_OUTPUT_PUSHPULL
  palSetPadMode(GPIOC, 11, PAL_MODE_ALTERNATE(7)); // USART3_RX, PAL_MODE_OUTPUT_PUSHPULL

  // Digital outputs
  palSetPadMode(STARTUP_LED_PORT, STARTUP_LED_PIN,
                PAL_MODE_OUTPUT_PUSHPULL);

  // Init LED states to LOW
  palClearPad(STARTUP_LED_PORT, STARTUP_LED_PIN);

  /**
   * Create subsystems
   * @note Subsystems DO NOT RUN unless their run function(s) is/are
   *       called within static threads
   */
  EventQueue fsmEventQueue = EventQueue();

  // Start UART driver 1 (make sure before starting UART RX thread)
  //uartStart(&UARTD3, &uart_cfg_1);
  //uartStopSend(&UARTD3);
  //char txPacketArray[6] = {'S','T','A','R','T', ' '};
  //uartStartSend(&UARTD3, 6, txPacketArray);

  UartChSubsys uartChSubsys = UartChSubsys(fsmEventQueue);

  /*
   * Create threads (many of which are driving subsystems)
   */
  //// thread names necessary for signaling between threads
  //uart1RxThread = chThdCreateStatic(uart1RxThreadFuncWa,
  //    sizeof(uart1RxThreadFuncWa), NORMALPRIO, uart1RxThreadFunc,
  //    &fsmEventQueue);
  chThdCreateStatic(uartRxThreadFuncWa,
      sizeof(uartRxThreadFuncWa), NORMALPRIO, uartRxThreadFunc,
      &uartChSubsys);

  uartChSubsys.addInterface(UartInterface::kD3);

  // Indicate startup - blink then stay on
  for (uint8_t i = 0; i < 5; i++) {
    palSetPad(STARTUP_LED_PORT, STARTUP_LED_PIN);
    chThdSleepMilliseconds(50);
    palClearPad(STARTUP_LED_PORT, STARTUP_LED_PIN);
    chThdSleepMilliseconds(50);
  }

  palClearPad(STARTUP_LED_PORT, STARTUP_LED_PIN);
  chThdSleepMilliseconds(300);

  char txPacketArray[6] = {'A','B','C','D','E', ' '};
  char txPacketArray2[6] = {'F','G','H','I','J', ' '};
  uartChSubsys.send(txPacketArray, 6);
  uartChSubsys.send(txPacketArray2, 6);

  while (1) {
    // always deplete the queue to help ensure that events are
    // processed faster than they're generated
    while (fsmEventQueue.size() > 0) {
      Event e = fsmEventQueue.pop();

      if (e.type() == Event::Type::kUartRx) {
        //palTogglePad(STARTUP_LED_PORT, STARTUP_LED_PIN);
        uint8_t byteVal = e.getByte();
        //uartStopSend(&UARTD3);
        //uartStartSend(&UARTD3, 1, txPacketArray2);
        char txPacketArray2[1];
        txPacketArray2[0] = (char)byteVal;
        uartChSubsys.send(txPacketArray2, 1);
      }
    }

    // TODO: use condition var to signal that events are present in the queue
    chThdSleepMilliseconds(
        1);  // must be fast enough to deplete event queue quickly enough
  }
}
