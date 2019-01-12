# chibios-subsys
A work-in-progress for C++ wrapping of the ChibiOS threading and hardware abstraction mechanisms

# Why not just use chibios directly?
ChibiOS/RT is written purely in C, meaning that C++ style abstractions and encapsulation cannot be applied without first wrapping ChibiOS in object-friendly code. One such issue is that ChibiOS forces event callbacks to be staticly declared functions, which are a nightmare to deal with in the land of OOP and member functions that require a reference to "this". This library will provide abstractions on top of ChibiOS's drivers so that they don't clutter the global namespace and have a cleanly defined C++ style interface.

# Background
I'm writing this for use in a personal embedded audio project. More on that later.
