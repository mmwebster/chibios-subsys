#include "UartChSubsys.h"

#include <mutex>
#include <cstring>

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

// Definitions for static members
std::array<UARTDriver *,4> UartChSubsys::registeredDrivers = {};
std::array<UartChSubsys *,4> UartChSubsys::driverToSubsysLookup = {};

/**
 * TODO: Add private pin mappings for each interface and notes about
 *       which interface uses which pins in constructor docs
 * @note: Must use m_uartMut when writing to UART interface
 */
UartChSubsys::UartChSubsys(EventQueue& eq) : m_eventQueue(eq) {}

/**
 * TODO: Move this logic to the constructor for the new non-singleton
 *       implementation
 * TODO: Support multiple interfaces -- need event queus and driver
 *       configs for each interface
 * TODO: Implement variable baud
 * TODO: Implement the timeout callback w/ proper event generation (may
 *       only be in a new chibios version)
 */
void UartChSubsys::addInterface(UartInterface ui) {
  // set default config
  // TODO: Initialize interface pins
  m_uartConfig = {
    &UartChSubsys::txEmpty,   //txend1,// callback: transmission buffer completely read
                     // by the driver
    NULL,   //txend2,// callback: a transmission has physically completed
    &UartChSubsys::rxDone, // callback: a receive buffer has been
                                  //completelywritten
    NULL,   //rxchar,// callback: a character is received but the
                     //           application was not ready to receive
                     //           it (param)
    NULL,            // callback: receive error w/ errors mask as param
    115200,          // Bit rate.
    0,               // Initialization value for the CR1 register.
    USART_CR2_LINEN, // Initialization value for the CR2 register.
    0                // Initialization value for the CR3 register.
  };
  // set default driver (TODO: make it based on ui)
  m_uartp = &UARTD3;
  // register driver statically on class for lookup in static callbacks
  UartChSubsys::registeredDrivers[0] = m_uartp;
    //UartChSubsys::registeredDrivers.size()] = m_uartp;
  UartChSubsys::driverToSubsysLookup[0] = this;
      //UartChSubsys::driverToSubsysLookup.size()] = this;
  // start interface with default config
  uartStart(m_uartp, &m_uartConfig);
}

/**
 * @brief Convert a float to a string
 */
std::string UartChSubsys::to_string(float num) {
  int num_int = static_cast<int>(num);
  return std::to_string(num_int) + std::string(".X");
}

/*
 * @brief wrapper for send(char*,uint16) that takes a char
 */
void UartChSubsys::send(char byte) {
  send(&byte, 1);
}

void UartChSubsys::send(float num) {
  send(UartChSubsys::to_string(num));
}

void UartChSubsys::send(int num) {
  send(std::to_string(num));
}


/*
 * @brief wrapper for send(char*,uint16) that takes a std::string
 */
void UartChSubsys::send(std::string str) {
  if (str.length() < kMaxMsgLen) {
    send(str.c_str(), str.length());
  } else {
    std::string errorMsg = "*UART SubSystem Error: Message exceeds 100"
                           " character limit*\n";
    send(errorMsg.c_str(), errorMsg.length());
  }
}

/*
 * @brief wrapper for send(char*,uint16) that takes a c-style null-terminated
 *        character string
 */
void UartChSubsys::send(char * str) {
  send(static_cast<const char*>(str), strlen(str));
}

void UartChSubsys::send(const char * str) {
  if (strlen(str) < kMaxMsgLen) {
    send(str, strlen(str));
  } else {
    send("\n*UART SubSystem Error: Message exceeds 100 character limit*\n");
  }
}

/**
 * @TODO optimize with DMA and/or more efficient copies or moves
 */
void UartChSubsys::send(const char * str, uint16_t len) {
  // start data transmit if interface is ready
  if (m_d3IsReady) {
    m_d3IsReady = false;
    // copy string contents to class memory
    std::memcpy(m_txBuffer, str, len);
    // start transmit from the copied memory
    uartStopSend(m_uartp);
    uartStartSend(m_uartp, len, m_txBuffer);
  } else {
    // @TODO Consider performance of and alternatives to pushing back every byte
    //       of the passed string one at a time
    // @TODO Consider what happens if/when the CircularBuffer tx queue runs out
    //       of space

    // otherwise, queue for later
    std::lock_guard<chibios_rt::Mutex> txQueueGuard(m_d3TxQueueMut);
    // push every byte of the string onto the queue
    for (uint32_t i = 0; i < len; i++) {
      // push byte
      m_d3TxQueue.PushBack(*(str + i));
    }
  }
}

/*
 * @brief subsystem run function for CAN RX called from within a
 *        ChibiOS static thread
 */
void UartChSubsys::runRxThread() {
  uartStopReceive(m_uartp);
  uartStartReceive(m_uartp, 1, m_rxBuffer);

  // sleep forever, let callbacks take the wheel
  chThdSleepMilliseconds(1000*60*60*24);
}

UartChSubsys * UartChSubsys::getDriversSubsys(UARTDriver *uartp) {
  UartChSubsys *uartChSubsys = nullptr;

  for (uint32_t i = 0; i < UartChSubsys::registeredDrivers.size(); i++) {
    UARTDriver *_uartp = UartChSubsys::registeredDrivers[i];
    if (_uartp == uartp) {
      // found driver's subsys instance
      uartChSubsys = UartChSubsys::driverToSubsysLookup[i];
    }
  }

  return uartChSubsys;
}

void UartChSubsys::rxDone(UARTDriver *uartp) {
  // find class pointer corresponding to this uart driver via static
  // lookup table
  UartChSubsys *_this = UartChSubsys::getDriversSubsys(uartp);

  if (_this != nullptr) {
    // push byte-read event to the subsystem's event consumer
    // @TODO Add support for multi-byte messages (complicated given the
    //       limits on chibios 32b mailboxes, probably need to abstract
    //       that with the threading). Actually, could pretty easily make
    //       Events use a pointer to the multiple-byte message
    Event e = Event(Event::Type::kUartRx, _this->m_rxBuffer[0]);
    // if the event queue cannot be immediately acquired, the push fails
    bool success = _this->m_eventQueue.tryPush(e);
    if (!success) {

      // @TODO implement a callback / condition var notified function
      // that pushes the event to the queue when it frees up but
      // doesn't block in this thread when the lock on the consumer's
      // queue can't immediately be acquired

      // @note currently we drop RX bytes on the floor when the consumer
      //       isn't ready to receive them (this LED toggles)
      palTogglePad(STARTUP_LED_PORT, STARTUP_LED_PIN);
    }
    // start next rx
    uartStopReceive(uartp);
    uartStartReceive(uartp, 1, _this->m_rxBuffer);
  } else {
    // ERROR
    // ...
  }
}

void UartChSubsys::txEmpty(UARTDriver *uartp) {
  // this is called every time the UART interface finishes copying
  // over bytes from a software-level tx buffer (the one passed to
  // the async start send function)

  UartChSubsys *_this = UartChSubsys::getDriversSubsys(uartp);

  // if the queue is non-empty, acquire, copy up to kMaxMsgLen of its
  // content to the output buffer and start a new transmission
  if (_this->m_d3TxQueue.Size() > 0) {
    // acquire lock guard in current scope

    // @TODO Is this lock really necessary? What's wrong with the size
    //       changing between the corresponding instructions
    //std::lock_guard<chibios_rt::Mutex> lock(_this->m_d3TxQueueMut);

    // determine num tx bytes to transmit, clamping to kMaxMsgLen
    uint32_t txCount = _this->m_d3TxQueue.Size() > kMaxMsgLen
      ? kMaxMsgLen : _this->m_d3TxQueue.Size();
    // copy over msg contents
    for (uint32_t i = 0; i < txCount; i++) {
      _this->m_txBuffer[i] = _this->m_d3TxQueue.PopFront();
    }
    // start the tx
    uartStopSend(uartp);
    uartStartSend(uartp, txCount, _this->m_txBuffer);
  } else {
    // otherwise, clear the interface ready flag
    _this->m_d3IsReady = true;
  }
}
