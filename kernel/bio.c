// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

struct {
  struct spinlock lock[NBUCKETS];
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  // struct buf head;
  struct buf hashbucket[NBUCKETS]; //每个哈希队列一个linked list及一个lock
} bcache;

// 哈希函数
// 输入磁盘块号，返回哈希桶号
int hash(uint blockno)
{
  return blockno % NBUCKETS;
}

void
binit(void)
{
  struct buf *b;

  for(int i = 0; i < NBUCKETS; i++) {
    initlock(&bcache.lock[i], "bcache.hashbucket");
    bcache.hashbucket[i].prev = &bcache.hashbucket[i];
    bcache.hashbucket[i].next = &bcache.hashbucket[i];
  }

  // Create linked list of buffers
  // 因为buffer的分配在bget()中实现，所以这里先将所有buffer放到0号hashbucket中
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.hashbucket[0].next;
    b->prev = &bcache.hashbucket[0];
    initsleeplock(&b->lock, "buffer");
    bcache.hashbucket[0].next->prev = b;
    bcache.hashbucket[0].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  int bucketno = hash(blockno); // 磁盘块号对应的哈希桶号
  acquire(&bcache.lock[bucketno]);

  // Is the block already cached?
  for(b = bcache.hashbucket[bucketno].next; b != &bcache.hashbucket[bucketno]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock[bucketno]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer from other hashbuckets.
  for(int newbucketno = 0; newbucketno < NBUCKETS; newbucketno++) {
    if(newbucketno == bucketno)
      continue;
    if(newbucketno < bucketno && bcache.lock[newbucketno].locked == 1)
      continue;
    
    acquire(&bcache.lock[newbucketno]);
    for(b = bcache.hashbucket[newbucketno].prev; b != &bcache.hashbucket[newbucketno]; b = b->prev){
      if(b->refcnt == 0) {
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        // 从newbucketno号hashbucket中移出
        b->next->prev = b->prev;
        b->prev->next = b->next;
        // 移入到bucketno号hashbucket中使用(不考虑归还buffer)
        b->next = bcache.hashbucket[bucketno].next;
        b->prev = &bcache.hashbucket[bucketno];
        bcache.hashbucket[bucketno].next->prev = b;
        bcache.hashbucket[bucketno].next = b;
        // 释放锁
        release(&bcache.lock[newbucketno]);
        release(&bcache.lock[bucketno]);
        acquiresleep(&b->lock);
        return b;
      }
    }
    // 在newbucketno号hashbucket中没有找到未被使用的buffer，释放newbucketno号hashbucket的锁
    release(&bcache.lock[newbucketno]);
  }

  // 所有hashbucket中都没有找到未被使用的buffer，释放bucketno号hashbucket的锁
  release(&bcache.lock[bucketno]);
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int bucketno = hash(b->blockno);
  acquire(&bcache.lock[bucketno]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.hashbucket[bucketno].next;
    b->prev = &bcache.hashbucket[bucketno];
    bcache.hashbucket[bucketno].next->prev = b;
    bcache.hashbucket[bucketno].next = b;
  }
  
  release(&bcache.lock[bucketno]);
}

void
bpin(struct buf *b) {
  int bucketno = hash(b->blockno);
  acquire(&bcache.lock[bucketno]);
  b->refcnt++;
  release(&bcache.lock[bucketno]);
}

void
bunpin(struct buf *b) {
  int bucketno = hash(b->blockno);
  acquire(&bcache.lock[bucketno]);
  b->refcnt--;
  release(&bcache.lock[bucketno]);
}


