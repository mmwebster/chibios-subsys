#pragma once

#include <stdint.h>

#include <mutex>
#include <vector>
#include <string>

// @TODO finish the chibios-subsys common header that includes all (cal.hpp)
#include "../../cal.h"
#include "../uart/Uart.h"
#include "../../common/Gpio.h"
#include "../../common/Event.h"
#include "../../common/EventQueue.h"
#include "../../common/CircularBuffer.h"
#include "ch.h"
#include "hal.h"

namespace cal {

class EventSim {
 public:
  /**
   * @TODO After initial proof of concept with internal test generation, the
   *       provided UART interface may be used to receive simulated events
   *       externally
   * @TODO In all modules abstracted in the future, make them purely event
   *       driven and isolated. Then make them transmit their responses to the
   *       event queue of the module that sent them the request
   * @param uart The UART interface (already constructed) that this event
   *             simulator should output over
   * @param eq The event queue that this EventSim instance will send events to
   *           and receive events from
   */
  EventSim(cal::Uart& uart, EventQueue& testConsumer);

  void registerTest(std::string name,
      std::function<bool(EventQueue&, EventQueue&)> test);

 private:
  // interface to log over (and possibly receive test conditions from in the
  // future)
  cal::Uart& m_uart;

  // event queue to transmit events to and (eventually) receive responses from
  EventQueue& m_testConsumer;

  // evnet queue to receive events back from test consumer(s)
  EventQueue m_receiveQueue;
};

}
