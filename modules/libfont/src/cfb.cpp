/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
/* 
 *
 * Implements a link between the (cfb) CFontBroker Interface implementation
 * and its C++ implementation viz (fb) FontBrokerObject.
 *
 * dp Suresh <dp@netscape.com>
 */


#include "libfont.h"

#include "nf.h"
#include "Mcfb.h"
#include "Pcfb.h"
#include "fb.h"

#include "wfList.h"

 /* Hack: libnet defines this locally. We need to move it out into a public
  * header like net.h
  */
extern "C" int NET_FindURLInCache(URL_Struct *URL_s, MWContext *ctxt);

int wf_trace_flag = 0;

#ifdef MOZILLA_CLIENT
#define WF_PREF_ENABLE_WEBFONTS "browser.use_document_fonts"
PR_STATIC_CALLBACK(int)
/*ARGSUSED*/
wf_PrefHandler(const char *pref, void *data)
{
	int32 value = 2;
	struct nffbc *fbc = (struct nffbc *)data;
	struct nffbu *fbu = (struct nffbu *)nffbc_getInterface(fbc, &nffbu_ID, NULL);
#ifndef NO_PREF_CHANGE
	PREF_GetIntPref(WF_PREF_ENABLE_WEBFONTS, &value);
#endif /* NO_PREF_CHANGE */
	if (value == 0 || value == 1)
	{
		nffbu_DisableWebfonts(fbu, NULL);				
	}
	else
	{
		nffbu_EnableWebfonts(fbu, NULL);
	}
	return (0);
}
#endif /* MOZILLA_CLIENT */

/*
 * Broker initialization. I would like to put this in libfont.c but
 * since that defines JMC_INITID to define objects, adding this which means
 * adding Mcfb.h would interfere. Hence I am putting this with the cfb stuff.
 */
NF_PUBLIC_API_IMPLEMENT(struct nffbc *)
NF_FontBrokerInitialize()
{
	/* Create the broker */	
	struct nffbc *fbc = (struct nffbc *)cfbFactory_Create(NULL);

	/* Register the converters for font streaming */
	NF_RegisterConverters();

#ifdef DEBUG
	const char *WF_TRACE = getenv("WF_TRACE");
	if (WF_TRACE)
	{
		wf_trace_flag = atoi(WF_TRACE);
	}
#endif

#ifdef MOZILLA_CLIENT
#ifndef NO_PREF_CHANGE
	// Register for preference changes
	PREF_RegisterCallback(WF_PREF_ENABLE_WEBFONTS, wf_PrefHandler, fbc);
#endif /* NO_PREF_CHANGE */

	// Initialize the pref by faking a prefchange
	wf_PrefHandler(WF_PREF_ENABLE_WEBFONTS, fbc);
#endif /* MOZILLA_CLIENT */

	return (fbc);
}


/****************************************************************************
 *				 Implementation of common interface methods					*
 ****************************************************************************/

#ifdef OVERRIDE_cfb_getInterface
#include "Mnffbc.h"
#include "Mnffbp.h"
#include "Mnffbu.h"

extern "C" JMC_PUBLIC_API(void*)
/* ARGSUSED */
_cfb_getInterface(struct cfb* self, jint op, const JMCInterfaceID* iid, JMCException* *exc)
{
	if (memcmp(iid, &nffbc_ID, sizeof(JMCInterfaceID)) == 0)
		return cfbImpl2cfb(cfb2cfbImpl(self));
	if (memcmp(iid, &nffbp_ID, sizeof(JMCInterfaceID)) == 0)
		return cfbImpl2nffbp(cfb2cfbImpl(self));
	if (memcmp(iid, &nffbu_ID, sizeof(JMCInterfaceID)) == 0)
		return cfbImpl2nffbu(cfb2cfbImpl(self));
	return _cfb_getBackwardCompatibleInterface(self, iid, exc);
}
#endif /* OVERRIDE_cfb_getInterface */

extern "C" JMC_PUBLIC_API(void*)
/*ARGSUSED*/
_cfb_getBackwardCompatibleInterface(struct cfb* self,
									const JMCInterfaceID* iid,
									struct JMCException* *exceptionThrown)
{
	return(NULL);
}

extern "C" JMC_PUBLIC_API(void)
/*ARGSUSED*/
_cfb_init(struct cfb* self, struct JMCException* *exceptionThrown)
{
	cfbImpl *oimpl = cfb2cfbImpl(self);

	// Populate our global interface variables before creating
	// FontBrokerObject as the constructor uses them.
	WF_fbc = (struct nffbc *) self;
	WF_fbp = cfbImpl2nffbp(cfb2cfbImpl(self));
	WF_fbu = cfbImpl2nffbu(cfb2cfbImpl(self));

	FontBrokerObject *fbobj = new FontBrokerObject();
	if (fbobj == NULL)
	  {
		WF_fbc = NULL;
		WF_fbp = NULL;
		WF_fbu = NULL;
		JMC_EXCEPTION(exceptionThrown, JMCEXCEPTION_OUT_OF_MEMORY);
	  }
	else
	  {
		oimpl->object = fbobj;
	  }

	return;
}

#ifdef OVERRIDE_cfb_finalize
extern "C" JMC_PUBLIC_API(void)
/*ARGSUSED*/
_cfb_finalize(struct cfb* self, jint op, JMCException* *exception)
{
	struct cfbImpl *oimpl = cfb2cfbImpl(self);
	FontBrokerObject *fbobj = (FontBrokerObject *)oimpl->object;
	delete fbobj;
	
	/* Finally, free the memory for the object containter. */
	XP_FREEIF(self);
}
#endif /* OVERRIDE_cfb_finalize */




/****************************************************************************
 *				 Implementation of Object specific methods					*
 ****************************************************************************/


extern "C" JMC_PUBLIC_API(struct nff*)
/*ARGSUSED*/
_cfb_LookupFont(struct cfb* self, jint op, struct nfrc* rc, struct nffmi* fmi,
				const char *accessor, struct JMCException* *exceptionThrown)
{
	cfbImpl *oimpl = cfb2cfbImpl(self);
	XP_ASSERT(oimpl->header.refcount);
	FontBrokerObject *fbobj = (FontBrokerObject *)oimpl->object;
	return (fbobj->LookupFont(rc, fmi, accessor));
}


extern "C" JMC_PUBLIC_API(struct nff*)
/*ARGSUSED*/
_cfb_CreateFontFromUrl(struct cfb* self, jint op, struct nfrc* rc,
					   const char* url_of_font, const char *url_of_page,
					   jint faux, struct nfdoer* completion_callback,
					   MWContext* context,
					   struct JMCException* *exceptionThrown)
{
	cfbImpl *oimpl = cfb2cfbImpl(self);
	XP_ASSERT(oimpl->header.refcount);
	FontBrokerObject *fbobj = (FontBrokerObject *)oimpl->object;
	return (fbobj->CreateFontFromUrl(rc, url_of_font, url_of_page, faux, completion_callback, context));
}


extern "C" JMC_PUBLIC_API(struct nff*)
/*ARGSUSED*/
_cfb_CreateFontFromFile(struct cfb* self, jint op, struct nfrc *rc,
						const char *mimetype, const char* fontfilename,
						const char *url_of_page,
						struct JMCException* *exceptionThrown)
{
	cfbImpl *oimpl = cfb2cfbImpl(self);
	XP_ASSERT(oimpl->header.refcount);
	FontBrokerObject *fbobj = (FontBrokerObject *)oimpl->object;
	return (fbobj->CreateFontFromFile(rc, mimetype, fontfilename, url_of_page));
}


extern "C" JMC_PUBLIC_API(void*)
/*ARGSUSED*/
_cfb_ListFonts(struct cfb* self, jint op, struct nfrc* rc, struct nffmi* fmi,
					struct JMCException* *exceptionThrown)
{
	cfbImpl *oimpl = cfb2cfbImpl(self);
	XP_ASSERT(oimpl->header.refcount);
	FontBrokerObject *fbobj = (FontBrokerObject *)oimpl->object;
	return fbobj->ListFonts(rc, fmi);
}


extern "C" JMC_PUBLIC_API(void*)
/*ARGSUSED*/
_cfb_ListSizes(struct cfb* self, jint op, struct nfrc* rc, struct nffmi* fmi,
			   struct JMCException* *exceptionThrown)
{
	cfbImpl *oimpl = cfb2cfbImpl(self);
	XP_ASSERT(oimpl->header.refcount);
	FontBrokerObject *fbobj = (FontBrokerObject *)oimpl->object;
	return fbobj->ListSizes(rc, fmi);
}


extern "C" JMC_PUBLIC_API(struct nff*)
/*ARGSUSED*/
_cfb_GetBaseFont(struct cfb* self, jint op, struct nfrf* rf,
				 struct JMCException* *exceptionThrown)
{
	cfbImpl *oimpl = cfb2cfbImpl(self);
	XP_ASSERT(oimpl->header.refcount);
	FontBrokerObject *fbobj = (FontBrokerObject *)oimpl->object;
	return (fbobj->GetBaseFont(rf));
}

extern "C" JMC_PUBLIC_API(jint)
/*ARGSUSED*/
_cfb_nffbp_RegisterFontDisplayer(struct nffbp* self, jint op,
								struct nffp* fp,
								struct JMCException* *exceptionThrown)
{
	cfbImpl *oimpl = nffbp2cfbImpl(self);
	XP_ASSERT(oimpl->header.refcount);
	FontBrokerObject *fbobj = (FontBrokerObject *)oimpl->object;
	return (fbobj->RegisterFontDisplayer(fp));
}

extern "C" JMC_PUBLIC_API(jint)
/*ARGSUSED*/
_cfb_nffbp_CreateFontDisplayerFromDLM(struct nffbp* self, jint op,
									 const char* dlm_name,
									 struct JMCException* *exceptionThrown)
{
	cfbImpl *oimpl = nffbp2cfbImpl(self);
	XP_ASSERT(oimpl->header.refcount);
	FontBrokerObject *fbobj = (FontBrokerObject *)oimpl->object;
	return (fbobj->CreateFontDisplayerFromDLM(dlm_name));
}


extern "C" JMC_PUBLIC_API(jint)
/*ARGSUSED*/
_cfb_nffbp_ScanForFontDisplayers(struct nffbp* self, jint op,
								const char* dlm_dir,
								struct JMCException* *exceptionThrown)
{
	cfbImpl *oimpl = nffbp2cfbImpl(self);
	XP_ASSERT(oimpl->header.refcount);
	FontBrokerObject *fbobj = (FontBrokerObject *)oimpl->object;
	return (fbobj->ScanForFontDisplayers(dlm_dir));
}


extern "C" JMC_PUBLIC_API(void)
/*ARGSUSED*/
_cfb_nffbp_RfDone(struct nffbp* self, jint op, struct nfrf *rf,
				  struct JMCException* *exceptionThrown)
{
	cfbImpl *oimpl = nffbp2cfbImpl(self);
	XP_ASSERT(oimpl->header.refcount);
	FontBrokerObject *fbobj = (FontBrokerObject *)oimpl->object;
	fbobj->RfDone(rf);
}


extern "C" JMC_PUBLIC_API(struct nffmi*)
/*ARGSUSED*/
_cfb_nffbu_CreateFontMatchInfo(struct nffbu* self, jint op, const char* name,
							   const char* charset, const char* encoding,
							   jint weight, jint pitch, jint style,
							   jint underline, jint strikeOut,
							   jint resX, jint resY,
							   struct JMCException* *exceptionThrown)
{
	cfbImpl *oimpl = nffbu2cfbImpl(self);
	XP_ASSERT(oimpl->header.refcount);
	FontBrokerObject *fbobj = (FontBrokerObject *)oimpl->object;
	return (fbobj->CreateFontMatchInfo(name, charset, encoding, weight,
									   pitch, style, underline, strikeOut,
									   resX, resY));
}


extern "C" JMC_PUBLIC_API(struct nfrc*)
/*ARGSUSED*/
_cfb_nffbu_CreateRenderingContext(struct nffbu* self, jint op,
								  jint majorType, jint minorType,
								  void ** args, jsize nargs,
								  struct JMCException* *exceptionThrown)
{
	cfbImpl *oimpl = nffbu2cfbImpl(self);
	XP_ASSERT(oimpl->header.refcount);
	FontBrokerObject *fbobj = (FontBrokerObject *)oimpl->object;
	return (fbobj->CreateRenderingContext(majorType, minorType, args, nargs));
}

extern "C" JMC_PUBLIC_API(struct nfdoer*)
/*ARGSUSED*/
_cfb_nffbu_CreateFontObserver(struct nffbu* self, jint op,
							  nfFontObserverCallback callback,
							  void *client_data,
							  struct JMCException* *exceptionThrown)
{
	cfbImpl *oimpl = nffbu2cfbImpl(self);
	XP_ASSERT(oimpl->header.refcount);
	FontBrokerObject *fbobj = (FontBrokerObject *)oimpl->object;
	return (fbobj->CreateFontObserver(callback, client_data));
}


extern "C" JMC_PUBLIC_API(void *)
/*ARGSUSED*/
_cfb_nffbu_malloc(struct nffbu* self, jint op, jint size,
				  struct JMCException* *exceptionThrown)
{
	return (WF_ALLOC(size));
}

extern "C" JMC_PUBLIC_API(void)
/*ARGSUSED*/
_cfb_nffbu_free(struct nffbu* self, jint op, void *mem,
				struct JMCException* *exceptionThrown)
{
	if (mem)
	  {
		WF_FREE(mem);
	  }
}

extern "C" JMC_PUBLIC_API(void *)
/*ARGSUSED*/
_cfb_nffbu_realloc(struct nffbu* self, jint op, void *mem, jint size,
				   struct JMCException* *exceptionThrown)
{
	return (WF_REALLOC(mem, size));
}

//
// Font Broker Preferences
//

extern "C" JMC_PUBLIC_API(jint)
/*ARGSUSED*/
_cfb_nffbu_IsWebfontsEnabled(struct nffbu* self, jint op,
							 struct JMCException* *exceptionThrown)
{
	cfbImpl *oimpl = nffbu2cfbImpl(self);
	XP_ASSERT(oimpl->header.refcount);
	FontBrokerObject *fbobj = (FontBrokerObject *)oimpl->object;
	return (fbobj->IsWebfontsEnabled());
}


extern "C" JMC_PUBLIC_API(jint)
/*ARGSUSED*/
_cfb_nffbu_EnableWebfonts(struct nffbu* self, jint op,
						  struct JMCException* *exceptionThrown)
{
	cfbImpl *oimpl = nffbu2cfbImpl(self);
	XP_ASSERT(oimpl->header.refcount);
	FontBrokerObject *fbobj = (FontBrokerObject *)oimpl->object;
	return (fbobj->EnableWebfonts());
}


extern "C" JMC_PUBLIC_API(jint)
/*ARGSUSED*/
_cfb_nffbu_DisableWebfonts(struct nffbu* self, jint op,
						   struct JMCException* *exceptionThrown)
{
	cfbImpl *oimpl = nffbu2cfbImpl(self);
	XP_ASSERT(oimpl->header.refcount);
	FontBrokerObject *fbobj = (FontBrokerObject *)oimpl->object;
	return (fbobj->DisableWebfonts());
}

// The following are font preference related queries

extern "C" JMC_PUBLIC_API(void *)
/*ARGSUSED*/
_cfb_nffbu_ListFontDisplayers(struct nffbu* self, jint op,
							  struct JMCException* *exceptionThrown)
{
	cfbImpl *oimpl = nffbu2cfbImpl(self);
	XP_ASSERT(oimpl->header.refcount);
	FontBrokerObject *fbobj = (FontBrokerObject *)oimpl->object;
	return ((void *)fbobj->ListFontDisplayers());
}

extern "C" JMC_PUBLIC_API(jint)
/*ARGSUSED*/
_cfb_nffbu_IsFontDisplayerEnabled(struct nffbu* self, jint op,
								  const char *displayer,
								  struct JMCException* *exceptionThrown)
{
	cfbImpl *oimpl = nffbu2cfbImpl(self);
	XP_ASSERT(oimpl->header.refcount);
	FontBrokerObject *fbobj = (FontBrokerObject *)oimpl->object;
	return (fbobj->IsFontDisplayerEnabled(displayer));
}

extern "C" JMC_PUBLIC_API(void *)
/*ARGSUSED*/
_cfb_nffbu_ListFontDisplayersForMimetype(struct nffbu* self, jint op,
										 const char *mimetype,
										 struct JMCException* *exceptionThrown)
{
	cfbImpl *oimpl = nffbu2cfbImpl(self);
	XP_ASSERT(oimpl->header.refcount);
	FontBrokerObject *fbobj = (FontBrokerObject *)oimpl->object;
	return ((void *)fbobj->ListFontDisplayersForMimetype(mimetype));
}

extern "C" JMC_PUBLIC_API(const char *)
/*ARGSUSED*/
_cfb_nffbu_FontDisplayerForMimetype(struct nffbu* self, jint op,
									const char *mimetype,
									struct JMCException* *exceptionThrown)
{
	cfbImpl *oimpl = nffbu2cfbImpl(self);
	XP_ASSERT(oimpl->header.refcount);
	FontBrokerObject *fbobj = (FontBrokerObject *)oimpl->object;
	return (fbobj->FontDisplayerForMimetype(mimetype));
}

// The following are used to change state

extern "C" JMC_PUBLIC_API(jint)
/*ARGSUSED*/
_cfb_nffbu_EnableFontDisplayer(struct nffbu* self, jint op,
								const char *displayer,
								struct JMCException* *exceptionThrown)
{
	cfbImpl *oimpl = nffbu2cfbImpl(self);
	XP_ASSERT(oimpl->header.refcount);
	FontBrokerObject *fbobj = (FontBrokerObject *)oimpl->object;
	return (fbobj->EnableFontDisplayer(displayer));
}


extern "C" JMC_PUBLIC_API(jint)
/*ARGSUSED*/
_cfb_nffbu_DisableFontDisplayer(struct nffbu* self, jint op,
								const char *displayer,
								struct JMCException* *exceptionThrown)
{
	cfbImpl *oimpl = nffbu2cfbImpl(self);
	XP_ASSERT(oimpl->header.refcount);
	FontBrokerObject *fbobj = (FontBrokerObject *)oimpl->object;
	return (fbobj->DisableFontDisplayer(displayer));
}


extern "C" JMC_PUBLIC_API(jint)
/*ARGSUSED*/
_cfb_nffbu_EnableMimetype(struct nffbu* self, jint op,
						  const char *displayer, const char *mimetype,
						  struct JMCException* *exceptionThrown)
{
	cfbImpl *oimpl = nffbu2cfbImpl(self);
	XP_ASSERT(oimpl->header.refcount);
	FontBrokerObject *fbobj = (FontBrokerObject *)oimpl->object;
	return (fbobj->EnableMimetype(displayer, mimetype));
}


extern "C" JMC_PUBLIC_API(jint)
/*ARGSUSED*/
_cfb_nffbu_DisableMimetype(struct nffbu* self, jint op,
						   const char *displayer, const char *mimetype,
						   struct JMCException* *exceptionThrown)
{
	cfbImpl *oimpl = nffbu2cfbImpl(self);
	XP_ASSERT(oimpl->header.refcount);
	FontBrokerObject *fbobj = (FontBrokerObject *)oimpl->object;
	return (fbobj->DisableMimetype(displayer, mimetype));
}


//
// Font Broker Catalog
//

extern "C" JMC_PUBLIC_API(jint)
/*ARGSUSED*/
_cfb_nffbu_LoadCatalog(struct nffbu* self, jint op,
					   const char *catalogFilename,
					   struct JMCException* *exceptionThrown)
					   
{
	cfbImpl *oimpl = nffbu2cfbImpl(self);
	XP_ASSERT(oimpl->header.refcount);
	FontBrokerObject *fbobj = (FontBrokerObject *)oimpl->object;
	return (fbobj->LoadCatalog(catalogFilename));
}

extern "C" JMC_PUBLIC_API(jint)
/*ARGSUSED*/
_cfb_nffbu_SaveCatalog(struct nffbu* self, jint op,
					   const char *catalogFilename,
					   struct JMCException* *exceptionThrown)
					   
{
	cfbImpl *oimpl = nffbu2cfbImpl(self);
	XP_ASSERT(oimpl->header.refcount);
	FontBrokerObject *fbobj = (FontBrokerObject *)oimpl->object;
	return (fbobj->SaveCatalog(catalogFilename));
}

//
// Webfonts & MWContext specific
//

#ifndef XP_OS2
extern "C" 
#endif
void
/*ARGSUSED*/
release_webfonts(wfList *list, void *item)
{
	struct nff *f = (struct nff *)item;
	nff_release(f, NULL);
}

extern "C" void wf_ObserverCallback(struct nff *ff, void *data)
{
	MWContext *context = (MWContext *) data;

	// fprintf(stderr, "fontObserverCallback: State of font is %d.", nff_GetState(f, NULL));
	 switch (nff_GetState(ff, NULL))
	 {
	 case NF_FONT_COMPLETE:
		 {
			 wfList *webfontsList = (wfList *)context->webfontsList;
			 if (!webfontsList)
			 {
				 webfontsList = new wfList(release_webfonts);
				 context->webfontsList = webfontsList;
			 }
			 webfontsList->add(ff);
			 context->WebFontDownLoadCount++;
			 nff_addRef(ff, NULL);
			 break;
		 }
	 case NF_FONT_ERROR:
	 case NF_FONT_INCOMPLETE:		// This should not happen.
	 default :
		 break;
	 }
	 return;
}

extern "C" JMC_PUBLIC_API(jint)
/*ARGSUSED*/
_cfb_nffbu_LoadWebfont(struct nffbu* self, jint op,
					   MWContext *context, const char *url, jint force,
					   struct JMCException* *exceptionThrown)
{
 	cfbImpl *oimpl = nffbu2cfbImpl(self);
	XP_ASSERT(oimpl->header.refcount);

	int ret = 0;
	struct nff *f;
	struct nfdoer *observer;
	struct nfrc *rc;
   	void *rcbuf[2]; 
	int url_type;
	URL_Struct *url_s = NULL;
	char *fullFilePath = NULL;
	NET_ReloadMethod reloadMethod = (NET_ReloadMethod) force;
	History_entry *he = NULL;
	const char *accessing_url_str = NULL;
	
	if (reloadMethod == NET_RESIZE_RELOAD)
	{
		return (ret);
	}

	rcbuf[0] = (void *)NULL;
	rcbuf[1] = 0;
	rc = nffbu_CreateRenderingContext(self, NF_RC_DIRECT, 0, rcbuf, 1, NULL);

	url_type = NET_URL_Type(url);

	/* Do cache checking only if this isn't NET_SUPER_RELOAD and
	 * this isn't a mail/news url.
	 */
	if (reloadMethod != NET_SUPER_RELOAD &&
		url_type != MAILBOX_TYPE_URL && url_type != NEWS_TYPE_URL)
	{
		url_s = NET_CreateURLStruct(url, NET_NORMAL_RELOAD); 
		if (url_s) 
		{ 
			NET_FindURLInCache(url_s, context); 
		}
		if (url_s && url_s->cache_file && *url_s->cache_file)
		{
			fullFilePath = WH_FileName(url_s->cache_file, xpCache);
		}

		// See if this a local file url
		if (!fullFilePath && url_s && url_s->address &&
			// NET_IsLocalFile() says Yes to mailbox: urls too. Yuck.
			!wf_strncasecmp(url_s->address, "file:", 5) &&
			NET_IsLocalFileURL(url_s->address))
		{
			fullFilePath = NET_ParseURL(url_s->address, GET_PATH_PART);
			if (fullFilePath && *fullFilePath)
			{
				fullFilePath = NET_UnEscape(fullFilePath);
				char *s = WH_FileName(fullFilePath, xpTemporary);
				XP_FREE(fullFilePath);
				fullFilePath = s;
			}
		}
	}

	// Find the url that is loading this font
	he = SHIST_GetCurrent(&context->hist);
	if (he)
	{
		accessing_url_str = he->address;
	}

	if (fullFilePath && *fullFilePath)
	{
		// The url was cached.
		struct nffbc *fbc =
			(struct nffbc *)nffbu_getInterface(self, &nffbc_ID, NULL);

		f =	nffbc_CreateFontFromFile(fbc, rc, url_s->content_type,
									fullFilePath, accessing_url_str, NULL);

		// Add the font to the list of created webfonts
		if (f)
		{
			wfList *webfontsList = (wfList *)context->webfontsList;
			if (!webfontsList)
			{
				webfontsList = new wfList(release_webfonts);
				context->webfontsList = webfontsList;
			}

			// Add the font to the list of created webfonts
			// and increment its refcount
			webfontsList->add(f);
			nff_addRef(f, NULL);
		}
	}
	else
	{
		// Create the font observer
		observer = nffbu_CreateFontObserver(self, wf_ObserverCallback, context,
											NULL);
		
		if (!observer)
		{
			return (-2);
		}
		
		f =	nffbc_CreateFontFromUrl(
			(struct nffbc *)nffbu_getInterface(self, &nffbc_ID, NULL),
			rc, url, accessing_url_str, 0, observer, context, NULL);
	}

	if (url_s)
	{
        NET_FreeURLStruct(url_s); 
	}
	if (fullFilePath)
	{
		XP_FREE(fullFilePath);
	}

	if (!f)
	{
		// Coudn't create font
		ret = -1;
	}
	else
	{
		// Release the current 'f' that we are holding
		nff_release(f, NULL);
	}
	return (ret);
}

extern "C" JMC_PUBLIC_API(jint)
/*ARGSUSED*/
_cfb_nffbu_ReleaseWebfonts(struct nffbu* self, jint op,
						   MWContext *context,
						   struct JMCException* *exceptionThrown)
{
	wfList *webfontsList = (wfList *) context->webfontsList;
	if (!webfontsList)
	{
		// No webfonts were ever loaded
		return (-1);
	}

	delete webfontsList;
	context->webfontsList = NULL;
	context->WebFontDownLoadCount = 0;
	context->MissedFontFace = 0;
	return (0);
}

extern "C" JMC_PUBLIC_API(jint)
/*ARGSUSED*/
_cfb_nffbu_WebfontsNeedReload(struct nffbu* self, jint op,
							  MWContext *context,
							  struct JMCException* *exceptionThrown)
{
	if (!context)
	{
		return (-1);
	}

	if (context->WebFontDownLoadCount <= 0)
	{
		// No webfonts downloaded by this document
		return (0);
	}

	/* A reload should happen iff
	 * 1. There were successful downloads of webfonts.
	 *		Creating webfonts from cache doesn't count
     * 2. If the current document was a NET_RESIZE_RELOAD
	 *
	 * Even if FE didn't fail to load a font we have to download
	 * because webfonts always override system fonts and we could
	 * have a request that was earlier satisfied by a system font be
	 * satisfied with a webfont.
	 */
    NET_ReloadMethod reloadMethod =  LO_GetReloadMethod(context);

	/* Before returning reset context->MissedFontFace */
	context->MissedFontFace = 0; /* Not used for now */
	return (reloadMethod != NET_RESIZE_RELOAD);
}


extern "C" JMC_PUBLIC_API(jint)
/*ARGSUSED*/
_cfb_nffbu_LookupFailed(struct nffbu* self, jint op,
						MWContext *context, struct nfrc *rc,
						struct nffmi *fmi,
						struct JMCException* *exceptionThrown)
{
	// The simplest implementation is to set a flag and use it
	// when deciding to reload.
	//
	// More complicated schemes where we keep this list of fmi
	// and enable reload only if these fmis will be satisfied by
	// the downloaded fonts.
	if (!context)
	{
		return (-1);
	}

	context->MissedFontFace = 1;
	return (0);
}


extern "C" JMC_PUBLIC_API(jint)
/*ARGSUSED*/
_cfb_nffbu_ToUnicode(struct nffbu* self, jint op,
					 const char* encoding,
					 jbyte* src, jsize src_length,
					 jshort* dest, jsize dest_length,
					 struct JMCException* *exceptionThrown)
{
	jint ret = 0;
    
	if (src_length)
	{
		INTL_Encoding_ID encoding_ID = INTL_CharSetNameToID((char *)encoding);
		ret = INTL_TextToUnicode(encoding_ID,
			(unsigned char *)src, (uint32)src_length,
			(INTL_Unicode *)dest, (uint32)dest_length);
	}
	return (ret);
}


extern "C" JMC_PUBLIC_API(jint)
/*ARGSUSED*/
_cfb_nffbu_UnicodeLen(struct nffbu* self, jint op,
					  const char* encoding,
					  jbyte* src, jsize src_length,
					  struct JMCException* *exceptionThrown)
{
	jint ret = 0;
    
	if (src_length)
	{
		INTL_Encoding_ID encoding_ID = INTL_CharSetNameToID((char *)encoding);
		ret = INTL_TextToUnicodeLen(encoding_ID,
			(unsigned char *)src, (uint32) src_length);
	}
	return (ret);
}
