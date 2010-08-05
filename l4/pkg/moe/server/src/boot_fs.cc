/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/util/mb_info.h>
#include <l4/util/util.h>
#include <l4/cxx/iostream>
#include <l4/cxx/l4iostream>

#include "boot_fs.h"
#include "dataspace_static.h"
#include "page_alloc.h"
#include "globals.h"
#include "name_space.h"
#include "debug.h"

#include <cstring>
#include <cstdlib>

using L4Re::Util::Names::Name_space;
namespace Names { using namespace L4Re::Util::Names; }

static Dbg dbg(Dbg::Boot_fs, "fs");
static Moe::Name_space rom_ns;

#if 0
#include <cstdio>
static void dump_mb_module(l4util_mb_mod_t const *mod)
{
  printf("  cmdline: '%s'\n"
         "  range: [%08x; %08x)\n", 
	 (char const *)mod->cmdline, mod->mod_start, mod->mod_end);
}

static void dump_mbi(l4util_mb_info_t const* mbi)
{
  printf("MBI Version: %08x\n", mbi->flags);
  printf("cmdline: '%s'\n", (char const*)mbi->cmdline);
  l4util_mb_mod_t const *modules = (l4util_mb_mod_t const *)mbi->mods_addr;
  for (unsigned i = 0; i < mbi->mods_count; ++i)
    dump_mb_module(modules +i);
}
#endif

static inline cxx::String cmdline_to_name(char const *cmdl)
{
  int len = strlen(cmdl);
  int i;
  for (i = 0; i < len; ++i)
    {
      if (i > 0 && cmdl[i] == ' ' && cmdl[i-1] != '\\')
	break;
    }

  int s;
  for (s = i-1; s >= 0; --s)
    {
      if (cmdl[s] == '/')
	break;
    }

  ++s;

  len = i-s;
  char *b = (char *)GC_MALLOC_ATOMIC(len);
  for (char *t = b; s < i; ++s, ++t)
    *t = cmdl[s];

  return cxx::String(b, len);
}


void
Moe::Boot_fs::init_stage1()
{
  l4util_mb_info_t const *mbi
    = (l4util_mb_info_t const *)kip()->user_ptr;

  l4_touch_ro(mbi,10);
}

void
Moe::Boot_fs::init_stage2()
{
  L4::Cap<void> rom_ns_cap = object_pool.cap_alloc()->alloc(&rom_ns);
  L4::cout << "MOE: rom name space cap -> " << rom_ns_cap << '\n';
  root_name_space()->register_obj("rom", Names::Obj(0, &rom_ns));

  L4::Cap<void> object;
  l4util_mb_info_t const *mbi
    = (l4util_mb_info_t const *)kip()->user_ptr;

  //dump_mbi(mbi);

  unsigned dirinfo_space = L4_PAGESIZE;
  char *dirinfo = (char *)Single_page_alloc_base::_alloc(dirinfo_space, L4_PAGESHIFT);
  unsigned dirinfo_size = 0;

  l4util_mb_mod_t const *modules = (l4util_mb_mod_t const *)mbi->mods_addr;
  unsigned num_modules = mbi->mods_count;
  for (unsigned mod = 3; mod < num_modules; ++mod)
    {
      l4_touch_ro((void*)modules[mod].mod_start,
	  modules[mod].mod_end - modules[mod].mod_start);

      //l4_addr_t end = l4_round_page(modules[mod].mod_end);
      l4_addr_t end = modules[mod].mod_end;

      Names::Name name = cmdline_to_name((char const *)modules[mod].cmdline);

      Moe::Dataspace_static *rf;
      rf = new Moe::Dataspace_static((void*)modules[mod].mod_start,
                                     end - modules[mod].mod_start,
                                     Dataspace::Cow_enabled);
      object = object_pool.cap_alloc()->alloc(rf);
      rom_ns.register_obj(name, Names::Obj(0, rf));

      do
        {
          unsigned left = dirinfo_space - dirinfo_size;
          unsigned written = snprintf(dirinfo + dirinfo_size, left, "%d:%.*s\n",
                                      name.len(), name.len(), name.start());
          if (written > left)
            {
              char *n = (char *)Single_page_alloc_base::_alloc(dirinfo_space + L4_PAGESIZE,
                                                               L4_PAGESHIFT);
              memcpy(n, dirinfo, dirinfo_space);
              Single_page_alloc_base::_free(dirinfo, dirinfo_space, true);
              dirinfo = n;
              dirinfo_space += L4_PAGESIZE;
            }
          else
            {
              dirinfo_size += written;
              break;
            }
        }
      while (1);


      L4::cout << "  BOOTFS: [" << (void*)modules[mod].mod_start << "-"
               << (void*)end << "] " << object << " "
               << name << "\n";
    }

  Moe::Dataspace_static *dirinfods;
  dirinfods = new Moe::Dataspace_static((void *)dirinfo,
                                        dirinfo_size,
                                        Dataspace::Read_only);

  object_pool.cap_alloc()->alloc(dirinfods);
  rom_ns.register_obj(".dirinfo", Names::Obj(0, dirinfods));
}


Moe::Dataspace *
Moe::Boot_fs::open_file(cxx::String const &name)
{
  dbg.printf("open file '%.*s' from root name space\n", name.len(), name.start());
  Names::Entry *n = root_name_space()->find_iter(name);
  if (n)
    return dynamic_cast<Moe::Dataspace*>(n->obj()->obj());

  return 0;
}

