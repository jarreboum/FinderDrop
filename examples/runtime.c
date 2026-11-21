/*
 * runtime.c — what GCC assumes the target provides, for the two examples
 * that are code resources (the desk accessory and the control panel),
 * where there is no C library to take it from.
 *
 * SPDX-License-Identifier: 0BSD
 * Copyright (C) 2026 by the FinderDrop authors (LICENSE).
 *
 * The 68000 has no 32-bit multiply or divide, so GCC calls __mulsi3,
 * __udivsi3 and __umodsi3, and a struct copy is a call to memcpy whatever
 * the source says.  libgcc's are not compiled -mpcrel and would put
 * absolute addresses in a resource that is never relocated; these are,
 * and build.sh's two-address link proves nothing else slipped in.
 * Byte-wise, because the 68000 faults on a misaligned word.  If signed
 * division appears in the link, find the code that asked for it rather
 * than adding the symbol.
 */
#include <stddef.h>
#include <stdint.h>

uint32_t __udivsi3(uint32_t num, uint32_t den);
uint32_t __umodsi3(uint32_t num, uint32_t den);
uint32_t __mulsi3(uint32_t a, uint32_t b);
void *memcpy(void *dst, const void *src, size_t n);
void *memset(void *dst, int c, size_t n);
int memcmp(const void *a, const void *b, size_t n);

uint32_t __udivsi3(uint32_t num, uint32_t den)
{
    uint32_t quot = 0, rem = 0;
    int i;
    if (den == 0)
        return 0xFFFFFFFFuL;
    for (i = 31; i >= 0; i--) {
        rem = (rem << 1) | ((num >> i) & 1u);
        if (rem >= den) {
            rem -= den;
            quot |= (uint32_t)1u << i;
        }
    }
    return quot;
}

uint32_t __umodsi3(uint32_t num, uint32_t den)
{
    uint32_t rem = 0;
    int i;
    if (den == 0)
        return num;
    for (i = 31; i >= 0; i--) {
        rem = (rem << 1) | ((num >> i) & 1u);
        if (rem >= den)
            rem -= den;
    }
    return rem;
}

uint32_t __mulsi3(uint32_t a, uint32_t b)
{
    uint32_t acc = 0;
    while (b != 0) {
        if (b & 1u)
            acc += a;
        a <<= 1;
        b >>= 1;
    }
    return acc;
}

void *memcpy(void *dst, const void *src, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    size_t i;
    for (i = 0; i < n; i++)
        d[i] = s[i];
    return dst;
}

void *memset(void *dst, int c, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    size_t i;
    for (i = 0; i < n; i++)
        d[i] = (uint8_t)c;
    return dst;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const uint8_t *p = (const uint8_t *)a, *q = (const uint8_t *)b;
    size_t i;
    for (i = 0; i < n; i++)
        if (p[i] != q[i])
            return p[i] < q[i] ? -1 : 1;
    return 0;
}
