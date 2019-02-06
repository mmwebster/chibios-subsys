#include "UartWriter.h"

#include "Uart.h"
#include "ch.hpp"
#include "hal.h"
#include "pinconf.h"

#include <utest/utest.hpp>
#include <utest/test_writer.hpp>

UartWriter::UartWriter(cal::Uart* uart) : m_uart(uart) {}

void UartWriter::write(const utest::TestString& str) noexcept {
  // send the test log over UART
  m_uart->send(str.data());
  // @TODO Optimize the Uart subsystem so that it can keep up with continusouly
  //       spammed prints and has a fall-back for and/or indication of when it
  //       can't. Actually just optimize and warn the user. There's no way to
  //       continusouly drive an interface with more than it can handle
  chThdSleepMilliseconds(1);
}

void UartWriter::color(utest::TestColor c) {
  (void)c;
}
