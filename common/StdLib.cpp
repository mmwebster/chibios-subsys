#include "StdLib.h"
#include "hal.h"  // NOLINT

#include <cstdarg>
#include <cstdlib>

#include "ch.h"
#include "chprintf.h"

// @NOTE Not sure what this was here for, but it conflicts with std::string bad
//       alloc functions
//namespace std {
//void __throw_bad_alloc() {
//#if (HAL_USE_SERIAL == TRUE)
//  printf("Unable to allocate memory");
//#endif
//}
//
//void __throw_length_error(const char* e) {
//#if (HAL_USE_SERIAL == TRUE)
//  printf("Length Error :%s\n", e);
//#else
//  static_cast<void>(e);
//#endif
//}
//}

void* operator new(size_t size) { return chHeapAllocAligned(nullptr, size, 1); }

void* operator new[](size_t size) {
  return chHeapAllocAligned(nullptr, size, 1);
}

void operator delete(void* ptr) { chHeapFree(ptr); }

void operator delete(void* ptr, size_t size) {
  static_cast<void>(size);

  chHeapFree(ptr);
}

void operator delete[](void* ptr) { chHeapFree(ptr); }

void operator delete[](void* ptr, size_t size) {
  static_cast<void>(size);

  chHeapFree(ptr);
}

// int __cxa_guard_acquire(__guard *g) {return !*(char *)(g);};
// void __cxa_guard_release (__guard *g) {*(char *)g = 1;};
// void __cxa_guard_abort (__guard *) {};

// void __cxa_pure_virtual(void) {};

#if (HAL_USE_SERIAL == TRUE)

namespace std {
int printf(const char* format, ...) {
  static BaseSequentialStream* chp =
      reinterpret_cast<BaseSequentialStream*>(&SD1);
  static bool init = false;
  if (!init) {
    // Activate serial driver 1 using the default driver configuration
    sdStart(&SD1, nullptr);
    init = true;
  }

  int size = 0;
  va_list ap;

  va_start(ap, format);
  size = chvprintf(chp, format, ap);
  va_end(ap);

  return size;
}
}  // namespace std
#endif
