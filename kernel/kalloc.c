// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

#define PA2IDX(p) (((uint64)(p)) / PGSIZE)

struct {
  struct spinlock lock;
  int cnt[PHYSTOP / PGSIZE];
} kref;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&kref.lock, "kref");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  acquire(&kref.lock);

  kref.cnt[PA2IDX(pa)] -= 1;
  if (kref.cnt[PA2IDX(pa)] <= 0) {
    release(&kref.lock);

    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);

    r = (struct run*)pa;

    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);

    return;
  }

  release(&kref.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r) {
    memset((char*)r, 5, PGSIZE); // fill with junk
    kref.cnt[PA2IDX(r)] = 1;
  }
  return (void*)r;
}

void
kref_inc(void *pa)
{
  acquire(&kref.lock);
  kref.cnt[PA2IDX(pa)] += 1;
  release(&kref.lock);
}

void *
kref_copy(void *pa)
{
  acquire(&kref.lock);
  if (kref.cnt[PA2IDX(pa)] <= 1) {
    release(&kref.lock);
    return pa;
  }

  void *newpa = kalloc();
  if (newpa == 0) {
    release(&kref.lock);
    return 0;
  }

  kref.cnt[PA2IDX(pa)] -= 1;
  release(&kref.lock);

  memmove((void*)newpa, (void*)pa, PGSIZE);

  return newpa;
}