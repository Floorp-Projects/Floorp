/*
 * $XFree86: xc/lib/Xrender/Xrender.c,v 1.8 2001/12/16 18:27:55 keithp Exp $
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

XExtensionInfo XRenderExtensionInfo;
char XRenderExtensionName[] = RENDER_NAME;

static int XRenderCloseDisplay(Display *dpy, XExtCodes *codes);

static /* const */ XExtensionHooks render_extension_hooks = {
    NULL,				/* create_gc */
    NULL,				/* copy_gc */
    NULL,				/* flush_gc */
    NULL,				/* free_gc */
    NULL,				/* create_font */
    NULL,				/* free_font */
    XRenderCloseDisplay,		/* close_display */
    NULL,				/* wire_to_event */
    NULL,				/* event_to_wire */
    NULL,				/* error */
    NULL,				/* error_string */
};

XExtDisplayInfo *
XRenderFindDisplay (Display *dpy)
{
    XExtDisplayInfo *dpyinfo;

    dpyinfo = XextFindDisplay (&XRenderExtensionInfo, dpy);
    if (!dpyinfo)
	dpyinfo = XextAddDisplay (&XRenderExtensionInfo, dpy, 
				  XRenderExtensionName,
				  &render_extension_hooks,
				  0, 0);
    return dpyinfo;
}

static int
XRenderCloseDisplay (Display *dpy, XExtCodes *codes)
{
    XExtDisplayInfo *info = XRenderFindDisplay (dpy);
    if (info->data) XFree (info->data);
    
    return XextRemoveDisplay (&XRenderExtensionInfo, dpy);
}
    
/****************************************************************************
 *                                                                          *
 *			    Render public interfaces                         *
 *                                                                          *
 ****************************************************************************/

Bool XRenderQueryExtension (Display *dpy, int *event_basep, int *error_basep)
{
    XExtDisplayInfo *info = XRenderFindDisplay (dpy);

    if (XextHasExtension(info)) {
	*event_basep = info->codes->first_event;
	*error_basep = info->codes->first_error;
	return True;
    } else {
	return False;
    }
}


Status XRenderQueryVersion (Display *dpy,
			    int	    *major_versionp,
			    int	    *minor_versionp)
{
    XExtDisplayInfo *info = XRenderFindDisplay (dpy);
    xRenderQueryVersionReply rep;
    xRenderQueryVersionReq  *req;

    RenderCheckExtension (dpy, info, 0);

    LockDisplay (dpy);
    GetReq (RenderQueryVersion, req);
    req->reqType = info->codes->major_opcode;
    req->renderReqType = X_RenderQueryVersion;
    req->majorVersion = RENDER_MAJOR;
    req->minorVersion = RENDER_MINOR;
    if (!_XReply (dpy, (xReply *) &rep, 0, xTrue)) {
	UnlockDisplay (dpy);
	SyncHandle ();
	return 0;
    }
    *major_versionp = rep.majorVersion;
    *minor_versionp = rep.minorVersion;
    UnlockDisplay (dpy);
    SyncHandle ();
    return 1;
}

static XRenderPictFormat *
_XRenderFindFormat (XRenderInfo *xri, PictFormat format)
{
    int	nf;
    
    for (nf = 0; nf < xri->nformat; nf++)
	if (xri->format[nf].id == format)
	    return &xri->format[nf];
    return 0;
}

static Visual *
_XRenderFindVisual (Display *dpy, VisualID vid)
{
    return _XVIDtoVisual (dpy, vid);
}

Status
XRenderQueryFormats (Display *dpy)
{
    XExtDisplayInfo *info = XRenderFindDisplay (dpy);
    xRenderQueryPictFormatsReply rep;
    xRenderQueryPictFormatsReq  *req;
    XRenderInfo			*xri;
    XRenderPictFormat		*format;
    XRenderScreen		*screen;
    XRenderDepth		*depth;
    XRenderVisual		*visual;
    xPictFormInfo		*xFormat;
    xPictScreen			*xScreen;
    xPictDepth			*xDepth;
    xPictVisual			*xVisual;
    void			*xData;
    int				nf, ns, nd, nv;
    int				rlength;
    
    RenderCheckExtension (dpy, info, 0);
    LockDisplay (dpy);
    if (info->data)
    {
	UnlockDisplay (dpy);
	return 1;
    }
    GetReq (RenderQueryPictFormats, req);
    req->reqType = info->codes->major_opcode;
    req->renderReqType = X_RenderQueryPictFormats;
    if (!_XReply (dpy, (xReply *) &rep, 0, xFalse)) {
	UnlockDisplay (dpy);
	SyncHandle ();
	return 0;
    }
    xri = (XRenderInfo *) Xmalloc (sizeof (XRenderInfo) +
				  rep.numFormats * sizeof (XRenderPictFormat) +
				  rep.numScreens * sizeof (XRenderScreen) +
				  rep.numDepths * sizeof (XRenderDepth) +
				  rep.numVisuals * sizeof (XRenderVisual));
    xri->format = (XRenderPictFormat *) (xri + 1);
    xri->nformat = rep.numFormats;
    xri->screen = (XRenderScreen *) (xri->format + rep.numFormats);
    xri->nscreen = rep.numScreens;
    xri->depth = (XRenderDepth *) (xri->screen + rep.numScreens);
    xri->ndepth = rep.numDepths;
    xri->visual = (XRenderVisual *) (xri->depth + rep.numDepths);
    xri->nvisual = rep.numVisuals;
    rlength = (rep.numFormats * sizeof (xPictFormInfo) +
	       rep.numScreens * sizeof (xPictScreen) +
	       rep.numDepths * sizeof (xPictDepth) +
	       rep.numVisuals * sizeof (xPictVisual));
    xData = (void *) Xmalloc (rlength);
    
    if (!xri || !xData)
    {
	if (xri) Xfree (xri);
	if (xData) Xfree (xData);
	_XEatData (dpy, rlength);
	UnlockDisplay (dpy);
	SyncHandle ();
	return 0;
    }
    _XRead (dpy, (char *) xData, rlength);
    format = xri->format;
    xFormat = (xPictFormInfo *) xData;
    for (nf = 0; nf < rep.numFormats; nf++)
    {
	format->id = xFormat->id;
	format->type = xFormat->type;
	format->depth = xFormat->depth;
	format->direct.red = xFormat->direct.red;
	format->direct.redMask = xFormat->direct.redMask;
	format->direct.green = xFormat->direct.green;
	format->direct.greenMask = xFormat->direct.greenMask;
	format->direct.blue = xFormat->direct.blue;
	format->direct.blueMask = xFormat->direct.blueMask;
	format->direct.alpha = xFormat->direct.alpha;
	format->direct.alphaMask = xFormat->direct.alphaMask;
	format->colormap = xFormat->colormap;
	format++;
	xFormat++;
    }
    xScreen = (xPictScreen *) xFormat;
    screen = xri->screen;
    depth = xri->depth;
    visual = xri->visual;
    for (ns = 0; ns < xri->nscreen; ns++)
    {
	screen->depths = depth;
	screen->ndepths = xScreen->nDepth;
	screen->fallback = _XRenderFindFormat (xri, xScreen->fallback);
	xDepth = (xPictDepth *) (xScreen + 1);
	for (nd = 0; nd < screen->ndepths; nd++)
	{
	    depth->depth = xDepth->depth;
	    depth->nvisuals = xDepth->nPictVisuals;
	    depth->visuals = visual;
	    xVisual = (xPictVisual *) (xDepth + 1);
	    for (nv = 0; nv < depth->nvisuals; nv++)
	    {
		visual->visual = _XRenderFindVisual (dpy, xVisual->visual);
		visual->format = _XRenderFindFormat (xri, xVisual->format);
		visual++;
		xVisual++;
	    }
	    depth++;
	    xDepth = (xPictDepth *) xVisual;
	}
	xScreen = (xPictScreen *) xDepth;	    
    }
    info->data = (XPointer) xri;
    UnlockDisplay (dpy);
    SyncHandle ();
    Xfree (xData);
    return 1;
}

XRenderPictFormat *
XRenderFindVisualFormat (Display *dpy, _Xconst Visual *visual)
{
    XExtDisplayInfo *info = XRenderFindDisplay (dpy);
    int		    nv;
    XRenderInfo	    *xri;
    XRenderVisual   *xrv;
    
    RenderCheckExtension (dpy, info, 0);
    if (!XRenderQueryFormats (dpy))
        return 0;
    xri = (XRenderInfo *) info->data;
    for (nv = 0, xrv = xri->visual; nv < xri->nvisual; nv++, xrv++)
	if (xrv->visual == visual)
	    return xrv->format;
    return 0;
}

XRenderPictFormat *
XRenderFindFormat (Display		*dpy,
		   unsigned long	mask,
		   _Xconst XRenderPictFormat	*template,
		   int			count)
{
    XExtDisplayInfo *info = XRenderFindDisplay (dpy);
    int		    nf;
    XRenderInfo     *xri;
    
    RenderCheckExtension (dpy, info, 0);
    if (!XRenderQueryFormats (dpy))
	return 0;
    xri = (XRenderInfo *) info->data;
    for (nf = 0; nf < xri->nformat; nf++)
    {
	if (mask & PictFormatID)
	    if (template->id != xri->format[nf].id)
		continue;
	if (mask & PictFormatType)
	if (template->type != xri->format[nf].type)
		continue;
	if (mask & PictFormatDepth)
	    if (template->depth != xri->format[nf].depth)
		continue;
	if (mask & PictFormatRed)
	    if (template->direct.red != xri->format[nf].direct.red)
		continue;
	if (mask & PictFormatRedMask)
	    if (template->direct.redMask != xri->format[nf].direct.redMask)
		continue;
	if (mask & PictFormatGreen)
	    if (template->direct.green != xri->format[nf].direct.green)
		continue;
	if (mask & PictFormatGreenMask)
	    if (template->direct.greenMask != xri->format[nf].direct.greenMask)
		continue;
	if (mask & PictFormatBlue)
	    if (template->direct.blue != xri->format[nf].direct.blue)
		continue;
	if (mask & PictFormatBlueMask)
	    if (template->direct.blueMask != xri->format[nf].direct.blueMask)
		continue;
	if (mask & PictFormatAlpha)
	    if (template->direct.alpha != xri->format[nf].direct.alpha)
		continue;
	if (mask & PictFormatAlphaMask)
	    if (template->direct.alphaMask != xri->format[nf].direct.alphaMask)
		continue;
	if (mask & PictFormatColormap)
	    if (template->colormap != xri->format[nf].colormap)
		continue;
	if (count-- == 0)
	    return &xri->format[nf];
    }
    return 0;
}

