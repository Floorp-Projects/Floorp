/*
 * $XFree86: xc/lib/Xrender/Picture.c,v 1.8 2001/12/16 18:27:55 keithp Exp $
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
#include "region.h"

static void
_XRenderProcessPictureAttributes (Display		    *dpy,
				  xRenderChangePictureReq   *req,
				  unsigned long		    valuemask,
				  _Xconst XRenderPictureAttributes  *attributes)
{
    unsigned long values[32];
    register unsigned long *value = values;
    unsigned int nvalues;
    
    if (valuemask & CPRepeat)
	*value++ = attributes->repeat;
    if (valuemask & CPAlphaMap)
	*value++ = attributes->alpha_map;
    if (valuemask & CPAlphaXOrigin)
	*value++ = attributes->alpha_x_origin;
    if (valuemask & CPAlphaYOrigin)
	*value++ = attributes->alpha_y_origin;
    if (valuemask & CPClipXOrigin)
	*value++ = attributes->clip_x_origin;
    if (valuemask & CPClipYOrigin)
	*value++ = attributes->clip_y_origin;
    if (valuemask & CPClipMask)
	*value++ = attributes->clip_mask;
    if (valuemask & CPGraphicsExposure)
	*value++ = attributes->graphics_exposures;
    if (valuemask & CPSubwindowMode)
	*value++ = attributes->subwindow_mode;
    if (valuemask & CPPolyEdge)
	*value++ = attributes->poly_edge;
    if (valuemask & CPPolyMode)
	*value++ = attributes->poly_mode;
    if (valuemask & CPDither)
	*value++ = attributes->dither;
    if (valuemask & CPComponentAlpha)
	*value++ = attributes->component_alpha;

    req->length += (nvalues = value - values);

    nvalues <<= 2;			    /* watch out for macros... */
    Data32 (dpy, (long *) values, (long)nvalues);
}

Picture
XRenderCreatePicture (Display			*dpy,
		      Drawable			drawable,
		      _Xconst XRenderPictFormat		*format,
		      unsigned long		valuemask,
		      _Xconst XRenderPictureAttributes	*attributes)
{
    XExtDisplayInfo	    *info = XRenderFindDisplay (dpy);
    Picture		    pid;
    xRenderCreatePictureReq *req;

    RenderCheckExtension (dpy, info, 0);
    LockDisplay(dpy);
    GetReq(RenderCreatePicture, req);
    req->reqType = info->codes->major_opcode;
    req->renderReqType = X_RenderCreatePicture;
    req->pid = pid = XAllocID(dpy);
    req->drawable = drawable;
    req->format = format->id;
    if ((req->mask = valuemask))
	_XRenderProcessPictureAttributes (dpy,
					  (xRenderChangePictureReq *) req,
					  valuemask,
					  attributes);
    UnlockDisplay(dpy);
    SyncHandle();
    return pid;
}

void
XRenderChangePicture (Display                   *dpy,
		      Picture			picture,
		      unsigned long             valuemask,
		      _Xconst XRenderPictureAttributes  *attributes)
{
    XExtDisplayInfo	    *info = XRenderFindDisplay (dpy);
    xRenderChangePictureReq *req;
    
    RenderSimpleCheckExtension (dpy, info);
    LockDisplay(dpy);
    GetReq(RenderChangePicture, req);
    req->reqType = info->codes->major_opcode;
    req->renderReqType = X_RenderChangePicture;
    req->picture = picture;
    req->mask = valuemask;
    _XRenderProcessPictureAttributes (dpy,
				      req,
				      valuemask,
				      attributes);
    UnlockDisplay(dpy);
    SyncHandle();
}

static void
_XRenderSetPictureClipRectangles (Display	    *dpy,
				  XExtDisplayInfo   *info,
				  Picture	    picture,
				  int		    xOrigin,
				  int		    yOrigin,
				  _Xconst XRectangle	    *rects,
				  int		    n)
{
    xRenderSetPictureClipRectanglesReq	*req;
    long				len;

    GetReq (RenderSetPictureClipRectangles, req);
    req->reqType = info->codes->major_opcode;
    req->renderReqType = X_RenderSetPictureClipRectangles;
    req->picture = picture;
    req->xOrigin = xOrigin;
    req->yOrigin = yOrigin;
    len = ((long) n) << 1;
    SetReqLen (req, len, 1);
    len <<= 2;
    Data16 (dpy, (short *) rects, len);
}

void
XRenderSetPictureClipRectangles (Display	*dpy,
				 Picture	picture,
				 int		xOrigin,
				 int		yOrigin,
				 _Xconst XRectangle	*rects,
				 int		n)
{
    XExtDisplayInfo	    *info = XRenderFindDisplay (dpy);
    
    RenderSimpleCheckExtension (dpy, info);
    LockDisplay(dpy);
    _XRenderSetPictureClipRectangles (dpy, info, picture, 
				      xOrigin, yOrigin, rects, n);
    UnlockDisplay (dpy);
    SyncHandle ();
}

void
XRenderSetPictureClipRegion (Display	    *dpy,
			     Picture	    picture,
			     Region	    r)
{
    XExtDisplayInfo *info = XRenderFindDisplay (dpy);
    int		    i;
    XRectangle	    *xr, *pr;
    BOX		    *pb;
    unsigned long   total;
    
    RenderSimpleCheckExtension (dpy, info);
    LockDisplay(dpy);
    total = r->numRects * sizeof (XRectangle);
    if ((xr = (XRectangle *) _XAllocTemp(dpy, total))) {
	for (pr = xr, pb = r->rects, i = r->numRects; --i >= 0; pr++, pb++) {
	    pr->x = pb->x1;
	    pr->y = pb->y1;
	    pr->width = pb->x2 - pb->x1;
	    pr->height = pb->y2 - pb->y1;
	}
    }
    if (xr || !r->numRects)
	_XRenderSetPictureClipRectangles (dpy, info, picture, 0, 0, 
					  xr, r->numRects);
    if (xr)
	_XFreeTemp(dpy, (char *)xr, total);
    UnlockDisplay(dpy);
    SyncHandle();
}    

void
XRenderFreePicture (Display                   *dpy,
		    Picture                   picture)
{
    XExtDisplayInfo         *info = XRenderFindDisplay (dpy);
    xRenderFreePictureReq   *req;

    RenderSimpleCheckExtension (dpy, info);
    LockDisplay(dpy);
    GetReq(RenderFreePicture, req);
    req->reqType = info->codes->major_opcode;
    req->renderReqType = X_RenderFreePicture;
    req->picture = picture;
    UnlockDisplay(dpy);
    SyncHandle();
}
