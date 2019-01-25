#pragma once

#include <stdint.h>

#include <mutex>
#include <vector>
#include <string>

// @TODO finish the chibios-subsys common header that includes all (cal.hpp)
#include "../../cal.h"
#include "../../common/Gpio.h"
#include "../../common/Event.h"
#include "../../common/EventQueue.h"
#include "../../common/CircularBuffer.h"
#include "ch.h"
#include "hal.h"

namespace cal {

/**
 *
 * UART subsystem, sitting on top of the chibios UART driver. Only one
 * subsystem should be instantiated, and currently, only one driver
 * interface (UARTD3 is supported). This subsystem is meant to abstract
 * usage of the ChibiOS UART driver for C++. ChibiOS uses static
 * functions as callbacks to handle UART interface conditions/events. This
 * abstraction encapsulates those callbacks into a singleton by defining static
 * methods on the class for driver-independent callback handling that use a
 * static lookup table to acquire a reference to "this" and then operate on the
 * correct UART driver (e.g. &UARTD3) instance
 *
 * @TODO Eventually all chibios subsystem abstractions should be able run
 *       without user-provided thread run functions. Wrap this entire
 *       abstraction in a "cal" (ChibiOS Abstraction Layer... heh) namespace
 *       with standalone class names like cal::Uart
 *
 * @TODO Change this class's model from singleton subsystem to one instance per
 *       UART interface. The original singleton model revolved around needing
 *       a thread to do "synchronous" UART transmission before I figured out
 *       the callbacks. Now that callbacks are working, this class can be
 *       instantiated once for every UART interface instance (with guards
 *       against instantiating one multiple times) so that they're easier to use
 *       and keep track of in user code.
 *
 *       The new constructor for this class should accept an event queue, a UART
 *       interface, and (maybe optional) UART config parameters. The config
 *       params should be a different structure from the ChibiOS one b/c the
 *       user doesn't need to specify things like callback signatures and (for
 *       most cases) register values
 *
 * @TODO Confirm that the RX char static callback does a try-lock then queues
 *       the byte for later transmission if not successful. Must do a try
 *       because we can't block inside a chibios callback (it's basically a
 *       software interrupt that can starve the kernel). Must queue for later
 *       to ensure that they do eventually get to the desired event queue
 *
 * @TODO Set interface-specific event queues so that this abstraction supports
 *       multiple multi-producer, single-consumer event/message queues for
 *       different threads (e.g. Could be running two independent state machines
 *       that each use a different UART interface for messaging)
 *
 * @note The UART subsystem currently only supports one interface,
 *       which is currently hardcoded to UARTD3. Supporting mutliple
 *       interfaces requires moving constructor params to
 *       addInterface(...), figuring out how best to store driver
 *       config structs, and handling signals from each of the
 *       respective interfaces in the thread run function (and
 *       defining them with the callbacks passed to the config)
 */
class Uart {
 public:
  /**
   * @brief Initialize the UART interface instance and IMMEDIATELY begin
   *        receiving RX byte events in the provided event queue. Can also
   *        immediately begin sending bytes over the interface.
   * @param eventQueue Reference to queue to send this subsystem's
   *        events to. The event queue notifies itself.
   * @param ui Desired UART hardware interface to transmit and receive over
   */
  Uart(UartInterface ui, EventQueue& eq);

  /**
   * @brief Convert a float to a string
   * @note For now, this just truncates the float
   * @TODO move this out of the UART subsystem and into a common utils
   */
  static std::string to_string(float num);

  /*
   * @brief wrapper for send(char*,uint16) that takes a char
   */
  void send(char byte);

  void send(float num);
  void send(int num);

  /**
   * @brief C++ style wrapper for send(char*, uint16_t)
   */
  void send(std::string str);

  /*
   * @brief wrapper for send(char*,uint16) that takes a c-style null-terminated
   *        character string
   */
  void send(char * str);

  void send(const char * str);

  /**
   * @brief Queue a TX UART Frame for async transmission
   * @note The contents of str is immediately copied to memory
   *       allocated by the subsystem instance, and can therefore be
   *       forgotten about after send() exits
   * TODO: Make it UART interface-specific with the new multi-instance UART
   *       abstraction model
   */
  void send(const char * str, uint16_t len);

  /**
   * @TODO Make these private and only accessible from within the class
   *
   * @note below are callbacks used for each interface (currently just
   *       one). They are static so that their signatures match those
   *       required by ChibiOS and are still encapsulated by the class.
   *       To gain access to interface states, static vars are used in
   *       the class, that are essentially globals encapsulated in this
   *       class's namespace. These globals are necessary because the
   *       callbacks do not provide an arg for arbitrary data (like
   *       the virtual timer callbacks), which could otherwise have
   *       been used to pass a reference to a class instance.
   */
  // @brief This callback fires when an RX software buffer has been
  //        completely written
  static void rxDone(UARTDriver *uartp);

  // @brief This callback fires when a TX software buffer has been
  //        completely written to hardware interface buffers
  static void txEmpty(UARTDriver *uartp);

  // @brief Return pointer to the instance of self associated with the
  //        passed driver
  // @note Since this is a subsystem, it probably shouldn't need or
  //       have multiple instances, but who knows (currently always
  //       returns the single subsys instance)
  static Uart *getDriversSubsys(UARTDriver *uartp);

  // Max length of messages in bytes. Anything longer than this many
  // bytes must be broken down into multiple messages
  static constexpr uint32_t kMaxMsgLen = 100;

 private:
  // interface-specific members
  UartInterface m_uartInterface;

  // interface-wide members (static members facilitating static chibios
  // callbacks)

  /*
   * @note To navigate the ChibiOS constraint of static UART callbacks
   *       with a predefined signature (no arbitrary params), and
   *       since this callbacks are provided a pointer to the
   *       UARTDriver struct, callbacks will be statically registered
   *       (and encapsulated) within this class's (hopefully private)
   *       members, allowing a lookup of the interface instance
   *       corresponding to the callback that was hit
   */
  // contains pointers to all registered UARTDrivers, their index in
  // this array corresponds to the index of their Uart pointer
  // in driverToSubsysLookup
  static std::array<UARTDriver *,4> registeredDrivers;

  // contains pointers to instances of this class, used for lookup of
  // members from static C-style callbacks
  static std::array<Uart *,4> driverToSubsysLookup;

  // UARTDriver UARTD3 specific fields (need to generalize)
  CircularBuffer<char> m_d3TxQueue{kMaxMsgLen*2};
  chibios_rt::Mutex m_d3TxQueueMut;
  bool m_d3IsReady = true;
  char m_txBuffer[Uart::kMaxMsgLen] = {};

  static constexpr uint8_t kUartOkMask = EVENT_MASK(1);
  static constexpr uint8_t kUartChMask = EVENT_MASK(4);
  uint16_t m_rxBuffer[11];
  UARTDriver *m_uartp;
  UARTConfig m_uartConfig;
  chibios_rt::Mutex m_uartMut;
  EventQueue& m_eventQueue;
};

}
