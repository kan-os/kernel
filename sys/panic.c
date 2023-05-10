/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#include <machine/cpu.h>
#include <sys/console.h>
#include <sys/systm.h>

void __noreturn fpanic(const char *restrict file, unsigned long line, const char *restrict fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fvpanic(file, line, fmt, ap);
    UNREACHABLE();
    va_end(ap);
}

void __noreturn fvpanic(const char *restrict file, unsigned long line, const char *restrict fmt, va_list ap)
{
    cpu_disable_interrupts();

    console_unblank();

    kprintf("panic: at [%s:%lu]", file, line);
    kvprintf(fmt, ap);

    /* UNDONE: backtrace */
    /* print_backtrace(NULL); */

    for(;;) {
        cpu_idle();
        cpu_disable_interrupts();
        continue;
    }

    UNREACHABLE();
}
