/* SPDX-License-Identifier: GPL-2.0-only */
#include <sys/boot.h>
#include <sys/page.h>
#include <sys/panic.h>
#include <sys/pmm.h>
#include <sys/printf.h>

#if !defined(DMA_APPROX_END)
#define DMA_APPROX_END 0x3FFFFFF
#endif

static void **page_list = NULL;
static uintptr_t dma_end_addr = 0;
static uint64_t *dma_bitmap = NULL;
static size_t dma_numpages = 0;
static size_t dma_lastpage = 0;

static void bitmap_mark_free(size_t a, size_t b)
{
    size_t i;
    size_t ax = ALIGN_CEIL(a, 64);
    size_t bx = ALIGN_FLOOR(b, 64);
    size_t axi = ax / 64;
    size_t bxi = bx / 64;
    for(i = a; i < ax; dma_bitmap[axi - 1] |= (UINT64_C(1) << (i++ % 64)));
    if(bxi >= axi)
        for(i = bx + 1; i <= b; dma_bitmap[bxi] |= (UINT64_C(1) << (i++ % 64)));
    for(i = axi; i < bxi; dma_bitmap[i++] = UINT64_C(0xFFFFFFFFFFFFFFFF));
}

static void bitmap_mark_used(size_t a, size_t b)
{
    size_t i;
    size_t ax = ALIGN_CEIL(a, 64);
    size_t bx = ALIGN_FLOOR(b, 64);
    size_t axi = ax / 64;
    size_t bxi = bx / 64;
    for(i = a; i < ax; dma_bitmap[axi - 1] &= ~(UINT64_C(1) << (i++ % 64)));
    if(bxi >= axi)
        for(i = bx + 1; i <= b; dma_bitmap[bxi] &= ~(UINT64_C(1) << (i++ % 64)));
    for(i = axi; i < bxi; dma_bitmap[i++] = UINT64_C(0x0000000000000000));
}

static int bitmap_try_mark_used(size_t a, size_t b)
{
    size_t i;
    size_t chunk;
    uint64_t mask;

    for(i = a; i <= b; ++i) {
        mask = (UINT64_C(1) << (i % 64));
        chunk = (i / 64);

        if(dma_bitmap[chunk] & mask) {
            dma_bitmap[chunk] &= ~mask;
            continue;
        }

        if(i > a)
            bitmap_mark_free(a, i - 1);
        return 0;
    }

    return 1;
}

uintptr_t pmm_alloc(size_t npages)
{
    size_t page;
    size_t limit;
    uintptr_t address;

    if(npages == 1 && page_list) {
        address = ((uintptr_t)(page_list) - hhdm_offset);
        page_list = page_list[0];
        return address;
    }

    limit = dma_numpages - npages;
    for(page = dma_lastpage; page < limit; ++page) {
        if(bitmap_try_mark_used(page, page + npages - 1)) {
            dma_lastpage = page + npages;
            return page * PAGE_SIZE;
        }
    }

    limit = dma_lastpage - npages;
    for(page = 0; page < limit; ++page) {
        if(bitmap_try_mark_used(page, page + npages - 1)) {
            dma_lastpage = page + npages;
            return page * PAGE_SIZE;
        }
    }

    return 0;
}

void *pmm_alloc_hhdm(size_t npages)
{
    uintptr_t address;
    if((address = pmm_alloc(npages)) != 0)
        return (void *)(address + hhdm_offset);
    return 0;
}

void pmm_free(uintptr_t address, size_t npages)
{
    size_t page;
    void **head_ptr;

    if(address != 0) {
        if(npages == 1 && address >= dma_end_addr) {
            head_ptr = (void **)(address + hhdm_offset);
            head_ptr[0] = page_list;
            page_list = head_ptr;
            return;
        }

        page = address / PAGE_SIZE;
        bitmap_mark_free(page, page + npages - 1);
        dma_lastpage = page;
    }
}

void pmm_free_hhdm(void *restrict ptr, size_t npages)
{
    if(!ptr)
        return;
    pmm_free(((uintptr_t)(ptr) - hhdm_offset), npages);
}

static void init_pmm(void)
{
    size_t i;
    size_t page;
    size_t npages;
    size_t bitmap_size;
    uintptr_t bitmap_base;
    uintptr_t end_addr, off;
    void **head_ptr;
    struct limine_memmap_entry *entry;

    for(i = 0; i < memmap_request.response->entry_count; ++i) {
        entry = memmap_request.response->entries[i];

        if(entry->type == LIMINE_MEMMAP_USABLE && entry->base <= DMA_APPROX_END) {
            end_addr = entry->base + entry->length - 1;
            if(dma_end_addr < end_addr)
                dma_end_addr = end_addr;
            break;
        }
    }

    npages = page_count(dma_end_addr + 1);
    dma_numpages = ALIGN_CEIL(npages, 64);
    bitmap_size = dma_numpages / 8;
    npages = page_count(bitmap_size);

    for(i = 0; i < memmap_request.response->entry_count; ++i) {
        entry = memmap_request.response->entries[i];

        if(entry->type == LIMINE_MEMMAP_USABLE && entry->length >= npages) {
            bitmap_base = entry->base;
            dma_bitmap = (uint64_t *)(bitmap_base + hhdm_offset);
            break;
        }
    }

    if(dma_bitmap != NULL) {
        bitmap_mark_used(0, dma_numpages - 1);

        for(i = 0; i < memmap_request.response->entry_count; ++i) {
            entry = memmap_request.response->entries[i];

            if(entry->type == LIMINE_MEMMAP_USABLE && entry->base <= DMA_APPROX_END && entry->length != 0) {
                npages = page_count(entry->length);
                page = entry->base / PAGE_SIZE;
                bitmap_mark_free(page, page + npages - 1);
                break;
            }
        }

        if(bitmap_base <= dma_end_addr) {
            page = bitmap_base / PAGE_SIZE;
            npages = page_count(bitmap_size);
            bitmap_mark_used(page, page + npages - 1);
        }

        for(i = 0; i < memmap_request.response->entry_count; ++i) {
            entry = memmap_request.response->entries[i];

            if(entry->type == LIMINE_MEMMAP_USABLE && entry->base > dma_end_addr) {
                for(off = 0; off < entry->length; off += PAGE_SIZE) {
                    head_ptr = (void **)(entry->base + off + hhdm_offset);
                    head_ptr[0] = page_list;
                    page_list = head_ptr;
                }
            }
        }

        kprintf(KP_DEBUG, "pmm: dma_end_addr=%p, dma_numpages=%zu", (void *)(dma_end_addr), dma_numpages);
        kprintf(KP_DEBUG, "pmm: dma_bitmap=%p", dma_bitmap);

        return;
    }

    panic("pmm: out of memory");
    UNREACHABLE();
}
core_initcall(pmm, init_pmm);
