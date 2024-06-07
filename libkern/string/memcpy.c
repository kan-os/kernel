/* SPDX-License-Identifier: GPL-2.0-only */
#include <string.h>

void *memcpy(void *restrict dst, const void *restrict src, size_t sz)
{
    unsigned char *dp = dst;
    const unsigned char *sp = src;
    while(sz--)
        *dp++ = *sp++;
    return dst;
}
