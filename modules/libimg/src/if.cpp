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
 *   if.c --- Top-level image library routines
 */

#include "if.h"

#include "merrors.h"

#include "nsIImgDecoder.h"
#include "nsImgDCallbk.h"
#include "xpcompat.h"

#include "nsImgDecCID.h"
#include "prtypes.h"


#include "il_strm.h"


PR_BEGIN_EXTERN_C
extern int XP_MSG_IMAGE_PIXELS;
extern int MK_UNABLE_TO_LOCATE_FILE;
extern int MK_OUT_OF_MEMORY;
PR_END_EXTERN_C

#define HOWMANY(x, r)     (((x) + ((r) - 1)) / (r))
#define ROUNDUP(x, r)     (HOWMANY(x, r) * (r))

#ifdef PROFILE
#pragma profile on
#endif

int il_debug=0;

/* Global list of image group contexts. */
static IL_GroupContext *il_global_img_cx_list = NULL;


/*-------------------------------------------------*/
class ImgDecoder : public nsIImgDecoder {
	
public:

  NS_DECL_ISUPPORTS
  
  il_container *GetContainer() {return mContainer;};
  il_container *SetContainer(il_container *ic) {mContainer=ic; return ic;};

  ImgDecoder(il_container *aContainer){mContainer=aContainer;};
  ~ImgDecoder();

private:
  il_container* mContainer;
};

NS_IMETHODIMP ImgDecoder::AddRef()
{
  NS_INIT_REFCNT();
  return NS_OK;
}

NS_IMETHODIMP ImgDecoder::Release()
{
  return NS_OK;
}

NS_IMETHODIMP ImgDecoder::QueryInterface(const nsIID& aIID, void** aResult)
{   	
  if (NULL == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
   
  if (aIID.Equals(kImgDecoderIID)) {
	  *aResult = (void*) this;
    NS_INIT_REFCNT();
    return NS_OK;
  }
  
  return NS_NOINTERFACE;
}

/*-----------------------------------------*/
/*-----------------------------------------*/
NS_IMETHODIMP ImgDCallbk::ImgDCBSetupColorspaceConverter()
{  
  if( mContainer != NULL ) {
    il_setup_color_space_converter(mContainer);
  }
  return 0;
}

NI_ColorSpace*
ImgDCallbk::ImgDCBCreateGreyScaleColorSpace()
{
  if( mContainer != NULL ) {
    return IL_CreateGreyScaleColorSpace(1,1);
  }
  return 0;
}
  
NS_IMETHODIMP ImgDCallbk::ImgDCBResetPalette()
{
   if( mContainer != NULL ) {
       il_reset_palette(mContainer);
  }
  return NS_OK;
}


NS_IMETHODIMP
ImgDCallbk::ImgDCBHaveHdr(int destwidth, int destheight)
{
  if( mContainer != NULL ) {
    il_dimensions_notify(mContainer, destwidth, destheight);
  }
  return 0;

}

NS_IMETHODIMP
ImgDCallbk::ImgDCBInitTransparentPixel()
{
    if( mContainer != NULL ) {
      il_init_image_transparent_pixel(mContainer);
  }
  return 0;

}

NS_IMETHODIMP
ImgDCallbk::ImgDCBDestroyTransparentPixel()
{
    if( mContainer != NULL ) {
      il_destroy_image_transparent_pixel(mContainer);
  }
  return 0;

}

NS_IMETHODIMP 
ImgDCallbk :: ImgDCBHaveRow(uint8 *rowbuf, uint8* rgbrow, 
                            int x_offset, int len,
                            int row, int dup_rowcnt, 
                            uint8 draw_mode, 
                            int pass )
{
  if( mContainer != NULL ) {
    
  	il_emit_row(mContainer, rowbuf, rgbrow, x_offset, len,
                       row, dup_rowcnt, (il_draw_mode)draw_mode, pass );
  }
  return 0;

}

NS_IMETHODIMP 
ImgDCallbk :: ImgDCBHaveImageFrame()
{
  if( mContainer != NULL ) {
         il_frame_complete_notify(mContainer);
  }
  return 0;
}

NS_IMETHODIMP 
ImgDCallbk::ImgDCBFlushImage()
{
  if( mContainer != NULL ) {
	  il_flush_image_data(mContainer);
  }
  return 0;
}

NS_IMETHODIMP 
ImgDCallbk:: ImgDCBImageSize()
{ 
  if( mContainer != NULL ) {
    il_size(mContainer);
  }
  return 0;
}

NS_IMETHODIMP 
ImgDCallbk :: ImgDCBHaveImageAll()
{
  if( mContainer != NULL ) {
    il_image_complete(mContainer);
  }
  return 0;

}

NS_IMETHODIMP 
ImgDCallbk :: ImgDCBError()
{
  if( mContainer != NULL ) {
  }
  return 0;

}	

void*
ImgDCallbk :: ImgDCBSetTimeout(TimeoutCallbackFunction func, void* closure, uint32 msecs)
{
  return( IL_SetTimeout(func, closure, msecs ));

}

NS_IMETHODIMP
ImgDCallbk :: ImgDCBClearTimeout(void *timer_id)
{
  IL_ClearTimeout(timer_id);
  return 0;
}

/*-------------------------------------------------*/
ImgDecoder *imgdec;
/*********************** Image Observer Notification. *************************
*
* These functions are used to send messages to registered observers of an
* individual image client i.e. an IL_ImageReq.
*
******************************************************************************/

/* Fabricate a descriptive title for the image and notify observers.  Used for
   calls to the image viewer. */
static void
il_description_notify(il_container *ic)
{
    char buf[36], buf2[12];
    IL_MessageData message_data;
    IL_ImageReq *image_req;
    NI_PixmapHeader *img_header = &ic->image->header;
    
    XP_BZERO(&message_data, sizeof(IL_MessageData));

    switch (ic->type) {
        /* XXX - This needs to be fixed for Image Plugins. */
    case IL_GIF : PL_strcpy(buf2, "GIF"); break;
    case IL_XBM : PL_strcpy(buf2, "XBM"); break;
    case IL_JPEG : PL_strcpy(buf2, "JPEG"); break;
    case IL_PNG : PL_strcpy(buf2, "PNG"); break;

    default : PL_strcpy(buf2, "");
    }
    XP_SPRINTF(buf, XP_GetString(XP_MSG_IMAGE_PIXELS), buf2,
               img_header->width, img_header->height);

	PR_ASSERT(ic->clients);
	for (image_req = ic->clients; image_req; image_req = image_req->next) {
        if (image_req->is_view_image) {
            message_data.image_instance = image_req;
            message_data.description = buf; /* Note scope limitation.  Observer
                                               must copy if necessary. */
            XP_NotifyObservers(image_req->obs_list, IL_DESCRIPTION, &message_data);
        }
    }
}

/* Notify observers of the target dimensions of the image. */
void
il_dimensions_notify(il_container *ic, int dest_width, int dest_height)
{
    IL_MessageData message_data;
    IL_ImageReq *image_req;
    
    XP_BZERO(&message_data, sizeof(IL_MessageData));

	PR_ASSERT(ic->clients);
    for (image_req = ic->clients; image_req; image_req = image_req->next) {
        message_data.image_instance = image_req;
        message_data.width = dest_width;   /* Note: these are stored as */
        message_data.height = dest_height; /* uint16s. */
        XP_NotifyObservers(image_req->obs_list, IL_DIMENSIONS, &message_data);
    }
}

/* Notify observers that the image is transparent and has a mask. */
static void
il_transparent_notify(il_container *ic)
{
    IL_MessageData message_data;
    IL_ImageReq *image_req;
    
    XP_BZERO(&message_data, sizeof(IL_MessageData));

	PR_ASSERT(ic->clients);
    for (image_req = ic->clients; image_req; image_req = image_req->next) {
        message_data.image_instance = image_req;
        XP_NotifyObservers(image_req->obs_list, IL_IS_TRANSPARENT,
                           &message_data);
    }
}

/* Notify observers that the image pixmap has been updated. */
void
il_pixmap_update_notify(il_container *ic)
{
    IL_MessageData message_data;
    IL_Rect *update_rect = &message_data.update_rect;
    IL_ImageReq *image_req;

    XP_BZERO(&message_data, sizeof(IL_MessageData));

    update_rect->x_origin = 0;
    update_rect->y_origin = (uint16)ic->update_start_row;
    update_rect->width = (uint16)ic->image->header.width;
    update_rect->height = (uint16)(ic->update_end_row-ic->update_start_row+1);

	PR_ASSERT(ic->clients);
    for(image_req = ic->clients; image_req; image_req = image_req->next) {
        /* The user aborted the image loading.  Don't display any more image
           data */
        if (image_req->stopped)
            continue;

        message_data.image_instance = image_req;
        XP_NotifyObservers(image_req->obs_list, IL_PIXMAP_UPDATE,
                           &message_data);
    }
}

/* Notify observers that the image has finished decoding. */
void
il_image_complete_notify(il_container *ic)
{
    IL_MessageData message_data;
    IL_ImageReq *image_req;
    
    XP_BZERO(&message_data, sizeof(IL_MessageData));

#if !defined(STANDALONE_IMAGE_LIB)
	PR_ASSERT(ic->clients);
#endif
    for (image_req = ic->clients; image_req; image_req = image_req->next) {
        message_data.image_instance = image_req;
        XP_NotifyObservers(image_req->obs_list, IL_IMAGE_COMPLETE,
                           &message_data);
    }
}
#if 1
/* Notify observers that a frame of an image animation has finished
   decoding. */
void
il_frame_complete_notify(il_container *ic)
{
    IL_MessageData message_data;
    IL_ImageReq *image_req;
    
    XP_BZERO(&message_data, sizeof(IL_MessageData));

#if !defined(STANDALONE_IMAGE_LIB)
	PR_ASSERT(ic->clients);
#endif
    for (image_req = ic->clients; image_req; image_req = image_req->next) {
        message_data.image_instance = image_req;
        XP_NotifyObservers(image_req->obs_list, IL_FRAME_COMPLETE,
                           &message_data);
    }
}
#endif

int
il_compute_percentage_complete(int row, il_container *ic)
{
    uint percent_height;
    int percent_done = 0;
    
    percent_height = (uint)(row * (uint32)100 / ic->image->header.height);
    switch(ic->pass) {
    case 0: percent_done = percent_height; /* non-interlaced GIF */
        break;
    case 1: percent_done =      percent_height / 8;
        break;
    case 2: percent_done = 12 + percent_height / 8;
        break;
    case 3: percent_done = 25 + percent_height / 4;
        break;
    case 4: percent_done = 50 + percent_height / 2;
        break;
    default:
        ILTRACE(0,("Illegal interlace pass"));
        break;
    }

    return percent_done;
}

/* Notify observers of image progress. */
void
il_progress_notify(il_container *ic)
{
    uint percent_done;
    static uint last_percent_done = 0;
    int row = ic->update_end_row;
    IL_MessageData message_data;
    IL_ImageReq *image_req;
    NI_PixmapHeader *img_header = &ic->image->header;
    
    XP_BZERO(&message_data, sizeof(IL_MessageData));

    /* No more progress bars for GIF animations after initial load. */
    if (ic->is_looping)
        return;

    /* Calculate the percentage of image decoded (not displayed) */
    if(ic->content_length) {
        percent_done =
            (uint32)100 * ic->bytes_consumed / ((uint32)ic->content_length);

    /* Some protocols, e.g. gopher, don't support content-length, so
     * show the percentage of the image displayed instead
     */
    } else {
        /* Could be zero if before il_size() is called. */
        if (img_header->height == 0)
            return;

        /* Interlaced GIFs are weird */
        if (ic->type == IL_GIF) {
            percent_done = il_compute_percentage_complete(row, ic);
        }
        else
        {
            /* This isn't correct for progressive JPEGs, but there's
             * really nothing we can do about that because the number
             * of scans in a progressive JPEG isn't known until the
             * whole file has been read.
             */
            percent_done = (uint)(row * (uint32)100 / img_header->height);

        }
    }

    if (percent_done != last_percent_done) {
        PR_ASSERT(ic->clients);
        for (image_req = ic->clients; image_req; image_req = image_req->next) {
            if (image_req->is_view_image) { /* XXXM12N Eliminate this test? */
                message_data.image_instance = image_req;
                message_data.percent_progress = percent_done;
                
                XP_NotifyObservers(image_req->obs_list, IL_PROGRESS,
                                   &message_data);
            }
        }
        last_percent_done = percent_done;
    }
}


/* Notify observers of information pertaining to a cached image.  Note that
   this notification is per image request and not per image container. */
void
il_cache_return_notify(IL_ImageReq *image_req)
{
    IL_MessageData message_data;
    il_container *ic = image_req->ic;

    XP_BZERO(&message_data, sizeof(IL_MessageData));
    message_data.image_instance = image_req;

    /* This function should only be called if the dimensions are known. */
    PR_ASSERT(ic->state >= IC_SIZED);

    /* First notify observers of the image dimensions. */
    message_data.width = ic->dest_width;   /* Note: these are stored as */
    message_data.height = ic->dest_height; /* uint16s. */
    XP_NotifyObservers(image_req->obs_list, IL_DIMENSIONS, &message_data);
    message_data.width = message_data.height = 0;

    /* Fabricate a title for the image and notify observers.  Image Plugins
       will need to supply information on the image type. */
    il_description_notify(ic);

    /* If the image is transparent and has a mask, notify observers
       accordingly. */
    if (ic->mask)
        XP_NotifyObservers(image_req->obs_list, IL_IS_TRANSPARENT,
                           &message_data);

    /* Now send observers a pixmap update notification for the displayable
       area of the image. */
    XP_BCOPY(&ic->displayable_rect, &message_data.update_rect,
             sizeof(IL_Rect));
    XP_NotifyObservers(image_req->obs_list, IL_PIXMAP_UPDATE, &message_data);
    XP_BZERO(&message_data.update_rect, sizeof(IL_Rect));

    if (ic->state == IC_COMPLETE) {
        /* Send the observers a frame complete message. */
        XP_NotifyObservers(image_req->obs_list, IL_FRAME_COMPLETE,
                           &message_data);

        /* Finally send the observers a complete message. */
        XP_NotifyObservers(image_req->obs_list, IL_IMAGE_COMPLETE,
                           &message_data);
    }
}


/* Notify observers that the image request is being destroyed.  This
   provides an opportunity for observers to remove themselves from the
   observer callback list and to do any related cleanup. */
void
il_image_destroyed_notify(IL_ImageReq *image_req)
{
    IL_MessageData message_data;
    
    XP_BZERO(&message_data, sizeof(IL_MessageData));
    message_data.image_instance = image_req;
    XP_NotifyObservers(image_req->obs_list, IL_IMAGE_DESTROYED, &message_data);

    /* Now destroy the observer list itself. */
    XP_DisposeObserverList(image_req->obs_list);
}


/* Notify observers that an error has occured.  The observer should call
   IL_DisplaySubImage in order to display the icon.  Note that this function
   is called per image request and not per image. */
static void
il_icon_notify(IL_ImageReq *image_req, int icon_number,
               XP_ObservableMsg message)
{
    IL_GroupContext *img_cx = image_req->img_cx;
    int icon_width, icon_height;
    IL_MessageData message_data;

    XP_BZERO(&message_data, sizeof(IL_MessageData));
  
    /* Obtain the dimensions of the icon. */
#ifdef STANDALONE_IMAGE_LIB
    img_cx->img_cb->GetIconDimensions(img_cx->dpy_cx, &icon_width,
                                      &icon_height, icon_number);
#else
    IMGCBIF_GetIconDimensions(img_cx->img_cb, img_cx->dpy_cx, &icon_width,
                              &icon_height, icon_number);
#endif /* STANDALONE_IMAGE_LIB */

    /* Fill in the message data and notify observers. */
    message_data.image_instance = image_req;
    message_data.icon_number = icon_number;
    message_data.icon_width = icon_width;
    message_data.icon_height = icon_height;
    XP_NotifyObservers(image_req->obs_list, message, &message_data);
}

/********************* Image Group Observer Notification. *********************
*
* This function is used to send messages to registered observers of an image
* group i.e. an IL_GroupContext.
*
******************************************************************************/

/* Notify observers that images have started/stopped loading in the context,
   or started/stopped looping in the context. */
void
il_group_notify(IL_GroupContext *img_cx, XP_ObservableMsg message)
{
    IL_GroupMessageData message_data;
    
    XP_BZERO(&message_data, sizeof(IL_GroupMessageData));
    
    /* Fill in the message data and notify observers. */
    message_data.display_context = img_cx->dpy_cx;
    message_data.image_context = img_cx;
    XP_NotifyObservers(img_cx->obs_list, message, &message_data);
}


static int
il_init_scaling(il_container *ic)
{
    int scaled_width = ic->image->header.width;
        
    /* Allocate temporary scale space */
    if (scaled_width != (int)ic->src_header->width)
	{
        PR_FREEIF(ic->scalerow);
        
        if (!(ic->scalerow = (unsigned char *)PR_MALLOC(scaled_width * 3)))
		{
            ILTRACE(0,("il: MEM scale row"));
            return MK_OUT_OF_MEMORY;
        }
    }
    return 0;
}

/* Allocate and initialize the destination image's transparent_pixel with
   the Image Library's preferred transparency color i.e. the background color
   passed into IL_GetImage.  The image decoder is encouraged to use this
   color, but may opt not to do so. */
int
il_init_image_transparent_pixel(il_container *ic)
{
    IL_IRGB *img_trans_pixel = ic->image->header.transparent_pixel;
    
    if (!img_trans_pixel) {
        img_trans_pixel = PR_NEWZAP(IL_IRGB);
        if (!img_trans_pixel)
            return FALSE;

        if (ic->background_color) {
            XP_MEMCPY(img_trans_pixel, ic->background_color, sizeof(IL_IRGB));
        }
        else {
            /* A mask will always be used if no background color was
               requested. */
        }
            
        ic->image->header.transparent_pixel = img_trans_pixel;
    }

    return TRUE;
}

/* Destroy the destination image's transparent pixel. */
void
il_destroy_image_transparent_pixel(il_container *ic)
{
    NI_PixmapHeader *img_header = &ic->image->header;

    PR_FREEIF(img_header->transparent_pixel);
    img_header->transparent_pixel = NULL;
}

/* Inform the Image Library of the source image's dimensions.  This function
   determines the size of the destination image, and calls the front end to
   allocate storage for the destination image's bits.  If the source image's
   transparent pixel is set, and a background color was not specified for this
   image request, then a mask will also be allocated for the destination
   image. */
int
il_size(il_container *ic)
{
    float aspect;
    int status;
    uint8 img_depth;
    uint32 src_width, src_height;
    int32 image_bytes, old_image_bytes;
    IL_GroupContext *img_cx = ic->img_cx;
    NI_PixmapHeader *src_header = ic->src_header; /* Source image header. */
    NI_PixmapHeader *img_header = &ic->image->header; /* Destination image
                                                         header. */
    uint32 req_w=0, req_h=0;  /* store requested values for printing.*/

    
    /* Get the dimensions of the source image. */
    src_width = src_header->width;
    src_height = src_header->height;

	/* Ensure that the source image has a sane area. */
	if (!src_width || !src_height            ||
        (src_width > MAX_IMAGE_WIDTH)        ||
        (src_height > MAX_IMAGE_HEIGHT)) {            
		ILTRACE(1,("il: bad image size: %dx%d", src_width, src_height));
		return MK_IMAGE_LOSSAGE;
	}

	ic->state = IC_SIZED;
	if (ic->state == IC_MULTI)
		return 0;

    if(ic->display_type == IL_Printer){
		req_w = ic->dest_width;
		req_h = ic->dest_height;
		ic->dest_width = 0;   /*to decode natural size*/
		ic->dest_height = 0;
	}

    /* For now, we don't allow an image to change output size on the
     * fly, but we do allow the source image size to change, and thus
     * we may need to change the scaling factor to fit it into the
     * same size container on the display.
     */
    if (ic->sized) {
        status = il_init_scaling(ic);
        return status;
    }

    /* This must appear before il_init_img_header. */
    old_image_bytes = (int32)img_header->widthBytes * img_header->height;
    
    /* Initialize the dimensions of the destination image header structure
       to be the dimensions of the source image.  The Display Front End will
       reset these dimensions to be the target dimensions of the image if
       it does not handle scaling. */
    img_header->width = src_width;
    img_header->height = src_height;

    /* Determine the target dimensions of the image.  */
    aspect = (float)src_width/(float)src_height;
    if (!ic->dest_width && !ic->dest_height) {
        /* Both target dimensions were unspecified, so use the dimensions of
           the source image. */
        ic->dest_width  = src_width;
        ic->dest_height = src_height;
    }
    else if (ic->dest_width && ic->dest_height) {
        /* Both target dimensions were specified; determine if this causes
           aspect ratio distortion. */
        if (ic->dest_width * src_height != ic->dest_height * src_width)
            ic->aspect_distorted = PR_TRUE;
    }
    else if (ic->dest_width) {
        /* Only the target width was specified.  Determine the target height
           which preserves aspect ratio. */
        ic->dest_height = (int)((float)ic->dest_width / aspect + 0.5);
    }
    else {
        /* Only the target height was specified.  Determine the target width
           which preserves aspect ratio. */
        ic->dest_width  = (int)(aspect * (float)ic->dest_height + 0.5);
    }
    if (ic->dest_width == 0) ic->dest_width = 1;
    if (ic->dest_height == 0) ic->dest_height = 1;

    /* Determine if the image will be displayed at its natural size. */
    if (ic->dest_width == (int)src_width && ic->dest_height == (int)src_height)
        ic->natural_size = PR_TRUE;

    /* Check that the target image dimensions are reasonable. */
    if ((ic->dest_width > MAX_IMAGE_WIDTH)     ||
        (ic->dest_height > MAX_IMAGE_HEIGHT)) {
        return MK_IMAGE_LOSSAGE;
    }

	ILTRACE(2,("il: size %dx%d, scaled from %dx%d, ic = 0x%08x\n",
               ic->dest_width, ic->dest_height, src_width, src_height, ic));

    /* Determine the number of bytes per scan-line.  Image data must be
       quadlet aligned for optimizations. */
    img_depth = img_header->color_space->pixmap_depth;
    img_header->widthBytes = (img_header->width * img_depth + 7) / 8;
    img_header->widthBytes = ROUNDUP(img_header->widthBytes, 4);

    /* Create and initialize the mask pixmap structure, if required.  A mask
       is allocated only if the image is transparent and no background color
       was specified for this image request. */
    if (src_header->transparent_pixel && !ic->background_color) {
        if (!ic->mask) {
            NI_PixmapHeader *mask_header;
    
            if (!(ic->mask = PR_NEWZAP(IL_Pixmap))) {
                return MK_OUT_OF_MEMORY;
            }

            mask_header = &ic->mask->header;
            mask_header->color_space = IL_CreateGreyScaleColorSpace(1, 1);
            if (!mask_header->color_space)
                return MK_OUT_OF_MEMORY;
            mask_header->width = img_header->width;
            mask_header->height = img_header->height;
            mask_header->widthBytes = (mask_header->width + 7) / 8;

            /* Mask data must be quadlet aligned for optimizations */
            mask_header->widthBytes = ROUNDUP(mask_header->widthBytes, 4);
        }

        /* Notify observers that the image is transparent and has a mask. */
        il_transparent_notify(ic);
    }
    else {                      /* Mask not required. */

        /*  for png alpha*/
        if (ic->mask) {
                il_transparent_notify(ic);
                if(ic->background_color){
/*
                    src_header->transparent_pixel = ic->background_color;
                    img_header->transparent_pixel = ic->background_color;
               
                    XP_MEMCPY(img_trans_pixel, ic->background_color, sizeof(IL_IRGB)); 
*/
        }
/*
            il_destroy_pixmap(ic->img_cb, ic->mask);
            ic->mask = NULL;
*/
        }
        
    }

    ic->sized = 1;
    /* Notify observers of the target dimensions of the image. */
     ic->imgdcb->ImgDCBHaveHdr(ic->dest_width, ic->dest_height);

    /* Fabricate a title for the image and notify observers.  Image Plugins
       will need to supply information on the image type. */
    il_description_notify(ic);


    /* If the display front-end doesn't support scaling, IMGCBIF_NewPixmap will
       set the image and mask dimensions to scaled_width and scaled_height. */
#ifdef STANDALONE_IMAGE_LIB
    img_cx->img_cb->NewPixmap(img_cx->dpy_cx, ic->dest_width,
                              ic->dest_height, ic->image, ic->mask);
#else
    IMGCBIF_NewPixmap(img_cx->img_cb, img_cx->dpy_cx, ic->dest_width,
                      ic->dest_height, ic->image, ic->mask);
#endif /* STANDALONE_IMAGE_LIB */
    
	if (!ic->image->bits)
		return MK_OUT_OF_MEMORY;

	if (ic->mask && !ic->mask->bits)
		return MK_OUT_OF_MEMORY;

	/* Adjust the total cache byte count to reflect any departure from the
       original predicted byte count for this image. */
    image_bytes = (int32)img_header->widthBytes * img_header->height;
    if (image_bytes - old_image_bytes)
        il_adjust_cache_fullness(image_bytes - old_image_bytes);

    /* If we have a mask, initialize its bits. */
    if (ic->mask) {
        NI_PixmapHeader *mask_header = &ic->mask->header;
        uint32 mask_size = mask_header->widthBytes * mask_header->height;

#ifdef STANDALONE_IMAGE_LIB
        img_cx->img_cb->ControlPixmapBits(img_cx->dpy_cx, ic->mask,
                                          IL_LOCK_BITS);
#else
        IMGCBIF_ControlPixmapBits(img_cx->img_cb, img_cx->dpy_cx, ic->mask,
                                  IL_LOCK_BITS);
#endif /* STANDALONE_IMAGE_LIB */
                                
        memset (ic->mask->bits, ~0, mask_size);
        
#ifdef STANDALONE_IMAGE_LIB
        img_cx->img_cb->ControlPixmapBits(img_cx->dpy_cx, ic->mask,
                                          IL_UNLOCK_BITS);
#else
        IMGCBIF_ControlPixmapBits(img_cx->img_cb, img_cx->dpy_cx, ic->mask,
                                  IL_UNLOCK_BITS);
#endif /* STANDALONE_IMAGE_LIB */
    }
    
    if ((status = il_init_scaling(ic)) < 0)
        return status;


    /* XXXM12N Clean this up */
	/* Get memory for quantizing.  (required amount depends on image width) */
	if (img_header->color_space->type == NI_PseudoColor) {
		if (!il_init_quantize(ic)) {
			ILTRACE(0,("il: MEM il_init_quantize"));
			return MK_OUT_OF_MEMORY;
		}
	}

    if((ic->display_type == IL_Printer)&& (req_w || req_h)){
		ic->dest_width = req_w;
		ic->dest_height = req_h;
	}

	return 0;
}


#ifdef XP_OS2
#define IL_SIZE_CHUNK   16384
#else
#define IL_SIZE_CHUNK	128
#endif
#ifdef XP_MAC
#    if GENERATINGPOWERPC
#        define IL_PREFERRED_CHUNK 8192
#        define IL_OFFSCREEN_CHUNK 128
#    else	/* normal mac */
#        define IL_PREFERRED_CHUNK 4096
#        define IL_OFFSCREEN_CHUNK 128
#    endif
#else /* !XP_MAC */
#ifdef XP_OS2
#    define IL_PREFERRED_CHUNK (12*1024)
#    define IL_OFFSCREEN_CHUNK ( 6*1024)
#else
#    define IL_PREFERRED_CHUNK  2048 
#    define IL_OFFSCREEN_CHUNK 128 
#endif
#endif

unsigned int
IL_StreamWriteReady(il_container *ic)
{
    uint request_size = 1;

    if (ic->imgdec)
  //      request_size = (*ic->write_ready)();
    request_size = ic->imgdec->ImgDWriteReady();

    if (!request_size)
        return 0;

	/*
     * It could be that layout aborted image loading by calling IL_FreeImage
     * before the netlib finished transferring data.  Don't do anything.
     */
	if (ic->state == IC_ABORT_PENDING)
        return IL_OFFSCREEN_CHUNK;

	if (!ic->sized)
		/* A (small) default initial chunk */
		return IL_SIZE_CHUNK;

    return IL_PREFERRED_CHUNK;
}

/* Given the first few bytes of a stream, identify the image format */
static int
il_type(int suspected_type, const char *buf, int32 len)
{
	int i;

	if (len >= 4 && !strncmp(buf, "GIF8", 4)) 
	{
		return IL_GIF;
	}

  	/* for PNG */
	if (len >= 4 && ((unsigned char)buf[0]==0x89 &&
		             (unsigned char)buf[1]==0x50 &&
					 (unsigned char)buf[2]==0x4E &&
					 (unsigned char)buf[3]==0x47))
	{ 
		return IL_PNG;
	}


	/* JFIF files start with SOI APP0 but older files can start with SOI DQT
	 * so we test for SOI followed by any marker, i.e. FF D8 FF
	 * this will also work for SPIFF JPEG files if they appear in the future.
	 *
	 * (JFIF is 0XFF 0XD8 0XFF 0XE0 <skip 2> 0X4A 0X46 0X49 0X46 0X00)
     */
	if (len >= 3 &&
	   ((unsigned char)buf[0])==0xFF &&
	   ((unsigned char)buf[1])==0xD8 &&
	   ((unsigned char)buf[2])==0xFF)
	{
		return IL_JPEG;
	}

	/* no simple test for XBM vs, say, XPM so punt for now */
	if (len >= 8 && !strncmp(buf, "#define ", 8) ) 
	{
        /* Don't contradict the given type, since this ID isn't definitive */
        if ((suspected_type == IL_UNKNOWN) || (suspected_type == IL_XBM))
            return IL_XBM;
	}

	if (len < 35) 
	{
		ILTRACE(1,("il: too few bytes to determine type"));
		return suspected_type;
	}

	/* all the servers return different formats so root around */
	for (i=0; i<28; i++)
	{
		if (!strncmp(&buf[i], "Not Fou", 7))
			return IL_NOTFOUND;
	}
	
	return suspected_type;
}

/*
 *	determine what kind of image data we are dealing with
 */
IL_IMPLEMENT(int)
IL_Type(const char *buf, int32 len)
{
    return il_type(IL_UNKNOWN, buf, len);
}

int 
IL_StreamWrite(il_container *ic, const unsigned char *str, int32 len)
{
	unsigned int err = 0;

    ILTRACE(4, ("il: write with %5d bytes for %s\n", len, ic->url_address));

    /*
     * Layout may have decided to abort this image in mid-stream,
     * but netlib doesn't know about it yet and keeps sending
     * us data.  Force the netlib to abort.
     */
    if (ic->state == IC_ABORT_PENDING)
        return -1;

    /* Has user hit the stop button ? */
    if (il_image_stopped(ic))
        return -1;

    ic->bytes_consumed += len;
    
	if (len)

	//	err = (*ic->write)(ic, (unsigned char *)str, len);

		err = ic->imgdec->ImgDWrite(str,  len);



      /* Notify observers of image progress. */
    il_progress_notify(ic);

	if (err < 0)
		return err;
	else
		return len;
}


int
IL_StreamFirstWrite(il_container *ic, const unsigned char *str, int32 len)
{
  int ret =0;

	PR_ASSERT(ic);
	PR_ASSERT(ic->image);

    /* If URL redirection occurs, the url stored in the
    image container is the redirect url not the image file url.
    If the image is animated, the imglib will never match the
    file name in the cache unless you update ic->url_address.
    ic->fetch_url keeps the actual url for you.
     */	

    FREE_IF_NOT_NULL(ic->fetch_url);

    if((ic->url)&& ic->url->GetAddress()){
	    ic->fetch_url = PL_strdup(ic->url->GetAddress());
    }
    else{
	if(ic->url_address) /* check needed because of mkicons.c */
        	ic->fetch_url = PL_strdup(ic->url_address);
	else
		ic->fetch_url = NULL;
    }
 
    /* Figure out the image type, possibly overriding the given MIME type */
    ic->type = il_type(ic->type, (const char*) str, len);


	/* Grab the URL's expiration date */
	nsresult result;

	if (ic->url)
	  ic->expires = ic->url->GetExpires();


  ImgDecoder *imgdec;	

  char imgtype[150];
  char imgtypestr[200];

  switch (ic->type) {
    case IL_GIF : PL_strcpy(imgtype, "gif"); break;
    case IL_XBM : PL_strcpy(imgtype, "xbm"); break;
    case IL_JPEG : PL_strcpy(imgtype, "jpeg"); break;
    case IL_PNG : PL_strcpy(imgtype, "png"); break;
    default : PL_strcpy(imgtype, "");
    }

    sprintf(imgtypestr, "component://netscape/image/decoder&type=image/%s"
            , imgtype );
 
  result = nsRepository::CreateInstance(imgtypestr,
                                           NULL,    
                                           kImgDecoderIID,
                                       (void **)&imgdec);

  if (NS_FAILED(result))
    return MK_IMAGE_LOSSAGE;
  
  
  imgdec->SetContainer(ic);
  ic->imgdec = imgdec;
  ret = imgdec->ImgDInit();
  if(ret == 0)
  {
    ILTRACE(0,("il: image init failed"));
    return MK_OUT_OF_MEMORY;
  }
		
	return 0;
}


/* Called when a container is aborted by Netlib. */
static void
il_container_aborted(il_container *ic)
{
    IL_ImageReq *image_req;
    IL_GroupContext *img_cx;
    il_context_list *current;

    /* Keep track of the fact that this container was aborted by Netlib,
       and guard against multiple calls to this routine. */
    if (ic->is_aborted)
        return;
    ic->is_aborted = PR_TRUE;

    /* Notify image observers that the image was aborted. */
    for (image_req = ic->clients; image_req; image_req = image_req->next) {
        il_icon_notify(image_req, IL_IMAGE_DELAYED, IL_ABORTED);
    }

    /* Now handle image group observers. */
    for (current = ic->img_cx_list; current; current = current->next) {
        /* Increment the count of images in this context which were aborted
           by Netlib.  Observer notification takes place if there were
           previously no aborted images in this context. */
        img_cx = current->img_cx;
        if (!img_cx->num_aborted)
            il_group_notify(img_cx, IL_ABORTED_LOADING);
        img_cx->num_aborted++;

#ifdef DEBUG_GROUP_OBSERVER
        ILTRACE(1,("1 img_cx=%x total=%d loading=%d looping=%d aborted=%d",
                  img_cx, img_cx->num_containers, img_cx->num_loading,
                  img_cx->num_looping, img_cx->num_aborted));
#endif /* DEBUG_GROUP_OBSERVER */
    }
}


static void
il_bad_container(il_container *ic)
{
      IL_ImageReq *image_req;

      ILTRACE(4,("il: bad container, sending icon"));
      if (ic->type == IL_NOTFOUND) {
          ic->state = IC_MISSING;
          for (image_req = ic->clients; image_req; image_req = image_req->next)
              il_icon_notify(image_req, IL_IMAGE_NOT_FOUND, IL_ERROR_NO_DATA);
      }
      else {
          if (ic->state == IC_INCOMPLETE) {
              il_container_aborted(ic);
          }
          else {
              for (image_req = ic->clients; image_req;
                   image_req = image_req->next)
                  il_icon_notify(image_req, IL_IMAGE_BAD_DATA,
                                 IL_ERROR_IMAGE_DATA_TRUNCATED);
          }
      }

      /* Image container gets deleted later. */
}

/* IL_GetURL completion callback. */
void
IL_NetRequestDone(il_container *ic, ilIURL *url, int status)
{
	IL_ImageReq *image_req;
	PR_ASSERT(ic);

	/* Record the fact that NetLib is done loading. */
    if (ic->url == url) {
	    ic->is_url_loading = PR_FALSE;
    }
    /* The (ic->url == url) check is for a weird timer issue, see the comment at the
       end of this function. */    

    /*
     * It could be that layout aborted image loading by calling IL_DestroyImage
     * before the netlib finished transferring data.  If so, really do the
     * freeing of the data that was deferred there.
     */
	if (ic->state == IC_ABORT_PENDING) {
		il_delete_container(ic);
        NS_RELEASE(url);
        return;
    }
    
	if (status < 0) {
		ILTRACE(2,("il:net done ic=0x%08x, status=%d, ic->state=0x%02x\n",
                   ic, status, ic->state));

        /* Netlib detected failure before a stream was even created. */
		if (ic->state < IC_BAD)	{
			if (status == MK_OBJECT_NOT_IN_CACHE)
				ic->state = IC_NOCACHE;
			else if (status == MK_UNABLE_TO_LOCATE_FILE)
                ic->state = IC_MISSING;
            else {
                /* status is probably MK_INTERRUPTED */
				ic->state = IC_INCOMPLETE; /* try again on reload */
            }

			if (!ic->sized)	{
                if (status == MK_OBJECT_NOT_IN_CACHE) {
                    for (image_req = ic->clients; image_req;
                         image_req = image_req->next)
                        il_icon_notify(image_req, IL_IMAGE_DELAYED,
                                       IL_NOT_IN_CACHE); 
                }
                else if (status == MK_INTERRUPTED) {
                    il_container_aborted(ic);
                }
                else if (status == MK_UNABLE_TO_LOCATE_FILE) {
                    for (image_req = ic->clients; image_req;
                         image_req = image_req->next)
                        il_icon_notify(image_req, IL_IMAGE_NOT_FOUND,
                                       IL_ERROR_NO_DATA);
                }
                else {
                    for (image_req = ic->clients; image_req;
                         image_req = image_req->next)
                        il_icon_notify(image_req, IL_IMAGE_BAD_DATA,
                                       IL_ERROR_IMAGE_DATA_ILLEGAL);
                }
			}
        }
	}
	else {
        /* It is possible for NetLib to call the exit routine with a success status,
           even though the stream was never created (this can happen when fetching
           a mime image part which contains no image data.)  Show a missing image
           icon. */ 
	    if (ic->state < IC_STREAM) {
		    ic->state = IC_MISSING;
	        for (image_req = ic->clients; image_req; image_req = image_req->next)
                il_icon_notify(image_req, IL_IMAGE_NOT_FOUND, IL_ERROR_NO_DATA);
	    }
    }

    if (ic->url == url) {
        NS_RELEASE(url);
        ic->url = NULL;
    }
    /* else there is actually another load going on from a looping gif. 
     * Weird timer issue.  If ic->url does not equal url, then il_image_complete was
     * already called for "url" and a new load started.  ic->url is the url for 
     * the new load and will be released in its IL_NetRequestDone.  "url" was already
     * released by special code in il_image_complete.
     */
}


void
il_image_abort(il_container *ic)
{
  if (ic->imgdec)
	  ic->imgdec->ImgDAbort();

    /* Clear any pending timeouts */
    if (ic->row_output_timeout) {
        IL_ClearTimeout(ic->row_output_timeout);
        ic->row_output_timeout = NULL;
    }
}

void
IL_StreamComplete(il_container *ic, PRBool is_multipart)
{
#ifdef DEBUG
	PRTime cur_time;
	PRInt32 difference;
#endif /* DEBUG */

	PR_ASSERT(ic);

#ifdef DEBUG
	cur_time = PR_Now();
	LL_SUB(cur_time, cur_time, ic->start_time);
	difference = LL_L2I(difference, cur_time);
    ILTRACE(1, ("il: complete: %d seconds for %s\n",
                difference, ic->url_address));
#endif /* DEBUG */

    ic->is_multipart = is_multipart;

	if (ic->imgdec)
        ic->imgdec->ImgDComplete();
    else
        il_image_complete(ic);
}


/* Called when the container has finished loading to update the number of
   actively loading image containers in each of the client contexts.
   Non-looping images are considered to have been loaded upon natural
   completion of network activity for the image.  Looping images, on the
   other hand, are deemed to have been loaded once the first iteration of
   the animation is complete. */
static void
il_container_loaded(il_container *ic)
{
    il_context_list *current;
    IL_GroupContext *img_cx;
    
    for (current = ic->img_cx_list; current; current = current->next) {
        /* Decrement the number of actively loading image containers in the
           client context. */
        img_cx = current->img_cx;
        img_cx->num_loading--;
        if (!img_cx->num_loading) {
            /* If this is the last image to have loaded in the group,
               notify observers that images stopped loading. */
            il_group_notify(img_cx, IL_FINISHED_LOADING);
        }

#ifdef DEBUG_GROUP_OBSERVER
        ILTRACE(1,("2 img_cx=%x total=%d loading=%d looping=%d aborted=%d",
                  img_cx, img_cx->num_containers, img_cx->num_loading,
                  img_cx->num_looping, img_cx->num_aborted));
#endif /* DEBUG_GROUP_OBSERVER */
    }
}


/* Called when a container starts/stops looping to update the number of
   looping image containers in each of the client containers.  An animated
   image is considered to have started looping at the beginning of the
   second iteration. */
static void
il_container_looping(il_container *ic) 
{
    PRBool is_looping = (PRBool)ic->is_looping;
    il_context_list *current;
    IL_GroupContext *img_cx;

    for (current = ic->img_cx_list; current; current = current->next) {
        img_cx = current->img_cx;
        if (is_looping) {       /* Image has started looping. */
            /* Increment the number of looping image containers in the client
               context.  Observer notification takes place if the image group
               previously had no looping images. */
            if (!img_cx->num_looping)
                il_group_notify(img_cx, IL_STARTED_LOOPING);
            img_cx->num_looping++;

#ifdef DEBUG_GROUP_OBSERVER
            ILTRACE(1,("3 img_cx=%x total=%d loading=%d looping=%d aborted=%d",
                      img_cx, img_cx->num_containers, img_cx->num_loading,
                      img_cx->num_looping, img_cx->num_aborted));
#endif /* DEBUG_GROUP_OBSERVER */
        }
        else {                  /* Image has stopped looping. */
            /* Decrement the number of looping image containers in the client
               context.  Observer notification takes place if this was
               the last looping image in the group. */
            img_cx->num_looping--;
            if (!img_cx->num_looping)
                il_group_notify(img_cx, IL_FINISHED_LOOPING);

#ifdef DEBUG_GROUP_OBSERVER
            ILTRACE(1,("4 img_cx=%x total=%d loading=%d looping=%d aborted=%d",
                      img_cx, img_cx->num_containers, img_cx->num_loading,
                      img_cx->num_looping, img_cx->num_aborted));
#endif /* DEBUG_GROUP_OBSERVER */
        }
    }
}


/* Called when all activity within a container has successfully completed. */
static void
il_container_complete(il_container *ic)
{
    IL_GroupContext *img_cx = ic->img_cx;

    /* Update the image (and mask) pixmaps for the final time. */
    il_flush_image_data(ic);

    /* Tell the Front Ends that we will not modify the bits any further. */
#ifdef STANDALONE_IMAGE_LIB
    img_cx->img_cb->ControlPixmapBits(img_cx->dpy_cx, ic->image,
                                      IL_RELEASE_BITS);
#else
    IMGCBIF_ControlPixmapBits(img_cx->img_cb, img_cx->dpy_cx, ic->image,
                              IL_RELEASE_BITS);
#endif /* STANDALONE_IMAGE_LIB */

    if (ic->mask) {
#ifdef STANDALONE_IMAGE_LIB
        img_cx->img_cb->ControlPixmapBits(img_cx->dpy_cx, ic->mask,
                                          IL_RELEASE_BITS);
#else
        IMGCBIF_ControlPixmapBits(img_cx->img_cb, img_cx->dpy_cx, ic->mask,
                                  IL_RELEASE_BITS);
#endif /* STANDALONE_IMAGE_LIB */
	}

    if (!ic->is_looping) {
        /* Tell the client contexts that the container has finished
           loading.  We don't do this for looping images which have just
           finished looping, since we called il_container_loaded at the
           time the first iteration completed. */
        il_container_loaded(ic);
    }
    else {
        /* This is a looping image whose loop count has reached zero, so
           set the container's state to indicate that it is no longer
           looping. */
        ic->is_looping = FALSE;			 

        /* Inform the client contexts that the container has stopped
           looping. */
        il_container_looping(ic);
    }
        
    /* Set the container's state to complete.  This indicates that the
       natural completion of network activity for the image. */
    ic->state = IC_COMPLETE;
    
}


/* XXXM12N This routine needs to be broken up into smaller components. */
void
il_image_complete(il_container *ic)
{
    NI_PixmapHeader *src_header = ic->src_header;
    IL_GroupContext *img_cx = ic->img_cx;
    IL_DisplayType display_type = ic->display_type;
	ilINetReader *reader;


    switch( ic->state ){

        case IC_ABORT_PENDING:
            /* It could be that layout aborted image loading by calling
               IL_DestroyImage() before the netlib finished transferring data.
               Don't do anything. */
            il_scour_container(ic);
            break;
    
        case IC_VIRGIN:
        case IC_START:
        case IC_STREAM:
            /* If we didn't size the image, but the stream finished loading, the
               image must be corrupt or truncated. */
            ic->state = IC_BAD;
            il_bad_container(ic);
            break;

        case IC_SIZED:
        case IC_MULTI:
         
		    PR_ASSERT(ic->image && ic->image->bits);

		    ILTRACE(1,("il: complete %d image width %d (%d) height %d,"
                       " depth %d, %d colors",
				       ic->multi,
				       ic->image->header.width,
                       ic->image->header.widthBytes,
                       ic->image->header.height,
                       ic->image->header.color_space->pixmap_depth, 
				       ic->image->header.color_space->cmap.num_colors));

		    /* 3 cases: simple, multipart MIME, multi-image animation */
		    if (!ic->loop_count && !ic->is_multipart) {
                /* A single frame, single part image. */
                il_container_complete(ic);
            }
            else {
                /* Display the rest of the last image before starting a new one */
			    il_flush_image_data(ic);

                /* Force new colormap to be loaded in case its different from the
                 * LOSRC or previous images in the multipart message.
                 * XXX - fur - Shouldn't do this for COLORMAP case.
                 */
                il_reset_palette(ic);

                FREE_IF_NOT_NULL(src_header->color_space->cmap.map);
                FREE_IF_NOT_NULL(src_header->transparent_pixel);
			    il_destroy_image_transparent_pixel(ic);
                FREE_IF_NOT_NULL(ic->comment);
                ic->comment_length = 0;

                /* Handle looping, which can be used to replay an animation. */
                if (ic->loop_count) {
                    int is_infinite_loop = (ic->loop_count == -1);
                    ilIURL *netRequest = NULL;                
                    if (!is_infinite_loop)
                        ic->loop_count--;
                
                    ILTRACE(1,("il: loop %s", ic->url_address));

                    netRequest = ic->net_cx->CreateURL(ic->fetch_url, NET_DONT_RELOAD);
                    if (!netRequest) {   /* OOM */
                        il_container_complete(ic);
                        break;
                    }
                
                    /* Only loop if the image stream is available locally.
                       Also, if the user hit the "stop" button, don't
                       allow the animation to loop. */
    #ifdef NU_CACHE
                    if ((ic->net_cx->IsLocalFileURL(ic->fetch_url)   ||
                         ic->net_cx->IsURLInCache(netRequest))          &&
                        (!il_image_stopped(ic))                &&
                        ic->net_cx &&
                        (display_type == IL_Console))
    #else
                    if ((ic->net_cx->IsLocalFileURL(ic->fetch_url)   ||
                         ic->net_cx->IsURLInMemCache(netRequest)       ||
                         ic->net_cx->IsURLInDiskCache(netRequest))          &&
                        (!il_image_stopped(ic))                &&
                        ic->net_cx &&
                        (display_type == IL_Console))
    #endif
                    {
                        if (!ic->is_looping) {
                            /* If this is the end of the first pass of the
                               animation, then set the state of the container
                               to indicate that we have started looping. */
                            ic->is_looping = TRUE;

                            /* At this point the animation is considered to have
                               loaded, so we need to tell the client contexts that
                               the container has loaded. */
                            il_container_loaded(ic);

                            /* This is also the point at which the animation is
                               considered to have started looping, so inform the
                               client contexts accordingly. */
                            il_container_looping(ic);
                        }
                    
                        ic->bytes_consumed = 0;
                        ic->state = IC_START;

                        /* This is to deal with a weird timer bug, see the comment at the
                           end of IL_NetRequestDone. */
                        NS_IF_RELEASE(ic->url);

                        ic->url = netRequest;
                        /* Record the fact that we are calling NetLib to load a URL. */
                        ic->is_url_loading = PR_TRUE;

                        /* Suppress thermo & progress bar */
					    netRequest->SetBackgroundLoad(PR_TRUE);
					    reader = IL_NewNetReader(ic);
                        (void) ic->net_cx->GetURL(ic->url, NET_DONT_RELOAD, 
											      reader);
                        /* Release reader, GetURL will keep a ref to it. */
                        NS_RELEASE(reader);
                    } else {
                        ic->loop_count = 0;
                        NS_RELEASE(netRequest);
                        il_container_complete(ic);
                    }
                }
                else if (ic->is_multipart) {
				    ic->multi++;
				    ic->state = IC_MULTI;
			    }
            }
		    break;
        
        case IC_MISSING:
        case IC_NOCACHE:

        default:
            break;

	}/*switch*/
    
    
    /* Clear any pending timeouts */
    if (ic->row_output_timeout) {
        IL_ClearTimeout(ic->row_output_timeout);
        ic->row_output_timeout = NULL;
    }

    /* Notify observers that we are done decoding. */
    if (ic->state != IC_ABORT_PENDING && ic->state != IC_BAD)
        il_image_complete_notify(ic);
}


void 
IL_StreamAbort(il_container *ic, int status)
{
    int old_state;
    IL_ImageReq *image_req;	

	PR_ASSERT(ic);

	ILTRACE(4,("il: abort, status=%d ic->state=%d", status, ic->state));

    /* Abort the image. */
    il_image_abort(ic);

	if(ic->state >= IC_SIZED || (ic->state == IC_ABORT_PENDING)){
		if (status == MK_INTERRUPTED){
			il_container_aborted(ic);
		}
		else{
			for (image_req = ic->clients; image_req; image_req = image_req->next)
                il_icon_notify(image_req, IL_IMAGE_BAD_DATA,
                           IL_ERROR_IMAGE_DATA_ILLEGAL);
		}
	}
	/*
     * It could be that layout aborted image loading by calling IL_DestroyImage()
     * before the netlib finished transferring data.  Don't do anything.
     */
	if (ic->state == IC_ABORT_PENDING)
		return;

    old_state = ic->state;
        
    if (status == MK_INTERRUPTED)
        ic->state = IC_INCOMPLETE;
    else
        ic->state = IC_BAD;

    if (old_state < IC_SIZED)
        il_bad_container(ic);
}

PRBool
IL_StreamCreated(il_container *ic,
				 ilIURL *url,
				 int type)
{
	PR_ASSERT(ic);

	/*
     * It could be that layout aborted image loading by calling IL_FreeImage()
     * before the netlib finished transferring data.  Don't do anything.
     */
	if (ic->state == IC_ABORT_PENDING)
		return PR_FALSE;
	
	ic->type = (int)type;
	ic->content_length = url->GetContentLength();
	ILTRACE(4,("il: new stream, type %d, %s", ic->type, 
			   url->GetAddress()));
	ic->state = IC_STREAM;

#ifndef M12N                    /* XXXM12N Fix me. */
#ifdef XP_MAC
    ic->image->hasUniqueColormap = FALSE;
#endif
#endif /* M12N */

	return PR_TRUE;
}


/*	Phong's linear congruential hash  */
uint32
il_hash(const char *ubuf)
{
	unsigned char * buf = (unsigned char*) ubuf;
	uint32 h=1;
	while(*buf)
	{
		h = 0x63c63cd9*h + 0x9c39c33d + (int32)*buf;
		buf++;
	}
	return h;
}

#define IL_LAST_ICON 62
/* Extra factor of 7 is to account for duplications between
   mc-icons and ns-icons */
static uint32 il_icon_table[(IL_LAST_ICON + 7) * 2];

static void
il_setup_icon_table(void)
{
    int inum = 0;

	/* gopher/ftp icons */
	il_icon_table[inum++] = il_hash("internal-gopher-text");
	il_icon_table[inum++] = IL_GOPHER_TEXT;
	il_icon_table[inum++] = il_hash("internal-gopher-image");
	il_icon_table[inum++] = IL_GOPHER_IMAGE;
	il_icon_table[inum++] = il_hash("internal-gopher-binary");
	il_icon_table[inum++] = IL_GOPHER_BINARY;
	il_icon_table[inum++] = il_hash("internal-gopher-sound");
	il_icon_table[inum++] = IL_GOPHER_SOUND;
	il_icon_table[inum++] = il_hash("internal-gopher-movie");
	il_icon_table[inum++] = IL_GOPHER_MOVIE;
	il_icon_table[inum++] = il_hash("internal-gopher-menu");
	il_icon_table[inum++] = IL_GOPHER_FOLDER;
	il_icon_table[inum++] = il_hash("internal-gopher-index");
	il_icon_table[inum++] = IL_GOPHER_SEARCHABLE;
	il_icon_table[inum++] = il_hash("internal-gopher-telnet");
	il_icon_table[inum++] = IL_GOPHER_TELNET;
	il_icon_table[inum++] = il_hash("internal-gopher-unknown");
	il_icon_table[inum++] = IL_GOPHER_UNKNOWN;

	/* news icons */
	il_icon_table[inum++] = il_hash("internal-news-catchup-group");
	il_icon_table[inum++] = IL_NEWS_CATCHUP;
	il_icon_table[inum++] = il_hash("internal-news-catchup-thread");
	il_icon_table[inum++] = IL_NEWS_CATCHUP_THREAD;
	il_icon_table[inum++] = il_hash("internal-news-followup");
	il_icon_table[inum++] = IL_NEWS_FOLLOWUP;
	il_icon_table[inum++] = il_hash("internal-news-go-to-newsrc");
	il_icon_table[inum++] = IL_NEWS_GOTO_NEWSRC;
	il_icon_table[inum++] = il_hash("internal-news-next-article");
	il_icon_table[inum++] = IL_NEWS_NEXT_ART;
	il_icon_table[inum++] = il_hash("internal-news-next-article-gray");
	il_icon_table[inum++] = IL_NEWS_NEXT_ART_GREY;
	il_icon_table[inum++] = il_hash("internal-news-next-thread");
	il_icon_table[inum++] = IL_NEWS_NEXT_THREAD;
	il_icon_table[inum++] = il_hash("internal-news-next-thread-gray");
	il_icon_table[inum++] = IL_NEWS_NEXT_THREAD_GREY;
	il_icon_table[inum++] = il_hash("internal-news-post");
	il_icon_table[inum++] = IL_NEWS_POST;
	il_icon_table[inum++] = il_hash("internal-news-prev-article");
	il_icon_table[inum++] = IL_NEWS_PREV_ART;
	il_icon_table[inum++] = il_hash("internal-news-prev-article-gray");
	il_icon_table[inum++] = IL_NEWS_PREV_ART_GREY;
	il_icon_table[inum++] = il_hash("internal-news-prev-thread");
	il_icon_table[inum++] = IL_NEWS_PREV_THREAD;
	il_icon_table[inum++] = il_hash("internal-news-prev-thread-gray");
	il_icon_table[inum++] = IL_NEWS_PREV_THREAD_GREY;
	il_icon_table[inum++] = il_hash("internal-news-reply");
	il_icon_table[inum++] = IL_NEWS_REPLY;
	il_icon_table[inum++] = il_hash("internal-news-rtn-to-group");
	il_icon_table[inum++] = IL_NEWS_RTN_TO_GROUP;
	il_icon_table[inum++] = il_hash("internal-news-show-all-articles");
	il_icon_table[inum++] = IL_NEWS_SHOW_ALL_ARTICLES;
	il_icon_table[inum++] = il_hash("internal-news-show-unread-articles");
	il_icon_table[inum++] = IL_NEWS_SHOW_UNREAD_ARTICLES;
	il_icon_table[inum++] = il_hash("internal-news-subscribe");
	il_icon_table[inum++] = IL_NEWS_SUBSCRIBE;
	il_icon_table[inum++] = il_hash("internal-news-unsubscribe");
	il_icon_table[inum++] = IL_NEWS_UNSUBSCRIBE;
	il_icon_table[inum++] = il_hash("internal-news-newsgroup");
	il_icon_table[inum++] = IL_NEWS_FILE;
	il_icon_table[inum++] = il_hash("internal-news-newsgroups");
	il_icon_table[inum++] = IL_NEWS_FOLDER;

	/* httpd file icons */
	il_icon_table[inum++] = il_hash("/mc-icons/menu.gif");
	il_icon_table[inum++] = IL_GOPHER_FOLDER;
	il_icon_table[inum++] = il_hash("/mc-icons/unknown.gif");  
	il_icon_table[inum++] = IL_GOPHER_UNKNOWN;
	il_icon_table[inum++] = il_hash("/mc-icons/text.gif");	
	il_icon_table[inum++] = IL_GOPHER_TEXT;
	il_icon_table[inum++] = il_hash("/mc-icons/image.gif"); 
	il_icon_table[inum++] = IL_GOPHER_IMAGE;
	il_icon_table[inum++] = il_hash("/mc-icons/sound.gif");	 
	il_icon_table[inum++] = IL_GOPHER_SOUND;
	il_icon_table[inum++] = il_hash("/mc-icons/movie.gif");	 
	il_icon_table[inum++] = IL_GOPHER_MOVIE;
	il_icon_table[inum++] = il_hash("/mc-icons/binary.gif"); 
	il_icon_table[inum++] = IL_GOPHER_BINARY;

    /* Duplicate httpd icons, but using new naming scheme. */
	il_icon_table[inum++] = il_hash("/ns-icons/menu.gif");
	il_icon_table[inum++] = IL_GOPHER_FOLDER;
	il_icon_table[inum++] = il_hash("/ns-icons/unknown.gif");  
	il_icon_table[inum++] = IL_GOPHER_UNKNOWN;
	il_icon_table[inum++] = il_hash("/ns-icons/text.gif");	
	il_icon_table[inum++] = IL_GOPHER_TEXT;
	il_icon_table[inum++] = il_hash("/ns-icons/image.gif"); 
	il_icon_table[inum++] = IL_GOPHER_IMAGE;
	il_icon_table[inum++] = il_hash("/ns-icons/sound.gif");	 
	il_icon_table[inum++] = IL_GOPHER_SOUND;
	il_icon_table[inum++] = il_hash("/ns-icons/movie.gif");	 
	il_icon_table[inum++] = IL_GOPHER_MOVIE;
	il_icon_table[inum++] = il_hash("/ns-icons/binary.gif"); 
	il_icon_table[inum++] = IL_GOPHER_BINARY;

	/* ... and names for all the image icons */
	il_icon_table[inum++] = il_hash("internal-icon-delayed"); 
	il_icon_table[inum++] = IL_IMAGE_DELAYED;
	il_icon_table[inum++] = il_hash("internal-icon-notfound"); 
	il_icon_table[inum++] = IL_IMAGE_NOT_FOUND;
	il_icon_table[inum++] = il_hash("internal-icon-baddata"); 
	il_icon_table[inum++] = IL_IMAGE_BAD_DATA;
	il_icon_table[inum++] = il_hash("internal-icon-insecure"); 
	il_icon_table[inum++] = IL_IMAGE_INSECURE;
	il_icon_table[inum++] = il_hash("internal-icon-embed"); 
	il_icon_table[inum++] = IL_IMAGE_EMBED;

	/* This belongs up in the `news icons' section */
	il_icon_table[inum++] = il_hash("internal-news-followup-and-reply");
	il_icon_table[inum++] = IL_NEWS_FOLLOWUP_AND_REPLY;

	/* editor icons. */
	il_icon_table[inum++] = il_hash("internal-edit-named-anchor");
	il_icon_table[inum++] = IL_EDIT_NAMED_ANCHOR;
	il_icon_table[inum++] = il_hash("internal-edit-form-element");
	il_icon_table[inum++] = IL_EDIT_FORM_ELEMENT;
	il_icon_table[inum++] = il_hash("internal-edit-unsupported-tag");
	il_icon_table[inum++] = IL_EDIT_UNSUPPORTED_TAG;
	il_icon_table[inum++] = il_hash("internal-edit-unsupported-end-tag");
	il_icon_table[inum++] = IL_EDIT_UNSUPPORTED_END_TAG;
	il_icon_table[inum++] = il_hash("internal-edit-java");
	il_icon_table[inum++] = IL_EDIT_JAVA;
	il_icon_table[inum++] = il_hash("internal-edit-PLUGIN");
	il_icon_table[inum++] = IL_EDIT_PLUGIN;

	/* Security Advisor and S/MIME icons */
	il_icon_table[inum++] = il_hash("internal-sa-signed");
    il_icon_table[inum++] = IL_SA_SIGNED;
	il_icon_table[inum++] = il_hash("internal-sa-encrypted");
    il_icon_table[inum++] = IL_SA_ENCRYPTED;
	il_icon_table[inum++] = il_hash("internal-sa-nonencrypted");
    il_icon_table[inum++] = IL_SA_NONENCRYPTED;
	il_icon_table[inum++] = il_hash("internal-sa-signed-bad");
    il_icon_table[inum++] = IL_SA_SIGNED_BAD;
	il_icon_table[inum++] = il_hash("internal-sa-encrypted-bad");
    il_icon_table[inum++] = IL_SA_ENCRYPTED_BAD;
	il_icon_table[inum++] = il_hash("internal-smime-attached");
    il_icon_table[inum++] = IL_SMIME_ATTACHED;
	il_icon_table[inum++] = il_hash("internal-smime-signed");
    il_icon_table[inum++] = IL_SMIME_SIGNED;
	il_icon_table[inum++] = il_hash("internal-smime-encrypted");
    il_icon_table[inum++] = IL_SMIME_ENCRYPTED;
	il_icon_table[inum++] = il_hash("internal-smime-encrypted-signed");
    il_icon_table[inum++] = IL_SMIME_ENC_SIGNED;
	il_icon_table[inum++] = il_hash("internal-smime-signed-bad");
    il_icon_table[inum++] = IL_SMIME_SIGNED_BAD;
	il_icon_table[inum++] = il_hash("internal-smime-encrypted-bad");
    il_icon_table[inum++] = IL_SMIME_ENCRYPTED_BAD;
	il_icon_table[inum++] = il_hash("internal-smime-encrypted-signed-bad");
    il_icon_table[inum++] = IL_SMIME_ENC_SIGNED_BAD;

	/* LibMsg Attachment Icon */
	il_icon_table[inum++] = il_hash("internal-attachment-icon");
    il_icon_table[inum++] = IL_MSG_ATTACH;

    PR_ASSERT(inum <= (sizeof(il_icon_table) / sizeof(il_icon_table[0])));
}


static uint32
il_internal_image(const char *image_url)
{
	int i;
    uint32 hash = il_hash(image_url);
	if (il_icon_table[0]==0)
		il_setup_icon_table();

	for (i=0; i< (sizeof(il_icon_table) / sizeof(il_icon_table[0])); i++)
	{
		if (il_icon_table[i<<1] == hash)
		{
			return il_icon_table[(i<<1)+1];
		}
	}
	return 0;
}



IL_IMPLEMENT(IL_ImageReq *)
IL_GetImage(const char* image_url,
            IL_GroupContext *img_cx,
            XP_ObserverList obs_list,
            NI_IRGB *background_color,
            uint32 req_width, uint32 req_height,
            uint32 flags,
            void *opaque_cx)
{
    ilINetContext *net_cx = (ilINetContext *)opaque_cx;
    NET_ReloadMethod cache_reload_policy = net_cx->GetReloadPolicy();

    ilIURL *url = NULL;

    IL_ImageReq *image_req;
	ilINetReader *reader;
	il_container *ic = NULL;
    int req_depth = img_cx->color_space->pixmap_depth;
	int err;
#ifndef STANDALONE_IMAGE_LIB
    int is_internal_external_reconnect = FALSE;
#endif /* STANDALONE_IMAGE_LIB */
    int is_view_image;
    int is_internal_external_reconnect = FALSE;

    /* Create a new instance for this image request. */
    image_req = PR_NEWZAP(IL_ImageReq);
    if (!image_req)
        return NULL;
    image_req->img_cx = img_cx;
    /*
     * Remember the net context for this image request, It is possible that
     * the lifetime of the image container's net context will not be as
     * long as that of the image request itself (for example if the image
     * request for which the container was originally created happens to
     * be destroyed,) in which case we may need to give the container a
     * handle on this backup net context.
     */
    image_req->net_cx = net_cx->Clone();
	if (!image_req->net_cx) {
		PR_FREEIF(image_req);
		return NULL;
	}
    image_req->obs_list = obs_list;
    XP_SetObserverListObservable(obs_list, (void *)image_req);
    
    ILTRACE(1, ("il: IL_GetImage, url=%s\n", image_url));

	if (!image_url)
	{
		ILTRACE(0,("il: no url, sending delayed icon"));
        il_icon_notify(image_req, IL_IMAGE_DELAYED, IL_ERROR_NO_DATA);
        return image_req;
	}

    /* Check for any special internal-use URLs */
	if (*image_url == 'i'                  ||
        !PL_strncmp(image_url, "/mc-", 4)  ||
        !PL_strncmp(image_url, "/ns-", 4))
	{
		uint32 icon;

        /* A built-in icon ? */
        icon = il_internal_image(image_url);
		if (icon)
		{
			ILTRACE(4,("il: internal icon %d", icon));

            /* XXXM12N In response to this notification, layout should set
               lo_image->image_attr->attrmask |= LO_ATTR_INTERNAL_IMAGE; */
            il_icon_notify(image_req, icon, IL_INTERNAL_IMAGE);

            return image_req;
		}

#ifndef STANDALONE_IMAGE_LIB
        /* Image viewer URLs look like "internal-external-reconnect:REALURL.gif"
         * Strip off the prefix to get the real URL name.
         */
		if (!PL_strncmp(image_url, "internal-external-reconnect:", 28)) {
            image_url += 28;
			is_internal_external_reconnect = TRUE;
        }
#endif /* STANDALONE_IMAGE_LIB */
	}

    ic = il_get_container(img_cx, cache_reload_policy, image_url,
                          background_color, img_cx->dither_mode, req_depth,
                          req_width, req_height);
    if (!ic)
    {
        ILTRACE(0,("il: MEM il_container"));
        il_icon_notify(image_req, IL_IMAGE_DELAYED, IL_ERROR_INTERNAL);
#ifndef STANDALONE_IMAGE_LIB
        if (is_internal_external_reconnect)
            il_abort_reconnect();
#endif /* STANDALONE_IMAGE_LIB */
        return image_req;
    }
     
    /* Give the client a handle into the imagelib world. */
    image_req->ic = ic;

    /* Is this a call to the image viewer ? */
#ifndef STANDALONE_IMAGE_LIB
#ifndef M12N /* XXXM12N fix me. */
    is_view_image = is_internal_external_reconnect &&
        (cx->type != MWContextNews) && (cx->type != MWContextMail);
#else
    is_view_image = is_internal_external_reconnect;
#endif /* M12N */
#else
    is_view_image = FALSE;
  
#endif /* STANDALONE_IMAGE_LIB */

    if (!il_add_client(img_cx, ic, image_req, is_view_image))
    {
        il_icon_notify(image_req, IL_IMAGE_DELAYED, IL_ERROR_INTERNAL);
#ifndef STANDALONE_IMAGE_LIB
        if (is_internal_external_reconnect)
            il_abort_reconnect();
#endif /* STANDALONE_IMAGE_LIB */
        return image_req;
    }

    /* If the image is already in memory ... */
	if (ic->state != IC_VIRGIN) {
        switch (ic->state) {
        case IC_BAD:
            il_icon_notify(image_req, IL_IMAGE_BAD_DATA,
                           IL_ERROR_IMAGE_DATA_ILLEGAL);
            break;

        case IC_MISSING:
            il_icon_notify(image_req, IL_IMAGE_NOT_FOUND, IL_ERROR_NO_DATA);
#ifndef STANDALONE_IMAGE_LIB
            PR_ASSERT(! is_internal_external_reconnect);
#endif /* STANDALONE_IMAGE_LIB */
            break;

        case IC_INCOMPLETE:
            il_icon_notify(image_req, IL_IMAGE_DELAYED,
                           IL_ERROR_IMAGE_DATA_TRUNCATED);
            break;

        case IC_SIZED:
        case IC_MULTI:
            /* This is a cached image that hasn't completed decoding. */
            il_cache_return_notify(image_req);
            break;

        case IC_COMPLETE:
#ifndef M12N
            /* M12N Fix custom colormaps. */
            il_set_color_palette(cx, ic);
#else
#endif /* M12N */
            /* This is a cached image that has already completed decoding. */
            il_cache_return_notify(image_req);

            break;

        case IC_START:
        case IC_STREAM:
        case IC_ABORT_PENDING:
        case IC_NOCACHE:
            break;
            
        default:
            PR_ASSERT(0);
            NS_RELEASE(image_req->net_cx);
            PR_FREEIF(image_req);
            return NULL;
        }

#ifndef STANDALONE_IMAGE_LIB
        if (is_internal_external_reconnect) {
            /* Since we found the image in the cache, we don't
             * need any of the data now streaming into the image viewer.
             */
            il_abort_reconnect();
        }
#endif /* STANDALONE_IMAGE_LIB */

		/* NOCACHE falls through to be tried again */
        if (ic->state != IC_NOCACHE)
             return image_req;
    }

    /* This is a virgin (never-used) image container. */
	else
    {
		ic->forced = FORCE_RELOAD(cache_reload_policy);
	}

	ic->state = IC_START;
    
#ifdef DEBUG
    ic->start_time = PR_Now();
#endif

    /* Record the context that actually initiates and controls the transfer. */
    ic->net_cx = net_cx->Clone();
 
#ifndef STANDALONE_IMAGE_LIB
	if (is_internal_external_reconnect)
	{
        /* "Reconnect" to the stream feeding IL_ViewStream(), already
           created. */
        il_reconnect(ic);
        return image_req;
    }
#endif /* STANDALONE_IMAGE_LIB */
        
    /* need to make a net request */
    ILTRACE(1,("il: net request for %s, %s", image_url,
               ic->forced ? "force" : ""));

    url = ic->net_cx->CreateURL(image_url, cache_reload_policy);

    if (!url)
    {
#ifndef M12N /* XXXM12N fix me. */
        /* xxx clean up previously allocated objects */
        return MK_OUT_OF_MEMORY;
#else
        NS_RELEASE(image_req->net_cx);
        PR_FREEIF(image_req);
        return NULL;
#endif /* M12N */
    }        
    
#if 0
    /* Add the referer to the URL. */
    ic->net_cx->AddReferer(url);
#endif

    ic->is_looping = FALSE;
    ic->url = url;
	/* Record the fact that we are calling NetLib to load a URL. */
    ic->is_url_loading = PR_TRUE;

    /* save away the container */
	reader = IL_NewNetReader(ic);
	if (!reader) {
        NS_RELEASE(image_req->net_cx);
        PR_FREEIF(image_req);
        return NULL;
	}
    err = ic->net_cx->GetURL(url, cache_reload_policy, reader);
    /* Release reader, GetURL will keep a ref to it. */
    NS_RELEASE(reader);
	return image_req;
}


IL_IMPLEMENT(void)
IL_ReloadImages(IL_GroupContext *img_cx, void *net_cx)
{
    /* XXXM12N - Implement me. */

}


IL_IMPLEMENT(void)
IL_SetDisplayMode(IL_GroupContext *img_cx, uint32 display_flags,
                  IL_DisplayData *display_data)
{
    if (display_flags & IL_DISPLAY_CONTEXT)
        img_cx->dpy_cx = display_data->display_context;
    
    if (display_flags & IL_DISPLAY_TYPE)
        img_cx->display_type = display_data->display_type;

    if (display_flags & IL_COLOR_SPACE) {
        if (img_cx->color_space)
            IL_ReleaseColorSpace(img_cx->color_space);
        img_cx->color_space = display_data->color_space;
        IL_AddRefToColorSpace(img_cx->color_space);
    }

    if (display_flags & IL_PROGRESSIVE_DISPLAY)
        img_cx->progressive_display = display_data->progressive_display;

    if (display_flags & IL_DITHER_MODE)
        if (img_cx->color_space && img_cx->color_space->pixmap_depth == 1) {
            /* Dithering always on in monochrome mode. */
            img_cx->dither_mode = IL_Dither;
        }
        else {
            img_cx->dither_mode = display_data->dither_mode;
        }
}



/* Interrupts all images loading in a context.  Specifically, stops all
   looping image animations. */
IL_IMPLEMENT(void)
IL_InterruptContext(IL_GroupContext *img_cx)
{
    il_container *ic;
    IL_ImageReq *image_req;
	il_container_list *ic_list;

    if (!img_cx)
        return;

    /* Mark all clients in this context as interrupted. */
	for (ic_list = img_cx->container_list; ic_list; ic_list = ic_list->next) {
        ic = ic_list->ic;
        for (image_req = ic->clients; image_req; image_req = image_req->next) {
            if (image_req->img_cx == img_cx) {
                image_req->stopped = TRUE;
            }
        }
    }
}

/* Interrupts just a specific image request */
IL_IMPLEMENT(void)
IL_InterruptRequest(IL_ImageReq *image_req)
{
  if (image_req != NULL) {
	image_req->stopped = TRUE;
  }
}

/* Has the user aborted the image load ? */
PRBool
il_image_stopped(il_container *ic)
{
    IL_ImageReq *image_req;
    for (image_req = ic->clients; image_req; image_req = image_req->next) {
        if (!image_req->stopped)
            return PR_FALSE;
    }

    /* All clients must be stopped for image container to become dormant. */
    return PR_TRUE;
}


/* Create an IL_GroupContext, which represents an aggregation of state
   for one or more images and which contains an interface to access
   external service functions and callbacks.  IL_NewGroupContext will use
   the IMGCBIF_AddRef callback to increment the reference count for the
   interface.

   The dpy_cx argument is opaque to the image library and is passed back to
   all of the callbacks in the IMGCBIF interface. */
IL_IMPLEMENT(IL_GroupContext*)
IL_NewGroupContext(void *dpy_cx, 
#ifdef STANDALONE_IMAGE_LIB
                   ilIImageRenderer *img_cb)
#else
                   IMGCBIF *img_cb)
#endif /* STANDALONE_IMAGE_LIB */
{
    IL_GroupContext *img_cx;
    
    if (!img_cb)
        return NULL;

    img_cx = (IL_GroupContext*)PR_NEWZAP(IL_GroupContext);
    if (!img_cx)
        return NULL;

    img_cx->dpy_cx = dpy_cx;
    img_cx->img_cb = img_cb;

    img_cx->progressive_display = TRUE;

    /* Create an observer list for the image context. */
    if (XP_NewObserverList((void *)img_cx, &img_cx->obs_list)
        == MK_OUT_OF_MEMORY) {
        PR_FREEIF(img_cx);
        return NULL;
    }

    /* Add the context to the global image context list. */
    img_cx->next = il_global_img_cx_list;
    il_global_img_cx_list = img_cx;

    return img_cx;
}


/* Free an image context.  IL_DestroyGroupContext will make a call
   to the IMGCBIF_Release callback function of the JMC interface prior to
   releasing the IL_GroupContext structure.  The IMGCBIF_Release callback
   is expected to decrement the reference count for the IMGCBIF interface,
   and to free the callback vtable and the interface structure if the
   reference count is zero. */
IL_IMPLEMENT(void)
IL_DestroyGroupContext(IL_GroupContext *img_cx)
{
    if (img_cx) {
        /* Remove ourself from the global image context list. */
        if (img_cx == il_global_img_cx_list) {
            il_global_img_cx_list = NULL;
        }
        else {
            IL_GroupContext *current, *next;
        
            for (current = il_global_img_cx_list; current; current = next) {
                next = current->next;
                if (next == img_cx) {
                    current->next = next->next;
                    break;
                }
            }
        }
        
        /* Destroy any images remaining in the context. */
        if (img_cx->num_containers) {
            PR_ASSERT(img_cx->container_list != NULL);
            IL_DestroyImageGroup(img_cx);
        }

        /* Destroy the observer list. */
        XP_DisposeObserverList(img_cx->obs_list);

        /* Release the image context's reference to the colorspace. */
        if (img_cx->color_space) {
            IL_ReleaseColorSpace(img_cx->color_space);
            img_cx->color_space = NULL;
        }

        /* Release the JMC callback interface. */
#ifdef STANDALONE_IMAGE_LIB
        NS_RELEASE(img_cx->img_cb);
#else
        IMGCBIF_release(img_cx->img_cb, NULL); /* XXXM12N Need to use
                                                  exceptions. */
#endif /* STANDALONE_IMAGE_LIB */

        PR_FREEIF(img_cx);
    }
}


/* Add an observer/closure pair to an image group context's observer list.
   Returns PR_TRUE if the observer is successfully registered. */
IL_IMPLEMENT(PRBool)
IL_AddGroupObserver(IL_GroupContext *img_cx, XP_ObserverProc observer,
                    void *closure)
{
    return (PRBool)(!XP_AddObserver(img_cx->obs_list, observer, closure));    
}


/* Remove an observer/closure pair from an image group context's observer
   list.  Returns PR_TRUE if successful. */
IL_IMPLEMENT(PRBool)
IL_RemoveGroupObserver(IL_GroupContext *img_cx, XP_ObserverProc observer,
                    void *closure)
{
    return XP_RemoveObserver(img_cx->obs_list, observer, closure);
}


/* Display a rectangular portion of an image.  x and y refer to the top left
   corner of the image, measured in pixels, with respect to the document origin.
   x_offset and y_offset are measured in pixels, with the upper-left-hand corner
   of the pixmap as the origin, increasing left-to-right, top-to-bottom.

   If the width and height values would otherwise cause the sub-image
   to extend off the edge of the source image, the function should
   perform tiling of the source image.  This is used to draw document,
   layer and table cell backdrops.  (Note: it is assumed this case will
   apply only to images which do not require any scaling.)

   If at any time the image library determines that an image request cannot
   be fulfilled or that the image has been delayed, it will notify the client
   synchronously through the observer mechanism.  The client may then choose to
   request that an icon be drawn instead by making a call to IL_DisplayIcon. */
IL_IMPLEMENT(void)
IL_DisplaySubImage(IL_ImageReq *image_req, int x, int y, int x_offset,
                   int y_offset, int width, int height)
{
    IL_GroupContext *img_cx;
    void *dpy_cx;
    il_container *ic;
    IL_Rect *displayable_rect;
    NI_PixmapHeader *img_header;
    
    /* If we were passed a NULL image handle, don't do anything. */
    if (!image_req)
        return;

    /* Determine the image context and the display context for this display
       operation. */
    img_cx = image_req->img_cx;
    dpy_cx = img_cx->dpy_cx;
    if (!dpy_cx)
        return;
    
    /* Determine the image container. */
    ic = image_req->ic;
	
    if (!ic )
	return;

    /* Perform the drawing operation, but only do so for displayable areas
       of the image pixmap. */
    displayable_rect = &ic->displayable_rect;
    img_header = &ic->image->header;
        
    if (displayable_rect->width < img_header->width ||
        displayable_rect->height < img_header->height) {
        /* The image pixmap is only partially displayable (since the image
           is only partially decoded,) so draw the intersection of the
           requested area with the displayable area.  The client will
           receive observer notification as the pixmap is updated, and it
           is expected to call the image library again to display the
           updated area.  Note that tiling operations are not performed
           unless the entire image pixmap is displayable. */
        int32 display_left, display_top, display_bottom, display_right;
        int32 display_width, display_height;
            
        /* Determine the intersection of the requested area and the
           displayable area, in pixmap coordinates. */
        display_left = MAX(x_offset, displayable_rect->x_origin);
        display_top = MAX(y_offset, displayable_rect->y_origin);
        display_right = MIN(x_offset + width, displayable_rect->x_origin
                            + displayable_rect->width);
        display_bottom = MIN(y_offset + height, displayable_rect->y_origin
                             + displayable_rect->height);
        display_width = display_right - display_left;
        display_height = display_bottom - display_top;
        
        /* Draw the intersection of the requested area and the displayable
           area. */
        if ((display_width > 0) && (display_height > 0)) {
#ifdef STANDALONE_IMAGE_LIB
            img_cx->img_cb->DisplayPixmap(dpy_cx, ic->image, ic->mask,
                                          x, y, display_left, display_top,
                                          display_width, display_height);
#else
            IMGCBIF_DisplayPixmap(img_cx->img_cb, dpy_cx, ic->image, ic->mask,
                                  x, y, display_left, display_top,
                                  display_width, display_height, ic->dest_width, ic->dest_height);
#endif /* STANDALONE_IMAGE_LIB */
        }
    }
    else {
        /* The entire image pixmap is displayable, (although this does not
           necessarily mean that the image has finished decoding,) so draw
           the entire area requested.  In the event that the requested
           area extends beyond the bounds of the image pixmap, tiling will
           be performed. */
        if (width && height) {
#ifdef STANDALONE_IMAGE_LIB
            img_cx->img_cb->DisplayPixmap(dpy_cx, ic->image, ic->mask,
                                          x, y, x_offset, y_offset, 
                                          width, height);
#else
            IMGCBIF_DisplayPixmap(img_cx->img_cb, dpy_cx, ic->image, ic->mask,
                                  x, y, x_offset, y_offset, width, height, 
                                  ic->dest_width, ic->dest_height);
#endif /* STANDALONE_IMAGE_LIB */
        }
    }
}


/* Display an icon.  x and y refer to the top left corner of the icon, measured
   in pixels, with respect to the document origin.  x_offset, y_offset, width
   and height specify a clipping rectangle for this drawing request. */
IL_IMPLEMENT(void)
IL_DisplayIcon(IL_GroupContext *img_cx, int icon_number, int x, int y)
{
    /* Check for a NULL image context. */
    if (!img_cx)
        return;

    /* Ask the Front End to display the icon. */
#ifdef STANDALONE_IMAGE_LIB
    img_cx->img_cb->DisplayIcon(img_cx->dpy_cx, x, y, icon_number);
#else
    IMGCBIF_DisplayIcon(img_cx->img_cb, img_cx->dpy_cx, x, y, icon_number);
#endif /* STANDALONE_IMAGE_LIB */
}


/* Return the dimensions of an icon. */
IL_IMPLEMENT(void)
IL_GetIconDimensions(IL_GroupContext *img_cx, int icon_number, int *width,
                     int *height)
{
    /* Check for a NULL image context. */
    if (!img_cx)
        return;

    /* Obtain the dimensions of the icon. */
#ifdef STANDALONE_IMAGE_LIB
    img_cx->img_cb->GetIconDimensions(img_cx->dpy_cx, width, height,
                                      icon_number);
#else
    IMGCBIF_GetIconDimensions(img_cx->img_cb, img_cx->dpy_cx, width, height,
                              icon_number);
#endif /* STANDALONE_IMAGE_LIB */
}


/* Return the image IL_Pixmap associated with an image request. */
IL_IMPLEMENT(IL_Pixmap *)
IL_GetImagePixmap(IL_ImageReq *image_req)
{
    if (image_req && image_req->ic)
        return image_req->ic->image;
    else
        return NULL;
}


/* Return the mask IL_Pixmap associated with an image request. */
IL_IMPLEMENT(IL_Pixmap *)
IL_GetMaskPixmap(IL_ImageReq *image_req)
{
    if (image_req && image_req->ic)
        return image_req->ic->mask;
    else
        return NULL;
}


/* Return the natural dimensions of the image.  Returns 0,0 if the dimensions
   are unknown. */
IL_IMPLEMENT(void)
IL_GetNaturalDimensions(IL_ImageReq *image_req, int *width, int *height)
{
	NI_PixmapHeader *src_header;

	if (width)	
		*width = 0;
	if (height)
		*height = 0;

	if (!image_req || !image_req->ic)
        return;

	src_header = image_req->ic->src_header;

	if (src_header) {
		if (width)
			*width = src_header->width;
		if (height)
			*height = src_header->height;
	}
}



#ifdef PROFILE
#pragma profile off
#endif


