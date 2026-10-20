/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kyle Palaniuk */
/* Gnomad (FORK.md): a PNG writer for the fork's host programs. RGB, 8 bits per channel, every row
 * with the Sub filter, compressed by a deflate of our own (LZ77 with a fixed hash and chain
 * length, fixed Huffman codes), so the same pixels give the same bytes on every machine and with
 * every C library: the approved pictures in the repository are compared byte for byte. */
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint8_t *b;
    size_t n, cap;
    uint32_t bits, nbits;          /* deflate: bits not yet written, LSB first */
} gn_out_t;

static void gn_out_byte(gn_out_t *o, uint8_t v)
{
    if (o->n == o->cap) {
        o->cap = o->cap ? o->cap * 2u : 65536u;
        o->b = realloc(o->b, o->cap);
        if (!o->b) {
            fprintf(stderr, "png: out of memory\n");
            exit(2);
        }
    }
    o->b[o->n++] = v;
}

static void gn_out_be32(gn_out_t *o, uint32_t v)
{
    gn_out_byte(o, (uint8_t)(v >> 24));
    gn_out_byte(o, (uint8_t)(v >> 16));
    gn_out_byte(o, (uint8_t)(v >> 8));
    gn_out_byte(o, (uint8_t)v);
}

static void gn_bits(gn_out_t *o, uint32_t v, uint32_t n)        /* n bits of v, LSB first */
{
    o->bits |= v << o->nbits;
    o->nbits += n;
    while (o->nbits >= 8u) {
        gn_out_byte(o, (uint8_t)o->bits);
        o->bits >>= 8;
        o->nbits -= 8u;
    }
}

static void gn_huff(gn_out_t *o, uint32_t code, uint32_t n)     /* a Huffman code: MSB first */
{
    uint32_t r = 0, i;
    for (i = 0; i < n; i++)
        r |= ((code >> i) & 1u) << (n - 1u - i);
    gn_bits(o, r, n);
}

static void gn_lit(gn_out_t *o, uint32_t v)                      /* fixed literal / length code */
{
    if (v < 144u)
        gn_huff(o, 0x30u + v, 8);
    else if (v < 256u)
        gn_huff(o, 0x190u + v - 144u, 9);
    else if (v < 280u)
        gn_huff(o, v - 256u, 7);
    else
        gn_huff(o, 0xC0u + v - 280u, 8);
}

static void gn_match(gn_out_t *o, uint32_t len, uint32_t dist)
{
    static const uint16_t LB[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
                                    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
    static const uint8_t LE[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
    static const uint16_t DB[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385,
                                    513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
    static const uint8_t DE[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10,
                                   11, 11, 12, 12, 13, 13};
    uint32_t i = 28, d = 29;
    while (LB[i] > len)
        i--;
    gn_lit(o, 257u + i);
    gn_bits(o, len - LB[i], LE[i]);
    while (DB[d] > dist)
        d--;
    gn_huff(o, d, 5);
    gn_bits(o, dist - DB[d], DE[d]);
}

#define GN_WIN 32768u
#define GN_HASH 32768u
static void gn_deflate(gn_out_t *o, const uint8_t *in, uint32_t n)   /* one final block, fixed codes */
{
    int32_t *head = malloc(GN_HASH * sizeof *head), *prev = malloc(GN_WIN * sizeof *prev);
    uint32_t i = 0, k;
    if (!head || !prev) {
        fprintf(stderr, "png: out of memory\n");
        exit(2);
    }
    for (k = 0; k < GN_HASH; k++)
        head[k] = -1;
    gn_bits(o, 1u | (1u << 1), 3);                 /* BFINAL, BTYPE 01 */
#define GN_H(p) ((((uint32_t)in[p] << 10) ^ ((uint32_t)in[(p) + 1u] << 5) ^ in[(p) + 2u]) & (GN_HASH - 1u))
    while (i < n) {
        uint32_t best = 0, bdist = 0;
        if (i + 2u < n) {
            int32_t c = head[GN_H(i)];
            uint32_t chain = 128;
            while (c >= 0 && i - (uint32_t)c <= GN_WIN && chain--) {
                uint32_t l = 0, max = n - i < 258u ? n - i : 258u;
                while (l < max && in[(uint32_t)c + l] == in[i + l])
                    l++;
                if (l > best) {
                    best = l;
                    bdist = i - (uint32_t)c;
                    if (l == max)
                        break;
                }
                c = prev[(uint32_t)c % GN_WIN];
            }
        }
        if (best >= 3u) {
            gn_match(o, best, bdist);
            for (k = 0; k < best; k++, i++)
                if (i + 2u < n) {
                    prev[i % GN_WIN] = head[GN_H(i)];
                    head[GN_H(i)] = (int32_t)i;
                }
        } else {
            gn_lit(o, in[i]);
            if (i + 2u < n) {
                prev[i % GN_WIN] = head[GN_H(i)];
                head[GN_H(i)] = (int32_t)i;
            }
            i++;
        }
    }
#undef GN_H
    gn_lit(o, 256);                                 /* end of block */
    if (o->nbits)
        gn_bits(o, 0, 8u - o->nbits);
    free(head);
    free(prev);
}

static uint32_t gn_crc(uint32_t c, const uint8_t *p, size_t n)
{
    static uint32_t t[256];
    uint32_t i, k;
    if (!t[1])
        for (i = 0; i < 256u; i++) {
            uint32_t v = i;
            for (k = 0; k < 8u; k++)
                v = v & 1u ? 0xEDB88320u ^ (v >> 1) : v >> 1;
            t[i] = v;
        }
    c = ~c;
    while (n--)
        c = t[(c ^ *p++) & 255u] ^ (c >> 8);
    return ~c;
}

static void gn_chunk(gn_out_t *png, const char *type, const uint8_t *data, size_t n)
{
    size_t start;
    gn_out_be32(png, (uint32_t)n);
    start = png->n;
    gn_out_byte(png, (uint8_t)type[0]);
    gn_out_byte(png, (uint8_t)type[1]);
    gn_out_byte(png, (uint8_t)type[2]);
    gn_out_byte(png, (uint8_t)type[3]);
    while (n--)
        gn_out_byte(png, *data++);
    gn_out_be32(png, gn_crc(0, png->b + start, png->n - start));
}

/* rgb: w x h x 3 bytes, rows top to bottom. Returns the PNG (malloc'd), its size in *size */
static uint8_t *gn_png(const uint8_t *rgb, uint32_t w, uint32_t h, size_t *size)
{
    static const uint8_t SIG[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    uint32_t stride = w * 3u + 1u, x, y, a = 1, b = 0;
    uint8_t *raw = malloc((size_t)stride * h), ihdr[13];
    gn_out_t z = {0}, png = {0};
    size_t i;
    if (!raw) {
        fprintf(stderr, "png: out of memory\n");
        exit(2);
    }
    for (y = 0; y < h; y++) {                       /* filter 1 (Sub): each byte minus the one 3 before */
        const uint8_t *s = rgb + (size_t)y * w * 3u;
        uint8_t *d = raw + (size_t)y * stride;
        d[0] = 1;
        for (x = 0; x < w * 3u; x++)
            d[1u + x] = (uint8_t)(s[x] - (x >= 3u ? s[x - 3u] : 0u));
    }
    gn_out_byte(&z, 0x78);                          /* zlib: deflate, 32 KiB window, no dictionary */
    gn_out_byte(&z, 0x01);
    gn_deflate(&z, raw, stride * h);
    for (i = 0; i < (size_t)stride * h; i++) {      /* Adler-32 of the uncompressed data */
        a = (a + raw[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    gn_out_be32(&z, b << 16 | a);
    free(raw);
    for (i = 0; i < 8u; i++)
        gn_out_byte(&png, SIG[i]);
    ihdr[0] = (uint8_t)(w >> 24);
    ihdr[1] = (uint8_t)(w >> 16);
    ihdr[2] = (uint8_t)(w >> 8);
    ihdr[3] = (uint8_t)w;
    ihdr[4] = (uint8_t)(h >> 24);
    ihdr[5] = (uint8_t)(h >> 16);
    ihdr[6] = (uint8_t)(h >> 8);
    ihdr[7] = (uint8_t)h;
    ihdr[8] = 8;                                    /* bits per channel */
    ihdr[9] = 2;                                    /* RGB */
    ihdr[10] = ihdr[11] = ihdr[12] = 0;
    gn_chunk(&png, "IHDR", ihdr, sizeof ihdr);
    gn_chunk(&png, "IDAT", z.b, z.n);
    gn_chunk(&png, "IEND", 0, 0);
    free(z.b);
    *size = png.n;
    return png.b;
}
