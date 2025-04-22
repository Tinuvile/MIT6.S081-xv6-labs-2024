#include "param.h"
#include "types.h"
#include "memlayout.h"
#include "elf.h"
#include "riscv.h"
#include "defs.h"
#include "spinlock.h"
#include "proc.h"
#include "fs.h"

/*
 * the kernel's page table.
 */
pagetable_t kernel_pagetable;

extern char etext[];  // kernel.ld sets this to end of kernel code.

extern char trampoline[]; // trampoline.S

// Make a direct-map page table for the kernel.
pagetable_t
kvmmake(void)
{
  pagetable_t kpgtbl;

  kpgtbl = (pagetable_t) kalloc();
  memset(kpgtbl, 0, PGSIZE);

  // uart registers
  kvmmap(kpgtbl, UART0, UART0, PGSIZE, PTE_R | PTE_W);

  // virtio mmio disk interface
  kvmmap(kpgtbl, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

#ifdef LAB_NET
  // PCI-E ECAM (configuration space), for pci.c
  kvmmap(kpgtbl, 0x30000000L, 0x30000000L, 0x10000000, PTE_R | PTE_W);

  // pci.c maps the e1000's registers here.
  kvmmap(kpgtbl, 0x40000000L, 0x40000000L, 0x20000, PTE_R | PTE_W);
#endif  

  // PLIC
  kvmmap(kpgtbl, PLIC, PLIC, 0x4000000, PTE_R | PTE_W);

  // map kernel text executable and read-only.
  kvmmap(kpgtbl, KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);

  // map kernel data and the physical RAM we'll make use of.
  kvmmap(kpgtbl, (uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);

  // map the trampoline for trap entry/exit to
  // the highest virtual address in the kernel.
  kvmmap(kpgtbl, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);

  // allocate and map a kernel stack for each process.
  proc_mapstacks(kpgtbl);
  
  return kpgtbl;
}

// Initialize the one kernel_pagetable
void
kvminit(void)
{
  kernel_pagetable = kvmmake();
}

// Switch h/w page table register to the kernel's page table,
// and enable paging.
void
kvminithart()
{
  // wait for any previous writes to the page table memory to finish.
  sfence_vma();

  w_satp(MAKE_SATP(kernel_pagetable));

  // flush stale entries from the TLB.
  sfence_vma();
}

// Return the address of the PTE in page table pagetable
// that corresponds to virtual address va.  If alloc!=0,
// create any required page-table pages.
//
// plevel indicates the page level deepping into to alloc or look up for a pte
// If plevel == 0, search for a normal 4096-byte page until level 0
// If plevel == 1, search for a 2-megabyte super page until level 1
//
// The risc-v Sv39 scheme has three levels of page-table
// pages. A page-table page contains 512 64-bit PTEs.
// A 64-bit virtual address is split into five fields:
//   39..63 -- must be zero.
//   30..38 -- 9 bits of level-2 index.
//   21..29 -- 9 bits of level-1 index.
//   12..20 -- 9 bits of level-0 index.
//    0..11 -- 12 bits of byte offset within the page.
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc, int plevel)
{
  if(va >= MAXVA)
    panic("walk");

  for(int level = 2; level > plevel; level--) {
    pte_t *pte = &pagetable[PX(level, va)];
    if(*pte & PTE_V) {
      pagetable = (pagetable_t)PTE2PA(*pte);
#ifdef LAB_PGTBL
      if(PTE_LEAF(*pte)) {
        return pte;
      }
#endif
    } else {
      if(!alloc || (pagetable = (pde_t*)kalloc()) == 0)
        return 0;
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  return &pagetable[PX(plevel, va)];
}

// A wrapper function to look up pte of a virtual address
// where page may be a super page or a normal page
// return 0 if not mapped
pte_t *walkpte(pagetable_t pagetable, uint64 va){
  if(va >= MAXVA)
    return 0;

  pte_t *pte = 0;

  // try super page firstly
  pte = walk(pagetable, va, 0, 1);

  if (pte && !PTE_LEAF(*pte)){
    pte = walk(pagetable, va, 0, 0);
  }

  return pte;
}

// Look up a virtual address, return the physical address,
// or 0 if not mapped.
// Can only be used to look up user pages.
uint64
walkaddr(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa;

  if(va >= MAXVA)
    return 0;

  pte = walkpte(pagetable, va);
  if(pte == 0)
    return 0;
  if((*pte & PTE_V) == 0)
    return 0;
  if((*pte & PTE_U) == 0)
    return 0;
  pa = PTE2PA(*pte);
  return pa;
}


// add a mapping to the kernel page table.
// only used when booting.
// does not flush TLB or enable paging.
void
kvmmap(pagetable_t kpgtbl, uint64 va, uint64 pa, uint64 sz, int perm)
{
  if(mappages(kpgtbl, 0, va, sz, pa, perm) != 0)
    panic("kvmmap");
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa.
// plevel indicates page size, where is a 2-megabyte superpage if plevel == 1 and normal 4096-byte page if plevel == 0
// va and size MUST be both page-aligned or superpage-aligned
// Returns 0 on success, -1 if walk() couldn't
// allocate a needed page-table page.
int
mappages(pagetable_t pagetable, int plevel, uint64 va, uint64 size, uint64 pa, int perm)
{
  uint64 a, last;
  pte_t *pte;
  uint64 alignment = ALIGNMENT(plevel);

  if((va % alignment) != 0)
    panic("mappages: va not aligned");

  if((size % alignment) != 0)
    panic("mappages: size not aligned");

  if(size == 0)
    panic("mappages: size");
  
  a = va;
  last = va + size - alignment;
  for(;;){
    if((pte = walk(pagetable, a, 1, plevel)) == 0)
      return -1;
    if(*pte & PTE_V)
      panic("mappages: remap");
    *pte = PA2PTE(pa) | perm | PTE_V;
    if(a == last)
      break;
    a += alignment;
    pa += alignment;
  }
  return 0;
}

// Remove npages of mappings starting from va.
// plevel indicates page size, where is a 2-megabyte superpage if plevel == 1 and normal 4096-byte page if plevel == 0
// va must be page-aligned or superpage-aligned. The mappings must exist.
// Optionally free the physical memory.
void
uvmunmap(pagetable_t pagetable, int plevel, uint64 va, uint64 npages, int do_free)
{
  uint64 a;
  pte_t *pte;
  int sz;
  uint64 alignment = ALIGNMENT(plevel);

  if((va % alignment) != 0)
    panic("uvmunmap: not aligned");

  for(a = va; a < va + npages * alignment; a += sz){
    sz = alignment;
    if((pte = walk(pagetable, a, 0, plevel)) == 0)
      panic("uvmunmap: walk");
    if((*pte & PTE_V) == 0) {
      printf("va=%ld pte=%ld\n", a, *pte);
      panic("uvmunmap: not mapped");
    }
    if(PTE_FLAGS(*pte) == PTE_V)
      panic("uvmunmap: not a leaf");
    if(do_free){
      uint64 pa = PTE2PA(*pte);
      if (alignment == SUPERPGSIZE)
        superfree((void*)pa);
      else
        kfree((void*)pa);
    }
    *pte = 0;
  }
}

// create an empty user page table.
// returns 0 if out of memory.
pagetable_t
uvmcreate()
{
  pagetable_t pagetable;
  pagetable = (pagetable_t) kalloc();
  if(pagetable == 0)
    return 0;
  memset(pagetable, 0, PGSIZE);
  return pagetable;
}

// Load the user initcode into address 0 of pagetable,
// for the very first process.
// sz must be less than a page.
void
uvmfirst(pagetable_t pagetable, uchar *src, uint sz)
{
  char *mem;

  if(sz >= PGSIZE)
    panic("uvmfirst: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pagetable, 0, 0, PGSIZE, (uint64)mem, PTE_W |PTE_R|PTE_X|PTE_U);
  memmove(mem, src, sz);
}


// Allocate PTEs and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
uint64
uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz, int xperm)
{
  char *mem;
  uint64 a;
  int sz, level;
  int hassuper = 1;

  if(newsz < oldsz)
    return oldsz;

  oldsz = PGROUNDUP(oldsz);
  for(a = oldsz; a < newsz; a += sz){
    int superpage = hassuper && newsz - a >= SUPERPGSIZE && a % SUPERPGSIZE == 0;

    level = superpage ? 1 : 0;
    sz = superpage ? SUPERPGSIZE : PGSIZE;
    mem = superpage ? superalloc() : kalloc();

    // super page failed. cache the result then try normal page
    if(mem == 0 && superpage){
      level = 0;
      sz = PGSIZE;
      hassuper = 0;
      mem = kalloc();
    }

    // still failed
    if (mem == 0){
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
#ifndef LAB_SYSCALL
    memset(mem, 0, sz);
#endif
    if(mappages(pagetable, level, a, sz, (uint64)mem, PTE_R |PTE_U|xperm) != 0){
      superpage ? superfree(mem) : kfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
uint64
uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  if(newsz >= oldsz)
    return oldsz;

  uint64 page_size;

  for (uint64 va = PGROUNDUP(newsz); va < PGROUNDUP(oldsz); va += page_size){
    // try super page firstly
    page_size = SUPERPGSIZE;
    int level = 1;

    pte_t *pte = walk(pagetable, va, 0, level);

    if (pte == 0)
      panic("uvmfree");

    if (!PTE_LEAF(*pte)){
      page_size = PGSIZE;
      level = 0;
    }

    uvmunmap(pagetable, level, va, 1, 1);
  }

  return newsz;
}

// Recursively free page-table pages.
// All leaf mappings must already have been removed.
void
freewalk(pagetable_t pagetable)
{
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
      // this PTE points to a lower-level page table.
      uint64 child = PTE2PA(pte);
      freewalk((pagetable_t)child);
      pagetable[i] = 0;
    } else if(pte & PTE_V){
      panic("freewalk: leaf");
    }
  }
  kfree((void*)pagetable);
}

// Free user memory pages,
// then free page-table pages.
void
uvmfree(pagetable_t pagetable, uint64 sz)
{
  uint64 page_size;
  sz = PGROUNDUP(sz);

  for (uint64 va = 0; va < sz; va += page_size){
    // try super page firstly
    page_size = SUPERPGSIZE;
    int level = 1;

    pte_t *pte = walk(pagetable, va, 0, level);

    if (pte == 0)
      panic("uvmfree");

    if (!PTE_LEAF(*pte)){
      page_size = PGSIZE;
      level = 0;
    }

    uvmunmap(pagetable, level, va, 1, 1);
  }

  freewalk(pagetable);
}

// Given a parent process's page table, copy
// its memory into a child's page table.
// Copies both the page table and the
// physical memory.
// returns 0 on success, -1 on failure.
// frees any allocated pages on failure.
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t* pte;
  uint64 pa, va, page_size;
  uint flags;
  char* mem;
  int plevel;

  // va indicates such space which have been copied successfully
  for (va = 0; va < sz; va += page_size)
  {
    // try super page firstly
    page_size = SUPERPGSIZE;
    plevel = 1;
    if ((pte = walk(old, va, 0, plevel)) == 0)
      panic("uvmcopy: pte should exist");

    // try normal page if super page not found
    if (!PTE_LEAF(*pte))
    {
      page_size = PGSIZE;
      plevel = 0;
      if ((pte = walk(old, va, 0, plevel)) == 0)
        panic("uvmcopy: pte should exist");
    }

    // pte should be valid regardless of normal page or super page
    if ((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");

    // That pte is leaf indicates a 2mega-byte super page.
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);

    if ((mem = page_size == SUPERPGSIZE ? superalloc() : kalloc()) == 0){
      goto err;
    }

    memmove(mem, (char*)pa, page_size);

    if (mappages(new, plevel, va, page_size, (uint64)mem, flags) != 0)
    {
      page_size == SUPERPGSIZE ? superfree(mem) : kfree(mem);
      goto err;
    }
  }
  return 0;

err:
  for (uint64 free_va = 0; free_va < va; free_va += page_size)
  {
    // try super page firstly
    page_size = SUPERPGSIZE;
    plevel = 1;
    pte = walk(new, free_va, 0, plevel);

    // try normal page if super page not found
    if (!PTE_LEAF(*pte))
    {
      page_size = PGSIZE;
      plevel = 0;
    }

    uvmunmap(new, plevel, free_va, 1, 1);
  }

  return -1;
}

// mark a PTE invalid for user access.
// used by exec for the user stack guard page.
void
uvmclear(pagetable_t pagetable, uint64 va)
{
  pte_t *pte = walkpte(pagetable, va);

  if(pte == 0)
    panic("uvmclear");

  *pte &= ~PTE_U;
}

// Copy from kernel to user.
// Copy len bytes from src to virtual address dstva in a given page table.
// Return 0 on success, -1 on error.
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;
  pte_t *pte;

  while(len > 0){
    va0 = PGROUNDDOWN(dstva);
    if (va0 >= MAXVA)
      return -1;
    if((pte = walkpte(pagetable, va0)) == 0) {
      // printf("copyout: pte should exist 0x%x %d\n", dstva, len);
      return -1;
    }

    // forbid copyout over read-only user text pages.
    if((*pte & PTE_W) == 0)
      return -1;
    
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (dstva - va0);
    if(n > len)
      n = len;
    memmove((void *)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}

// Copy from user to kernel.
// Copy len bytes to dst from virtual address srcva in a given page table.
// Return 0 on success, -1 on error.
int
copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
  uint64 n, va0, pa0;
  
  while(len > 0){
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (srcva - va0);
    if(n > len)
      n = len;
    memmove(dst, (void *)(pa0 + (srcva - va0)), n);

    len -= n;
    dst += n;
    srcva = va0 + PGSIZE;
  }
  return 0;
}

// Copy a null-terminated string from user to kernel.
// Copy bytes to dst from virtual address srcva in a given page table,
// until a '\0', or max.
// Return 0 on success, -1 on error.
int
copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{
  uint64 n, va0, pa0;
  int got_null = 0;

  while(got_null == 0 && max > 0){
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (srcva - va0);
    if(n > max)
      n = max;

    char *p = (char *) (pa0 + (srcva - va0));
    while(n > 0){
      if(*p == '\0'){
        *dst = '\0';
        got_null = 1;
        break;
      } else {
        *dst = *p;
      }
      --n;
      --max;
      p++;
      dst++;
    }

    srcva = va0 + PGSIZE;
  }
  if(got_null){
    return 0;
  } else {
    return -1;
  }
}


#ifdef LAB_PGTBL
void
vmprint_recursive(pagetable_t pagetable, uint64 level, uint64 base_va){
    for(uint64 i = 0; i < 512; i++){
      pte_t pte = pagetable[i];
      if (!(pte & PTE_V))
        continue;

      for (uint64 i = 3; i > level; --i){
        printf(" ..");
      }

      uint64 va = base_va;
      va |= (i & PXMASK) << PXSHIFT(level);
      printf("%p: pte %p pa %p\n", (void*)va, (void*)pte, (void*)PTE2PA(pte));

      if (!PTE_LEAF(pte)){
        vmprint_recursive((pagetable_t)PTE2PA(pte), level - 1, va);
      }
    }
}

void
vmprint(pagetable_t pagetable) {
  printf("page table %p\n", pagetable);
  vmprint_recursive(pagetable, 2, 0);
}
#endif

#ifdef LAB_PGTBL
pte_t*
pgpte(pagetable_t pagetable, uint64 va) {
  return walk(pagetable, va, 0, 0);
}
#endif
