// SPDX-License-Identifier: Zlib
#include <kern/console.h>
#include <kern/fbcon.h>
#include <kern/panic.h>
#include <kern/printf.h>
#include <limine.h>
#include <mm/page.h>
#include <mm/pmm.h>
#include <string.h>

#define CHR_BS  0x08
#define CHR_HT  0x09
#define CHR_LF  0x0A
#define CHR_VT  0x0B
#define CHR_FF  0x0C
#define CHR_CR  0x0D
#define CHR_WS  0x20
#define CHR_DEL 0x7F

#define FONT_WIDTH      0x08
#define FONT_HEIGHT     0x10
#define FONT_STRIDE     0x10 // bytes per glyph
#define FONT_SCANLINE   0x01 // bytes per line
#define FONT_MAX_CP     0xFF

// FIXME: this is not a good place to house the framebuffer request
static volatile struct limine_framebuffer_request __used framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0,
    .response = NULL,
};

static const unsigned char font[4096] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x7E, 0x81, 0xA5, 0x81, 0x81, 0xBD, 0x99, 0x81, 0x7E, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x7E, 0xFF, 0xDB, 0xFF, 0xFF, 0xC3, 0xE7, 0xFF, 0x7E, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x36, 0x7F, 0x7F, 0x7F, 0x7F, 0x3E, 0x1C, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x08, 0x1C, 0x3E, 0x7F, 0x3E, 0x1C, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x1C, 0x1C, 0x08, 0x6B, 0x7F, 0x6B, 0x08, 0x1C, 0x1C, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x08, 0x1C, 0x3E, 0x7F, 0x7F, 0x3E, 0x08, 0x1C, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x3C, 0x3C, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xE7, 0xC3, 0xC3, 0xE7, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x3C, 0x66, 0x42, 0x42, 0x66, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xC3, 0x99, 0xBD, 0xBD, 0x99, 0xC3, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x07, 0x03, 0x05, 0x08, 0x3E, 0x41, 0x41, 0x41, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x3E, 0x41, 0x41, 0x41, 0x3E, 0x08, 0x7F, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x18, 0x14, 0x12, 0x12, 0x10, 0x10, 0x30, 0x70, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x1F, 0x11, 0x1F, 0x11, 0x11, 0x11, 0x13, 0x37, 0x72, 0x20, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x08, 0x49, 0x2A, 0x14, 0x63, 0x14, 0x2A, 0x49, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x40, 0x60, 0x70, 0x7C, 0x7F, 0x7C, 0x70, 0x60, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x03, 0x07, 0x1F, 0x7F, 0x1F, 0x07, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x10, 0x38, 0x7C, 0x10, 0x10, 0x10, 0x7C, 0x38, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x00, 0x24, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x3F, 0x7A, 0x7A, 0x7A, 0x3A, 0x0A, 0x0A, 0x0A, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x0C, 0x12, 0x10, 0x0C, 0x12, 0x21, 0x21, 0x12, 0x0C, 0x02, 0x12, 0x0C, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7F, 0x7F, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x10, 0x38, 0x7C, 0x10, 0x10, 0x10, 0x7C, 0x38, 0x10, 0x7C, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x10, 0x38, 0x7C, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x7C, 0x38, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x04, 0x06, 0x7F, 0x06, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x10, 0x30, 0x7F, 0x30, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x40, 0x40, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x14, 0x36, 0x7F, 0x36, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x08, 0x1C, 0x1C, 0x3E, 0x3E, 0x7F, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x7F, 0x7F, 0x3E, 0x3E, 0x1C, 0x1C, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x12, 0x12, 0x3F, 0x12, 0x12, 0x3F, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x08, 0x08, 0x1C, 0x22, 0x20, 0x1C, 0x02, 0x22, 0x1C, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x30, 0x48, 0x32, 0x04, 0x08, 0x10, 0x26, 0x09, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x18, 0x24, 0x24, 0x18, 0x18, 0x25, 0x42, 0x24, 0x1A, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x0C, 0x04, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x02, 0x04, 0x08, 0x10, 0x10, 0x10, 0x10, 0x10, 0x08, 0x04, 0x02, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x10, 0x08, 0x04, 0x02, 0x02, 0x02, 0x02, 0x02, 0x04, 0x08, 0x10, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x12, 0x0C, 0x33, 0x0C, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x3E, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x04, 0x08, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x3E, 0x41, 0x43, 0x45, 0x49, 0x51, 0x61, 0x41, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x08, 0x18, 0x28, 0x08, 0x08, 0x08, 0x08, 0x08, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x1E, 0x21, 0x01, 0x01, 0x0E, 0x10, 0x20, 0x20, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x1E, 0x21, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x21, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x20, 0x22, 0x22, 0x22, 0x22, 0x3F, 0x02, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x3E, 0x20, 0x20, 0x3C, 0x22, 0x01, 0x01, 0x22, 0x1C, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x1E, 0x20, 0x20, 0x20, 0x3E, 0x21, 0x21, 0x21, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x3F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x1E, 0x21, 0x21, 0x21, 0x1E, 0x21, 0x21, 0x21, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x1E, 0x21, 0x21, 0x21, 0x1F, 0x01, 0x01, 0x01, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x04, 0x08, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x02, 0x04, 0x08, 0x10, 0x20, 0x10, 0x08, 0x04, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x20, 0x10, 0x08, 0x04, 0x02, 0x04, 0x08, 0x10, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x1E, 0x21, 0x21, 0x02, 0x04, 0x08, 0x00, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x1E, 0x21, 0x27, 0x29, 0x29, 0x29, 0x27, 0x20, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x0C, 0x12, 0x21, 0x21, 0x21, 0x3F, 0x21, 0x21, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x3E, 0x21, 0x21, 0x21, 0x3E, 0x21, 0x21, 0x21, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x1E, 0x21, 0x20, 0x20, 0x20, 0x20, 0x20, 0x21, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x3C, 0x22, 0x21, 0x21, 0x21, 0x21, 0x21, 0x22, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x3F, 0x20, 0x20, 0x20, 0x3E, 0x20, 0x20, 0x20, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x3F, 0x20, 0x20, 0x20, 0x3E, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x1E, 0x21, 0x20, 0x20, 0x27, 0x21, 0x21, 0x21, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x21, 0x21, 0x21, 0x21, 0x3F, 0x21, 0x21, 0x21, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x3E, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x21, 0x21, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x21, 0x22, 0x24, 0x28, 0x30, 0x28, 0x24, 0x22, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x21, 0x33, 0x2D, 0x21, 0x21, 0x21, 0x21, 0x21, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x21, 0x21, 0x31, 0x29, 0x25, 0x23, 0x21, 0x21, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x1E, 0x21, 0x21, 0x21, 0x21, 0x21, 0x21, 0x21, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x3E, 0x21, 0x21, 0x21, 0x3E, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x1E, 0x21, 0x21, 0x21, 0x21, 0x21, 0x25, 0x25, 0x1E, 0x03, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x3E, 0x21, 0x21, 0x21, 0x3E, 0x21, 0x21, 0x21, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x1E, 0x21, 0x20, 0x20, 0x1E, 0x01, 0x01, 0x21, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x7F, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x21, 0x21, 0x21, 0x21, 0x21, 0x21, 0x21, 0x21, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x22, 0x14, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x41, 0x41, 0x41, 0x41, 0x49, 0x49, 0x55, 0x63, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x21, 0x21, 0x21, 0x12, 0x0C, 0x12, 0x21, 0x21, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x41, 0x41, 0x41, 0x22, 0x14, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x7F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x1E, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1E, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x1E, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x1E, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x08, 0x14, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x18, 0x10, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x1E, 0x01, 0x1F, 0x21, 0x23, 0x1D, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x20, 0x20, 0x20, 0x2E, 0x31, 0x21, 0x21, 0x31, 0x2E, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x1E, 0x21, 0x20, 0x20, 0x21, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x01, 0x01, 0x1D, 0x23, 0x21, 0x21, 0x23, 0x1D, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x1E, 0x21, 0x3F, 0x20, 0x21, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x06, 0x09, 0x08, 0x3E, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x1D, 0x23, 0x21, 0x21, 0x23, 0x1D, 0x01, 0x1E, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x20, 0x20, 0x20, 0x2E, 0x31, 0x21, 0x21, 0x21, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x08, 0x08, 0x00, 0x18, 0x08, 0x08, 0x08, 0x08, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x02, 0x02, 0x00, 0x06, 0x02, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x20, 0x20, 0x20, 0x22, 0x24, 0x28, 0x34, 0x22, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x18, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x7E, 0x49, 0x49, 0x49, 0x49, 0x49, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x2E, 0x31, 0x21, 0x21, 0x21, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x1E, 0x21, 0x21, 0x21, 0x21, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x2E, 0x31, 0x21, 0x21, 0x31, 0x2E, 0x20, 0x20, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x1D, 0x23, 0x21, 0x21, 0x23, 0x1D, 0x01, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x2C, 0x32, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x1E, 0x21, 0x18, 0x06, 0x21, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x08, 0x08, 0x08, 0x3E, 0x08, 0x08, 0x08, 0x09, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x21, 0x21, 0x21, 0x21, 0x23, 0x1D, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x41, 0x41, 0x41, 0x22, 0x14, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x41, 0x49, 0x49, 0x55, 0x22, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x21, 0x12, 0x0C, 0x0C, 0x12, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x21, 0x21, 0x21, 0x21, 0x23, 0x1D, 0x01, 0x1E, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x02, 0x04, 0x08, 0x10, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x06, 0x08, 0x08, 0x08, 0x08, 0x30, 0x08, 0x08, 0x08, 0x08, 0x06, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x30, 0x08, 0x08, 0x08, 0x08, 0x06, 0x08, 0x08, 0x08, 0x08, 0x30, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x32, 0x4C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x14, 0x14, 0x22, 0x22, 0x41, 0x41, 0x7F, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x0E, 0x11, 0x20, 0x20, 0x20, 0x20, 0x20, 0x11, 0x0E, 0x04, 0x0C, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x12, 0x12, 0x00, 0x21, 0x21, 0x21, 0x21, 0x23, 0x1D, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x02, 0x04, 0x00, 0x1E, 0x21, 0x3F, 0x20, 0x21, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x0C, 0x12, 0x00, 0x1E, 0x01, 0x1F, 0x21, 0x23, 0x1D, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x12, 0x12, 0x00, 0x1E, 0x01, 0x1F, 0x21, 0x23, 0x1D, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x10, 0x08, 0x00, 0x1E, 0x01, 0x1F, 0x21, 0x23, 0x1D, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x0C, 0x12, 0x0C, 0x1E, 0x01, 0x1F, 0x21, 0x23, 0x1D, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x11, 0x20, 0x20, 0x11, 0x0E, 0x04, 0x0C, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x0C, 0x12, 0x00, 0x1E, 0x21, 0x3F, 0x20, 0x21, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x12, 0x12, 0x00, 0x1E, 0x21, 0x3F, 0x20, 0x21, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x10, 0x08, 0x00, 0x1E, 0x21, 0x3F, 0x20, 0x21, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x22, 0x22, 0x00, 0x18, 0x08, 0x08, 0x08, 0x08, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x18, 0x24, 0x00, 0x18, 0x08, 0x08, 0x08, 0x08, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x10, 0x08, 0x00, 0x18, 0x08, 0x08, 0x08, 0x08, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x12, 0x12, 0x00, 0x0C, 0x12, 0x21, 0x21, 0x3F, 0x21, 0x21, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0C, 0x12, 0x0C, 0x00, 0x0C, 0x12, 0x21, 0x3F, 0x21, 0x21, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x02, 0x04, 0x00, 0x3F, 0x20, 0x20, 0x3E, 0x20, 0x20, 0x20, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3E, 0x05, 0x1F, 0x24, 0x24, 0x1B, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x1F, 0x24, 0x24, 0x24, 0x3F, 0x24, 0x24, 0x24, 0x27, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x0C, 0x12, 0x00, 0x1E, 0x21, 0x21, 0x21, 0x21, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x12, 0x12, 0x00, 0x1E, 0x21, 0x21, 0x21, 0x21, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x10, 0x08, 0x00, 0x1E, 0x21, 0x21, 0x21, 0x21, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x0C, 0x12, 0x00, 0x21, 0x21, 0x21, 0x21, 0x23, 0x1D, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x10, 0x08, 0x00, 0x21, 0x21, 0x21, 0x21, 0x23, 0x1D, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x12, 0x12, 0x00, 0x21, 0x21, 0x21, 0x21, 0x23, 0x1D, 0x01, 0x3E, 0x00, 0x00, 0x00,
    0x12, 0x12, 0x00, 0x1E, 0x21, 0x21, 0x21, 0x21, 0x21, 0x21, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x12, 0x12, 0x00, 0x21, 0x21, 0x21, 0x21, 0x21, 0x21, 0x21, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x04, 0x04, 0x0E, 0x11, 0x20, 0x20, 0x11, 0x0E, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x0E, 0x10, 0x10, 0x38, 0x10, 0x38, 0x10, 0x11, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x41, 0x22, 0x14, 0x14, 0x7F, 0x08, 0x7F, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x78, 0x44, 0x44, 0x78, 0x44, 0x5F, 0x44, 0x44, 0x45, 0x42, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x07, 0x08, 0x08, 0x08, 0x3E, 0x08, 0x08, 0x08, 0x48, 0x30, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x02, 0x04, 0x00, 0x1E, 0x01, 0x1F, 0x21, 0x23, 0x1D, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x04, 0x08, 0x00, 0x18, 0x08, 0x08, 0x08, 0x08, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x02, 0x04, 0x00, 0x1E, 0x21, 0x21, 0x21, 0x21, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x02, 0x04, 0x00, 0x21, 0x21, 0x21, 0x21, 0x23, 0x1D, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x19, 0x26, 0x00, 0x2E, 0x31, 0x21, 0x21, 0x21, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x19, 0x26, 0x00, 0x21, 0x21, 0x31, 0x29, 0x25, 0x23, 0x21, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x1E, 0x01, 0x1F, 0x21, 0x23, 0x1D, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x1E, 0x21, 0x21, 0x21, 0x21, 0x1E, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x04, 0x04, 0x00, 0x04, 0x08, 0x10, 0x21, 0x21, 0x1E, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x7F, 0x40, 0x40, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x7F, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x04, 0x0C, 0x04, 0x04, 0x04, 0x00, 0x3F, 0x00, 0x0C, 0x12, 0x04, 0x08, 0x1E, 0x00, 0x00, 0x00,
    0x08, 0x18, 0x08, 0x08, 0x08, 0x00, 0x7F, 0x00, 0x0C, 0x14, 0x24, 0x7E, 0x04, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x08, 0x08, 0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x12, 0x24, 0x48, 0x24, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x48, 0x24, 0x12, 0x24, 0x48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00,
    0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55,
    0x44, 0x22, 0x44, 0x22, 0x44, 0x22, 0x44, 0x22, 0x44, 0x22, 0x44, 0x22, 0x44, 0x22, 0x44, 0x22,
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0xF0, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0xF0, 0x10, 0x10, 0xF0, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0xE4, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFC, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x10, 0x10, 0xF0, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0xE4, 0x04, 0x04, 0xE4, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
    0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFC, 0x04, 0x04, 0xE4, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
    0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0xE4, 0x04, 0x04, 0xFC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0xFC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0xF0, 0x10, 0x10, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0xFF, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F, 0x10, 0x10, 0x1F, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x27, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
    0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x27, 0x20, 0x20, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x20, 0x20, 0x27, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
    0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0xE7, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xE7, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
    0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x27, 0x20, 0x20, 0x27, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0xE7, 0x00, 0x00, 0xE7, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
    0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F, 0x10, 0x10, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x10, 0x10, 0x1F, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
    0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0xFF, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0xFF, 0x00, 0x00, 0xFF, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
    0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x19, 0x25, 0x42, 0x42, 0x42, 0x25, 0x19, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x1E, 0x21, 0x21, 0x22, 0x24, 0x22, 0x21, 0x21, 0x2E, 0x20, 0x20, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x7F, 0x41, 0x41, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0x54, 0x14, 0x14, 0x14, 0x14, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x7F, 0x21, 0x10, 0x08, 0x04, 0x08, 0x10, 0x21, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x1F, 0x24, 0x42, 0x42, 0x42, 0x24, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x3D, 0x20, 0x40, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0x48, 0x08, 0x08, 0x08, 0x08, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x3E, 0x08, 0x1C, 0x2A, 0x49, 0x2A, 0x1C, 0x08, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x1C, 0x22, 0x41, 0x41, 0x7F, 0x41, 0x41, 0x22, 0x1C, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x1C, 0x22, 0x41, 0x41, 0x41, 0x41, 0x22, 0x14, 0x77, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x1C, 0x20, 0x20, 0x10, 0x38, 0x44, 0x42, 0x42, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x49, 0x49, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x02, 0x3E, 0x45, 0x49, 0x51, 0x3E, 0x20, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x1F, 0x20, 0x40, 0x40, 0x7F, 0x40, 0x40, 0x20, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x1C, 0x22, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x7F, 0x00, 0x00, 0x7F, 0x00, 0x00, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x08, 0x08, 0x3E, 0x08, 0x08, 0x00, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x20, 0x10, 0x08, 0x04, 0x08, 0x10, 0x20, 0x00, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x04, 0x08, 0x10, 0x20, 0x10, 0x08, 0x04, 0x00, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x0E, 0x1B, 0x1B, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00,
    0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0xD8, 0xD8, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x7E, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x10, 0x2A, 0x04, 0x00, 0x10, 0x2A, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x1C, 0x22, 0x22, 0x1C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x0F, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x48, 0x28, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x40, 0x2E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x01, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x3C, 0x02, 0x02, 0x1C, 0x20, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x3E, 0x3E, 0x3E, 0x3E, 0x3E, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

struct vcell {
    char v_value;
    int v_dirty;
};

static size_t fb_width = 0;
static size_t fb_height = 0;
static size_t fb_depth = 0;
static size_t fb_length = 0;
static volatile uint8_t *fb_8 = NULL;
static volatile uint16_t *fb_16 = NULL;
static volatile uint32_t *fb_32 = NULL;

static size_t scr_width = 0;
static size_t scr_height = 0;
static size_t scr_length = 0;
static struct vcell *scr_cells = NULL;

static size_t cur_pxpos = 0;
static size_t cur_pypos = 0;
static size_t cur_xpos = 0;
static size_t cur_ypos = 0;

static __always_inline inline void putpixel(size_t offset, int white)
{
    switch(fb_depth) {
        case 8:
            fb_8[offset] = white ? UINT8_C(0xFF) : UINT8_C(0x00);
            return;
        case 16:
            fb_16[offset] = white ? UINT16_C(0xFFFF) : UINT16_C(0x0000);
            return;
        case 32:
            fb_32[offset] = white ? UINT32_C(0xFFFFFFFF) : UINT32_C(0x00000000);
            return;
    }
}

static void draw_cell(size_t sx, size_t sy, int value)
{
    size_t i;
    size_t gx, gy;
    size_t pixel = 0;
    const unsigned char *gptr;

    i = sy * FONT_HEIGHT;
    if(i > fb_height)
        return;
    pixel += i * fb_width;

    i = sx * FONT_WIDTH;
    if(i > fb_width)
        return;
    pixel += i;

    if(value < 0x00 || value > FONT_MAX_CP)
        value = 0x00;
    gptr = &font[value * FONT_STRIDE];

    for(gy = 0; gy < FONT_HEIGHT; ++gy) {
        for(gx = 0; gx < FONT_WIDTH; ++gx)
            putpixel(pixel + gx, (gptr[gx >> 3] & (0x80 >> (gx & 7))));
        pixel += fb_width;
        gptr += FONT_SCANLINE;
    }
}

static void draw_cursor(size_t sx, size_t sy)
{
    size_t i;
    size_t gx, gy;
    size_t pixel = 0;

    i = sy * FONT_HEIGHT;
    if(i > fb_height)
        return;
    pixel += i * fb_width;

    i = sx * FONT_WIDTH;
    if(i > fb_width)
        return;
    pixel += i;

    for(gy = 0; gy < FONT_HEIGHT; ++gy) {
        for(gx = 0; gx < FONT_WIDTH; ++gx)
            putpixel(pixel + gx, 1);
        pixel += fb_width;
    }
}

static void newline(void)
{
    size_t i;
    size_t subscreen;
    struct vcell *cell;

    if(++cur_ypos >= scr_height) {
        cur_ypos = scr_height - 1;
        subscreen = scr_length - scr_width;

        for(i = 0; i < subscreen; ++i) {
            cell = &scr_cells[i];
            cell[0] = cell[scr_width];
            cell[0].v_dirty = 1;
        }

        cell = &scr_cells[subscreen];
        for(i = 0; i < scr_width; ++i) {
            cell[i].v_dirty = 1;
            cell[i].v_value = CHR_WS;
        }
    }

}

static void redraw(void)
{
    size_t i, j;
    struct vcell *cell;

    for(i = 0; i < scr_width; ++i) {
        for(j = 0; j < scr_height; ++j) {
            cell = &scr_cells[j * scr_width + i];

            if(cell->v_dirty) {
                draw_cell(i, j, cell->v_value);
                cell->v_dirty = 0;
            }
        }
    }

    cell = &scr_cells[cur_pypos * scr_width + cur_pxpos];
    draw_cell(cur_pxpos, cur_pypos, cell->v_value);

    draw_cursor(cur_xpos, cur_ypos);

    cur_pxpos = cur_xpos;
    cur_pypos = cur_ypos;
}

static void clear(void)
{
    size_t i;
    for(i = 0; i < fb_length; putpixel(i++, 0));
    memset(scr_cells, 0, scr_length * sizeof(struct vcell));
}

static void fbcon_putchar(int ch)
{
    struct vcell *cell;

    switch(ch) {
        case CHR_BS:
        case CHR_DEL:
            if(cur_xpos > 0)
                --cur_xpos;
            redraw();
            return;
        case CHR_HT:
            cur_xpos += 4 - (cur_xpos % 4);
            if(cur_xpos >= scr_width)
                cur_xpos = scr_width - 1;
            redraw();
            return;
        case CHR_LF:
            cur_xpos = 0;
            newline();
            redraw();
            return;
        case CHR_VT:
            newline();
            redraw();
            return;
        case CHR_FF:
            clear();
            cur_pxpos = 0;
            cur_pypos = 0;
            cur_xpos = 0;
            cur_ypos = 0;
            redraw();
            return;
        case CHR_CR:
            cur_xpos = 0;
            redraw();
            return;
    }

    cell = &scr_cells[cur_ypos * scr_width + cur_xpos];
    cell->v_dirty = 1;
    cell->v_value = ch;

    if(++cur_xpos >= scr_width) {
        cur_xpos = 0;
        newline();
    }

    redraw();
}

static void fbcon_write(struct console *restrict con, const void *restrict buf, size_t sz)
{
    size_t i;
    const char *cbuf = buf;
    for(i = 0; i < sz; fbcon_putchar(cbuf[i++]));
}

static void fbcon_unblank(struct console *restrict con)
{
#if LATER
    clear();
    cur_pxpos = 0;
    cur_pypos = 0;
    cur_xpos = 0;
    cur_ypos = 0;
    redraw();
#endif
}

static struct console fbcon = {
    .cs_next = NULL,
    .cs_write = &fbcon_write,
    .cs_unblank = &fbcon_unblank,
    .cs_identity = "fbcon",
    .cs_flags = CS_PRINTBUFFER,
    .cs_private = NULL,
};

void init_fbcon(void)
{
    size_t i;
    struct limine_framebuffer *fb;

    if(framebuffer_request.response) {
        for(i = 0; i < framebuffer_request.response->framebuffer_count; ++i) {
            fb = framebuffer_request.response->framebuffers[i];

            if(fb->bpp == 8 || fb->bpp == 16 || fb->bpp == 32) {
                fb_width = fb->width;
                fb_height = fb->height;
                fb_depth = fb->bpp;
                fb_length = fb_width * fb_height;
                fb_8 = fb->address;
                fb_16 = fb->address;
                fb_32 = fb->address;

                scr_width = fb_width / FONT_WIDTH;
                scr_height = fb_height / FONT_HEIGHT;
                scr_length = scr_width * scr_height;

                if((scr_cells = dma_alloc_hhdm(page_count(scr_length * sizeof(struct vcell)))) != NULL) {
                    cur_pxpos = 0;
                    cur_pypos = 0;
                    cur_xpos = 0;
                    cur_ypos = 0;

                    clear();

                    register_console(&fbcon);

                    kprintf(KP_DEBUG, "fbcon: font: size=%zu, %dx%d, [%zux%zu]", sizeof(font), FONT_WIDTH, FONT_HEIGHT, scr_width, scr_height);
                    kprintf(KP_DEBUG, "fbcon: framebuffer: %zux%zux%zu", fb_width, fb_height, fb_depth);

                    return;
                }

                // FIXME: just bail out instead
                panic("fbcon: out of memory");
                unreachable();
            }
        }
    }
}
