/*
 * $XFree86: xc/lib/Xrender/FillRect.c,v 1.2 2001/12/16 18:27:55 keithp Exp $
 *
 * Copyright © 2000 SuSE, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of SuSE not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  SuSE makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * SuSE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL SuSE
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN 
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Keith Packard, SuSE, Inc.
 */

#include "Xrenderint.h"

/* precompute the maximum size of batching request allowed */

#define size (SIZEOF(xRenderFillRectanglesReq) + FRCTSPERBATCH * SIZEOF(xRectangle))

void
XRenderFillRectangle (Display	    *dpy,
		      int	    op,
		      Picture	    dst,
		      _Xconst XRenderColor  *color,
		      int	    x,
		      int	    y,
		      unsigned int  width,
		      unsigned int  height)
{
    XExtDisplayInfo		*info = XRenderFindDisplay (dpy);
    xRectangle			*rect;
    xRenderFillRectanglesReq	*req;
#ifdef MUSTCOPY
    xRectangle			rectdata;
    long			len = SIZEOF(xRectangle);

    rect = &rectdata;
#endif /* MUSTCOPY */

    RenderSimpleCheckExtension (dpy, info);
    LockDisplay(dpy);

    req = (xRenderFillRectanglesReq *) dpy->last_req;
    /* if same as previous request, with same drawable, batch requests */
    if (req->reqType == info->codes->major_opcode &&
	req->renderReqType == X_RenderFillRectangles &&
	req->op == op &&
	req->dst == dst &&
	req->color.red == color->red &&
	req->color.green == color->green &&
	req->color.blue == color->blue &&
	req->color.alpha == color->alpha &&
	dpy->bufptr + SIZEOF(xRectangle) <= dpy->bufmax &&
	(char *)dpy->bufptr - (char *)req < size)
    {
	req->length += SIZEOF(xRectangle) >> 2;
#ifndef MUSTCOPY
	rect = (xRectangle *) dpy->bufptr;
	dpy->bufptr += SIZEOF(xRectangle);
#endif /* not MUSTCOPY */
    }
    else 
    {
	GetReqExtra(RenderFillRectangles, SIZEOF(xRectangle), req);
	
	req->reqType = info->codes->major_opcode;
	req->renderReqType = X_RenderFillRectangles;
	req->op = op;
	req->dst = dst;
	req->color.red = color->red;
	req->color.green = color->green;
	req->color.blue = color->blue;
	req->color.alpha = color->alpha;
	
#ifdef MUSTCOPY
	dpy->bufptr -= SIZEOF(xRectangle);
#else
	rect = (xRectangle *) NEXTPTR(req,xRenderFillRectanglesReq);
#endif /* MUSTCOPY */
    }
    rect->x = x;
    rect->y = y;
    rect->width = width;
    rect->height = height;

#ifdef MUSTCOPY
    Data (dpy, (char *) rect, len);
#endif /* MUSTCOPY */
    UnlockDisplay(dpy);
    SyncHandle();
}

