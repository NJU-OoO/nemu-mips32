#include <SDL/SDL.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "m25p80.c"

#include "common.h"
#include "device.h"
#include "queue.h"
#include "utils.h"

#define SPI_SIZE 0x1000

/* SPI Control Register (spicr), [1] p9, [2] p8 */
#define SPICR_LSB_FIRST (1 << 9)
#define SPICR_MASTER_INHIBIT (1 << 8)
#define SPICR_MANUAL_SS (1 << 7)
#define SPICR_RXFIFO_RESET (1 << 6)
#define SPICR_TXFIFO_RESET (1 << 5)
#define SPICR_CPHA (1 << 4)
#define SPICR_CPOL (1 << 3)
#define SPICR_MASTER_MODE (1 << 2)
#define SPICR_SPE (1 << 1)
#define SPICR_LOOP (1 << 0)

/* SPI Status Register (spisr), [1] p11, [2] p10 */
#define SPISR_SLAVE_MODE_SELECT (1 << 5)
#define SPISR_MODF (1 << 4)
#define SPISR_TX_FULL (1 << 3)
#define SPISR_TX_EMPTY (1 << 2)
#define SPISR_RX_FULL (1 << 1)
#define SPISR_RX_EMPTY (1 << 0)

/* SPI Data Transmit Register (spidtr), [1] p12, [2] p12 */
#define SPIDTR_8BIT_MASK GENMASK(7, 0)
#define SPIDTR_16BIT_MASK GENMASK(15, 0)
#define SPIDTR_32BIT_MASK GENMASK(31, 0)

/* SPI Data Receive Register (spidrr), [1] p12, [2] p12 */
#define SPIDRR_8BIT_MASK GENMASK(7, 0)
#define SPIDRR_16BIT_MASK GENMASK(15, 0)
#define SPIDRR_32BIT_MASK GENMASK(31, 0)

/* SPI Slave Select Register (spissr), [1] p13, [2] p13 */
#define SPISSR_MASK(cs) (1 << (cs))
#define SPISSR_ACT(cs) ~SPISSR_MASK(cs)
#define SPISSR_OFF ~0UL

/* SPI Software Reset Register (ssr) */
#define SPISSR_RESET_VALUE 0x0a

#define XILSPI_MAX_XFER_BITS 8
#define XILSPI_SPICR_DFLT_ON \
  (SPICR_MANUAL_SS | SPICR_MASTER_MODE | SPICR_SPE | SPICR_MASTER_INHIBIT)
#define XILSPI_SPICR_DFLT_OFF (SPICR_MASTER_INHIBIT | SPICR_MANUAL_SS)

#ifndef CONFIG_XILINX_SPI_IDLE_VAL
#  define CONFIG_XILINX_SPI_IDLE_VAL GENMASK(7, 0)
#endif

#define XILINX_SPI_QUAD_MODE 2

#define XILINX_SPI_QUAD_EXTRA_DUMMY 3
#define SPI_QUAD_OUT_FAST_READ 0x6B

#define SRR 0x40     // write
#define SPICR 0x60   // rw,  0x180
#define SPISR 0x64   // r,   0x25
#define SPIDTR 0x68  // w
#define SPIDRR 0x6c  // r
#define SPISSR 0x70  // rw
#define SPITFOR 0x74 // r
#define SPIRFOR 0x78 // r

struct xilinx_spi_regs {
  u32 __space0__[7];
  u32 dgier; /* Device Global Interrupt Enable Register (DGIER) */
  u32 ipisr; /* IP Interrupt Status Register (IPISR) */
  u32 __space1__;
  u32 ipier; /* IP Interrupt Enable Register (IPIER) */
  u32 __space2__[5];

  u32 srr; /* !!  Softare Reset Register (SRR) */

  u32 __space3__[7];

  u32 spicr;  /* !!  SPI Control Register (SPICR) */
  u32 spisr;  /* !!  SPI Status Register (SPISR) */
  u32 spidtr; /* !!  SPI Data Transmit Register (SPIDTR) */
  u32 spidrr; /* !!  SPI Data Receive Register (SPIDRR) */
  u32 spissr; /* !!  SPI Slave Select Register (SPISSR) */

  u32 spitfor; /* SPI Transmit FIFO Occupancy Register (SPITFOR) */
  u32 spirfor; /* SPI Receive FIFO Occupancy Register (SPIRFOR) */
};

static queue_type(int, 1024) spi_tx_q;
static queue_type(int, 1024) spi_rx_q;

static struct xilinx_spi_regs xlnx_spi_regs;

static Flash flash;

void xlnx_spi_flush_txfifo() {
  while (!queue_is_empty(spi_tx_q)) {
    int data = queue_pop(spi_tx_q);
    int r = 0;
    if ((xlnx_spi_regs.spissr & 1) == 0)
      r = m25p80_transfer8(&flash, data);
    // printf("[NEMU] send %02x, recv %02x\n", data, r);
    queue_push(spi_rx_q, r);
  }
}

static void xlnx_spi_init(const char *filename) {
  m25p80_init(&flash);

  xlnx_spi_regs.spicr = 0x180;
  xlnx_spi_regs.spisr = 0x25;
  xlnx_spi_regs.spissr = ~0;
}

// static int counter = 0;

static uint32_t xlnx_spi_read(paddr_t addr, int len) {
  check_aligned_ioaddr(addr, len, SPI_SIZE, "spi.read");

  switch (addr) {
  case SPISR: {
    uint32_t spisr = xlnx_spi_regs.spisr & ~(SPISR_RX_EMPTY | SPISR_RX_FULL |
                                               SPISR_TX_EMPTY | SPISR_TX_FULL);
    if (queue_is_empty(spi_rx_q)) spisr |= SPISR_RX_EMPTY;
    if (queue_is_full(spi_rx_q)) spisr |= SPISR_RX_FULL;
    if (queue_is_empty(spi_tx_q)) spisr |= SPISR_TX_EMPTY;
    if (queue_is_full(spi_tx_q)) spisr |= SPISR_TX_FULL;
    return spisr;
  } break;
  case SPIDRR:
    // printf("[NEMU] recv %02x\n", queue_top(spi_rx_q));
    if (queue_is_empty(spi_rx_q)) return 0xdeadbeef;
    return queue_pop(spi_rx_q);
  default:
    if (addr + 4 <= sizeof(xlnx_spi_regs)) {
      uint32_t data = 0;
      void *regs_ptr = (void *)&xlnx_spi_regs;
      memcpy(&data, regs_ptr + addr, sizeof(data));
      return data;
    } else
      CPUAssert(false, "spi: address(0x%08x) is not readable", addr);
    break;
  }
  return 0;
}

static void xlnx_spi_write(paddr_t addr, int len, uint32_t data) {
  check_aligned_ioaddr(addr, len, SPI_SIZE, "spi.read");
  switch (addr) {
  case SRR:
    if (data == 0xa) {
      queue_reset(spi_rx_q);
      queue_reset(spi_tx_q);
      xlnx_spi_regs.spissr = ~0;
    }
    break;
  case SPICR:
    if (data & SPICR_RXFIFO_RESET) queue_reset(spi_rx_q);
    if (data & SPICR_TXFIFO_RESET) queue_reset(spi_tx_q);

    xlnx_spi_regs.spicr = data & ~(SPICR_RXFIFO_RESET | SPICR_TXFIFO_RESET);
    if (!(data & SPICR_MASTER_INHIBIT)) { xlnx_spi_flush_txfifo(); }
    break;
  case SPISR:
    /* xlnx_spi_regs.spisr = data; */
    break;
  case SPIDTR: {
    queue_push(spi_tx_q, data);
    if (xlnx_spi_regs.spicr & SPICR_MASTER_INHIBIT) xlnx_spi_flush_txfifo();
  } break;
  case SPIDRR: break;
  case SPISSR: xlnx_spi_regs.spissr = data & 1; break;
  case SPITFOR: xlnx_spi_regs.spitfor = data; break;
  case SPIRFOR: xlnx_spi_regs.spirfor = data; break;
  default:
    CPUAssert(false, "spi: address(0x%08x) is not writable", addr);
    break;
  }
}

DEF_DEV(xlnx_spi_dev) = {
    .name = "xilinx-spi",
    .start = CONFIG_XLNX_SPI_BASE,
    .end = CONFIG_XLNX_SPI_BASE + SPI_SIZE,
    .init = xlnx_spi_init,
    .read = xlnx_spi_read,
    .write = xlnx_spi_write,
};
