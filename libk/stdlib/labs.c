/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2024, VX/sys Contributors */
#include <limits.h>
#include <stdlib.h>

long labs(long j)
{
    return (j >= 0) ? j : -j;
}