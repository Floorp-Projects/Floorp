/*
 * $XFree86: xc/lib/Xrender/Composite.c,v 1.2 2000/08/28 02:43:13 tsi Exp $
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

void
XRenderComposite (Display   *dpy,
		  int	    op,
		  Picture   src,
		  Picture   mask,
		  Picture   dst,
		  int	    src_x,
		  int	    src_y,
		  int	    mask_x,
		  int	    mask_y,
		  int	    dst_x,
		  int	    dst_y,
		  unsigned int	width,
		  unsigned int	height)
{
    XExtDisplayInfo         *info = XRenderFindDisplay (dpy);
    xRenderCompositeReq	    *req;

    RenderSimpleCheckExtension (dpy, info);
    LockDisplay(dpy);
    GetReq(RenderComposite, req);
    req->reqType = info->codes->major_opcode;
    req->renderReqType = X_RenderComposite;
    req->op = (CARD8) op;
    req->src = src;
    req->mask = mask;
    req->dst = dst;
    req->xSrc = src_x;
    req->ySrc = src_y;
    req->xMask = mask_x;
    req->yMask = mask_y;
    req->xDst = dst_x;
    req->yDst = dst_y;
    req->width = width;
    req->height = height;
    UnlockDisplay(dpy);
    SyncHandle();
}
