/*
 * (c) 2008-2010 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>,
 *               Björn Döbel <doebel@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
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

#include "ds_util.h"
#include "fd_store.h"
#include "vcon_stream.h"
#include "ns_fs.h"
#include "vfs_api.h"

#include <l4/re/env>
#include <l4/re/rm>
#include <l4/re/dataspace>

#include <l4/l4re_vfs/backend>

#include <unistd.h>
#include <cstdarg>
#include <errno.h>
#include <sys/uio.h>

//#include <l4/sys/kdebug.h>
//static int debug_mmap = 1;
//#define DEBUG_LOG(level, dbg...) do { if (level) dbg } while (0)

#define DEBUG_LOG(level, dbg...) do { } while (0)

/**
 * If USE_BIG_ANON_DS is defined the implementation will use a really big
 * data space for backing anonymous memory. Otherwise each mmap call
 * with anonymous memory will allocate a separate data space.
 */
#define USE_BIG_ANON_DS

using L4Re::Rm;

namespace {

struct Dl_env_ops
{
  void *(*malloc)(size_t);
  void (*free)(void*);

  unsigned long (*cap_alloc)();
  void (*cap_free)(unsigned long);
};

using cxx::Ref_ptr;

class Fd_store : public L4Re::Core::Fd_store
{
public:
  Fd_store() throw();
};


Fd_store::Fd_store() throw()
{
  static L4Re::Core::Vcon_stream s(L4Re::Env::env()->log());
  // make sure that we never delete the static io stream thing
  s.add_ref();
  set(0, cxx::ref_ptr(&s)); // stdin
  set(1, cxx::ref_ptr(&s)); // stdout
  set(2, cxx::ref_ptr(&s)); // stderr
}

class Root_mount_tree : public L4Re::Vfs::Mount_tree
{
public:
  Root_mount_tree() : L4Re::Vfs::Mount_tree(0) {}
  void operator delete (void *) {}
};

class Vfs : public L4Re::Vfs::Ops
{
private:
  bool _early_oom;

public:
  Vfs()
  : _early_oom(true), _root_mount(), _root(L4Re::Env::env()),
    _annon_size(0x10000000)
  {
    _root_mount.add_ref();
    _root.add_ref();
    _root_mount.mount(cxx::ref_ptr(&_root));
    _cwd = &_root;

#if 0
    Ref_ptr<L4Re::Vfs::File> rom;
    _root.openat("rom", 0, 0, &rom);

    _root_mount.create_tree("lib/foo", rom);

    _root.openat("lib", 0, 0, &_cwd);

#endif
  }

  int alloc_fd(Ref_ptr<L4Re::Vfs::File> const &f) throw();
  Ref_ptr<L4Re::Vfs::File> free_fd(int fd) throw();
  Ref_ptr<L4Re::Vfs::File> get_root() throw();
  Ref_ptr<L4Re::Vfs::File> get_cwd() throw();
  void set_cwd(Ref_ptr<L4Re::Vfs::File> const &dir) throw();
  Ref_ptr<L4Re::Vfs::File> get_file(int fd) throw();
  Ref_ptr<L4Re::Vfs::File> set_fd(int fd, Ref_ptr<L4Re::Vfs::File> const &f = Ref_ptr<>::Nil) throw();
  L4Re::Cap_alloc *cap_alloc() throw();

  int mmap2(void *start, size_t len, int prot, int flags, int fd,
            off_t offset, void **ptr) throw();

  int munmap(void *start, size_t len) throw();
  int mremap(void *old, size_t old_sz, size_t new_sz, int flags,
             void **new_adr) throw();
  int mprotect(const void *a, size_t sz, int prot) throw();
  int msync(void *addr, size_t len, int flags) throw();
  int madvise(void *addr, size_t len, int advice) throw();

  int register_file_system(L4Re::Vfs::File_system *f) throw();
  int unregister_file_system(L4Re::Vfs::File_system *f) throw();
  L4Re::Vfs::File_system *get_file_system(char const *fstype) throw();

  void operator delete (void *) {}

private:
  Root_mount_tree _root_mount;
  L4Re::Core::Env_dir _root;
  Ref_ptr<L4Re::Vfs::File> _cwd;
  Fd_store fds;

  L4Re::Vfs::File_system *_fs_registry;

  l4_addr_t _annon_size;
  l4_addr_t _annon_offset;
  L4::Cap<L4Re::Dataspace> _annon_ds;

  int alloc_ds(unsigned long size, L4::Cap<L4Re::Dataspace> *ds);
};

static inline bool strequal(char const *a, char const *b)
{
  for (;*a && *a == *b; ++a, ++b)
    ;
  return *a == *b;
}

int
Vfs::register_file_system(L4Re::Vfs::File_system *f) throw()
{
  using L4Re::Vfs::File_system;

  if (!f)
    return -EINVAL;

  for (File_system *c = _fs_registry; c; c = c->next())
    if (strequal(c->type(), f->type()))
      return -EEXIST;

  f->next(_fs_registry);
  _fs_registry = f;

  return 0;
}

int
Vfs::unregister_file_system(L4Re::Vfs::File_system *f) throw()
{
  using L4Re::Vfs::File_system;

  if (!f)
    return -EINVAL;

  File_system **p = &_fs_registry;

  for (; *p; p = &(*p)->next())
    if (*p == f)
      {
        *p = f->next();
	f->next() = 0;
	return 0;
      }

  return -ENOENT;
}

L4Re::Vfs::File_system *
Vfs::get_file_system(char const *fstype) throw()
{
  bool try_dynamic = true;
  for (;;)
    {
      using L4Re::Vfs::File_system;
      for (File_system *c = _fs_registry; c; c = c->next())
	if (strequal(c->type(), fstype))
	  return c;

      if (!try_dynamic)
	return 0;

      // try to load a file system module dynamically
      int res = Vfs_config::load_module(fstype);

      if (res < 0)
	return 0;

      try_dynamic = false;
    }
}

int
Vfs::alloc_fd(Ref_ptr<L4Re::Vfs::File> const &f) throw()
{
  int fd = fds.alloc();
  if (fd < 0)
    return -EMFILE;

  if (f)
    fds.set(fd, f);

  return fd;
}

Ref_ptr<L4Re::Vfs::File>
Vfs::free_fd(int fd) throw()
{
  Ref_ptr<L4Re::Vfs::File> f = fds.get(fd);

  if (!f)
    return Ref_ptr<>::Nil;

  fds.free(fd);
  return f;
}


Ref_ptr<L4Re::Vfs::File>
Vfs::get_root() throw()
{
  return cxx::ref_ptr(&_root);
}

Ref_ptr<L4Re::Vfs::File>
Vfs::get_cwd() throw()
{
  return _cwd;
}

void
Vfs::set_cwd(Ref_ptr<L4Re::Vfs::File> const &dir) throw()
{
  // FIXME: check for is dir
  if (dir)
    _cwd = dir;
}

Ref_ptr<L4Re::Vfs::File>
Vfs::get_file(int fd) throw()
{
  return fds.get(fd);
}

Ref_ptr<L4Re::Vfs::File>
Vfs::set_fd(int fd, Ref_ptr<L4Re::Vfs::File> const &f) throw()
{
  Ref_ptr<L4Re::Vfs::File> old = fds.get(fd);
  fds.set(fd, f);
  return old;
}

L4Re::Cap_alloc *
Vfs::cap_alloc() throw()
{
  return L4Re::Core::cap_alloc();
}



#define GET_FILE_DBG(fd, err) \
  Ref_ptr<L4Re::Vfs::File> fi = fds.get(fd); \
  if (!fi)                           \
    {                                \
      return -err;                   \
    }

#define GET_FILE(fd, err) \
  Ref_ptr<L4Re::Vfs::File> fi = fds.get(fd); \
  if (!fi)                           \
    return -err;


int
Vfs::munmap(void *start, size_t len) L4_NOTHROW
{
  using namespace L4;
  using namespace L4Re;

  int err;
  Cap<Dataspace> ds;

  while (1)
    {
      DEBUG_LOG(debug_mmap, {
	  outstring("DETACH: ");
	  outhex32(l4_addr_t(start));
	  outstring(" ");
	  outhex32(len);
	  outstring("\n");
      });
      err = Env::env()->rm()->detach(l4_addr_t(start), len, &ds, This_task);
      if (err < 0)
	return err;

      switch (err & Rm::Detach_result_mask)
	{
	case Rm::Split_ds:
	  if (ds.is_valid())
	    ds->take();
	  return 0;
	case Rm::Detached_ds:
	  if (ds.is_valid())
	    L4Re::Core::release_ds(ds);
	  break;
	default:
	  break;
	}

      if (!(err & Rm::Detach_again))
	return 0;
    }
}

int
Vfs::alloc_ds(unsigned long size, L4::Cap<L4Re::Dataspace> *ds)
{
  *ds = Vfs_config::cap_alloc.alloc<L4Re::Dataspace>();

  if (!ds->is_valid())
    return -ENOMEM;

  int err;
  if ((err = Vfs_config::allocator()->alloc(size, *ds)) < 0)
    return err;

  DEBUG_LOG(debug_mmap, {
      outstring("ANNON DS ALLOCATED: size=");
      outhex32(size);
      outstring("  cap=");
      outhex32(ds->cap());
      outstring("\n");
      });

  return 0;
}

int
Vfs::mmap2(void *start, size_t len, int prot, int flags, int fd, off_t _offset,
           void **resptr) L4_NOTHROW
{
  using namespace L4Re;
  off64_t offset = _offset << 12;

  start = (void*)l4_trunc_page(l4_addr_t(start));
  len   = l4_round_page(len);
  l4_umword_t size = (len + L4_PAGESIZE-1) & ~(L4_PAGESIZE-1);

  // special code to just reserve an area of the virtual address space
  if (flags & 0x1000000)
    {
      int err;
      L4::Cap<Rm> r = Env::env()->rm();
      l4_addr_t area = (l4_addr_t)start;
      err = r->reserve_area(&area, size, L4Re::Rm::Search_addr);
      if (err < 0)
	return err;
      *resptr = (void*)area;
      DEBUG_LOG(debug_mmap, {
	  outstring("MMAP reserved area: ");
	  outhex32(area);
	  outstring("  size=");
	  outhex32(size);
	  outstring("\n");
      });
      return 0;
    }

  L4::Cap<L4Re::Dataspace> ds;
  l4_addr_t annon_offset = 0;
  unsigned rm_flags = 0;

  if (flags & (MAP_ANONYMOUS | MAP_PRIVATE))
    {
      rm_flags |= L4Re::Rm::Detach_free;

#ifdef USE_BIG_ANON_DS
      if (!_annon_ds.is_valid() || _annon_offset + size >= _annon_size)
	{
          if (_annon_ds.is_valid())
            L4Re::Core::release_ds(_annon_ds);

	  int err;
	  if ((err = alloc_ds(_annon_size, &ds)) < 0)
	    return err;

	  _annon_offset = 0;
	  _annon_ds = ds;
	}
      else
        ds = _annon_ds;

      ds->take();

      if (_early_oom)
	{
	  if (int err = ds->allocate(_annon_offset, size))
	    return err;
	}

      annon_offset = _annon_offset;
      _annon_offset += size;
#else
      int err;
      if ((err = alloc_ds(size, &ds)) < 0)
	return err;

      if (_early_oom)
	{
	  if ((err = ds->allocate(0, size)))
	    return err;
	}
#endif

      DEBUG_LOG(debug_mmap, {
	  outstring("USE ANNON MEM: ");
	  outhex32(ds.cap());
	  outstring(" offs=");
	  outhex32(annon_offset);
	  outstring("\n");
      });
    }

  if (!(flags & MAP_ANONYMOUS))
    {
      Ref_ptr<L4Re::Vfs::File> fi = fds.get(fd);
      if (!fi)
	{
	  return -EBADF;
	}

      L4::Cap<L4Re::Dataspace> fds = fi->data_space();

      if (!fds.is_valid())
	{
	  return -EINVAL;
	}

      if (size + offset > l4_round_page(fds->size()))
	{
	  return -EINVAL;
	}

      if (flags & MAP_PRIVATE)
	{
	  DEBUG_LOG(debug_mmap, outstring("COW\n"););
	  ds->copy_in(annon_offset, fds, l4_trunc_page(offset), l4_round_page(size));
	  offset = annon_offset;
	}
      else
	{
          ds = fds;
	  ds->take();
	}
    }
  else
    offset = annon_offset;


  if (!(flags & MAP_FIXED) && start == 0)
    start = (void*)L4_PAGESIZE;

  int err;
  char *data = (char *)start;
  L4::Cap<Rm> r = Env::env()->rm();
  l4_addr_t overmap_area = L4_INVALID_ADDR;

  if (flags & MAP_FIXED)
    {
      overmap_area = l4_addr_t(start);

      err = r->reserve_area(&overmap_area, size);
      if (err < 0)
	overmap_area = L4_INVALID_ADDR;

      rm_flags |= Rm::In_area;

      while (1)
	{
	  L4::Cap<L4Re::Dataspace> ds;
	  err = r->detach(l4_addr_t(start), len, &ds, This_task);
	  if (err == -L4_ENOENT)
	    break;

	  if (err < 0)
	    return err;

	  switch (err & Rm::Detach_result_mask)
	    {
	    case Rm::Split_ds:
	      if (ds.is_valid())
		ds->take();
	      break;
	    case Rm::Detached_ds:
	      if (ds.is_valid())
		L4Re::Core::release_ds(ds);
	      break;
	    default:
	      break;
	    }

	  if (!(err & Rm::Detach_again))
	    break;
	}
    }

  if (!(flags & MAP_FIXED))  rm_flags |= Rm::Search_addr;
  if (!(prot & PROT_WRITE))  rm_flags |= Rm::Read_only;

  err = r->attach(&data, size, rm_flags, ds, offset);

  DEBUG_LOG(debug_mmap, {
      outstring("  MAPPED: ");
      outhex32(ds.cap());
      outstring("  addr: ");
      outhex32(l4_addr_t(data));
      outstring("  bytes: ");
      outhex32(size);
      outstring("  offset: ");
      outhex32(offset);
      outstring("  err=");
      outdec(err);
      outstring("\n");
  });


  if (overmap_area != L4_INVALID_ADDR)
    r->free_area(overmap_area);

  if (err < 0)
    return err;


  if (start && !data)
    return -EINVAL;

  *resptr = data;

  return 0;
}

int
Vfs::mremap(void *old_adr, size_t old_size, size_t new_size, int flags, void **new_adr) L4_NOTHROW
{
  (void)old_adr;
  (void)old_size;
  (void)new_size;
  (void)flags;
  (void)new_adr;
  return -ENOMEM;
}

int
Vfs::mprotect(const void *a, size_t sz, int prot) L4_NOTHROW
{
  (void)a;
  (void)sz;
  return (prot & PROT_WRITE) ? -1 : 0;
}

int
Vfs::msync(void *, size_t, int) L4_NOTHROW
{ return 0; }

int
Vfs::madvise(void *, size_t, int) L4_NOTHROW
{ return 0; }

static Vfs vfs __attribute__((init_priority(1000)));

}

//L4Re::Vfs::Ops *__ldso_posix_vfs_ops = &vfs;
void *__rtld_l4re_env_posix_vfs_ops = &vfs;
extern void *l4re_env_posix_vfs_ops __attribute__((alias("__rtld_l4re_env_posix_vfs_ops"), visibility("default")));


#undef DEBUG_LOG
#undef GET_FILE_DBG
#undef GET_FILE

