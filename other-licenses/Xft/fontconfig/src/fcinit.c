/*
 * $XFree86: xc/lib/fontconfig/src/fcinit.c,v 1.3 2002/02/19 08:33:23 keithp Exp $
 *
 * Copyright © 2001 Keith Packard, member of The XFree86 Project, Inc.
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
#include "fcint.h"

FcBool
FcInitFonts (void)
{
    FcConfig	*config;

    config = FcConfigGetCurrent ();
    if (!config)
	return FcFalse;

    if (FcConfigGetFonts (config, FcSetSystem))
	return FcTrue;

    return FcConfigBuildFonts (config);
}

static FcBool
FcInitFallbackConfig (void)
{
    FcConfig	*config;

    config = FcConfigCreate ();
    if (!config)
	goto bail0;
    if (!FcConfigAddDir (config, (FcChar8 *) FC_FALLBACK_FONTS))
	goto bail1;
    FcConfigSetCurrent (config);
    return FcTrue;

bail1:
    FcConfigDestroy (config);
bail0:
    return FcFalse;
}

/*
 * Locate and parse the configuration file
 */
FcBool
FcInitConfig (void)
{
    FcConfig    *config;
    
    if (_fcConfig)
	return FcTrue;
    
    config = FcConfigCreate ();
    if (!config)
	return FcFalse;
    
    if (!FcConfigParseAndLoad (config, 0, FcTrue))
    {
	FcConfigDestroy (config);
	return FcInitFallbackConfig ();
    }
    
    FcConfigSetCurrent (config);
    return FcTrue;
}

FcBool
FcInit (void)
{
    return FcInitConfig () && FcInitFonts ();
}

static struct {
    char    *name;
    int	    alloc_count;
    int	    alloc_mem;
    int	    free_count;
    int	    free_mem;
} FcInUse[FC_MEM_NUM] = {
    { "charset", 0, 0 },
    { "charnode", 0 ,0 },
    { "fontset", 0, 0 },
    { "fontptr", 0, 0 },
    { "objectset", 0, 0 },
    { "objectptr", 0, 0 },
    { "matrix", 0, 0 },
    { "pattern", 0, 0 },
    { "patelt", 0, 0 },
    { "vallist", 0, 0 },
    { "substate", 0, 0 },
    { "string", 0, 0 },
    { "listbuck", 0, 0 },
};

static int  FcAllocCount, FcAllocMem;
static int  FcFreeCount, FcFreeMem;

static int  FcMemNotice = 1*1024*1024;

static int  FcAllocNotify, FcFreeNotify;

void
FcMemReport (void)
{
    int	i;
    printf ("Fc Memory Usage:\n");
    printf ("\t   Which       Alloc           Free           Active\n");
    printf ("\t           count   bytes   count   bytes   count   bytes\n");
    for (i = 0; i < FC_MEM_NUM; i++)
	printf ("\t%8.8s%8d%8d%8d%8d%8d%8d\n",
		FcInUse[i].name,
		FcInUse[i].alloc_count, FcInUse[i].alloc_mem,
		FcInUse[i].free_count, FcInUse[i].free_mem,
		FcInUse[i].alloc_count - FcInUse[i].free_count,
		FcInUse[i].alloc_mem - FcInUse[i].free_mem);
    printf ("\t%8.8s%8d%8d%8d%8d%8d%8d\n",
	    "Total",
	    FcAllocCount, FcAllocMem,
	    FcFreeCount, FcFreeMem,
	    FcAllocCount - FcFreeCount,
	    FcAllocMem - FcFreeMem);
    FcAllocNotify = 0;
    FcFreeNotify = 0;
}

void
FcMemAlloc (int kind, int size)
{
    if (FcDebug() & FC_DBG_MEMORY)
    {
	FcInUse[kind].alloc_count++;
	FcInUse[kind].alloc_mem += size;
	FcAllocCount++;
	FcAllocMem += size;
	FcAllocNotify += size;
	if (FcAllocNotify > FcMemNotice)
	    FcMemReport ();
    }
}

void
FcMemFree (int kind, int size)
{
    if (FcDebug() & FC_DBG_MEMORY)
    {
	FcInUse[kind].free_count++;
	FcInUse[kind].free_mem += size;
	FcFreeCount++;
	FcFreeMem += size;
	FcFreeNotify += size;
	if (FcFreeNotify > FcMemNotice)
	    FcMemReport ();
    }
}
