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

#define NBUCKET 13
#define HASH(id) ((id) % NBUCKET)

struct {
  struct spinlock lock;
  struct buf head;
} bcache[NBUCKET];

struct buf buf[NBUF];
struct spinlock steal_lock;

void
binit(void)
{
  struct buf *b;

  for (int i = 0; i < NBUCKET; i++) {
    initlock(&bcache[i].lock, "bcache");
    bcache[i].head.prev = &bcache[i].head;
    bcache[i].head.next = &bcache[i].head;
  }

  initlock(&steal_lock, "steal lock");

  // Average distribute
  b = buf;
  for (int i = 0; b < buf + NBUF; i = (i + 1) % NBUCKET) {
    b->next = bcache[i].head.next;
    b->prev = &bcache[i].head;
    initsleeplock(&b->lock, "buffer");
    bcache[i].head.next->prev = b;
    bcache[i].head.next = b;
    b = b + 1;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  int id = HASH(blockno);
  acquire(&bcache[id].lock);

  for (b = bcache[id].head.next; b != &bcache[id].head; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bcache[id].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  for (b = bcache[id].head.prev; b != &bcache[id].head; b = b->prev) {
    if (b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache[id].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  release(&bcache[id].lock);

  acquire(&steal_lock);
  acquire(&bcache[id].lock);

  // retry
  for (b = bcache[id].head.next; b != &bcache[id].head; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bcache[id].lock);
      release(&steal_lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  for (b = bcache[id].head.prev; b != &bcache[id].head; b = b->prev) {
    if (b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache[id].lock);
      release(&steal_lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  
  // steal
  for (int i = (id + 1) % NBUCKET; i != id; i = (i + 1) % NBUCKET) {
    acquire(&bcache[i].lock);

    for (b = bcache[i].head.prev; b != &bcache[i].head; b = b->prev) {
      if (b->refcnt == 0) {
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;

        b->prev->next = b->next;
        b->next->prev = b->prev;
        release(&bcache[i].lock);

        b->next = bcache[id].head.next;
        b->prev = &bcache[id].head;
        bcache[id].head.next->prev = b;
        bcache[id].head.next = b;

        release(&bcache[id].lock);
        release(&steal_lock);
        acquiresleep(&b->lock);
        return b;
      }
    }

    release(&bcache[i].lock);
  }

  release(&bcache[id].lock);
  release(&steal_lock);

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
  if (!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int id = HASH(b->blockno);
  acquire(&bcache[id].lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache[id].head.next;
    b->prev = &bcache[id].head;
    bcache[id].head.next->prev = b;
    bcache[id].head.next = b;
  }

  release(&bcache[id].lock);
}

void
bpin(struct buf *b) {
  int id = HASH(b->blockno);
  acquire(&bcache[id].lock);
  b->refcnt++;
  release(&bcache[id].lock);
}

void
bunpin(struct buf *b) {
  int id = HASH(b->blockno);
  acquire(&bcache[id].lock);
  b->refcnt--;
  release(&bcache[id].lock);
}


