INTERFACE [arm && pf_rk3288]:

#include "kmem.h"
#include "l4_types.h"
#include "platform.h"

EXTENSION class Clock_base
{
protected:
  typedef Mword Counter;
};

// --------------------------------------------------------------
IMPLEMENTATION [arm && pf_rk3288]:

#include "io.h"
#include <cstdio>

IMPLEMENT inline NEEDS["io.h", <cstdio>]
Clock::Counter
Clock::read_counter() const
{
  return Platform::sys->read<Mword>(Platform::Sys::Cnt_24mhz);
}

IMPLEMENT inline
Cpu_time
Clock::us(Time t)
{
  return t / 24;
}