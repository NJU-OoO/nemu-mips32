#ifndef DEVICE_H
#define DEVICE_H

#include <stdint.h>
#include "device.h"
#include "memory.h"


void *paddr_map(uint32_t paddr, uint32_t size);

typedef void* (*map_func) (uint32_t vaddr, uint32_t size);
typedef uint32_t (*read_func) (paddr_t addr, int len);
typedef void (*write_func)(paddr_t addr, int len, uint32_t data);

void *ddr_map(uint32_t vaddr, uint32_t size);
uint32_t ddr_read(paddr_t addr, int len);
void ddr_write(paddr_t addr, int len, uint32_t data);

/* uartlite protocol */
uint32_t serial_read(paddr_t addr, int len);
void serial_write(paddr_t addr, int len, uint32_t data);

void gpio_write(paddr_t addr, int len, uint32_t data);

uint32_t vga_read(paddr_t addr, int len);
void vga_write(paddr_t addr, int len, uint32_t data);

uint32_t invalid_read(paddr_t addr, int len);
void invalid_write(paddr_t addr, int len, uint32_t data);

uint32_t kb_read(paddr_t addr, int len);

void *bram_map(uint32_t vaddr, uint32_t size);
uint32_t bram_read(paddr_t addr, int len);
void bram_write(paddr_t addr, int len, uint32_t data);

/*
void *spi_map(uint32_t vaddr, uint32_t size);
uint32_t spi_read(paddr_t addr, int len);
void spi_write(paddr_t addr, int len, uint32_t data);
*/

// block ram
#define BRAM_BASE 0xbfc00000
#define BRAM_SIZE (1024 * 1024)

// DDR
#define DDR_BASE (0x80000000)
#define DDR_SIZE (128 * 1024 * 1024)

// UART
#define SERIAL_ADDR 0xbfe50000
#define SERIAL_SIZE  0x10

// KEYBOARD
#define KB_ADDR 0xbfe94000
#define KB_CODE 0x0
#define KB_STAT 0x4
#define KB_SIZE 0x10

// VGA
#define VGA_BASE 0xA0000000
#define VGA_SIZE 0x100000

#define SCR_W 400
#define SCR_H 300
#define WINDOW_W (SCR_W * 2)
#define WINDOW_H (SCR_H * 2)
#define VGA_HZ 25
#define TIMER_HZ 100

// GPIO
#define GPIO_BASE 0xA1000000
#define GPIO_SIZE 0x1000

#endif
