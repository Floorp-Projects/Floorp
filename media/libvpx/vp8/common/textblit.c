/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdlib.h>


void vp8_blit_text(const char *msg, unsigned char *address, const int pitch)
{
    int letter_bitmap;
    unsigned char *output_pos = address;
    int colpos;
    const int font[] =
    {
        0x0, 0x5C00, 0x8020, 0xAFABEA, 0xD7EC0, 0x1111111, 0x1855740, 0x18000,
        0x45C0, 0x74400, 0x51140, 0x23880, 0xC4000, 0x21080, 0x80000, 0x111110,
        0xE9D72E, 0x87E40, 0x12AD732, 0xAAD62A, 0x4F94C4, 0x4D6B7, 0x456AA,
        0x3E8423, 0xAAD6AA, 0xAAD6A2, 0x2800, 0x2A00, 0x8A880, 0x52940, 0x22A20,
        0x15422, 0x6AD62E, 0x1E4A53E, 0xAAD6BF, 0x8C62E, 0xE8C63F, 0x118D6BF,
        0x1094BF, 0xCAC62E, 0x1F2109F, 0x118FE31, 0xF8C628, 0x8A89F, 0x108421F,
        0x1F1105F, 0x1F4105F, 0xE8C62E, 0x2294BF, 0x164C62E, 0x12694BF, 0x8AD6A2,
        0x10FC21, 0x1F8421F, 0x744107, 0xF8220F, 0x1151151, 0x117041, 0x119D731,
        0x47E0, 0x1041041, 0xFC400, 0x10440, 0x1084210, 0x820
    };
    colpos = 0;

    while (msg[colpos] != 0)
    {
        char letter = msg[colpos];
        int fontcol, fontrow;

        if (letter <= 'Z' && letter >= ' ')
            letter_bitmap = font[letter-' '];
        else if (letter <= 'z' && letter >= 'a')
            letter_bitmap = font[letter-'a'+'A' - ' '];
        else
            letter_bitmap = font[0];

        for (fontcol = 6; fontcol >= 0 ; fontcol--)
            for (fontrow = 0; fontrow < 5; fontrow++)
                output_pos[fontrow *pitch + fontcol] =
                    ((letter_bitmap >> (fontcol * 5)) & (1 << fontrow) ? 255 : 0);

        output_pos += 7;
        colpos++;
    }
}

static void plot (const int x, const int y, unsigned char *image, const int pitch)
{
    image [x+y*pitch] ^= 255;
}

/* Bresenham line algorithm */
void vp8_blit_line(int x0, int x1, int y0, int y1, unsigned char *image, const int pitch)
{
    int steep = abs(y1 - y0) > abs(x1 - x0);
    int deltax, deltay;
    int error, ystep, y, x;

    if (steep)
    {
        int t;
        t = x0;
        x0 = y0;
        y0 = t;

        t = x1;
        x1 = y1;
        y1 = t;
    }

    if (x0 > x1)
    {
        int t;
        t = x0;
        x0 = x1;
        x1 = t;

        t = y0;
        y0 = y1;
        y1 = t;
    }

    deltax = x1 - x0;
    deltay = abs(y1 - y0);
    error  = deltax / 2;

    y = y0;

    if (y0 < y1)
        ystep = 1;
    else
        ystep = -1;

    if (steep)
    {
        for (x = x0; x <= x1; x++)
        {
            plot(y,x, image, pitch);

            error = error - deltay;
            if (error < 0)
            {
                y = y + ystep;
                error = error + deltax;
            }
        }
    }
    else
    {
        for (x = x0; x <= x1; x++)
        {
            plot(x,y, image, pitch);

            error = error - deltay;
            if (error < 0)
            {
                y = y + ystep;
                error = error + deltax;
            }
        }
    }
}
