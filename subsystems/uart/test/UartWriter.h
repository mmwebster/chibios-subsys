#pragma once

#include "../Uart.h"
//#include "../../../common/Event.h"
//#include "../../../common/EventQueue.h"
#include "ch.hpp"
#include "hal.h"

#include <utest/utest.hpp>
#include <utest/test_writer.hpp>

class UartWriter final : public utest::TestWriter {
  public:
    UartWriter(cal::Uart* uart) noexcept;

  private:
    virtual void write(const utest::TestString& str) noexcept override;

    virtual void color(utest::TestColor c) noexcept override;

    cal::Uart* m_uart;
};
