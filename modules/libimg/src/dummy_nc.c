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

/* This is a dummy Net Context which the Image Library uses for network
   operations in lieu of an MWContext.  It will be replaced by a true
   Net Context when the Network Library is modularized. */

#include "dummy_nc.h"
#include "il_strm.h"
#include "if.h"

typedef struct dum_TitleObsClosure {
    MWContext *context;
    XP_ObserverList obs_list;
} dum_TitleObsClosure;

extern void
lo_view_title( MWContext *context, char *title_str );

extern PRBool
il_load_image(OPAQUE_CONTEXT *cx, char *image_url, NET_ReloadMethod cache_reload_policy);


IL_NetContext *
IL_NewDummyNetContext(MWContext *context, NET_ReloadMethod cache_reload_policy)
{
    IL_NetContext *net_cx;

    net_cx = XP_NEW_ZAP(IL_NetContext);
    if (!net_cx)
        return NULL;

    net_cx->cx = context;
    net_cx->cache_reload_policy = cache_reload_policy;

    return net_cx;
}

void
IL_DestroyDummyNetContext(IL_NetContext *net_cx)
{
    XP_FREE(net_cx);
}

IL_NetContext *
IL_CloneDummyNetContext(IL_NetContext *net_cx)
{
    return IL_NewDummyNetContext((MWContext *)net_cx->cx,
                                 net_cx->cache_reload_policy);
}

#include "shist.h"              /* For use with IL_AddReferer */
#include "structs.h"            /* For use with IL_AddReferer */

void
IL_AddReferer(IL_NetContext *net_cx, URL_Struct *urls) 
{
    MWContext *cx = net_cx->cx;
    History_entry *he = SHIST_GetCurrent (&cx->hist);

    if (urls->referer)
        XP_FREE (urls->referer);
    if (he && he->address)
        urls->referer = XP_STRDUP (he->origin_url 
                                   ? he->origin_url 
                                   : he->address);
    else
        urls->referer = 0;
}

void
dum_TitleObserver(XP_Observable observerable, XP_ObservableMsg msg,
                  void *message_data, void *closure){

    IL_MessageData *data = (IL_MessageData*)message_data;
    MWContext *context; 
    dum_TitleObsClosure *title_obs_closure = (dum_TitleObsClosure *)closure;    
    context = (MWContext *)title_obs_closure->context;

    switch(msg){
        case IL_DESCRIPTION:
            lo_view_title(context, data->description);
            break;

        case IL_IMAGE_DESTROYED:
            /* Remove ourself from the observer callback list. */
            XP_RemoveObserver(title_obs_closure->obs_list, dum_TitleObserver,
                              title_obs_closure);
            XP_FREE(title_obs_closure);
            break;

        default:        
            break;
    }
    return;
}


XP_ObserverList
dum_NewTitleObserverList(MWContext *context)
{
    XP_ObserverList title_obs_list; /* List of observer callbacks. */
    dum_TitleObsClosure *title_obs_closure; /* Closure data to be passed back
                                              to lo_ImageObserver. */
    NS_Error status;

    /* Create an XP_ObserverList for the title */
    status = XP_NewObserverList(NULL, &title_obs_list);
    if (status < 0) {        
        return NULL;
    }

    /* Closure data for the image observer. */
    title_obs_closure = XP_NEW_ZAP(dum_TitleObsClosure);
    if (!title_obs_closure) {
        XP_DisposeObserverList(title_obs_list);
        return NULL;
    }
    title_obs_closure->context = context;
    title_obs_closure->obs_list = title_obs_list;

    /* Add the layout image observer to the observer list. */
    status = XP_AddObserver(title_obs_list, dum_TitleObserver, title_obs_closure);
    if (status < 0) {      
        XP_DisposeObserverList(title_obs_list);
        XP_RemoveObserver(title_obs_closure->obs_list, dum_TitleObserver,
            title_obs_closure );
        XP_FREE(title_obs_closure);
        return NULL;
    }

    return title_obs_list;
}



/* This function only handles view_images. Note that the title observer
created is only used to set the image dimensions and type in the 
title bar. */

PRBool
il_load_image(MWContext *cx, char *image_url, NET_ReloadMethod cache_reload_policy)
{
	PRBool ret_val = PR_TRUE;
	XP_ObserverList obs_list;
	IL_NetContext *net_cx;
	IL_IRGB *trans_pixel;
	IL_ImageReq *image_req;


	/* Create a new observer list for the image's title.  The destruction of this
	   list is managed by the image library once we call IL_GetImage. */
    obs_list = dum_NewTitleObserverList(cx);
    if (!obs_list)
        return PR_FALSE;    

	net_cx = IL_NewDummyNetContext(cx, cache_reload_policy);
	if (!net_cx) {
		XP_DisposeObserverList(obs_list);
		return PR_FALSE;
	}

    /* Determine whether to request a mask if this is a transparent image.
       In the case of a document backdrop, we ask the image library to fill
       in the transparent area with a solid color.  For all other transparent
       images, we force the creation of a mask by passing in NULL. */
    if (cx->type == MWContextPostScript) {
        trans_pixel = XP_NEW_ZAP(IL_IRGB);
        if (trans_pixel)
            trans_pixel->red = trans_pixel->green = trans_pixel->blue = 0xff;
    }
    else if (cx->type == MWContextPrint) {
        trans_pixel = cx->transparent_pixel;
    }
    else {
        trans_pixel = NULL;
    }

    /* Fetch the image. */
	image_req = IL_GetImage(image_url, cx->img_cx, obs_list, trans_pixel, 0, 0, 0, net_cx);
	if (!image_req) {
		ret_val = PR_FALSE;
	}

    /* Destroy the transparent pixel if this is a PostScript context. */
    if ((cx->type == MWContextPostScript) && trans_pixel)
        XP_FREE(trans_pixel);

    /* The Image Library clones the dummy Net Context, so it safe to destroy
       it. */
    IL_DestroyDummyNetContext(net_cx);

	return ret_val;
}
