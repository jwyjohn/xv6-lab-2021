#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "e1000_dev.h"
#include "net.h"

#define TX_RING_SIZE 16
static struct tx_desc tx_ring[TX_RING_SIZE] __attribute__((aligned(16)));
static struct mbuf *tx_mbufs[TX_RING_SIZE];

#define RX_RING_SIZE 16
static struct rx_desc rx_ring[RX_RING_SIZE] __attribute__((aligned(16)));
static struct mbuf *rx_mbufs[RX_RING_SIZE];

// remember where the e1000's registers live.
static volatile uint32 *regs;

struct spinlock e1000_lock;

// called by pci_init().
// xregs is the memory address at which the
// e1000's registers are mapped.
void
e1000_init(uint32 *xregs)
{
  int i;

  initlock(&e1000_lock, "e1000");

  regs = xregs;

  // Reset the device
  regs[E1000_IMS] = 0; // disable interrupts
  regs[E1000_CTL] |= E1000_CTL_RST;
  regs[E1000_IMS] = 0; // redisable interrupts
  __sync_synchronize();

  // [E1000 14.5] Transmit initialization
  memset(tx_ring, 0, sizeof(tx_ring));
  for (i = 0; i < TX_RING_SIZE; i++) {
    tx_ring[i].status = E1000_TXD_STAT_DD;
    tx_mbufs[i] = 0;
  }
  regs[E1000_TDBAL] = (uint64) tx_ring;
  if(sizeof(tx_ring) % 128 != 0)
    panic("e1000");
  regs[E1000_TDLEN] = sizeof(tx_ring);
  regs[E1000_TDH] = regs[E1000_TDT] = 0;
  
  // [E1000 14.4] Receive initialization
  memset(rx_ring, 0, sizeof(rx_ring));
  for (i = 0; i < RX_RING_SIZE; i++) {
    rx_mbufs[i] = mbufalloc(0);
    if (!rx_mbufs[i])
      panic("e1000");
    rx_ring[i].addr = (uint64) rx_mbufs[i]->head;
  }
  regs[E1000_RDBAL] = (uint64) rx_ring;
  if(sizeof(rx_ring) % 128 != 0)
    panic("e1000");
  regs[E1000_RDH] = 0;
  regs[E1000_RDT] = RX_RING_SIZE - 1;
  regs[E1000_RDLEN] = sizeof(rx_ring);

  // filter by qemu's MAC address, 52:54:00:12:34:56
  regs[E1000_RA] = 0x12005452;
  regs[E1000_RA+1] = 0x5634 | (1<<31);
  // multicast table
  for (int i = 0; i < 4096/32; i++)
    regs[E1000_MTA + i] = 0;

  // transmitter control bits.
  regs[E1000_TCTL] = E1000_TCTL_EN |  // enable
    E1000_TCTL_PSP |                  // pad short packets
    (0x10 << E1000_TCTL_CT_SHIFT) |   // collision stuff
    (0x40 << E1000_TCTL_COLD_SHIFT);
  regs[E1000_TIPG] = 10 | (8<<10) | (6<<20); // inter-pkt gap

  // receiver control bits.
  regs[E1000_RCTL] = E1000_RCTL_EN | // enable receiver
    E1000_RCTL_BAM |                 // enable broadcast
    E1000_RCTL_SZ_2048 |             // 2048-byte rx buffers
    E1000_RCTL_SECRC;                // strip CRC
  
  // ask e1000 for receive interrupts.
  regs[E1000_RDTR] = 0; // interrupt after every received packet (no timer)
  regs[E1000_RADV] = 0; // interrupt after every packet (no timer)
  regs[E1000_IMS] = (1 << 7); // RXDW -- Receiver Descriptor Write Back
}

int
e1000_transmit(struct mbuf *m)
{
  //
  // Your code here.
  //
  // the mbuf contains an ethernet frame; program it into
  // the TX descriptor ring so that the e1000 sends it. Stash
  // a pointer so that it can be freed after sending.
  //
  acquire(&e1000_lock);
  // printf("e1000_transmit: called mbuf=%p\n",m);
  uint32 idx = regs[E1000_TDT]; 
  // ask the E1000 for the TX ring index 
  // at which it's expecting the next packet, 
  // by reading the E1000_TDT control register.
  if (tx_ring[idx].status != E1000_TXD_STAT_DD)
  {
    // check if the the ring is overflowing
    // If E1000_TXD_STAT_DD is not set in the descriptor indexed by E1000_TDT, 
    // the E1000 hasn't finished the corresponding previous transmission request, 
    // so return an error.
    printf("e1000_transmit: tx queue full\n");
    __sync_synchronize();
    // https://www.cnblogs.com/weijunji/p/xv6-study-16.html 的作者这里有__sync_synchronize(); 但本猫还没搞明白其作用。
    release(&e1000_lock);
    
    return -1;
  } else {
    // Otherwise, use mbuffree() to free the last mbuf 
    // that was transmitted from that descriptor (if there was one).
    if (tx_mbufs[idx] != 0)
    {
      // printf("e1000_transmit: freeing old mbuf tx_mbufs[%d]=%p\n",idx,tx_mbufs[idx]);
      mbuffree(tx_mbufs[idx]);
    }
      
    // Then fill in the descriptor. 
    // m->head points to the packet's content in memory, 
    // and m->len is the packet length.
    tx_ring[idx].addr = (uint64) m->head;
    tx_ring[idx].length = (uint16) m->len;
    tx_ring[idx].cso = 0;
    tx_ring[idx].css = 0;
    // Set the necessary cmd flags 
    // (look at Section 3.3 in the E1000 manual)
    tx_ring[idx].cmd = E1000_TXD_CMD_RS | E1000_TXD_CMD_EOP;
    // and stash away a pointer to the mbuf for later freeing.
    tx_mbufs[idx] = m;
    // Finally, update the ring position 
    // by adding one to E1000_TDT modulo TX_RING_SIZE.
    regs[E1000_TDT] = (regs[E1000_TDT] + 1) % TX_RING_SIZE;
    // printf("e1000_transmit: package added to tx queue %d\n",idx);
  }
  __sync_synchronize();
  // https://www.cnblogs.com/weijunji/p/xv6-study-16.html 的作者这里有__sync_synchronize(); 但本猫还没搞明白其作用。
  release(&e1000_lock);

  return 0;
}

extern void net_rx(struct mbuf *);
static void
e1000_recv(void)
{
  //
  // Your code here.
  //
  // Check for packets that have arrived from the e1000
  // Create and deliver an mbuf for each packet (using net_rx()).
  //

  uint32 idx = (regs[E1000_RDT] + 1) % RX_RING_SIZE;
  struct rx_desc* dest = &rx_ring[idx];
  // check if a new packet is available 
  // by checking for the E1000_RXD_STAT_DD bit in the status portion of the descriptor
  // 查了https://www.cnblogs.com/weijunji/p/xv6-study-16.html
  // e1000_recv函数，这里注意一次中断应该把所有到达的数据都处理掉，剩下的按hints里面的来就行了
  while (rx_ring[idx].status & E1000_RXD_STAT_DD)
  {
    acquire(&e1000_lock);

    struct mbuf *buf = rx_mbufs[idx];
    mbufput(buf, dest->length);
    if (!(rx_mbufs[idx] = mbufalloc(0)))
      panic("mbuf alloc failed");
    dest->addr = (uint64)rx_mbufs[idx]->head;
    dest->status = 0;
    regs[E1000_RDT] = idx;
    __sync_synchronize();
    // https://www.cnblogs.com/weijunji/p/xv6-study-16.html 的作者这里有__sync_synchronize(); 但本猫还没搞明白其作用。
    // 好像会使得发包收包的顺序有变化
    release(&e1000_lock);

    net_rx(buf);
    idx = (regs[E1000_RDT] + 1) % RX_RING_SIZE;
    dest = &rx_ring[idx];
  }
  
}

void
e1000_intr(void)
{
  // tell the e1000 we've seen this interrupt;
  // without this the e1000 won't raise any
  // further interrupts.
  regs[E1000_ICR] = 0xffffffff;

  e1000_recv();
}
