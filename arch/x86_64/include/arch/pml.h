/* SPDX-License-Identifier: Zlib */
#ifndef ARCH_X86_64_INCLUDE_ARCH_PAGING_H
#define ARCH_X86_64_INCLUDE_ARCH_PAGING_H
#include <iserix/compiler.h>
#include <iserix/vprot.h>
#include <stdint.h>

#define X86_PML_ADDRESS 0x000FFFFFFFFFF000
#define X86_PML_PRESENT 0x0000000000000001
#define X86_PML_WRITE   0x0000000000000002
#define X86_PML_USER    0x0000000000000004
#define X86_PML_NOEXEC  0x8000000000000000

#define PMENTRY_LVL1_MASK UINT64_C(0x1FF)
#define PMENTRY_LVL2_MASK UINT64_C(0x1FF)
#define PMENTRY_LVL3_MASK UINT64_C(0x1FF)
#define PMENTRY_LVL4_MASK UINT64_C(0x1FF)
#define PMENTRY_LVL5_MASK UINT64_C(0x1FF)

#define PMENTRY_LVL1_SHIFT UINT64_C(12)
#define PMENTRY_LVL2_SHIFT UINT64_C(21)
#define PMENTRY_LVL3_SHIFT UINT64_C(30)
#define PMENTRY_LVL4_SHIFT UINT64_C(39)
#define PMENTRY_LVL5_SHIFT UINT64_C(48)

#define PMENTRY_NULL UINT64_C(0x0000000000000000)

#define PAGEMAP_SIZE 0x200
#define PAGEMAP_KERN 0x100
#define PAGEMAP_USER 0x000

#define PAGING_MODE_LVL3 LIMINE_PAGING_MODE_X86_64_4LVL
#define PAGING_MODE_LVL4 LIMINE_PAGING_MODE_X86_64_4LVL
#define PAGING_MODE_LVL5 LIMINE_PAGING_MODE_X86_64_5LVL

#define PREDICT_LVL3(cond) PREDICT_TRUE(cond) /* Enabled in Long Mode */
#define PREDICT_LVL4(cond) PREDICT_TRUE(cond) /* Enabled in Long Mode */
#define PREDICT_LVL5(cond) (cond)

typedef uint64_t pmentry_t;

static ALWAYS_INLINE NODISCARD inline int pmentry_valid(pmentry_t entry)
{
    return (int)(entry & X86_PML_PRESENT);
}

static ALWAYS_INLINE NODISCARD inline uintptr_t pmentry_address(pmentry_t entry)
{
    return (uintptr_t)(entry & X86_PML_ADDRESS);
}

static ALWAYS_INLINE NODISCARD inline pmentry_t make_pmentry(uintptr_t address, unsigned vprot)
{
    pmentry_t entry = PMENTRY_NULL;

    entry |= X86_PML_ADDRESS & address;
    entry |= X86_PML_PRESENT;
    entry |= X86_PML_NOEXEC;

    if(vprot & VPROT_WRITE) entry |= X86_PML_WRITE;
    if(vprot & VPROT_USER)  entry |= X86_PML_USER;
    if(vprot & VPROT_EXEC)  entry &= ~X86_PML_NOEXEC;

    return entry;
}

#endif /* ARCH_X86_64_INCLUDE_ARCH_PAGING_H */