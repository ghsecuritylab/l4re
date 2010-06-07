/**
 * \file
 * \brief  Cache functions
 *
 * \date   2007-11
 * \author Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *
 */
/*
 * (c) 2007-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 *
 * As a special exception, you may use this file as part of a free software
 * library without restriction.  Specifically, if other files instantiate
 * templates or use macros or inline functions from this file, or you compile
 * this file and link it with other files to produce an executable, this
 * file does not by itself cause the resulting executable to be covered by
 * the GNU General Public License.  This exception does not however
 * invalidate any other reasons why the executable file might be covered by
 * the GNU General Public License.
 */
#ifndef __L4SYS__INCLUDE__ARCH_ARM__CACHE_H__
#define __L4SYS__INCLUDE__ARCH_ARM__CACHE_H__

#include <l4/sys/compiler.h>
#include <l4/sys/syscall_defs.h>

#include_next <l4/sys/cache.h>

L4_INLINE void
l4_cache_op_arm_call(unsigned long start,
                     unsigned long end,
                     unsigned long op);

L4_INLINE void
l4_cache_op_arm_call(unsigned long op,
                     unsigned long start,
                     unsigned long end)
{
  register unsigned long _op    asm ("r0") = op;
  register unsigned long _start asm ("r1") = start;
  register unsigned long _end   asm ("r2") = end;

  __asm__ __volatile__
    ("@ l4_cache_op_arm_call(start) \n\t"
     "mov     lr, pc	            \n\t"
     "mov     pc, %[sc]	            \n\t"
     "@ l4_cache_op_arm_call(end)   \n\t"
       :
	"=r" (_op),
	"=r" (_start),
	"=r" (_end)
       :
       [sc] "i" (L4_SYSCALL_CACHE_OP),
	"0" (_op),
	"1" (_start),
	"2" (_end)
       :
	"cc", "memory", "lr"
       );
}

enum
{
  L4_CACHE_OP_CLEAN_DATA        = 0,
  L4_CACHE_OP_FLUSH_DATA        = 1,
  L4_CACHE_OP_INV_DATA          = 2,
  L4_CACHE_OP_COHERENT          = 3,
  L4_CACHE_OP_DMA_COHERENT      = 4,
  L4_CACHE_OP_DMA_COHERENT_FULL = 5,
};

L4_INLINE void
l4_cache_clean_data(unsigned long start,
                    unsigned long end) L4_NOTHROW
{
  l4_cache_op_arm_call(L4_CACHE_OP_CLEAN_DATA, start, end);
}

L4_INLINE void
l4_cache_flush_data(unsigned long start,
                    unsigned long end) L4_NOTHROW
{
  l4_cache_op_arm_call(L4_CACHE_OP_FLUSH_DATA, start, end);
}

L4_INLINE void
l4_cache_inv_data(unsigned long start,
                  unsigned long end) L4_NOTHROW
{
  l4_cache_op_arm_call(L4_CACHE_OP_INV_DATA, start, end);
}

L4_INLINE void
l4_cache_coherent(unsigned long start,
                  unsigned long end) L4_NOTHROW
{
  l4_cache_op_arm_call(L4_CACHE_OP_COHERENT, start, end);
}

L4_INLINE void
l4_cache_dma_coherent(unsigned long start,
                      unsigned long end) L4_NOTHROW
{
  l4_cache_op_arm_call(L4_CACHE_OP_DMA_COHERENT, start, end);
}

L4_INLINE void
l4_cache_dma_coherent_full(void) L4_NOTHROW
{
  l4_cache_op_arm_call(L4_CACHE_OP_DMA_COHERENT_FULL, 0, 0);
}

#endif /* ! __L4SYS__INCLUDE__ARCH_ARM__CACHE_H__ */
