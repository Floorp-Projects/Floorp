/*
 * $XFree86: xc/lib/Xft/xftswap.c,v 1.1 2002/02/15 07:37:35 keithp Exp $
 *
 * Copyright © 2002 Keith Packard, member of The XFree86 Project, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <X11/Xlib.h>
#include "xftint.h"

int
XftNativeByteOrder (void)
{
    int	    whichbyte = 1;

    if (*((char *) &whichbyte))
	return LSBFirst;
    return MSBFirst;
}

/* byte swap a 32-bit value */
#define swapl(x, n) { \
		 n = ((char *) (x))[0];\
		 ((char *) (x))[0] = ((char *) (x))[3];\
		 ((char *) (x))[3] = n;\
		 n = ((char *) (x))[1];\
		 ((char *) (x))[1] = ((char *) (x))[2];\
		 ((char *) (x))[2] = n; }

/* byte swap a short */
#define swaps(x, n) { \
		 n = ((char *) (x))[0];\
		 ((char *) (x))[0] = ((char *) (x))[1];\
		 ((char *) (x))[1] = n; }

/* byte swap a three-byte unit */
#define swapt(x, n) { \
		 n = ((char *) (x))[0];\
		 ((char *) (x))[0] = ((char *) (x))[2];\
		 ((char *) (x))[2] = n; }

void
XftSwapCARD32 (CARD32 *data, int u)
{
    char    n;
    while (u--)
    {
	swapl (data, n);
	data++;
    }
}

void
XftSwapCARD24 (CARD8 *data, int width, int height)
{
    int	    units, u;
    char    n;
    CARD8   *d;

    units = width / 3;
    while (height--)
    {
	d = data;
	data += width;
	u = units;
	while (u--)
	{
	    swapt (d, n);
	    d += 3;
	}
    }
}

void
XftSwapCARD16 (CARD16 *data, int u)
{
    char    n;
    while (u--)
    {
	swaps (data, n);
	data++;
    }
}

void
XftSwapImage (XImage *image)
{
    switch (image->bits_per_pixel) {
    case 32:
	XftSwapCARD32 ((CARD32 *) image->data, 
		       image->height * image->bytes_per_line >> 2);
	break;
    case 24:
	XftSwapCARD24 ((CARD8 *) image->data,
		       image->bytes_per_line,
		       image->height);
	break;
    case 16:
	XftSwapCARD16 ((CARD16 *) image->data,
		       image->height * image->bytes_per_line >> 1);
	break;
    default:
	break;
    }
}
