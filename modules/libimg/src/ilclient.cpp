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
 *   ilclient.c --- Management of imagelib client data structures,
 *                  including image cache.
 *
 *   $Id: ilclient.cpp,v 3.5 1999/04/22 22:39:55 pnunn%netscape.com Exp $
 */


#include "if.h"
#include "il_strm.h"            /* For OPAQUE_CONTEXT. */
#include "nsIImgDecoder.h"
#include "nsImgDCallbk.h"
#include "ilISystemServices.h"

/* for XP_GetString() */
#include "xpgetstr.h"

PR_BEGIN_EXTERN_C
extern int MK_OUT_OF_MEMORY;
extern int XP_MSG_IMAGE_NOT_FOUND;
extern int XP_MSG_XBIT_COLOR;	
extern int XP_MSG_1BIT_MONO;
extern int XP_MSG_XBIT_GREYSCALE;
extern int XP_MSG_XBIT_RGB;
extern int XP_MSG_DECODED_SIZE;	
extern int XP_MSG_WIDTH_HEIGHT;	
extern int XP_MSG_SCALED_FROM;	
extern int XP_MSG_IMAGE_DIM;	
extern int XP_MSG_COLOR;	
extern int XP_MSG_NB_COLORS;	
extern int XP_MSG_NONE;	
extern int XP_MSG_COLORMAP;	
extern int XP_MSG_BCKDRP_VISIBLE;	
extern int XP_MSG_SOLID_BKGND;	
extern int XP_MSG_JUST_NO;	
extern int XP_MSG_TRANSPARENCY;	
extern int XP_MSG_COMMENT;	
extern int XP_MSG_UNKNOWN;	
extern int XP_MSG_COMPRESS_REMOVE;	
PR_END_EXTERN_C

static uint32 image_cache_size;

#ifndef M12N                    /* XXXM12N Cache trace: cleanup/eliminate. */
static int il_cache_trace = FALSE; /* XXXM12N Clean up/eliminate */
#endif

PRLogModuleInfo *il_log_module = NULL;
ilISystemServices *il_ss = NULL;

/* simple list, in use order */
struct il_cache_struct {
	il_container *head;
	il_container *tail;
	int32 bytes;
    int32 max_bytes;
	int items;
};

struct il_cache_struct il_cache;
/*---------------------------------------------*/
/*-------------------------------*/    
NS_IMETHODIMP
ImgDCallbk::QueryInterface(const nsIID& aIID, void** aInstPtr)
{
  if (NULL == aInstPtr) {
    return NS_ERROR_NULL_POINTER;
  }

  if (aIID.Equals(kImgDCallbkIID) ||
      aIID.Equals(kISupportsIID)) {
          *aInstPtr = (void*) this;
    NS_INIT_REFCNT();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}                                      

NS_IMETHODIMP
ImgDCallbk::CreateInstance(const nsCID &aClass,
			     il_container *ic,
                             const nsIID &aIID,
                             void **ppv)
{
   ImgDCallbk *imgdcb = NULL;
   *ppv  = NULL;
 
   if (&aClass && !aIID.Equals(kISupportsIID))
       return NS_NOINTERFACE;

   imgdcb = new ImgDCallbk(ic);
   /* make sure interface = nsISupports, ImgDCallbk */
   nsresult res = imgdcb->QueryInterface(aIID,(void**)ppv);

    if (NS_FAILED(res)) {
    *ppv = NULL;
    delete imgdcb;
  }
  return res;
}
NS_IMETHODIMP ImgDCallbk::AddRef()
{
  NS_INIT_REFCNT();
  return NS_OK;
}

NS_IMETHODIMP ImgDCallbk::Release()
{
  return NS_OK;
}   
   
/*-------------------------*/

/* Add an image container to a context's container list. */
static PRBool
il_add_container_to_context(IL_GroupContext *img_cx, il_container *ic)
{
    il_container_list *ic_list;
    
    ic_list = PR_NEWZAP(il_container_list);
	if (!ic_list)
        return PR_FALSE;

    ic_list->ic = ic;
    ic_list->next = img_cx->container_list;
    img_cx->container_list = ic_list;
    return PR_TRUE;
}

/* Remove an image container from a context's container list. */
static PRBool
il_remove_container_from_context(IL_GroupContext *img_cx, il_container *ic)
{
	il_container_list *current, *next;

    current = img_cx->container_list;
    if (!current)
        return PR_FALSE;
    
	if (current->ic == ic) {
		img_cx->container_list = current->next;
        PR_FREEIF(current);
        return PR_TRUE;
    }
	else {
		for (; current; current = next) {
            next = current->next;
			if (next && (next->ic == ic)) {
				current->next = next->next;
                PR_FREEIF(next);
                return PR_TRUE;
			}
        }
	}
    return PR_FALSE;
}

/* Returns PR_TRUE if the context didn't belong to the container's client
   context list, and was successfully added to that list. */
static PRBool
il_add_client_context(IL_GroupContext *img_cx, il_container *ic)
{
    il_context_list *current = ic->img_cx_list;

    /* Check whether the context is already in the client context list. */
    if (current)
        for (; current; current = current->next) {
        if (current->img_cx == img_cx)
            return PR_FALSE;
    }

    if (il_add_container_to_context(img_cx, ic)) {
        /* If the client image context doesn't already belong to the
           container's client context list, then add it to the beginning
           of the list. */
        current = PR_NEWZAP(il_context_list);
        if (!current)
            return PR_FALSE;
        current->img_cx = img_cx;
        current->next = ic->img_cx_list;
        ic->img_cx_list = current;

        /* Increment the number of containers in this context.  */
        img_cx->num_containers++;

        /* If the image is still loading, increment the number of loading
           containers in the context.  Observer notification takes place
           if the image group previously had no actively loading images. */
        if (IMAGE_CONTAINER_ACTIVE(ic) && !ic->is_looping) {
            PR_ASSERT(!ic->is_aborted);
            if (!img_cx->num_loading)
                il_group_notify(img_cx, IL_STARTED_LOADING);
            img_cx->num_loading++;
        }

        /* If the image is looping, increment the number of looping containers
           in the context.  Observer notification takes place if the image
           group previously had no looping images. */
        if (ic->is_looping) {
            if (!img_cx->num_looping)
                il_group_notify(img_cx, IL_STARTED_LOOPING);
            img_cx->num_looping++;
        }

#ifdef DEBUG_GROUP_OBSERVER
        ILTRACE(1,("5 img_cx=%x total=%d loading=%d looping=%d aborted=%d",
                  img_cx, img_cx->num_containers, img_cx->num_loading,
                  img_cx->num_looping, img_cx->num_aborted));
#endif /* DEBUG_GROUP_OBSERVER */

        return PR_TRUE;
    }
    else {
        return PR_FALSE;
    }
}

/* Returns PR_TRUE if the client context belonged to the container's client
   context list, and was successfully deleted from that list. */
static PRBool
il_remove_client_context(IL_GroupContext *img_cx, il_container *ic)
{
    PRBool deleted_element = PR_FALSE;
    il_context_list *current, *next;

    current = ic->img_cx_list;
    if (!current)
        return PR_FALSE;

    if (current->img_cx == img_cx) {
        ic->img_cx_list = current->next;
        PR_FREEIF(current);
        deleted_element = PR_TRUE;
    }
    else {
        for (; current; current = next) {
            next = current->next;
            if (next && (next->img_cx == img_cx)) {
                current->next = next->next;
                PR_FREEIF(next);
                deleted_element = PR_TRUE;
                break;
            }
        }
    }

    if (deleted_element && il_remove_container_from_context(img_cx, ic)) {
        /* Decrement the number of containers in this context. */
        img_cx->num_containers--;

        /* If the image didn't load successfully, then decrement the number
           of loading containers in the context.  (If the image did load
           successfully, the number of loading containers in the context was
           previously decremented at the time the container loaded, so don't
           do it again.)  Observer notification takes place if this was
           the last image loading in the group. */
        if (!(ic->state == IC_COMPLETE || ic->is_looping)) {
            img_cx->num_loading--;
            if (!img_cx->num_loading)
                il_group_notify(img_cx, IL_FINISHED_LOADING);

            if (ic->is_aborted)           
                img_cx->num_aborted--;
        }

        /* If the image was looping, then decrement the number of looping
           containers in the context.  Observer notification takes place if
           this was the last looping image in the group. */
        if (ic->is_looping) {
            img_cx->num_looping--;
            if (!img_cx->num_looping)
                il_group_notify(img_cx, IL_FINISHED_LOOPING);
        }

#ifdef DEBUG_GROUP_OBSERVER
        ILTRACE(1,("6 img_cx=%x total=%d loading=%d looping=%d aborted=%d",
                  img_cx, img_cx->num_containers, img_cx->num_loading,
                  img_cx->num_looping, img_cx->num_aborted));
#endif /* DEBUG_GROUP_OBSERVER */

        return PR_TRUE;
    }
    else {
        return PR_FALSE;
    }
}

/* Returns TRUE if image container appears to match search parameters */
static int
il_image_match(il_container *ic,          /* Candidate for match. */
               IL_DisplayType display_type,
               const char *image_url,
               IL_IRGB *background_color,
               int req_depth,             /* Colorspace depth. */
               int req_width,             /* Target image width. */
               int req_height)            /* Target image height. */
{
    PRBool ic_sized = (PRBool)(ic->state >= IC_SIZED);
    NI_PixmapHeader *img_header = &ic->image->header;
    IL_IRGB *img_trans_pixel = img_header->transparent_pixel;

    /* Allowable conditions for there to be a size match.  req_width and
       req_height both zero indicate usage of the image's natural size.
       If only one is zero, it indicates scaling maintaining the image's
       aspect ratio.  If ic_size has been called for the image cache entry,
       then ic->dest_width and ic->dest_height represent the size of the
       image as it will be displayed.  If ic_size has not been called for
       the cache entry, then ic->dest_width and ic->dest_height represent the
       requested size. */
    if (!(
        /* Both dimensions match (either zero is a match.) */
        ((req_width == ic->dest_width) && (req_height == ic->dest_height)) ||
        /* Width matches, request height zero, aspect ratio same. */
        (ic_sized && (req_width == ic->dest_width) && !req_height &&
         !ic->aspect_distorted) ||
        /* Height matches, request width zero, aspect ratio same. */
        (ic_sized && (req_height == ic->dest_height) && !req_width &&
         !ic->aspect_distorted) ||
        /* Request dimensions zero, cache entry has natural dimensions. */
        (!req_width && !req_height && ic->natural_size)
        ))
        return FALSE;

	/* We allow any depth image through as the FE may have asked us to
	   decode to a colorspace other than the display colorspace. */

	/* Now check the background color.  This only applies to transparent
       images, so skip this test if the candidate for the match is known
       to be opaque. */
    if (!ic_sized || img_trans_pixel) {
        if (background_color) {
            if (ic->background_color) {
                /* Both request and candidate have a background color; check
                   whether they match. */
                if (background_color->red != ic->background_color->red)
                    return FALSE;
                if (background_color->green != ic->background_color->green)
                    return FALSE;
                if (background_color->blue != ic->background_color->blue)
                    return FALSE;
            }
            else {
                /* A background color was requested, but the candidate does
                   not have one. */
                return FALSE;
            }
        }
        else {                      /* No background color was requested. */
            if (ic->background_color) {
                /* A background color was not requested, but the candidate
                   has one.  This means that while the current request may
                   need a mask, the candidate definitely does not have one. */
                return FALSE;
            }
            else {
                /* Neither the request nor the candidate have a background
                   color, so don't disqualify the candidate. */
            }
        }
    }
    
    /* Check the url (we have already checked the hash value which is based
       on the url.) */
    if (strcmp(image_url, ic->url_address))
        return FALSE;

    /* Printer contexts and window contexts may have different
       native image formats, so don't reuse an image cache entry
       created for an onscreen context in a printer context or
       vice-versa. */
    if (display_type != ic->display_type)
        return FALSE;


    if((ic->display_type==IL_Printer) &&
	(ic->dest_width != ic->image->header.width) &&
	(ic->dest_height != ic->image->header.height ))
          return FALSE;

    /* XXX - temporary */
    if (ic->rendered_with_custom_palette)
        return FALSE;
                
    return TRUE;
}

static int
il_images_match(il_container *ic1, il_container *ic2)
{
    return il_image_match(ic1,
                          ic2->display_type,
                          ic2->url_address,
                          ic2->background_color,
                          ic2->image->header.color_space->pixmap_depth,
                          ic2->dest_width, ic2->dest_height);
}

static il_container *
il_find_in_cache(IL_DisplayType display_type,
                 uint32 hash,
                 const char *image_url,
                 IL_IRGB* background_color,
                 int req_depth,
                 int req_width,
                 int req_height)
{
	il_container *ic=0;
	PR_ASSERT(hash);
	for (ic=il_cache.head; ic; ic=ic->next)
	{
		if (ic->hash != hash)
            continue;
        if (il_image_match(ic, display_type, image_url, background_color, req_depth,
                           req_width, req_height))
			break;
	}
	if (ic)
	{
		ILTRACE(2,("il:  found ic=0x%08x in cache\n", ic));
        return ic;
	}

#ifndef STANDALONE_IMAGE_LIB
#ifndef M12N                    /* XXXM12N Cache trace: cleanup/eliminate. */
    if (il_cache_trace) {
        char buffer[1024];
        MWContext *alert_context =
            XP_FindContextOfType(NULL, MWContextBrowser);
        if(!alert_context)
            alert_context = XP_FindContextOfType(NULL, MWContextPane);

        XP_SPRINTF(buffer,
        			XP_GetString(XP_MSG_IMAGE_NOT_FOUND),
        			image_url);


        if (alert_context)
            FE_Alert(alert_context, buffer);
    }
#endif /* M12N */
#endif /* STANDALONE_IMAGE_LIB */
    
    return NULL;
}

static void
il_addtocache(il_container *ic);

il_container *
il_get_container(IL_GroupContext *img_cx,    
                 NET_ReloadMethod cache_reload_policy,
                 const char *image_url,
                 IL_IRGB *background_color,
                 IL_DitherMode dither_mode,
                 int req_depth,
                 int req_width,  /* Target width requested by client. */
                 int req_height) /* Target height requested by client. */
{
    uint32 urlhash, hash;
    il_container *ic;
    
    int result;

    urlhash = hash = il_hash(image_url);

    /*
     * Take into account whether color rendering preferences have
     * changed since the image was cached.
     */
    hash += 21117 * (int)dither_mode;
           
    /* Check the cache */
    ic = il_find_in_cache(img_cx->display_type, hash, image_url,
                          background_color, req_depth, req_width, req_height);
    
    if (ic) {
       
        /* We already started to discard this image container.
           Make a new one.*/
        if ((ic->state == IC_ABORT_PENDING))
            ic = NULL;

		/* Check if we have to reload or if there's an expiration date   */
		/* and we've expired or if this is a JavaScript generated image. */
		/* We don't want to cache JavaScript generated images because:   */
		/* 1) The assumption is that they are generated and might change */
		/*    if the page is reloaded.                                   */
		/* 2) Their namespace crosses document boundaries, so caching    */
		/*    could result in incorrect behavior.                        */

        else if((cache_reload_policy == NET_SUPER_RELOAD) ||
		((cache_reload_policy == NET_NORMAL_RELOAD) && (!ic->forced)) ||
				 (cache_reload_policy != NET_CACHE_ONLY_RELOAD &&
				  ic->expires && (time(NULL) > ic->expires))
#ifndef STANDALONE_IMAGE_LIB
				  || (NET_URL_Type(ic->url_address) == MOCHA_TYPE_URL)
#endif /* STANDALONE_IMAGE_LIB */
        ) {
            /* Get rid of the old copy of the image that we're replacing. */
            if (!ic->is_in_use) 
            {
                il_removefromcache(ic);
                il_delete_container(ic);
            }
            ic = NULL;
            
        }
    }

    /* Reorder the cache list so it remains in LRU order*/
    if (ic)
        il_removefromcache(ic);

    /* There's no existing matching container.  Make a new one. */
    if (!ic) {
		ic = PR_NEWZAP(il_container);
        if (!ic)
            return NULL;

        /* Allocate the source image header. */
		if (!(ic->src_header = PR_NEWZAP(NI_PixmapHeader))) {
            PR_FREEIF(ic);
            return NULL;
        }

        /* Allocate the source image's colorspace.  The fields will be
           filled in by the image decoder. */
        if (!(ic->src_header->color_space = PR_NEWZAP(IL_ColorSpace))) {
            PR_FREEIF(ic->src_header);
            PR_FREEIF(ic);
            return NULL;
        }
        IL_AddRefToColorSpace(ic->src_header->color_space);

        /* Allocate the destination image structure.  A destination mask
           structure will be allocated later if the image is determined to
           be transparent and no background_color has been provided. */
	if (!(ic->image = PR_NEWZAP(IL_Pixmap))) {
            IL_ReleaseColorSpace(ic->src_header->color_space);
            PR_FREEIF(ic->src_header);
            PR_FREEIF(ic);
            return NULL;
        }
        
        /* Set the destination image colorspace to be the source image's
           colorspace.  The Front Ends can override this with the display
           colorspace. */
        ic->image->header.color_space = ic->src_header->color_space;
        IL_AddRefToColorSpace(ic->image->header.color_space);

        /* Save the requested background color in the container. */
        if (background_color) {
            if (!(ic->background_color = PR_NEWZAP(IL_IRGB))) {
                PR_FREEIF(ic->image);
                IL_ReleaseColorSpace(ic->src_header->color_space);
                IL_ReleaseColorSpace(ic->image->header.color_space);
                PR_FREEIF(ic->src_header);
                PR_FREEIF(ic);
                return NULL;
            }
            XP_MEMCPY(ic->background_color, background_color, sizeof(IL_IRGB));
        }
        else {
            ic->background_color = NULL;
        }

        ILTRACE(2, ("il: create ic=0x%08x\n", ic));

        ic->hash = hash;
        ic->urlhash = urlhash;
        ic->url_address = PL_strdup(image_url);
		ic->is_url_loading = PR_FALSE;
        ic->dest_width  = req_width;
        ic->dest_height = req_height;

        /* The image context is saved for use during decoding only. */
        ic->img_cx = img_cx;
     
        /* The type of display for which this image is being decoded e.g.
           screen, printer, postscript. */
        ic->display_type = img_cx->display_type;

        /* Certain callbacks, like DestroyPixmap, can be invoked after the
           image context has been destroyed, so we hold on to the callback
           interface and increment its reference count.  The reference count
           is decremented when the container is destroyed. */
        ic->img_cb = img_cx->img_cb;
        NS_ADDREF(ic->img_cb);

        /* callbacks for the  image decoders */
        ImgDCallbk* imgdcb;

        imgdcb = new ImgDCallbk(ic);
        imgdcb->AddRef();
        nsresult res = imgdcb->QueryInterface(kImgDCallbkIID, (void**)&imgdcb);

        if (NS_FAILED(res)) {
	  if(imgdcb)
             *imgdcb = NULL;
          delete imgdcb; 
	  return NULL;
	}
/*
        result = ImgDCallbk::CreateInstance( kImgDCallbkCID, ic, kImgDCallbkIID,
                                             (void **)&imgdcb);
        if(NS_FAILED(result))
          return  NULL;
*/

        imgdcb->SetContainer(ic);
        ic->imgdcb = imgdcb;           
    }
    
    il_addtocache(ic);
    ic->is_in_use = TRUE;
    
    return ic;
}

void
il_scour_container(il_container *ic)
{
	/* scour quantize, ds, scalerow etc */
	if (ic->image->header.color_space->type == NI_PseudoColor)
		il_free_quantize(ic);
    FREE_IF_NOT_NULL(ic->scalerow);

	/* reset a bunch of stuff */
	ic->url    = NULL;
    if (ic->net_cx)
        NS_RELEASE(ic->net_cx);
	ic->net_cx     = NULL;

	ic->forced                  = FALSE;

	ic->is_alone                = FALSE;
}

/*
 * destroy an il_container, freeing it and the image
 */
void
il_delete_container(il_container *ic)
{
    PR_ASSERT(ic);
    if (ic) {
        ILTRACE(2,("il: delete ic=0x%08x", ic));

        /*
         * We can't throw away this container until the netlib
         * stops sending us stuff.  We defer the actual deletion
         * of the container until then.
         */
        if (ic->is_url_loading) {
            ic->state = IC_ABORT_PENDING;
            return;
        }
        
        PR_ASSERT(ic->clients == NULL);
		il_scour_container(ic);


        PR_FREEIF(ic->background_color);

        PR_FREEIF(ic->src_header->transparent_pixel);
        IL_ReleaseColorSpace(ic->src_header->color_space);
        PR_FREEIF(ic->src_header);

		/* delete the image */
        if (!(ic->image || ic->mask))
            return;
        il_destroy_pixmap(ic->img_cb, ic->image);
        if (ic->mask)
            il_destroy_pixmap(ic->img_cb, ic->mask);
        NS_RELEASE(ic->img_cb);

        FREE_IF_NOT_NULL(ic->comment);
        FREE_IF_NOT_NULL(ic->url_address);
        FREE_IF_NOT_NULL(ic->fetch_url);
		
        PR_FREEIF(ic);
	}
}


/* Destroy an IL_Pixmap. */
void
#ifdef STANDALONE_IMAGE_LIB
il_destroy_pixmap(ilIImageRenderer *img_cb, IL_Pixmap *pixmap) 
#else
il_destroy_pixmap(IMGCBIF *img_cb, IL_Pixmap *pixmap) 
#endif /* STANDALONE_IMAGE_LIB */
{
    NI_PixmapHeader *header = &pixmap->header;

    /* Free the pixmap's bits, as well as the client_data. */
#ifdef STANDALONE_IMAGE_LIB
    img_cb->DestroyPixmap(NULL, pixmap);
#else
    IMGCBIF_DestroyPixmap(img_cb, NULL, pixmap);
#endif /* STANDALONE_IMAGE_LIB */

    /* Release memory used by the header. */
    IL_ReleaseColorSpace(header->color_space);
    PR_FREEIF(header->transparent_pixel);

    /* Release the IL_Pixmap. */
    PR_FREEIF(pixmap);
}


static char *
il_visual_info(il_container *ic)
{
	char *msg = (char *)PR_Calloc(1, 50);
    NI_PixmapHeader *img_header = &ic->image->header;

    if (!msg)
        return NULL;
    
    switch (img_header->color_space->type)
        {
        case NI_PseudoColor:
            XP_SPRINTF(msg, XP_GetString(XP_MSG_XBIT_COLOR),
                       img_header->color_space->pixmap_depth); /* #### i18n */
            break;

        case NI_GreyScale:
            if (img_header->color_space->pixmap_depth == 1)
               XP_SPRINTF(msg, XP_GetString(XP_MSG_1BIT_MONO)); /* #### i18n */
            else
               XP_SPRINTF(msg, XP_GetString(XP_MSG_XBIT_GREYSCALE),
                          img_header->color_space->pixmap_depth);
                                /* #### i18n */
             break;

        case NI_TrueColor:
            XP_SPRINTF(msg, XP_GetString(XP_MSG_XBIT_RGB),
                       img_header->color_space->pixmap_depth); /* #### i18n */
            break;

        default:
            PR_ASSERT(0);
            *msg=0;
            break;
		}

    return msg;
}

/* Define some macros to help us output HTML */
#define CELL_TOP 							\
	StrAllocCat(output, 					\
				"<TR><TD VALIGN=BASELINE ALIGN=RIGHT><B>");	
#define CELL_TITLE(title)					\
	StrAllocCat(output, title);
#define CELL_MIDDLE							\
	StrAllocCat(output, 					\
				"</B></TD>"					\
				"<TD>");
#define CELL_BODY(body)						\
	StrAllocCat(output, body);
#define CELL_END							\
	StrAllocCat(output, 					\
				"</TD></TR>");
#define ADD_CELL(c_title, c_body)			\
	CELL_TOP;								\
	CELL_TITLE(c_title);					\
	CELL_MIDDLE;							\
	CELL_BODY(c_body);						\
	CELL_END;

static char *
il_HTML_image_info(il_container *ic, int long_form, int show_comment)
{
    char tmpbuf[512];           /* Temporary consing area */
    char *output = NULL;
    NI_PixmapHeader *src_header = ic->src_header; /* Source image header. */
    NI_PixmapHeader *img_header = &ic->image->header; /* Destination image
                                                         header. */
    NI_IRGB *img_trans_pixel = img_header->transparent_pixel;

    XP_SPRINTF(tmpbuf, "%lu", (long)img_header->widthBytes *
               img_header->height + sizeof(il_container));
    ADD_CELL(XP_GetString(XP_MSG_DECODED_SIZE), tmpbuf); /* #### i18n */

    /* #### i18n */
#ifdef XP_WIN16
    XP_SPRINTF(tmpbuf, XP_GetString(XP_MSG_WIDTH_HEIGHT), (short)img_header->width,
               (short)img_header->height);
#else
    XP_SPRINTF(tmpbuf, XP_GetString(XP_MSG_WIDTH_HEIGHT), img_header->width,
               img_header->height);
#endif
    if ((img_header->width != src_header->width) ||
        (img_header->height != src_header->height))
    {
        /* #### i18n */
        XP_SPRINTF(tmpbuf + strlen(tmpbuf),  XP_GetString(XP_MSG_SCALED_FROM),
                   src_header->width, src_header->height);
    }
    /* #### i18n */
    ADD_CELL(XP_GetString(XP_MSG_IMAGE_DIM), tmpbuf);

    if (long_form) {

        char *visual_info = il_visual_info(ic);
        if (visual_info) {
            ADD_CELL(XP_GetString(XP_MSG_COLOR), visual_info);
                                                             /* #### i18n */
            PR_FREEIF(visual_info);
        }
        
        if (img_header->color_space->cmap.map)
            XP_SPRINTF(tmpbuf, XP_GetString(XP_MSG_NB_COLORS),
                       img_header->color_space->cmap.num_colors);
                                                             /* #### i18n */
        else
            XP_SPRINTF(tmpbuf, XP_GetString(XP_MSG_NONE));   /* #### i18n */
        ADD_CELL(XP_GetString(XP_MSG_COLORMAP), tmpbuf);
                
        if (img_trans_pixel) {
            if (ic->mask)
                XP_SPRINTF(tmpbuf,
                           /* #### i18n */
                           XP_GetString(XP_MSG_BCKDRP_VISIBLE));
            else
                XP_SPRINTF(tmpbuf,
                           /* #### i18n */
                           XP_GetString(XP_MSG_SOLID_BKGND),
                           img_trans_pixel->red,
                           img_trans_pixel->green,
                           img_trans_pixel->blue);
        } else {
            XP_SPRINTF(tmpbuf, XP_GetString(XP_MSG_JUST_NO));    /* #### i18n */
        }
        ADD_CELL(XP_GetString(XP_MSG_TRANSPARENCY), tmpbuf);    /* #### i18n */
    }

    if (show_comment && ic->comment) {
        XP_SPRINTF(tmpbuf, "%.500s", ic->comment);
        ADD_CELL(XP_GetString(XP_MSG_COMMENT), tmpbuf);    /* #### i18n */
    }

    return output;
}

IL_IMPLEMENT(char *)
IL_HTMLImageInfo(char *url_address)
{
    il_container *ic;
    char *output = NULL;
    char *il_msg;
    
	for (ic=il_cache.head; ic; ic=ic->next)
	{
		if (!strcmp(ic->url_address, url_address))
			break;
	}

    if ((ic == NULL) || (ic->state != IC_COMPLETE))
        return NULL;

    il_msg = il_HTML_image_info(ic, TRUE, TRUE);
    if (il_msg == NULL)
        return NULL;

    StrAllocCat(output,
                "<TABLE CELLSPACING=0 CELLPADDING=1 "
                "BORDER=0 ALIGN=LEFT WIDTH=66%>");
    StrAllocCat(output, il_msg);
    StrAllocCat(output, "</TABLE> <A HREF=\"");
    StrAllocCat(output, url_address);
    StrAllocCat(output, "\"> <IMG WIDTH=90% ALIGN=CENTER SRC=\"");
    StrAllocCat(output, url_address);
    StrAllocCat(output, "\"></A>\n");

    PR_FREEIF(il_msg);

    return output;
}

#ifndef STANDALONE_IMAGE_LIB
/*
 * Create an HTML stream and generate HTML describing
 * the image cache.  Use "about:memory-cache" URL to acess.
 */
int 
IL_DisplayMemCacheInfoAsHTML(FO_Present_Types format_out, URL_Struct *urls,
                             OPAQUE_CONTEXT *cx)
{
	char buffer[2048];
    char *output = NULL;
    char *img_info;
	char *address;
	char *escaped;
   	NET_StreamClass *stream;
	Bool long_form = FALSE;
	int status;

	if(PL_strcasestr(urls->address, "?long"))
		long_form = TRUE;
#ifndef M12N                    /* XXXM12N Cache trace: cleanup/eliminate. */
	else if(PL_strcasestr(urls->address, "?traceon"))
		il_cache_trace = TRUE;
	else if(PL_strcasestr(urls->address, "?traceoff"))
		il_cache_trace = FALSE;
#endif /* M12N */

	StrAllocCopy(urls->content_type, TEXT_HTML);
	format_out = CLEAR_CACHE_BIT(format_out);

	stream = NET_StreamBuilder(format_out, urls, (MWContext *)cx);
	if (!stream)
		return 0;

	/*
     * Define a macro to push a string up the stream
	 * and handle errors
	 */
#define PUT_PART(part)														\
{                                                                           \
    status = (*stream->put_block)(stream,						\
                                  part ? part : "Unknown",					\
                                  part ? PL_strlen(part) : 7);				\
    if (status < 0)															\
        goto END;                                                           \
}

    /* Send something to the screen immediately.  This will force all
     * the images on the page to enter the cache now. Otherwise,
     * layout will be causing cache state to be modified even as we're
     * trying to display it.
     */
    PL_strcpy(buffer,
              "<TITLE>Information about the Netscape image cache</TITLE>\n");
    PUT_PART(buffer);
                                  
 	XP_SPRINTF(buffer, 
			   "<h2>Image Cache statistics</h2>\n"
			   "<TABLE CELLSPACING=0 CELLPADDING=1>\n"
			   "<TR>\n"
			   "<TD ALIGN=RIGHT><b>Maximum size:</b></TD>\n"
			   "<TD>%ld</TD>\n"
			   "</TR>\n"
			   "<TR>\n"
			   "<TD ALIGN=RIGHT><b>Current size:</b></TD>\n"
			   "<TD>%ld</TD>\n"
			   "</TR>\n"
			   "<TR>\n"
			   "<TD ALIGN=RIGHT><b>Number of images in cache:</b></TD>\n"
			   "<TD>%d</TD>\n"
			   "</TR>\n"
			   "<TR>\n"
			   "<TD ALIGN=RIGHT><b>Average cached image size:</b></TD>\n"
			   "<TD>%ld</TD>\n"
			   "</TR>\n"
			   "</TABLE>\n"
			   "<HR>",
			   (long)il_cache.max_bytes,
			   (long)il_cache.bytes,
			   il_cache.items,
			   (il_cache.bytes ?
                (long)il_cache.bytes / il_cache.items : 0L));

	PUT_PART(buffer);

    {
        il_container *ic, *l = NULL;
        for (ic=il_cache.head; ic; ic=ic->next) {
            
            /* Sanity check */
            if (l)
                PR_ASSERT(ic->prev == l);
            l = ic;

            /* Don't display uninteresting images */
            if ((ic->state != IC_COMPLETE) &&
                (ic->state != IC_SIZED)    &&
                (ic->state != IC_MULTI))
                continue;
            
            StrAllocCat(output, "<TABLE>\n");

            /* Emit DocInfo link to URL */
            address = ic->url_address;
            PL_strcpy(buffer, "<A TARGET=Internal_URL_Info HREF=\"about:");
            escaped = NET_EscapeDoubleQuote(address);
            PL_strcat(buffer, escaped);
            PR_Free(escaped);
            PL_strcat(buffer, "\">");
            escaped = NET_EscapeHTML(address);
            PL_strcat(buffer, escaped);
            PR_Free(escaped);
            PL_strcat(buffer, "</A>");
            ADD_CELL("URL:", buffer);
            
            /* Rest of image info (size, colors, depth, etc.) */
            img_info = il_HTML_image_info(ic, long_form, FALSE);
            if (img_info) {
                StrAllocCat(output, img_info);
                PR_FREEIF(img_info);
            }
            
            StrAllocCat(output, "</TABLE><P>\n");
        }

        if (output)
            PUT_PART(output);
    }
        
  END:
     
    PR_FREEIF(output);
     
	if(status < 0)
		(*stream->abort)(stream, status);
	else
		(*stream->complete)((NET_StreamClass *)stream);

	return 1;
}
#endif

il_container *
il_removefromcache(il_container *ic)
{
    int32 image_bytes;
    NI_PixmapHeader *img_header = &ic->image->header;

	PR_ASSERT(ic); 
	if (ic)
	{
		ILTRACE(2,("il: remove ic=0x%08x from cache\n", ic));
        PR_ASSERT(ic->next || ic->prev || (il_cache.head == il_cache.tail));

        /* Remove entry from doubly-linked list. */
		if (il_cache.head == ic)
            il_cache.head = ic->next;

        if (il_cache.tail == ic)
            il_cache.tail = ic->prev;

        if (ic->next)
            ic->next->prev = ic->prev;
        if (ic->prev)
            ic->prev->next = ic->next;

        ic->next = ic->prev = NULL;

        image_bytes = ((int32) img_header->widthBytes) * img_header->height;

/* this assert gets triggered all the time. Take it out or fix it */
/*        PR_ASSERT (il_cache.bytes >= (int32)image_bytes); */

        if (il_cache.bytes <  (int32)image_bytes)
            il_cache.bytes = 0;
        else        
            il_cache.bytes -= image_bytes;
		il_cache.items--;
        PR_ASSERT(il_cache.items >= 0);
	}
	return ic;
}

void
il_adjust_cache_fullness(int32 bytes)
{
    il_cache.bytes += bytes;
    PR_ASSERT(il_cache.bytes >= 0);

    /* Safety net - This should never happen. */
    if (il_cache.bytes < 0)
        il_cache.bytes = 0;

	/* We get all of the memory cache.  (Only images are cached in memory.) */
    il_reduce_image_cache_size_to(image_cache_size);

    /* Collect some debugging info. */
    if (il_cache.bytes > il_cache.max_bytes)
        il_cache.max_bytes = il_cache.bytes;
}

static void
il_addtocache(il_container *ic)
{
    NI_PixmapHeader *img_header = &ic->image->header;

	ILTRACE(2,("il:    add ic=0x%08x to cache\n", ic));
    PR_ASSERT(!ic->prev && !ic->next);

    /* Thread onto doubly-linked list, kept in LRU order */
    ic->prev = NULL;
    ic->next = il_cache.head;
	if (il_cache.head)
		il_cache.head->prev = ic;
    else {
        PR_ASSERT(il_cache.tail == NULL);
        il_cache.tail = ic;
    }
    il_cache.head = ic;

    /* Image storage is added in when image is sized */
    if (ic->sized)
        il_cache.bytes += (uint32)img_header->widthBytes * img_header->height;
	il_cache.items++;
}


/* Set limit on approximate size, in bytes, of all pixmap storage used
   by the imagelib.  */
IL_IMPLEMENT(void)
IL_SetCacheSize(uint32 new_size)
{
	image_cache_size = new_size;
    il_reduce_image_cache_size_to(new_size);
}


void
il_reduce_image_cache_size_to(uint32 new_size)
{
    int32 last_size = 0;
    
	while ((il_cache.bytes > (int32)new_size) && (il_cache.bytes != last_size)) {
        last_size = il_cache.bytes;
        IL_ShrinkCache();
    }
}

IL_IMPLEMENT(uint32)
IL_ShrinkCache(void)
{
	il_container *ic;
	ILTRACE(3,("shrink"));

    for (ic = il_cache.tail; ic; ic = ic->prev)
	{
		if (ic->is_in_use)
            continue;

#ifndef STANDALONE_IMAGE_LIB        
#ifndef M12N                    /* XXXM12N Cache trace: cleanup/eliminate. */
        if (il_cache_trace) {
            char buffer[1024];
            MWContext *alert_context =
                XP_FindContextOfType(NULL, MWContextBrowser);
            if(!alert_context)
                alert_context = XP_FindContextOfType(NULL, MWContextPane);
            
            sprintf(buffer,
                    XP_GetString(XP_MSG_COMPRESS_REMOVE), ic->url_address);

            if (alert_context)
                FE_Alert(alert_context, buffer);
        }
#endif /* M12N */
#endif /* STANDALONE_IMAGE_LIB */
        il_removefromcache(ic);
        il_delete_container(ic);
        break;
    }

    return il_cache.bytes;
}

IL_IMPLEMENT(uint32)
IL_GetCacheSize()
{	
	return il_cache.bytes;
}


/* Attempt to release the memory used by a specific image in the image
   cache.  The memory won't be released if the image is still in use
   by one or more clients.  XXX - Can we get rid of this call ?  Why
   the hell do we need this ? */
IL_IMPLEMENT(void)
IL_UnCache(IL_Pixmap *pixmap)
{
	il_container *ic;
	if (pixmap)
	{
		for (ic=il_cache.head; ic; ic=ic->next)
		{
            /* Check the pixmap argument against both the image and mask
               pixmaps of the container. */
			if (((ic->image == pixmap) || (ic->mask == pixmap)) &&
                !ic->is_in_use)
			{
				il_removefromcache(ic);
				il_delete_container(ic);
				return;
			}
		}
	}
}


/* Free num_bytes of memory by flushing the Least Recently Used (LRU) images
   from the image cache. */
IL_IMPLEMENT(void)
IL_FreeMemory(IL_GroupContext *image_context, uint32 num_bytes)
{
    /* XXXM12N Implement me. */
}

PRBool
il_add_client(IL_GroupContext *img_cx, il_container *ic,
              IL_ImageReq* image_req, int is_view_image)
{
    PRBool added_context;

    ILTRACE(3, ("il: add_client ic=0x%08x, image_req =0x%08x\n", ic,
                image_req));

    image_req->is_view_image = is_view_image;

	/* Add the client to the end of the container's client list. */
	if (!ic->lclient)
		ic->clients = image_req;
	else
		ic->lclient->next = image_req;
	ic->lclient = image_req;

    /* Now add the client context to the container's client context list,
       (if necessary,) and also add the container to the context's list of
       containers.  Note: a FALSE return value could mean that the context
       did not need to be added. */
    added_context = il_add_client_context(img_cx, ic);
    
    /* Always return TRUE. */
    return PR_TRUE;
}

/* Delete an IL_ImageReq from the list of clients for an image container.
   Return TRUE if successful, FALSE otherwise. */
static PRBool
il_delete_client(il_container *ic, IL_ImageReq *image_req)
{
    IL_GroupContext *img_cx;
    ilINetContext *net_cx;
    IL_ImageReq *current_req;
    IL_ImageReq *prev_req = NULL;

    /* Search for the given image request in the container's client list. */
    current_req = ic->clients;
    while (current_req) {
        if (current_req == image_req)
            break;
        prev_req = current_req;
        current_req = current_req->next;
    }

    /* If the image request wasn't found in the client list, return FALSE. */
    if (!current_req)
        return PR_FALSE;

    /* Image request was found in the client list i.e. current_req is the
       same as image_req. */
    if (current_req == ic->clients)
        ic->clients = current_req->next;
    else
        prev_req->next = current_req->next;

    if (current_req == ic->lclient)
        ic->lclient = prev_req;
    
    img_cx = current_req->img_cx;
    net_cx = current_req->net_cx;   /* This is destroyed below. */
    PR_FREEIF(current_req);           /* Delete the image request. */

    /* Decrement the number of unique images for the given image request's
       image context, but be careful to do this only when there are no other
       clients of this image with the same context. */
    current_req = ic->clients;
    while (current_req) {
        if (current_req->img_cx == img_cx)
            break;
        current_req = current_req->next;
    }
    if (!current_req) {         /* The given image request's context had no
                                   other requests associated with the image. */
        PRBool removed_context;
        
        /* Remove the context from the container's list of client contexts.
           Also remove the container from the context's list of containers. */
        removed_context = il_remove_client_context(img_cx, ic);

         if (ic->clients) {
            /* If the deleted image request's image context happened to be the
               image context used by the container, then we (arbitrarily) set the
               container's image context to be that of the first client in the
               list.  We only have to do this if the client list isn't empty. */ 
             if (ic->img_cx == img_cx)
                 ic->img_cx = ic->clients->img_cx;
            /* The container's net context may be about to become invalid, so
               give the container a different one which is known to be valid. */
             if (ic->net_cx == net_cx)
             {
                 NS_RELEASE(ic->net_cx);
                 ic->net_cx = ic->clients->net_cx->Clone();
             }
        } 
    }

	/* Destroy the net context for the client we just destroyed. */
    NS_RELEASE(net_cx);

    ILTRACE(3, ("il: delete_client ic=0x%08x, image_req =0x%08x\n", ic,
                image_req));
 
    return PR_TRUE;
}

static void
il_delete_all_clients(il_container *ic)
{
    IL_ImageReq *image_req;
    IL_ImageReq *next_req = NULL;

    for (image_req = ic->clients; image_req; image_req = next_req) {
        next_req = image_req->next;
        il_delete_client(ic, image_req);
    }
}

#if 1
/* One-time image library initialization.
   = Initialize internal state
   = Scan image plug-in directory
   = Register individual image decoders with the netlib */
IL_IMPLEMENT(int)
#ifdef STANDALONE_IMAGE_LIB
IL_Init(ilISystemServices *ss)
#else
IL_Init()
#endif /* STANDALONE_IMAGE_LIB */
{
    if (il_log_module == NULL) {
        il_log_module = PR_NewLogModule("IMGLIB");
    }

#ifdef STANDALONE_IMAGE_LIB
    il_ss = ss;
#endif /* STANDALONE_IMAGE_LIB */

    /* XXXM12N - finish me. */
    return TRUE;
}
#endif

/* Used when exiting the client, this code frees all imagelib data structures.
   This is done for two reasons:
     1) It makes leakage analysis of the heap easier, and
     2) It causes resources to be freed on Windows that would otherwise persist
        beyond the browser's lifetime.
 
   XXX - Actually, right now it just frees the cached images.  There are still
         a couple of other places that leaks need to be fixed. */
IL_IMPLEMENT(void)
IL_Shutdown()
{
    il_container *ic, *ic_next;
    for (ic = il_cache.head; ic; ic = ic_next)
	{
        ic_next = ic->next;
        il_delete_all_clients(ic);
        il_removefromcache(ic);
        il_delete_container(ic);
    }

    PR_ASSERT(il_cache.bytes == 0);
}

/* Release a reference to an image lib request.  If there are no other
   clients of the request's associated pixmap, any related netlib
   activity is terminated and pixmap storage may be reclaimed.  Also,
   if the last client is removed, the container can be reclaimed,
   either by storing it in the cache or throwing it away.
*/
IL_IMPLEMENT(void)
IL_DestroyImage(IL_ImageReq *image_req)
{
    IL_GroupContext *img_cx;
    PRBool client_deleted;
    il_container *ic, *ic2, *ic2_next;
    il_container_list *ic_list;

    /* Check for a NULL request. */
    if (!image_req)
        return;

    /* Notify observers that the image request is being destroyed.  This
       provides an opportunity for observers to remove themselves from the
       observer callback list and to do any related cleanup. */
    il_image_destroyed_notify(image_req);
    
    /* Get the container and context for this image request. */
    ic = image_req->ic;
    img_cx = image_req->img_cx;

    /* Certain requests, such as requests for internal image icons, do not
       have an image container.  For these cases, there is nothing else to
       be done, so return early.
       XXXM12N This will not be necessary once icons are completely separated
       from the Image Library. */
    if (!ic){
	/* editing icons don't have ic's but need to be freed */	
	if( image_req->net_cx )
		NS_RELEASE( image_req->net_cx );
	PR_FREEIF( image_req );
        return;
    }

    ic_list = img_cx->container_list;
    ILTRACE(1, ("il: IL_DestoyImage: ic = 0x%08x\n", ic));
    PR_ASSERT(ic_list);
    if (!ic_list)
        return;

    /* Delete the image request from the container's list of clients. */
    client_deleted = il_delete_client(ic, image_req);
    PR_ASSERT(client_deleted);
    
    /* The image container can't be deleted until all clients are done. */
    if (ic->clients)
        return;

    /* Don't allow any new streams to be created in
       il_image_complete() as a result of a looping animation.  (It's
       possible that this image is being freed before the stream
       completes/aborts.  This can happen in autoscroll mode and the
       editor apparently allows this to occur also.) */
    ic->loop_count = 0;

    il_image_abort(ic);
    
    PR_ASSERT(img_cx->num_containers >= 0);
    if (!img_cx->num_containers) {
        PR_ASSERT(!img_cx->container_list);
        img_cx->container_list = 0;
        img_cx->num_loading = 0;
        img_cx->num_aborted = 0;
    }

    if ((ic->state != IC_COMPLETE) || ic->multi ||
        ic->rendered_with_custom_palette
#ifndef STANDALONE_IMAGE_LIB
         || (NET_URL_Type(ic->url_address) == MOCHA_TYPE_URL)
#endif /* STANDALONE_IMAGE_LIB */
        ) {
        il_removefromcache(ic);
        il_delete_container(ic);
        return;
    }

    /* Don't allow duplicate entries in cache */
    for (ic2 = il_cache.head; ic2; ic2 = ic2_next) {
        ic2_next = ic2->next;
        if ((!ic2->is_in_use) && il_images_match(ic, ic2)) {
            il_removefromcache(ic2);
            il_delete_container(ic2);
        }
    }

    ic->is_in_use = FALSE;
}


/* This routine is a "safety net", in case layout didn't free up all the
 * images on a page.  It assumes layout made a mistake and frees them anyway.
 */
IL_IMPLEMENT(void)
IL_DestroyImageGroup(IL_GroupContext *img_cx)
{
    IL_ImageReq *image_req, *next_req;
    il_container *ic;
    il_container_list *ic_list, *ic_list_next;
    
    if (img_cx == NULL)
        return;
	
    if (img_cx->num_containers > 0) {
        PR_ASSERT(img_cx->container_list);

		for (ic_list = img_cx->container_list; ic_list;
             ic_list = ic_list_next) {
            ic_list_next = ic_list->next;
            ic = ic_list->ic;            
            ic->forced = 0;

            for (image_req = ic->clients; image_req; image_req = next_req) {
                next_req = image_req->next;
                if (image_req->img_cx == img_cx)
                    IL_DestroyImage(image_req);
            }
        }
        PR_ASSERT(img_cx->num_containers == 0);
        PR_ASSERT(img_cx->container_list == NULL);
    }
  
    ILTRACE(1, ("il: IL_DestroyImageGroup\n"));
}
