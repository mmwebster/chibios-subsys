# chibios-subsys
A work-in-progress C++ wrapper of the ChibiOS/RT threading and hardware abstraction mechanisms

# Why not just use chibios directly?
ChibiOS/RT is written purely in C, meaning that C++ style abstractions and encapsulation cannot be applied without first wrapping ChibiOS in object-friendly code. One such issue is that ChibiOS forces event callbacks to be staticly declared functions, which are a nightmare to deal with in the land of OOP and member functions that require a reference to "this". This library will provide abstractions on top of ChibiOS's drivers so that they don't clutter the global namespace and have a cleanly defined C++ style interface.

# ChibiOS Inteface Support
- **UART** (WIP, currently hard-coded to 1 driver but functional and mostly generalized)
- **EventSim** (Planned... maybe) (i.e. generate internal events from external UART event messages to simulate interfaces)
- **ADC** (Planned)
- **CAN** (Planned)
- **SPI** (Planned)
- **D-In** (Planned) (i.e. events on digital input transitions)
- **Thread** (Planned)
- **AnalogFilter** (Planned) (i.e. FIR and IIR lambdas generating sample events, using the ADC abstraction under the hood)

# Dependencies
## uTest
 - original source: https://github.com/tymonx/utest

uTest is an xUnit testing framework that I use for this project. I chose it because of its small footprint (targeted specifically toward cortex-MX), since it doesn't use macros, and since it has a cleanly defined interface for implementing a custom test writers/loggers (e.g. a UART one).

uTest can be installed manually by cloning the project (https://github.com/mmwebster/utest for the version this project is currently using), then using cmake/make to build and install it. The following cmake command is an example for the cortex-M4:

* `mkdir -p build && cd build`
* `cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/Toolchain-gcc-arm-none-eabi.cmake -DCMAKE_SYSTEM_PROCESSOR=cortex-m4 -DSEMIHOSTING=OFF -DEXAMPLES=OFF ..`
* `make install`

__NOTE:__ Currently the project is setup for the utest lib to be installed to /usr/local/. This will be abstracted into a user-specific config in the future.

# Background
I'm writing this for use in a personal embedded audio project. More on that later.
