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
} kmem[NCPU];

void
kinit()
{
  for (int i = 0; i<NCPU; i++)
  {
    // char buf[9];
    // snprintf(buf, 6, "kmem_%d", i);
    // initlock(&kmem[i].lock, buf);
    initlock(&kmem[i].lock, "kmem");
  }
    
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
  push_off();
  int c = cpuid();
  pop_off();

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem[c].lock);
  r->next = kmem[c].freelist;
  kmem[c].freelist = r;
  release(&kmem[c].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  push_off();
  int c = cpuid();
  pop_off();

  acquire(&kmem[c].lock);
  r = kmem[c].freelist;
  if(r)
  {
    kmem[c].freelist = r->next;
    release(&kmem[c].lock);
  } else {
    release(&kmem[c].lock);
    for (int i = 0; i<NCPU; i++)
    {
      acquire(&kmem[i].lock);
      r = kmem[i].freelist;
      if(r)
      {
        kmem[i].freelist = r->next;
        release(&kmem[i].lock);
        break;
      } else {
        release(&kmem[i].lock);
      }
    }
  }
  if(r)
    memset((char*)r, 5, PGSIZE);  // fill with junk
  return (void*)r;
}
