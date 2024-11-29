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

struct kmem{
  struct spinlock lock;
  struct run *freelist;
};

struct kmem kmems[NCPU];

void
kinit()
{
  for (int i = 0; i < NCPU; i++) {
    initlock(&kmems[i].lock, "kmem");
    kmems[i].freelist = 0;  // 初始化每个CPU的freelist为空
  }
  freerange(end, (void*)PHYSTOP);  // 为物理内存分配空闲内存
}

void 
freerange(void *pa_start, void *pa_end)
{
  char *p;
  int cpu = 0;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    struct run *r = (struct run*)p;

    acquire(&kmems[cpu].lock);
    r->next = kmems[cpu].freelist;
    kmems[cpu].freelist = r;  // 将内存块放入freelist
    release(&kmems[cpu].lock);

    cpu = (cpu + 1) % NCPU;  // 将内存分配给不同的CPU
  }
}


// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;
  int id = cpuid();

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;
  acquire(&kmems[id].lock);
  r->next = kmems[id].freelist;
  kmems[id].freelist = r;
  // 释放锁
  release(&kmems[id].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *kalloc(void) {
  struct run *r;
  int id = cpuid();  // 获取当前CPU的ID
  
  // 从当前CPU的freelist中获取内存块
  acquire(&kmems[id].lock);
  r = kmems[id].freelist;
  if (r) {
    kmems[id].freelist = r->next;
  }
  release(&kmems[id].lock);

  // 如果当前CPU的freelist为空，从其他CPU的freelist窃取内存块
  if (!r) {
    for (int i = 0; i < NCPU; i++) {
      if (i == id) continue;  // 不从自己CPU窃取

      acquire(&kmems[i].lock);
      if (kmems[i].freelist) {
        r = kmems[i].freelist;
        kmems[i].freelist = r->next;
        release(&kmems[i].lock);
        break;  // 一旦从其他CPU窃取成功，退出
      }
      release(&kmems[i].lock);
    }
  }

  if (r) {
    memset((char*)r, 5, PGSIZE); 
  }

  return (void*)r;
}
