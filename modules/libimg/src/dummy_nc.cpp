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
#ifdef NU_CACHE
#include "nsCacheManager.h"
#endif

PR_BEGIN_EXTERN_C
extern int MK_OUT_OF_MEMORY;
PR_END_EXTERN_C

/* 
 * XXX Temporary inclusion of this prototype. It was originally in
 * libimg.h but needed to be removed since it required C++ compilation.
 * It should eventually return to libimg.h and may be remove when
 * a modularized netlib comes around.
 */
extern ilIURL *
IL_CreateIURL(URL_Struct *urls);

typedef struct dum_TitleObsClosure {
    MWContext *context;
    XP_ObserverList obs_list;
} dum_TitleObsClosure;

static NS_DEFINE_IID(kINetContextIID, IL_INETCONTEXT_IID);
static NS_DEFINE_IID(kIURLIID, IL_IURL_IID);

class NetContextImpl : public ilINetContext
{
public:
    NetContextImpl(MWContext *aContext, NET_ReloadMethod aReloadMethod);
    ~NetContextImpl();

    int operator==(NetContextImpl& aNetContext) const;

    NS_DECL_ISUPPORTS
    
    virtual ilINetContext* Clone();
    
    virtual NET_ReloadMethod GetReloadPolicy();
    
    virtual void AddReferer(ilIURL *aUrl);
    
    virtual void Interrupt();
    
    virtual ilIURL* CreateURL(const char *aUrl, 
			      NET_ReloadMethod aReloadMethod);
    
    virtual PRBool IsLocalFileURL(char *aAddress);
    
#ifdef NU_CACHE
    virtual PRBool IsURLInCache(ilIURL* iUrl);
#else 
    virtual PRBool IsURLInMemCache(ilIURL *aUrl);
    
    virtual PRBool IsURLInDiskCache(ilIURL *aUrl);
#endif
    
    virtual int GetURL (ilIURL * aUrl, NET_ReloadMethod aLoadMethod,
			ilINetReader *aReader);

private:
    MWContext *mContext;                             
    NET_ReloadMethod mReloadPolicy;
};

class URLImpl : public ilIURL {
public:
    URLImpl();
    URLImpl(URL_Struct *urls);
    ~URLImpl();

    nsresult Init(const char *aURL, NET_ReloadMethod aReloadMethod);

    NS_DECL_ISUPPORTS

    virtual void SetReader(ilINetReader *aReader);

    virtual ilINetReader *GetReader();

    virtual int GetContentLength();
    
    virtual const char* GetAddress();
    
    virtual time_t GetExpires();
    
    virtual void SetBackgroundLoad(PRBool aBgload);

    virtual int GetOwnerId();

    virtual void SetOwnerId(int aOwnerId);

    URL_Struct *GetURLStruct() { return mURLS; }

private:
    ilINetReader *mReader;
    URL_Struct *mURLS;
};

PR_BEGIN_EXTERN_C
extern void
lo_view_title( MWContext *context, char *title_str );
PR_END_EXTERN_C

extern PRBool
il_load_image(MWContext *cx, char *image_url, NET_ReloadMethod cache_reload_policy);

NetContextImpl::NetContextImpl(MWContext *aContext, 
			       NET_ReloadMethod aReloadPolicy)
{
    NS_INIT_REFCNT();
    mContext = aContext;
    mReloadPolicy = aReloadPolicy;
}

NetContextImpl::~NetContextImpl()
{
}


IL_NetContext *
IL_NewDummyNetContext(MWContext *context, NET_ReloadMethod cache_reload_policy)
{
    ilINetContext *cx = new NetContextImpl(context, cache_reload_policy);

    if (cx != NULL) {
        NS_ADDREF(cx);
    }
    
    return (IL_NetContext *)cx;
}

void
IL_DestroyDummyNetContext(IL_NetContext *net_cx)
{
    ilINetContext *cx = (ilINetContext *)net_cx;

    if (cx != NULL) {
        NS_RELEASE(cx);
    }
}

NS_IMPL_ISUPPORTS(NetContextImpl, kINetContextIID)

int 
NetContextImpl::operator==(NetContextImpl& aNetContext) const
{
   return ((mContext == aNetContext.mContext) &&
	   (mReloadPolicy == aNetContext.mReloadPolicy));
}

ilINetContext *
NetContextImpl::Clone()
{
    return (ilINetContext *)IL_NewDummyNetContext(mContext, mReloadPolicy);
}

NET_ReloadMethod
NetContextImpl::GetReloadPolicy()
{
    return mReloadPolicy;
}

#include "shist.h"              /* For use with IL_AddReferer */
#include "structs.h"            /* For use with IL_AddReferer */

void
NetContextImpl::AddReferer(ilIURL *url) 
{
    History_entry *he = SHIST_GetCurrent (&mContext->hist);
    URL_Struct *urls = ((URLImpl *)url)->GetURLStruct();

    PR_FREEIF (urls->referer);
    if (he && he->address)
        urls->referer = PL_strdup (he->origin_url 
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
#ifdef MOZ_NGLAYOUT
  XP_ASSERT(0);
#else
            lo_view_title(context, data->description);
#endif /* MOZ_NGLAYOUT */
            break;

        case IL_IMAGE_DESTROYED:
            /* Remove ourself from the observer callback list. */
            XP_RemoveObserver(title_obs_closure->obs_list, dum_TitleObserver,
                              title_obs_closure);
            PR_FREEIF(title_obs_closure);
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
    title_obs_closure = PR_NEWZAP(dum_TitleObsClosure);
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
        PR_FREEIF(title_obs_closure);
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
        trans_pixel = PR_NEWZAP(IL_IRGB);
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
	image_req = IL_GetImage(image_url, cx->img_cx, obs_list, trans_pixel, 0, 0, 0, (ilINetContext *)net_cx);
	if (!image_req) {
		ret_val = PR_FALSE;
	}

    /* Destroy the transparent pixel if this is a PostScript context. */
    if ((cx->type == MWContextPostScript) && trans_pixel)
        PR_FREEIF(trans_pixel);

    /* The Image Library clones the dummy Net Context, so it safe to destroy
       it. */
    IL_DestroyDummyNetContext(net_cx);

	return ret_val;
}

ilIURL *
NetContextImpl::CreateURL(const char *url, NET_ReloadMethod reload_method)
{
    URLImpl *iurl;
    
    iurl = new URLImpl();
    if (iurl == NULL) {
	return NULL;
    }

    if (iurl->Init(url, reload_method) != NS_OK) {
        delete iurl;
	return NULL;
    }

    NS_ADDREF(iurl);

    return (ilIURL *)iurl;
}

void 
NetContextImpl::Interrupt()
{
    if (mContext != NULL) {
        NET_InterruptWindow(mContext);
    }
}

PRBool
NetContextImpl::IsLocalFileURL(char *address)
{
    return (PRBool)NET_IsLocalFileURL(address);
}

#ifdef NU_CACHE
PRBool
NetContextImpl::IsURLInCache(ilIURL *iURL)
{
    return iURL ? 
        nsCacheManager::GetInstance()->Contains(((URLImpl *)iURL)->GetURLStruct()->address) : PR_FALSE;
}

#else
PRBool
NetContextImpl::IsURLInMemCache(ilIURL *aURL)
{
    if (aURL != NULL) {
        return (PRBool)NET_IsURLInMemCache(((URLImpl *)aURL)->GetURLStruct());
    }
    else {
        return PR_FALSE;
    }
}

PRBool
NetContextImpl::IsURLInDiskCache(ilIURL *aURL)
{
    if (aURL != NULL) {
        return (PRBool)NET_IsURLInDiskCache(((URLImpl *)aURL)->GetURLStruct());
    }
    else {
        return PR_FALSE;
    }
}
#endif /* NU_CACHE */

static void
il_netgeturldone(URL_Struct *URL_s, int status, OPAQUE_CONTEXT *cx)
{
    ilIURL *iurl = (ilIURL *)URL_s->fe_data;
    ilINetReader *reader;

    if (iurl != NULL) {
        reader = iurl->GetReader();
        if (reader != NULL) {
            reader->NetRequestDone(iurl, status);
            NS_RELEASE(reader);
        }
    }

    /* for mac */
    if (status == MK_OUT_OF_MEMORY)
       NET_InterruptWindow((MWContext *)cx);
}

int
NetContextImpl::GetURL (ilIURL *aURL,
			NET_ReloadMethod aLoadMethod,
			ilINetReader *aReader)
{ 
    URL_Struct *urls = ((URLImpl *)aURL)->GetURLStruct();
    FO_Present_Types type = (aLoadMethod == NET_CACHE_ONLY_RELOAD) ?
                     FO_ONLY_FROM_CACHE_AND_INTERNAL_IMAGE :
                     FO_CACHE_AND_INTERNAL_IMAGE;
    aURL->SetReader(aReader);

    return NET_GetURL (urls, type, mContext, 
		       (Net_GetUrlExitFunc*)&il_netgeturldone);
}


URLImpl::URLImpl()
{
    NS_INIT_REFCNT();
    mReader = NULL;
    mURLS = NULL;
}

URLImpl::URLImpl(URL_Struct *urls)
{
    NS_INIT_REFCNT();
    mReader = NULL;
    mURLS = urls;
    urls->fe_data = this;
}

URLImpl::~URLImpl()
{
    if (mURLS != NULL) {
        NET_FreeURLStruct(mURLS);
    }
    
    if (mReader != NULL) {
        NS_RELEASE(mReader);
    }
}

/* This is only used for the UNIX icon hack in 
   cmd/xfe/mkicons.cpp */
ilIURL *
IL_CreateIURL(URL_Struct *urls)
{
    ilIURL *iurl = new URLImpl(urls);
    NS_ADDREF(iurl);
    
    return iurl;
}

NS_IMPL_ISUPPORTS(URLImpl, kIURLIID)

nsresult 
URLImpl::Init(const char *url, NET_ReloadMethod reload_method)
{
    mURLS = NET_CreateURLStruct(url, reload_method);
    if (mURLS == NULL) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    
    mURLS->fe_data = this;

    return NS_OK;
}

void 
URLImpl::SetReader(ilINetReader *aReader)
{
    NS_ADDREF(aReader);
    mReader = aReader;
}

ilINetReader *
URLImpl::GetReader()
{
    NS_ADDREF(mReader);  
    return mReader;
}
 
int
URLImpl::GetContentLength()
{
    if (mURLS) {
        return mURLS->content_length;
    }
    else {
        return 0;
    }
}

const char *
URLImpl::GetAddress()
{
    if (mURLS) {
        return (const char *)mURLS->address;
    }
    else {
        return NULL;
    }
}

time_t
URLImpl::GetExpires()
{
    if (mURLS) {
        return mURLS->expires;
    }
    else {
        return 0;
    }
}

void
URLImpl::SetBackgroundLoad(PRBool bgload)
{
    if (mURLS) {
        mURLS->load_background = bgload;
    }
}

int
URLImpl::GetOwnerId()
{
    if (mURLS) {
        return mURLS->owner_id;
    }
    else {
        return 0;
    }
}


void
URLImpl::SetOwnerId(int ownerId)
{
    if (mURLS) {
        mURLS->owner_id = ownerId;
    }
}


