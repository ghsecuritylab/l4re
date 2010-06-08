/**
 * \file   l4re/lib/src/parent.cc
 * \brief  Parent
 */
/*
 * (c) 2008-2009 Technische Universität Dresden
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
#include <l4/cxx/ipc_stream>

#include <l4/re/parent>
#include <l4/re/parent-sys.h>
#include <l4/re/protocols>


namespace L4Re
{


long
Parent::signal(unsigned long sig, unsigned long val) const throw()
{
  L4::Ipc_iostream io(l4_utcb());
  io << L4::Opcode(Parent_::Signal) << sig << val;
  return l4_error(io.call(cap(), L4Re::Protocol::Parent));
}

};
