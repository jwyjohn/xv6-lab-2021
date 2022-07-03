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
  struct spinlock lock[NBUCKET];
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head[NBUCKET];
} bcache;

void
binit(void)
{
  struct buf *b;

  for (int i=0;i<NBUCKET;i++)
  {
    initlock(&bcache.lock[i], "bcache");
  }
  // Create linked list of buffers
  // bcache.head[0].prev = &bcache.head[0];
  bcache.head[0].next = &bcache.buf[0];
  for(b = bcache.buf; b < bcache.buf+NBUF-1; b++){
    b->next = b+1;
    initsleeplock(&b->lock, "buffer");
  }
  initsleeplock(&b->lock, "buffer");
}

// 抄的：https://zhuanlan.zhihu.com/p/426507542
void
write_cache(struct buf *take_buf, uint dev, uint blockno)
{
  take_buf->dev = dev;
  take_buf->blockno = blockno;
  take_buf->valid = 0;
  take_buf->refcnt = 1;
  take_buf->time = ticks;
}


// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
// 继续抄
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b, *last;
  struct buf *take_buf = 0;
  int id = HASH(blockno);
  acquire(&(bcache.lock[id]));

  // 在本池子中寻找是否已缓存，同时寻找空闲块，并记录链表最后一个节点便于待会插入新节点使用
  b = bcache.head[id].next;
  last = &(bcache.head[id]);
  for(; b; b = b->next, last = last->next)
  {

    if(b->dev == dev && b->blockno == blockno)
    {
      b->time = ticks;
      b->refcnt++;
      release(&(bcache.lock[id]));
      acquiresleep(&b->lock);
      return b;
    }
    if(b->refcnt == 0)
    {
      take_buf = b;
    }
  }

  //如果没缓存并且在本池子有空闲块，则使用它
  if(take_buf)
  { 
    write_cache(take_buf, dev, blockno);
    release(&(bcache.lock[id]));
    acquiresleep(&(take_buf->lock));
    return take_buf;
  }

  // 到其他池子寻找最久未使用的空闲块
  int lock_num = -1;

  uint64 time = __UINT64_MAX__;
  struct buf *tmp;
  struct buf *last_take = 0;
  for(int i = 0; i < NBUCKET; ++i)
  {

    if(i == id) continue;
    //获取寻找池子的锁
    acquire(&(bcache.lock[i]));

    for(b = bcache.head[i].next, tmp = &(bcache.head[i]); b; b = b->next,tmp = tmp->next)
    {
      if(b->refcnt == 0)
      {
        //找到符合要求的块
        if(b->time < time)
        {

          time = b->time;
          last_take = tmp;
          take_buf = b;
          //如果上一个空闲块不在本轮池子中，则释放那个空闲块的锁      
          if(lock_num != -1 && lock_num != i && holding(&(bcache.lock[lock_num])))
            release(&(bcache.lock[lock_num]));
          lock_num = i;
        }
      }
    }
    //没有用到本轮池子的块，则释放锁
    if(lock_num != i)
      release(&(bcache.lock[i]));
  }

  if (!take_buf) 
    panic("bget: no buffers");

  //将选中块从其他池子中拿出
  last_take->next = take_buf->next;
  take_buf->next = 0;
  release(&(bcache.lock[lock_num]));
  //将选中块放入本池子中，并写cache
  b = last;
  b->next = take_buf;
  write_cache(take_buf, dev, blockno);


  release(&(bcache.lock[id]));
  acquiresleep(&(take_buf->lock));

  return take_buf;
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

  int h = HASH(b->blockno);
  acquire(&bcache.lock[h]);
  b->refcnt--;
  release(&bcache.lock[h]);
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock[HASH(b->blockno)]);
  b->refcnt++;
  release(&bcache.lock[HASH(b->blockno)]);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock[HASH(b->blockno)]);
  b->refcnt--;
  release(&bcache.lock[HASH(b->blockno)]);
}

