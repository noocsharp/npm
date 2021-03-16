/*
 * Copyright (c) 2016 Thomas Pornin <pornin@bolet.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining 
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be 
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * Detect support for unaligned accesses with known endianness.
 *
 *  x86 (both 32-bit and 64-bit) is little-endian and allows unaligned
 *  accesses.
 *
 *  POWER/PowerPC allows unaligned accesses when big-endian. POWER8 and
 *  later also allow unaligned accesses when little-endian.
 */
#if !defined BR_LE_UNALIGNED && !defined BR_BE_UNALIGNED

#if __i386 || __i386__ || __x86_64__ || _M_IX86 || _M_X64
#define BR_LE_UNALIGNED   1
#elif BR_POWER8_BE
#define BR_BE_UNALIGNED   1
#elif BR_POWER8_LE
#define BR_LE_UNALIGNED   1
#elif (__powerpc__ || __powerpc64__ || _M_PPC || _ARCH_PPC || _ARCH_PPC64) \
	&& __BIG_ENDIAN__
#define BR_BE_UNALIGNED   1
#endif

#endif

typedef union {
	uint32_t u;
	unsigned char b[sizeof(uint32_t)];
} br_union_u32;

uint32_t br_chacha20_ct_run(const void *key, const void *iv, uint32_t cc, void *data, size_t len);
