/*
 * $XFree86: xc/lib/Xft/xftinit.c,v 1.3 2002/02/15 07:36:11 keithp Exp $
 *
 * Copyright © 2000 Keith Packard, member of The XFree86 Project, Inc.
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

#include <stdlib.h>
#include <stdio.h>
#include "xftint.h"
#include <X11/Xlibint.h>

Bool	    _XftConfigInitialized;

Bool
XftInit (char *config)
{
    if (_XftConfigInitialized)
	return True;
    _XftConfigInitialized = True;
    if (!FcInit ())
	return False;
    _XftNameInit ();
    return True;
}

static struct {
    char    *name;
    int	    alloc_count;
    int	    alloc_mem;
    int	    free_count;
    int	    free_mem;
} XftInUse[XFT_MEM_NUM] = {
    { "XftDraw", 0, 0 },
    { "XftFont", 0 ,0 },
    { "XftFtFile", 0, 0 },
    { "XftGlyph", 0, 0 },
};

static int  XftAllocCount, XftAllocMem;
static int  XftFreeCount, XftFreeMem;

static int  XftMemNotice = 1*1024*1024;

static int  XftAllocNotify, XftFreeNotify;

void
XftMemReport (void)
{
    int	i;
    printf ("Xft Memory Usage:\n");
    printf ("\t   Which       Alloc           Free\n");
    printf ("\t           count   bytes   count   bytes\n");
    for (i = 0; i < XFT_MEM_NUM; i++)
	printf ("\t%8.8s%8d%8d%8d%8d\n",
		XftInUse[i].name,
		XftInUse[i].alloc_count, XftInUse[i].alloc_mem,
		XftInUse[i].free_count, XftInUse[i].free_mem);
    printf ("\t%8.8s%8d%8d%8d%8d\n",
	    "Total",
	    XftAllocCount, XftAllocMem,
	    XftFreeCount, XftFreeMem);
    XftAllocNotify = 0;
    XftFreeNotify = 0;
}

void
XftMemAlloc (int kind, int size)
{
    if (XftDebug() & XFT_DBG_MEMORY)
    {
	XftInUse[kind].alloc_count++;
	XftInUse[kind].alloc_mem += size;
	XftAllocCount++;
	XftAllocMem += size;
	XftAllocNotify += size;
	if (XftAllocNotify > XftMemNotice)
	    XftMemReport ();
    }
}

void
XftMemFree (int kind, int size)
{
    if (XftDebug() & XFT_DBG_MEMORY)
    {
	XftInUse[kind].free_count++;
	XftInUse[kind].free_mem += size;
	XftFreeCount++;
	XftFreeMem += size;
	XftFreeNotify += size;
	if (XftFreeNotify > XftMemNotice)
	    XftMemReport ();
    }
}
