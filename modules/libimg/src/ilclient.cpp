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
 *   ilclient.c --- Management of imagelib client data structures,
 *                  including image cache.
 *
 */


#include "if.h"
#include "nsIImgDecoder.h"
#include "nsImgDCallbk.h"
#include "ilISystemServices.h"
#include "nsIFactory.h"
#include "nsCRT.h"
#include "xpcompat.h" //temporary, for timers

#undef PIN_CHROME 
/* Note that default cache size is set in 
   src/gfx/nsImageManager.cpp, ~line 62.
*/
static PRUint32 image_cache_size;
static PRUint32 max_cache_items = 192;

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
NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
NS_DEFINE_IID(kIImgDCallbkIID, NS_IIMGDCALLBK_IID);

ImgDCallbk::~ImgDCallbk()
{
    if(ilContainer) 
        il_delete_container(ilContainer);
}

NS_IMPL_ISUPPORTS(ImgDCallbk, kIImgDCallbkIID)

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
    ic->imgdcb = NULL;
  }
  return res;
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
        (((PRUint32)req_width == ic->dest_width) &&
         ((PRUint32)req_height == ic->dest_height)) ||
        /* Width matches, request height zero, aspect ratio same. */
        (ic_sized && ((PRUint32)req_width == ic->dest_width) && !req_height &&
         !ic->aspect_distorted) ||
        /* Height matches, request width zero, aspect ratio same. */
        (ic_sized && ((PRUint32)req_height == ic->dest_height) && !req_width &&
         !ic->aspect_distorted) ||
        /* Request dimensions zero, cache entry has natural dimensions. */
        (!req_width && !req_height && ic->natural_size)
        ))
        return PR_FALSE;

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
                    return PR_FALSE;
                if (background_color->green != ic->background_color->green)
                    return PR_FALSE;
                if (background_color->blue != ic->background_color->blue)
                    return PR_FALSE;
            }
            else {
                /* A background color was requested, but the candidate does
                   not have one. */
                return PR_FALSE;
            }
        }
        else {                      /* No background color was requested. */
            if (ic->background_color) {
                /* A background color was not requested, but the candidate
                   has one.  This means that while the current request may
                   need a mask, the candidate definitely does not have one. */
                return PR_FALSE;
            }
            else {
                /* Neither the request nor the candidate have a background
                   color, so don't disqualify the candidate. */
            }
        }
    }
    
    /* Check the url (we have already checked the hash value which is based
       on the url.) */
    if (nsCRT::strcmp(image_url, ic->url_address))
        return PR_FALSE;

    /* Printer contexts and window contexts may have different
       native image formats, so don't reuse an image cache entry
       created for an onscreen context in a printer context or
       vice-versa. */
    if (display_type != ic->display_type)
        return PR_FALSE;


    if((ic->display_type==IL_Printer) &&
       (ic->dest_width != ic->image->header.width) &&
       (ic->dest_height != ic->image->header.height ))
          return PR_FALSE;

    /* XXX - temporary */
    if (ic->rendered_with_custom_palette)
        return PR_FALSE;
                
    return PR_TRUE;
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
                 PRUint32 hash,
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
  return NULL;
}

static void
il_addtocache(il_container *ic);

il_container *
il_get_container(IL_GroupContext *img_cx,    
                 ImgCachePolicy cache_reload_policy,
                 const char *image_url,
                 IL_IRGB *background_color,
                 IL_DitherMode dither_mode,
                 int req_depth,
                 int req_width,  /* Target width requested by client. */
                 int req_height) /* Target height requested by client. */
{
    PRUint32 urlhash, hash;
    il_container *ic=NULL;
    PRBool isChromeAlready = PR_FALSE;
    
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
       
	ILTRACE(2,("il:  il_get_container: found ic=0x%08x in cache for url %s\n", ic, image_url));

        /* This ic is being destroyed. Need a new one */
        if ((ic->state == IC_ABORT_PENDING))
            ic = NULL;

        /* Check if we have to reload or if there's an expiration date   */
        /* and we've expired or if this is a JavaScript generated image. */
        /* We don't want to cache JavaScript generated images because:   */
        /* 1) The assumption is that they are generated and might change */
        /*    if the page is reloaded.                                   */
        /* 2) Their namespace crosses document boundaries, so caching    */
        /*    could result in incorrect behavior.                        */

	// if it is chrome, record that fact, and ensure it is not purged

        if(cache_reload_policy == DONT_USE_IMG_CACHE && ic->moz_type != TYPE_CHROME){
		ILTRACE(2,("il:  il_get_container: DONT_USE_IMG_CACHE ic=0x%08x discarding\n", ic)); 
            /* Don't use old copy and purge it from cache.*/
            	if (!ic->is_in_use) {
                	il_removefromcache(ic);
                	il_delete_container(ic);
            	}
            	ic = NULL;
        }
    }

#ifdef PIN_CHROME
    /* Reorder the cache list so it remains in LRU order*/
    if (ic && ic->moz_type != TYPE_CHROME)
        il_removefromcache(ic);
    else if (ic && ic->moz_type == TYPE_CHROME)
	    isChromeAlready = PR_TRUE;
#else
    if(ic)
        il_removefromcache(ic);
#endif

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

        /* Clear the flag to indicate that this is not a mask */
        ic->image->header.is_mask = PR_FALSE;

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
            nsCRT::memcpy(ic->background_color, background_color, sizeof(IL_IRGB));
        }
        else {
            ic->background_color = NULL;
        }

        ILTRACE(2, ("il: create ic=0x%08x\n", ic));

#ifdef PIN_CHROME
        if((nsCRT::strncmp(image_url,"chrome", 6) == 0)&&
		(nsCRT::strncmp(image_url+(nsCRT::strlen(image_url) - 9), "throb", 4) != 0)&&
		(nsCRT::strncmp(image_url+(nsCRT::strlen(image_url) - 9), "busy", 4) != 0))
		    ic->moz_type = TYPE_CHROME;
	    else 
#endif
		    ic->moz_type = TYPE_DEFAULT;

        ic->hash = hash;
        ic->urlhash = urlhash;
        ic->url_address = nsCRT::strdup(image_url);
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
        ImgDCallbk* imgdcb = new ImgDCallbk(ic);
        if (!imgdcb) {

          PR_FREEIF(ic->image);
          IL_ReleaseColorSpace(ic->src_header->color_space);
          IL_ReleaseColorSpace(ic->image->header.color_space);
          PR_FREEIF(ic->src_header);
          nsCRT::free(ic->url_address);
          PR_FREEIF(ic);
          return NULL;
        }
        nsresult res = imgdcb->QueryInterface(kIImgDCallbkIID, (void**)&imgdcb);
        if (NS_FAILED(res)) {
          delete imgdcb; 
        
          PR_FREEIF(ic->image);
          IL_ReleaseColorSpace(ic->src_header->color_space);
          IL_ReleaseColorSpace(ic->image->header.color_space);
          PR_FREEIF(ic->src_header);
          nsCRT::free(ic->url_address);
          PR_FREEIF(ic);
          return NULL;
        }
        imgdcb->SetContainer(ic);
        ic->imgdcb = imgdcb;
    }
#ifdef PIN_CHROME
    if ( isChromeAlready == PR_FALSE )  // image is new, or existing and type is
				 // not TYPE_CHROME
#endif
	    il_addtocache(ic);

    ic->is_in_use = PR_TRUE;
    
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

    ic->forced                  = PR_FALSE;

    ic->is_alone                = PR_FALSE;
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
#ifdef DEBUG_kipp
            printf("il_delete_container: bad: can't delete ic=%p '%s'\n",
                   ic, ic->url_address ? ic->url_address : "(null)");
#endif
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
        if (!(ic->image || ic->mask)) {
#ifdef DEBUG_kipp
            printf("il_delete_container: bad: ic=%p '%s' image=%p mask=%p\n",
                   ic, ic->url_address ? ic->url_address : "(null)",
                   ic->image, ic->mask);
#endif
            return;
        }
        il_destroy_pixmap(ic->img_cb, ic->image);
        if (ic->mask)
            il_destroy_pixmap(ic->img_cb, ic->mask);
        NS_RELEASE(ic->img_cb);
        if (ic->imgdcb) {
          ic->imgdcb->SetContainer(nsnull);
          NS_RELEASE(ic->imgdcb);
        }
        
        if (ic->imgdec) {
          ic->imgdec->SetContainer(nsnull);
          NS_RELEASE(ic->imgdec);
        }

        FREE_IF_NOT_NULL(ic->comment);
        nsCRT::free(ic->url_address);
        nsCRT::free(ic->type);
        FREE_IF_NOT_NULL(ic->fetch_url);
        
        PR_FREEIF(ic);
    }
}


/* Destroy an IL_Pixmap. */
void
il_destroy_pixmap(ilIImageRenderer *img_cb, IL_Pixmap *pixmap) 
{
    NI_PixmapHeader *header = &pixmap->header;

    /* Free the pixmap's bits, as well as the client_data. */
    img_cb->DestroyPixmap(NULL, pixmap);

    /* Release memory used by the header. */
    IL_ReleaseColorSpace(header->color_space);
    PR_FREEIF(header->transparent_pixel);

    /* Release the IL_Pixmap. */
    PR_FREEIF(pixmap);
}



il_container *
il_removefromcache(il_container *ic)
{
    int32 image_bytes;
    NI_PixmapHeader *img_header = &ic->image->header;

    PR_ASSERT(ic); 
    if (ic)
    {

        if ( ic->moz_type == TYPE_CHROME )
     	    ILTRACE(2,("\nRemoving a chrome image from the image cache")); 

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
        il_cache.bytes += (PRUint32)img_header->widthBytes * img_header->height;
    il_cache.items++;
}


/* Set limit on approximate size, in bytes, of all pixmap storage used
   by the imagelib.  */
IL_IMPLEMENT(void)
IL_SetCacheSize(PRUint32 new_size)
{
    image_cache_size = new_size;
    il_reduce_image_cache_size_to(new_size);
}


void
il_reduce_image_cache_size_to(PRUint32 new_size)
{
    int32 last_size = 0;
#ifdef XP_PC
/*
We are limiting he number of items(containers)  in the 
imagelib cache to 192 for windows only.We need to limit 
the number of containers in the image cache for win95/98,
because each container contains an nsIImage instance, and 
each nsIImage instance on window (nsImageWin) holds onto 
a HBITMAP which is a GDI resource. The number of GDI resources 
is limited on windows. Hence by limiting the number
of items in the cache, we ensure that we do not run
 out of resources. Note that even if we limit the number of
items in the cache to 192, the actual number of items may be more
than this, depending on the actual number of nsIImage instances in use. 
Currently, we are making this restriction only on platforms where 
there is a known problem.
*/
    int32 last_num_items = 0;

    while ((il_cache.items > (int32)max_cache_items) && (il_cache.items != last_num_items)) {
        last_num_items = il_cache.items;
        IL_ShrinkCache();
    }
#endif

    while ((il_cache.bytes > (int32)new_size) && (il_cache.bytes != last_size)) {
        last_size = il_cache.bytes;
        IL_ShrinkCache();
    }
}

IL_IMPLEMENT(PRUint32)
IL_ShrinkCache(void)
{
    il_container *ic;
    ILTRACE(3,("shrink"));

    for (ic = il_cache.tail; ic; ic = ic->prev)
    {
        if (ic->is_in_use || ic->moz_type == TYPE_CHROME) {
            continue;
        }

        il_removefromcache(ic);
        il_delete_container(ic);
        break;
    }

    return il_cache.bytes;
}

IL_IMPLEMENT(void)
IL_FlushCache(PRUint8 img_catagory)
{
    ILTRACE(3,("flush"));

    il_container *ic = il_cache.head;
    while (ic)
    {
        if (ic->is_in_use ||((img_catagory == 0 )&&(ic->moz_type == TYPE_CHROME))){
#ifdef DEBUG_kipp
            printf("IL_FlushCache: il_container %p in use '%s'\n",
                   ic, ic->url_address ? ic->url_address : "(null)");
#endif
            ic = ic->next;
            continue;
        }

        il_removefromcache(ic);
        il_delete_container(ic);
        ic = il_cache.head;
    }
}

IL_IMPLEMENT(PRUint32)
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
                !ic->is_in_use || ic->moz_type != TYPE_CHROME)
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
IL_FreeMemory(IL_GroupContext *image_context, PRUint32 num_bytes)
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
    if (!ic->lclient) //this is the first
        ic->clients = image_req;
    else
        ic->lclient->next = image_req;
    ic->lclient = image_req;

    /* Now add the client context to the container's client context list,
       (if necessary,) and also add the container to the context's list of
       containers.  Note: a PR_FALSE return value could mean that the context
       did not need to be added. */
    added_context = il_add_client_context(img_cx, ic);
    
    /* Always return PR_TRUE. */
    return PR_TRUE;
}

/* Delete an IL_ImageReq from the list of clients for an image container.
   Return PR_TRUE if successful, PR_FALSE otherwise. */
PRBool
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

    /* If the image request wasn't found in the client list, return PR_FALSE. */
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
             if ((ic->net_cx )&&(ic->net_cx == net_cx)) 
             {
                 NS_RELEASE(ic->net_cx);
                 ic->net_cx = ic->clients->net_cx->Clone();
             }
        } 
    }

    /* Destroy the net context for the client we just destroyed. */
    NS_IF_RELEASE(net_cx);

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
IL_Init(ilISystemServices *ss)
{
    if (il_log_module == NULL) {
        il_log_module = PR_NewLogModule("IMGLIB");
    }
    il_ss = ss;

    /* XXXM12N - finish me. */
    return PR_TRUE;
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
	    ic->moz_type = TYPE_DEFAULT;	// avoid assert
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
    il_container *ic, *ic2 = NULL, *ic2_next = NULL;
    il_container_list *ic_list;

    /* Check for a NULL request. */
    if (!image_req)
        return;

    ic = image_req->ic;

    /* Get the container and context for this image request. */
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

    /* Notify observers that the image request is being destroyed.  This
       provides an opportunity for observers to remove themselves from the
       observer callback list and to do any related cleanup. */
    il_image_destroyed_notify(image_req);
    
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

    if (ic->moz_type != TYPE_CHROME &&
        ((ic->state != IC_COMPLETE) || ic->multi || ic->rendered_with_custom_palette)) 
    {
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

    ic->is_in_use = PR_FALSE;
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
                if (image_req->img_cx == img_cx){
                    IL_DestroyImage(image_req);
            }
        }
            
        }
#if 0	// XXX now that we don't delete chrome images, this will fire
        PR_ASSERT(img_cx->num_containers == 0);
        PR_ASSERT(img_cx->container_list == NULL);
#endif
    }
  
    ILTRACE(1, ("il: IL_DestroyImageGroup\n"));
}
