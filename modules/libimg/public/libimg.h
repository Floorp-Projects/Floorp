/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/* -*- Mode: C; tab-width: 4 -*-
 *  libimg.h --- API calls to the Image Library.
 *  $Id: libimg.h,v 3.8 2000/07/10 07:13:26 cls%seawood.org Exp $
 */


#ifndef _LIBIMG_H
#define _LIBIMG_H

#include "prtypes.h"
#include "il_types.h"

#if 0
#include "dummy_nc.h"
#endif

#include "ilISystemServices.h"
#include "ilIImageRenderer.h"
#include "nsIImageObserver.h"

/*********************** Observers and Observables ***************************/

/* This is the cross-platform observer mechanism.  The image library's client
   should create an XP_ObserverList, and pass it into IL_GetImage().  This
   observer list represents the set of client-provided callbacks to be
   associated with the IL_ImageReq handle returned by IL_GetImage.  Callbacks
   will be invoked with an appropriate message, whenever the image library
   wishes to notify its clients of the availability of information about the
   image or other important changes in the image. The observer list's
   XP_Observable will be set by IL_GetImage, so the XP_ObserverList should be
   created with a NULL observable. */
#include "xp_obs.h"


/**************************** Initialization *********************************/
PR_BEGIN_EXTERN_C

/* One-time image library initialization.
   - Initialize internal state.
   - Scan image plug-in directory.
   - Register individual image decoders with the netlib. */
IL_EXTERN(int)
IL_Init(ilISystemServices *ss);

/* Used when exiting the client, this code frees all imagelib data structures.
   This is done for two reasons:
   - It makes leakage analysis of the heap easier.
   - It causes resources to be freed on 16-bit Windows that would otherwise
     persist beyond the program's lifetime. */
IL_EXTERN(void)
IL_Shutdown(void);


/********************** Image Group Contexts *********************************/

/* Create an IL_GroupContext, which represents an aggregation of state
   for one or more images and which contains an interface to access
   external service functions and callbacks.  IL_NewGroupContext will use
   the IMGCBIF_AddRef callback to increment the reference count for the
   interface.

   The display_context argument is opaque to the image library and is
   passed back to all of the callbacks in IMGCBIF interface. */
IL_EXTERN(IL_GroupContext *)
IL_NewGroupContext(void *display_context, 
                   ilIImageRenderer *image_render);

/* Free an image context.  IL_DestroyGroupContext will make a call
   to the IMGCBIF_Release callback function of the JMC interface prior to
   releasing the IL_GroupContext structure.  The IMGCBIF_Release callback
   is expected to decrement the reference count for the IMGCBIF interface,
   and to free the callback vtable and the interface structure if the
   reference count is zero. */
IL_EXTERN(void)
IL_DestroyGroupContext(IL_GroupContext *image_context);

/* Add an observer/closure pair to an image group context's observer list.
   Returns PR_TRUE if the observer is successfully registered. */
IL_EXTERN(PRBool)
IL_AddGroupObserver(IL_GroupContext *img_cx, XP_ObserverProc observer,
                    void *closure);

/* Remove an observer/closure pair from an image group context's observer
   list.  Returns PR_TRUE if successful. */
IL_EXTERN(PRBool)
IL_RemoveGroupObserver(IL_GroupContext *img_cx, XP_ObserverProc observer,
                       void *closure);


/************************* Primary API functions *****************************/

/* This is the primary entry point to the imagelib, used to map from a
   URL to a pixmap, fetching and decoding as necessary.

   The width and height parameters specify the target dimensions of
   the image.  The image will be stretched horizontally, vertically or
   both to meet these parameters.  If both width and height are zero,
   the image is decoded using its "natural" size.  If only one of
   width and height is zero, the image is scaled to the provided
   dimension, with the unspecified dimension scaled to maintain the
   image's original aspect ratio.

   If background_color is NULL, a separate mask pixmap will be
   constructed for any transparent images.  If background_color is
   non-NULL, it indicates the RGB value to be painted into the image
   for "transparent" areas of the image, in which case no mask is
   created.  This is intended for backdrops and printing.

   The observable is an opaque pointer that is passed back to observer
   callback functions.

   The following flags may be set when calling IL_GetImage:
   - IL_HIGH_PRIORITY   - For important images, like backdrops.
   - IL_STICKY          - Don't throw this image out of cache.
   - IL_BYPASS_CACHE    - Don't get image out of image cache.
   - IL_ONLY_FROM_CACHE - Don't load if image cache misses.

   The net_group_context is passed back to netlib functions.  It must
   encapsulate the notion of disk cache reload policy which, in
   previous incarnations of this function, was passed in explicitly.
   There is also an assumption being made here that there is some way to
   create a Net Context from this Net Group Context in which Navigator UI
   (animation, status, progress, etc.) can be suppressed.*/
IL_EXTERN(IL_ImageReq *)
IL_GetImage(const char* url,
            IL_GroupContext *image_context,
            XP_ObserverList observer_list,
            IL_IRGB *background_color,
            PRUint32 width, PRUint32 height,
            PRUint32 flags,
            void *net_context,
            nsIImageRequestObserver * aObserver);

/* Release a reference to an image lib request.  If there are no other
   clients of the request's associated pixmap, any related netlib
   activity is terminated and pixmap storage may be reclaimed. */
IL_EXTERN(void)
IL_DestroyImage (IL_ImageReq *image_req);

/* XXX - This is a new API call to reload all images associated with a
   given IL_GroupContext.  The purpose of this call is to allow all images
   in a document to be reloaded and redecoded to a different bit depth based
   on memory considerations.  This process involves the destruction of the
   old IL_Pixmap structures and the allocation of new structures corresponding
   to the new bit depth. */
IL_EXTERN(void)
IL_ReloadImages(IL_GroupContext *image_context, void *net_context);

/* Halt decoding of images or animation without destroying associated
   pixmap data.  This may abort any associated netlib streams.  All
   IL_ImageReq's created with the given IL_GroupContext are interrupted. */
IL_EXTERN(void)
IL_InterruptContext(IL_GroupContext *image_context);

/* Halt decoding or animation of a specific image request without 
   destroying associated pixmap data. */

IL_EXTERN(void)
IL_InterruptRequest(IL_ImageReq *image_req);

/* Display a rectangular portion of an image.  x and y refer to the top left
   corner of the image, measured in pixels, with respect to the document
   origin.  x_offset and y_offset are measured in pixels, with the
   upper-left-hand corner of the pixmap as the origin, increasing
   left-to-right, top-to-bottom.

   If the width and height values would otherwise cause the sub-image
   to extend off the edge of the source image, the function should
   perform tiling of the source image.  This is used to draw document,
   layer and table cell backdrops.  (Note: it is assumed this case will
   apply only to images which do not require any scaling.)

   If at any time the image library determines that an image request cannot
   be fulfilled or that the image has been delayed, it will notify the client
   synchronously through the observer mechanism.  The client may then choose to
   request that an icon be drawn instead by making a call to IL_DisplayIcon. */
IL_EXTERN(void)
IL_DisplaySubImage(IL_ImageReq *image_req, int x, int y, int x_offset,
                   int y_offset, int width, int height);

/* Display an icon.  x and y refer to the top left corner of the icon, measured
   in pixels, with respect to the document origin. */
IL_EXTERN(void)
IL_DisplayIcon(IL_GroupContext *img_cx, int icon_number, int x, int y);

/* Return the dimensions of an icon. */
IL_EXTERN(void)
IL_GetIconDimensions(IL_GroupContext *img_cx, int icon_number, int *width,
                     int *height);


/********************* Pixmap access functions *******************************/

/* Return the image IL_Pixmap associated with an image request. */
IL_EXTERN(IL_Pixmap *)
IL_GetImagePixmap(IL_ImageReq *image_req);

/* Return the mask IL_Pixmap associated with an image request. */
IL_EXTERN(IL_Pixmap *)
IL_GetMaskPixmap(IL_ImageReq *image_req);


/********************* Image query functions *******************************/

/* Return the natural dimensions of the image.  Returns 0,0 if the dimensions
   are unknown. */
IL_EXTERN(void)
IL_GetNaturalDimensions(IL_ImageReq *image_req, int *width, int *height);


/*********************** Per-context Preferences and Settings ****************/

/* Instruct the Image Library how to render images to the Display Front End.
   The display_flags argument indicates which display settings are to be
   affected.  Flags which may be set include:
   - IL_DISPLAY_CONTEXT     - Set the display context.  Must be compatible with
                              the one used to create the IL_GroupContext.
   - IL_DISPLAY_TYPE        - Set the display type.
   - IL_COLOR_SPACE         - Set the display colorspace.
   - IL_PROGRESSIVE_DISPLAY - Turn progressive display on or off.
   - IL_DITHER_MODE         - Set dither mode.

   The display_data argument provides the required data for the new settings.
   - display_context     - An opaque pointer to the FE's display context.
   - display_type        - IL_Console, IL_Printer, IL_PostScript or
                           IL_MetaFile.
   - color_space         - A pointer to the Display FE's colorspace.
   - progressive_display - Toggle for progressive image display.
   - dither_mode         - IL_ClosestColor, IL_Dither or IL_Auto. */
IL_EXTERN(void)
IL_SetDisplayMode(IL_GroupContext *image_context, PRUint32 display_flags,
                  IL_DisplayData *display_data);


/************************ Image format identification ************************/

/* Determine the type of the image, based on the first few bytes of data. */
IL_EXTERN(int)
IL_Type(const char *buf, int32 len);


/************************ Global imagelib settings ***************************/

/* Set limit on approximate size, in bytes, of all pixmap storage used by the
   Image Library. */
IL_EXTERN(void)
IL_SetCacheSize(PRUint32 new_size);


/************************ Memory management **********************************/

/* Free num_bytes of memory by flushing the Least Recently Used (LRU) images
   from the image cache. */
IL_EXTERN(void)
IL_FreeMemory(IL_GroupContext *image_context, PRUint32 num_bytes);


/********************** Mac-specific memory-management ***********************/

/* Attempt to release the memory used by a specific image in the image
   cache.  The memory won't be released if the image is still in use
   by one or more clients.  XXX - Can we get rid of this call ?  Why
   the hell do we need this ? */
IL_EXTERN(void)
IL_UnCache(IL_Pixmap *pixmap);

/* Attempts to release some memory by freeing an image from the image
   cache.  This may not always be possible either because all images
   in the cache are in use or because the cache is empty.  Returns the
   new approximate size of the imagelib cache. */
IL_EXTERN(PRUint32)
IL_ShrinkCache(void);

/* Remove as many images as possible from the image cache. The only
   images not removed are those that are in use. */
IL_EXTERN(void)
IL_FlushCache(void);

/* Return the approximate storage consumed by the imagelib cache, in bytes */
IL_EXTERN(PRUint32)
IL_GetCacheSize(void);


/************************ Miscellaneous garbage ******************************/

/* Returns a pointer to a string containing HTML appropriate for displaying
   in a DocInfo window.  The caller may dispose of the string using XP_FREE. */
IL_EXTERN(char *)
IL_HTMLImageInfo(char *url_address);

/* This is a legacy "safety-valve" routine, called each time a new HTML page
   is loaded.  It causes remaining references to images in the given group
   context to be freed, i.e. like calling IL_DestroyImage on each of them.
   This is primarily required because layout sometimes leaks images, and it
   should go away when we can fix layout. */
IL_EXTERN(void)
IL_DestroyImageGroup(IL_GroupContext *image_context);

PR_END_EXTERN_C
#endif /* _LIBIMG_H */



