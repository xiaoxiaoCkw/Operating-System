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

struct kmem {
  struct spinlock lock;
  struct run *freelist;
};

struct kmem kmems[NCPU]; // 每个CPU使用独立的链表

void
kinit()
{
  for(int i = 0; i < NCPU; i++)
    initlock(&kmems[i].lock, "kmem");
  freerange(end, (void*)PHYSTOP); // 将内存分配给调用此函数的CPU
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

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off(); // 关闭中断
  int cpu_id = cpuid(); // 获取运行进程的CPUid
  pop_off(); // 打开中断

  // 将释放的物理内存页添加到运行此函数的CPU的链表中
  acquire(&kmems[cpu_id].lock);
  r->next = kmems[cpu_id].freelist;
  kmems[cpu_id].freelist = r;
  release(&kmems[cpu_id].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off(); // 关闭中断
  int cpu_id = cpuid(); // 获取运行进程的CPUid
  pop_off(); // 打开中断

  acquire(&kmems[cpu_id].lock);
  // 优先分配当前CPU的freelist中的内存块
  r = kmems[cpu_id].freelist;
  if(r)
    kmems[cpu_id].freelist = r->next;
  else {
    // 当前CPU没有空闲内存块，则从其他CPU的freelist中窃取内存块
    for(int i = 0; i < NCPU; i++) {
      if(i == cpu_id)
        continue;
      
      acquire(&kmems[i].lock);
      r = kmems[i].freelist;
      if(r) {
        // i号CPU有空闲内存块，窃取内存块
        kmems[i].freelist = r->next;
        release(&kmems[i].lock);
        break;
      }
      // 否则，释放锁继续查找下一个CPU
      release(&kmems[i].lock);
    }
  }
  release(&kmems[cpu_id].lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
