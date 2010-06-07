INTERFACE [ppc32]:

#include "paging.h"

class Page_table;

class Kmem : public Mem_layout
{
public:
  static Pdir *kdir();
  static Pdir *dir();

  static Mword *kernel_sp();
  static void kernel_sp(Mword *);

  static Mword is_tcb_page_fault( Mword pfa, Mword error );
  static Mword is_kmem_page_fault( Mword pfa, Mword error );
  static Mword is_io_bitmap_page_fault( Mword pfa );

  static Address virt_to_phys(const void *addr);
private:
  static Pdir *_kdir;
  static Mword *_sp;
};

//---------------------------------------------------------------------------
IMPLEMENTATION [ppc32]:

#include "mem_layout.h"
#include "paging.h"
#include "panic.h"

char kernel_page_directory[sizeof(Pdir)];
Pdir *Kmem::_kdir = (Pdir *)&kernel_page_directory;
Mword *Kmem::_sp = 0;

IMPLEMENT inline
Pdir *Kmem::kdir()
{ return _kdir; }

IMPLEMENT inline
Pdir *Kmem::dir()
{ return _kdir; }

IMPLEMENT inline
Mword *Kmem::kernel_sp()
{ return _sp;}

IMPLEMENT inline
void Kmem::kernel_sp(Mword *sp)
{ _sp = sp; }

PUBLIC static inline NEEDS["mem_layout.h", "panic.h"]
Address Kmem::ipc_window(unsigned /*win*/)
{
  panic("%s not implemented", __PRETTY_FUNCTION__);
  return 0;
}

IMPLEMENT inline NEEDS["paging.h"]
Address Kmem::virt_to_phys(const void *addr)
{
  Address a = reinterpret_cast<Address>(addr);
  return kdir()->virt_to_phys(a);
}

//------------------------------------------------------------------------------
/*
 * dummy implementations 
 */

IMPLEMENT inline
Mword Kmem::is_kmem_page_fault(Mword pfa, Mword /*error*/)
{
  return in_kernel(pfa);
}

IMPLEMENT inline
Mword Kmem::is_tcb_page_fault(Mword /*pfa*/, Mword /*error*/ )
{
  return 0;
}

IMPLEMENT inline
Mword Kmem::is_io_bitmap_page_fault( Mword /*pfa*/ )
{
  return 0;
}
