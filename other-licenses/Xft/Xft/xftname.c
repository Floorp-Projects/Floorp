/*
 * $XFree86: xc/lib/Xft/xftname.c,v 1.11 2002/02/15 07:36:11 keithp Exp $
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

#include "xftint.h"

static const FcObjectType	_XftObjectTypes[] = {
    { XFT_CORE,		FcTypeBool, },
    { XFT_XLFD,		FcTypeString, },
    { XFT_RENDER,	FcTypeBool, },
};

#define NUM_OBJECT_TYPES    (sizeof _XftObjectTypes / sizeof _XftObjectTypes[0])

Bool	_XftNameInitialized;

void 
_XftNameInit (void)
{
    if (_XftNameInitialized)
	return;
    FcNameRegisterObjectTypes (_XftObjectTypes, NUM_OBJECT_TYPES);
}

FcPattern 
*XftNameParse (const char *name)
{ 
    _XftNameInit ();
    return FcNameParse ((FcChar8 *) name); 
}
