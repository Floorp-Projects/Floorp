/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* -*- Mode: C; tab-width: 4 -*-
 *   il.h --- Exported image library interface
 *
 *   $Id: il.h,v 3.1 1998/03/28 03:35:02 ltabb Exp $
 */


/*
 *  global defines for image lib users
 */

#ifndef _IL_H
#define _IL_H

#include "ntypes.h"

/* attr values */
#define IL_ATTR_RDONLY	      0
#define IL_ATTR_RW	          1
#define IL_ATTR_TRANSPARENT	  2
#define IL_ATTR_HONOR_INDEX   4

#undef ABS
#define ABS(x)     (((x) < 0) ? -(x) : (x))

/* A fast but limited, perceptually-weighted color distance function */
#define IL_COLOR_DISTANCE(r1, r2, g1, g2, b1, b2)                             \
  ((ABS((g1) - (g2)) << 2) + (ABS((r1) - (r2)) << 1) + (ABS((b1) - (b2))))

/* We don't distinguish between colors that are "closer" together
   than this.  The appropriate setting is a subjective matter. */
#define IL_CLOSE_COLOR_THRESHOLD  6

#ifdef M12N                     /* Get rid of this */
struct IL_Image_struct {
    void XP_HUGE *bits;
    int width, height;
    int widthBytes, maskWidthBytes;
    int depth, bytesPerPixel;
    int colors;
    int unique_colors;          /* Number of non-duplicated colors in colormap */
    IL_RGB *map;
    IL_IRGB *transparent;
    void XP_HUGE *mask;
    int validHeight;			/* number of valid scanlines */
    void *platformData;			/* used by platform-specific FE */
    Bool has_mask; 
	Bool hasUniqueColormap;
};

struct IL_IRGB_struct {
    uint8 index, attr;
    uint8 red, green, blue;
};

struct IL_RGB_struct {
    uint8 red, green, blue, pad;
    	/* windows requires the fourth byte & many compilers pad it anyway */
};
#endif /* M12N */


XP_BEGIN_PROTOS

#ifndef M12N                    /* Get rid of these prototypes */
extern Bool IL_ReplaceImage (MWContext *context, LO_ImageStruct *new_lo_image,
                             LO_ImageStruct *old_lo_image);

extern IL_IRGB *IL_UpdateColorMap(MWContext *context, IL_IRGB *in,
                                  int in_colors, int free_colors,
                                  int *out_colors, IL_IRGB *transparent);
extern int IL_RealizeDefaultColormap(MWContext *context, IL_IRGB *bgcolor,
                                     int max_colors);

extern int IL_EnableTrueColor(MWContext *context, int bitsperpixel,
                              int rshift, int gshift, int bshift,
                              int rbits, int gbits, int bbits,
                              IL_IRGB *transparent,
                              int wild_wacky_rounding);
extern int IL_GreyScale(MWContext *context, int depth, IL_IRGB *transparent);
extern int IL_InitContext(MWContext *cx);

extern void IL_StartPage(MWContext *context);
extern void IL_EndPage(MWContext *context);

extern void IL_DeleteImages(MWContext *cx);

extern void IL_SamePage(MWContext *cx);

extern int IL_Type(const char *buf, int32 len);

extern void IL_DisableScaling(MWContext *cx);

extern void IL_DisableLowSrc(MWContext *cx);

extern void IL_SetByteOrder(MWContext *cx, Bool ls_bit_first,
                            Bool ls_byte_first);

extern int IL_SetTransparent(MWContext *cx, IL_IRGB *transparent);

extern void IL_NoMoreImages(MWContext *cx);

extern uint32 IL_ShrinkCache(void);
extern uint32 IL_GetCacheSize(void);
extern void IL_Shutdown(void);

extern int IL_ColormapTag(const char *image_url, MWContext* cx);

extern int IL_DisplayMemCacheInfoAsHTML(FO_Present_Types format_out,
                                        URL_Struct *urls,
                                        OPAQUE_CONTEXT *net_cx);
extern void IL_ScourContext(MWContext *cx);
extern void IL_SetImageCacheSize(uint32 new_size);

extern char *IL_HTMLImageInfo(char *url_address);
extern void IL_UseDefaultColormapThisPage(MWContext *cx);

extern Bool IL_AreThereLoopingImagesForContext(MWContext *cx);



extern Bool IL_FreeImage (MWContext *context, IL_Image *portableImage,
                          LO_ImageStruct *lo_image);
extern void IL_SetColorRenderMode(MWContext *cx,
                                  IL_ColorRenderMode color_render_mode);
extern NET_StreamClass * IL_NewStream (FO_Present_Types format_out,
                                       void *data_obj, URL_Struct *URL_s,
                                       MWContext *context);

extern NET_StreamClass * IL_ViewStream (FO_Present_Types format_out,
                                        void *data_obj, URL_Struct *URL_s,
                                        MWContext *context);
#endif /* M12N */

XP_END_PROTOS

#endif /* _IL_H */


