/*
 * $XFree86: xc/lib/Xrender/Glyph.c,v 1.8 2002/02/12 07:17:37 keithp Exp $
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

GlyphSet
XRenderCreateGlyphSet (Display *dpy, _Xconst XRenderPictFormat *format)
{
    XExtDisplayInfo		*info = XRenderFindDisplay (dpy);
    GlyphSet			gsid;
    xRenderCreateGlyphSetReq	*req;

    RenderCheckExtension (dpy, info, 0);
    LockDisplay(dpy);
    GetReq(RenderCreateGlyphSet, req);
    req->reqType = info->codes->major_opcode;
    req->renderReqType = X_RenderCreateGlyphSet;
    req->gsid = gsid = XAllocID(dpy);
    req->format = format->id;
    UnlockDisplay(dpy);
    SyncHandle();
    return gsid;
}

GlyphSet
XRenderReferenceGlyphSet (Display *dpy, GlyphSet existing)
{
    XExtDisplayInfo             *info = XRenderFindDisplay (dpy);
    GlyphSet                    gsid;
    xRenderReferenceGlyphSetReq	*req;

    RenderCheckExtension (dpy, info, 0);
    LockDisplay(dpy);
    GetReq(RenderReferenceGlyphSet, req);
    req->reqType = info->codes->major_opcode;
    req->renderReqType = X_RenderReferenceGlyphSet;
    req->gsid = gsid = XAllocID(dpy);
    req->existing = existing;
    UnlockDisplay(dpy);
    SyncHandle();
    return gsid;
}

void
XRenderFreeGlyphSet (Display *dpy, GlyphSet glyphset)
{
    XExtDisplayInfo         *info = XRenderFindDisplay (dpy);
    xRenderFreeGlyphSetReq  *req;

    RenderSimpleCheckExtension (dpy, info);
    LockDisplay(dpy);
    GetReq(RenderFreeGlyphSet, req);
    req->reqType = info->codes->major_opcode;
    req->renderReqType = X_RenderFreeGlyphSet;
    req->glyphset = glyphset;
    UnlockDisplay(dpy);
    SyncHandle();
}

void
XRenderAddGlyphs (Display	*dpy,
		  GlyphSet	glyphset,
		  _Xconst Glyph		*gids,
		  _Xconst XGlyphInfo	*glyphs,
		  int		nglyphs,
		  _Xconst char		*images,
		  int		nbyte_images)
{
    XExtDisplayInfo         *info = XRenderFindDisplay (dpy);
    xRenderAddGlyphsReq	    *req;
    long		    len;

    if (nbyte_images & 3)
	nbyte_images += 4 - (nbyte_images & 3);
    RenderSimpleCheckExtension (dpy, info);
    LockDisplay(dpy);
    GetReq(RenderAddGlyphs, req);
    req->reqType = info->codes->major_opcode;
    req->renderReqType = X_RenderAddGlyphs;
    req->glyphset = glyphset;
    req->nglyphs = nglyphs;
    len = (nglyphs * (SIZEOF (xGlyphInfo) + 4) + nbyte_images) >> 2;
    SetReqLen(req, len, len);
    Data32 (dpy, (long *) gids, nglyphs * 4);
    Data16 (dpy, (short *) glyphs, nglyphs * SIZEOF (xGlyphInfo));
    Data (dpy, images, nbyte_images);
    UnlockDisplay(dpy);
    SyncHandle();
}

void
XRenderFreeGlyphs (Display   *dpy,
		   GlyphSet  glyphset,
		   _Xconst Glyph     *gids,
		   int       nglyphs)
{
    XExtDisplayInfo         *info = XRenderFindDisplay (dpy);
    xRenderFreeGlyphsReq    *req;
    long                    len;

    RenderSimpleCheckExtension (dpy, info);
    LockDisplay(dpy);
    GetReq(RenderFreeGlyphs, req);
    req->reqType = info->codes->major_opcode;
    req->renderReqType = X_RenderFreeGlyphs;
    req->glyphset = glyphset;
    len = nglyphs;
    SetReqLen(req, len, len);
    len <<= 2;
    Data32 (dpy, (long *) gids, len);
    UnlockDisplay(dpy);
    SyncHandle();
}
	   
void
XRenderCompositeString8 (Display	    *dpy,
			 int		    op,
			 Picture	    src,
			 Picture	    dst,
			 _Xconst XRenderPictFormat  *maskFormat,
			 GlyphSet	    glyphset,
			 int		    xSrc,
			 int		    ySrc,
			 int		    xDst,
			 int		    yDst,
			 _Xconst char	    *string,
			 int		    nchar)
{
    XExtDisplayInfo		*info = XRenderFindDisplay (dpy);
    xRenderCompositeGlyphs8Req	*req;
    long			len;
    xGlyphElt			*elt;
    int				nbytes;

    if (!nchar)
	return;
    
    RenderSimpleCheckExtension (dpy, info);
    LockDisplay(dpy);

    GetReq(RenderCompositeGlyphs8, req);
    req->reqType = info->codes->major_opcode;
    req->renderReqType = X_RenderCompositeGlyphs8;
    req->op = op;
    req->src = src;
    req->dst = dst;
    req->maskFormat = maskFormat ? maskFormat->id : None;
    req->glyphset = glyphset;
    req->xSrc = xSrc;
    req->ySrc = ySrc;    

    /*
     * xGlyphElt must be aligned on a 32-bit boundary; this is
     * easily done by filling no more than 252 glyphs in each
     * bucket
     */
    
#define MAX_8 252

    len = SIZEOF(xGlyphElt) * ((nchar + MAX_8-1) / MAX_8) + nchar;
    
    req->length += (len + 3)>>2;  /* convert to number of 32-bit words */
    
    /* 
     * If the entire request does not fit into the remaining space in the
     * buffer, flush the buffer first.
     */

    if (dpy->bufptr + len > dpy->bufmax)
    	_XFlush (dpy);

    while(nchar > MAX_8)
    {
	nbytes = MAX_8 + SIZEOF(xGlyphElt);
	BufAlloc (xGlyphElt *, elt, nbytes);
	elt->len = MAX_8;
	elt->deltax = xDst;
	elt->deltay = yDst;
	xDst = 0;
	yDst = 0;
	memcpy ((char *) (elt + 1), string, MAX_8);
	nchar = nchar - MAX_8;
	string += MAX_8;
    }
	
    if (nchar)
    {
	nbytes = (nchar + SIZEOF(xGlyphElt) + 3) & ~3;
	BufAlloc (xGlyphElt *, elt, nbytes); 
	elt->len = nchar;
	elt->deltax = xDst;
	elt->deltay = yDst;
	memcpy ((char *) (elt + 1), string, nchar);
    }
    
    UnlockDisplay(dpy);
    SyncHandle();
}
void
XRenderCompositeString16 (Display	    *dpy,
			  int		    op,
			  Picture	    src,
			  Picture	    dst,
			  _Xconst XRenderPictFormat *maskFormat,
			  GlyphSet	    glyphset,
			  int		    xSrc,
			  int		    ySrc,
			  int		    xDst,
			  int		    yDst,
			  _Xconst unsigned short    *string,
			  int		    nchar)
{
    XExtDisplayInfo		*info = XRenderFindDisplay (dpy);
    xRenderCompositeGlyphs8Req	*req;
    long			len;
    xGlyphElt			*elt;
    int				nbytes;

    if (!nchar)
	return;
    
    RenderSimpleCheckExtension (dpy, info);
    LockDisplay(dpy);
    
    GetReq(RenderCompositeGlyphs16, req);
    req->reqType = info->codes->major_opcode;
    req->renderReqType = X_RenderCompositeGlyphs16;
    req->op = op;
    req->src = src;
    req->dst = dst;
    req->maskFormat = maskFormat ? maskFormat->id : None;
    req->glyphset = glyphset;
    req->xSrc = xSrc;
    req->ySrc = ySrc;    

#define MAX_16	254

    len = SIZEOF(xGlyphElt) * ((nchar + MAX_8-1) / MAX_8) + nchar * 2;
    
    req->length += (len + 3)>>2;  /* convert to number of 32-bit words */
    
    /* 
     * If the entire request does not fit into the remaining space in the
     * buffer, flush the buffer first.
     */

    if (dpy->bufptr + len > dpy->bufmax)
    	_XFlush (dpy);

    while(nchar > MAX_16)
    {
	nbytes = MAX_16 * 2 + SIZEOF(xGlyphElt);
	BufAlloc (xGlyphElt *, elt, nbytes);
	elt->len = MAX_16;
	elt->deltax = xDst;
	elt->deltay = yDst;
	xDst = 0;
	yDst = 0;
	memcpy ((char *) (elt + 1), (char *) string, MAX_16 * 2);
	nchar = nchar - MAX_16;
	string += MAX_16;
    }
	
    if (nchar)
    {
	nbytes = (nchar * 2 + SIZEOF(xGlyphElt) + 3) & ~3;
	BufAlloc (xGlyphElt *, elt, nbytes); 
	elt->len = nchar;
	elt->deltax = xDst;
	elt->deltay = yDst;
	memcpy ((char *) (elt + 1), (char *) string, nchar * 2);
    }
    
    UnlockDisplay(dpy);
    SyncHandle();
}

void
XRenderCompositeString32 (Display	    *dpy,
			  int		    op,
			  Picture	    src,
			  Picture	    dst,
			  _Xconst XRenderPictFormat  *maskFormat,
			  GlyphSet	    glyphset,
			  int		    xSrc,
			  int		    ySrc,
			  int		    xDst,
			  int		    yDst,
			  _Xconst unsigned int	    *string,
			  int		    nchar)
{
    XExtDisplayInfo		*info = XRenderFindDisplay (dpy);
    xRenderCompositeGlyphs8Req	*req;
    long			len;
    xGlyphElt			*elt;
    int				nbytes;

    if (!nchar)
	return;
    
    RenderSimpleCheckExtension (dpy, info);
    LockDisplay(dpy);
    
    GetReq(RenderCompositeGlyphs32, req);
    req->reqType = info->codes->major_opcode;
    req->renderReqType = X_RenderCompositeGlyphs32;
    req->op = op;
    req->src = src;
    req->dst = dst;
    req->maskFormat = maskFormat ? maskFormat->id : None;
    req->glyphset = glyphset;
    req->xSrc = xSrc;
    req->ySrc = ySrc;    

#define MAX_32	254

    len = SIZEOF(xGlyphElt) * ((nchar + MAX_8-1) / MAX_8) + nchar * 4;
    
    req->length += (len + 3)>>2;  /* convert to number of 32-bit words */
    
    /* 
     * If the entire request does not fit into the remaining space in the
     * buffer, flush the buffer first.
     */

    if (dpy->bufptr + len > dpy->bufmax)
    	_XFlush (dpy);

    while(nchar > MAX_32)
    {
	nbytes = MAX_32 * 4 + SIZEOF(xGlyphElt);
	BufAlloc (xGlyphElt *, elt, nbytes);
	elt->len = MAX_32;
	elt->deltax = xDst;
	elt->deltay = yDst;
	xDst = 0;
	yDst = 0;
	memcpy ((char *) (elt + 1), (char *) string, MAX_32 * 4);
	nchar = nchar - MAX_32;
	string += MAX_32;
    }
	
    if (nchar)
    {
	nbytes = nchar * 4 + SIZEOF(xGlyphElt);
	BufAlloc (xGlyphElt *, elt, nbytes); 
	elt->len = nchar;
	elt->deltax = xDst;
	elt->deltay = yDst;
	memcpy ((char *) (elt + 1), (char *) string, nchar * 4);
    }
    
    UnlockDisplay(dpy);
    SyncHandle();
}

void
XRenderCompositeText8 (Display			    *dpy,
		       int			    op,
		       Picture			    src,
		       Picture			    dst,
		       _Xconst XRenderPictFormat    *maskFormat,
		       int			    xSrc,
		       int			    ySrc,
		       int			    xDst,
		       int			    yDst,
		       _Xconst XGlyphElt8	    *elts,
		       int			    nelt)
{
    XExtDisplayInfo		*info = XRenderFindDisplay (dpy);
    xRenderCompositeGlyphs8Req	*req;
    long			len;
    long			elen;
    xGlyphElt			*elt;
    int				i;
    _Xconst char		*chars;
    int				nchars;

    if (!nelt)
	return;
    
    RenderSimpleCheckExtension (dpy, info);
    LockDisplay(dpy);

    GetReq(RenderCompositeGlyphs8, req);
    req->reqType = info->codes->major_opcode;
    req->renderReqType = X_RenderCompositeGlyphs8;
    req->op = op;
    req->src = src;
    req->dst = dst;
    req->maskFormat = maskFormat ? maskFormat->id : None;
    req->glyphset = elts[0].glyphset;
    req->xSrc = xSrc;
    req->ySrc = ySrc;    

    /*
     * Compute the space necessary
     */
    len = 0;
    
    for (i = 0; i < nelt; i++)
    {
	if (elts[i].glyphset != req->glyphset)
	    len += (SIZEOF (xGlyphElt) + 4) >> 2;
	nchars = elts[i].nchars;
	/*
	 * xGlyphElt must be aligned on a 32-bit boundary; this is
	 * easily done by filling no more than 252 glyphs in each
	 * bucket
	 */
	elen = SIZEOF(xGlyphElt) * ((nchars + MAX_8-1) / MAX_8) + nchars;
	len += (elen + 3) >> 2;
    }
    
    req->length += len;
    /* 
     * If the entire request does not fit into the remaining space in the
     * buffer, flush the buffer first.
     */

    if (dpy->bufptr + (len << 2) > dpy->bufmax)
    	_XFlush (dpy);

    for (i = 0; i < nelt; i++)
    {
	/*
	 * Switch glyphsets
	 */
	if (elts[i].glyphset != req->glyphset)
	{
	    BufAlloc (xGlyphElt *, elt, SIZEOF (xGlyphElt));
	    elt->len = 0xff;
	    elt->deltax = 0;
	    elt->deltay = 0;
	    Data32(dpy, &elts[i].glyphset, 4);
	}
	nchars = elts[i].nchars;
	xDst = elts[i].xOff;
	yDst = elts[i].yOff;
	chars = elts[i].chars;
	while (nchars)
	{
	    BufAlloc (xGlyphElt *, elt, SIZEOF(xGlyphElt))
	    elt->len = nchars > MAX_8 ? MAX_8 : nchars;
	    elt->deltax = xDst;
	    elt->deltay = yDst;
	    xDst = 0;
	    yDst = 0;
	    Data (dpy, chars, elt->len);
	    nchars -= elt->len;
	    chars += elt->len;
	}
    }
    
    UnlockDisplay(dpy);
    SyncHandle();
}

void
XRenderCompositeText16 (Display			    *dpy,
			int			    op,
			Picture			    src,
			Picture			    dst,
			_Xconst XRenderPictFormat   *maskFormat,
			int			    xSrc,
			int			    ySrc,
			int			    xDst,
			int			    yDst,
			_Xconst XGlyphElt16	    *elts,
			int			    nelt)
{
    XExtDisplayInfo		*info = XRenderFindDisplay (dpy);
    xRenderCompositeGlyphs16Req	*req;
    long			len;
    long			elen;
    xGlyphElt			*elt;
    int				i;
    _Xconst unsigned short    	*chars;
    int				nchars;

    if (!nelt)
	return;
    
    RenderSimpleCheckExtension (dpy, info);
    LockDisplay(dpy);

    GetReq(RenderCompositeGlyphs16, req);
    req->reqType = info->codes->major_opcode;
    req->renderReqType = X_RenderCompositeGlyphs16;
    req->op = op;
    req->src = src;
    req->dst = dst;
    req->maskFormat = maskFormat ? maskFormat->id : None;
    req->glyphset = elts[0].glyphset;
    req->xSrc = xSrc;
    req->ySrc = ySrc;    

    /*
     * Compute the space necessary
     */
    len = 0;
    
    for (i = 0; i < nelt; i++)
    {
	if (elts[i].glyphset != req->glyphset)
	    len += (SIZEOF (xGlyphElt) + 4) >> 2;
	nchars = elts[i].nchars;
	/*
	 * xGlyphElt must be aligned on a 32-bit boundary; this is
	 * easily done by filling no more than 254 glyphs in each
	 * bucket
	 */
	elen = SIZEOF(xGlyphElt) * ((nchars + MAX_16-1) / MAX_16) + nchars * 2;
	len += (elen + 3) >> 2;
    }
    
    req->length += len;
    /* 
     * If the entire request does not fit into the remaining space in the
     * buffer, flush the buffer first.
     */

    if (dpy->bufptr + (len << 2) > dpy->bufmax)
    	_XFlush (dpy);

    for (i = 0; i < nelt; i++)
    {
	/*
	 * Switch glyphsets
	 */
	if (elts[i].glyphset != req->glyphset)
	{
	    BufAlloc (xGlyphElt *, elt, SIZEOF (xGlyphElt));
	    elt->len = 0xff;
	    elt->deltax = 0;
	    elt->deltay = 0;
	    Data32(dpy, &elts[i].glyphset, 4);
	}
	nchars = elts[i].nchars;
	xDst = elts[i].xOff;
	yDst = elts[i].yOff;
	chars = elts[i].chars;
	while (nchars)
	{
	    BufAlloc (xGlyphElt *, elt, SIZEOF(xGlyphElt))
	    elt->len = nchars > MAX_16 ? MAX_16 : nchars;
	    elt->deltax = xDst;
	    elt->deltay = yDst;
	    xDst = 0;
	    yDst = 0;
	    Data16 (dpy, chars, elt->len * 2);
	    nchars -= elt->len;
	    chars += elt->len;
	}
    }
    
    UnlockDisplay(dpy);
    SyncHandle();
}

void
XRenderCompositeText32 (Display			    *dpy,
			int			    op,
			Picture			    src,
			Picture			    dst,
			_Xconst XRenderPictFormat   *maskFormat,
			int			    xSrc,
			int			    ySrc,
			int			    xDst,
			int			    yDst,
			_Xconst XGlyphElt32	    *elts,
			int			    nelt)
{
    XExtDisplayInfo		*info = XRenderFindDisplay (dpy);
    xRenderCompositeGlyphs32Req	*req;
    long			len;
    long			elen;
    xGlyphElt			*elt;
    int				i;
    _Xconst unsigned int    	*chars;
    int				nchars;

    if (!nelt)
	return;
    
    RenderSimpleCheckExtension (dpy, info);
    LockDisplay(dpy);

    GetReq(RenderCompositeGlyphs32, req);
    req->reqType = info->codes->major_opcode;
    req->renderReqType = X_RenderCompositeGlyphs32;
    req->op = op;
    req->src = src;
    req->dst = dst;
    req->maskFormat = maskFormat ? maskFormat->id : None;
    req->glyphset = elts[0].glyphset;
    req->xSrc = xSrc;
    req->ySrc = ySrc;    

    /*
     * Compute the space necessary
     */
    len = 0;
    
    for (i = 0; i < nelt; i++)
    {
	if (elts[i].glyphset != req->glyphset)
	    len += (SIZEOF (xGlyphElt) + 4) >> 2;
	nchars = elts[i].nchars;
	elen = SIZEOF(xGlyphElt) * ((nchars + MAX_32) / MAX_32) + nchars *4;
	len += (elen + 3) >> 2;
    }
    
    req->length += len;
    /* 
     * If the entire request does not fit into the remaining space in the
     * buffer, flush the buffer first.
     */

    if (dpy->bufptr + (len << 2) > dpy->bufmax)
    	_XFlush (dpy);

    for (i = 0; i < nelt; i++)
    {
	/*
	 * Switch glyphsets
	 */
	if (elts[i].glyphset != req->glyphset)
	{
	    BufAlloc (xGlyphElt *, elt, SIZEOF (xGlyphElt));
	    elt->len = 0xff;
	    elt->deltax = 0;
	    elt->deltay = 0;
	    Data32(dpy, &elts[i].glyphset, 4);
	}
	nchars = elts[i].nchars;
	xDst = elts[i].xOff;
	yDst = elts[i].yOff;
	chars = elts[i].chars;
	while (nchars)
	{
	    BufAlloc (xGlyphElt *, elt, SIZEOF(xGlyphElt))
	    elt->len = nchars > MAX_32 ? MAX_32 : nchars;
	    elt->deltax = xDst;
	    elt->deltay = yDst;
	    xDst = 0;
	    yDst = 0;
	    Data32 (dpy, chars, elt->len * 4);
	    nchars -= elt->len;
	    chars += elt->len;
	}
    }
    
    UnlockDisplay(dpy);
    SyncHandle();
}
