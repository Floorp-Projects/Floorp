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
 * fb.cpp (FontBrokerObject.cpp)
 *
 * C++ implementation of the (fb) FontBrokerObject
 *
 * dp Suresh <dp@netscape.com>
 */


#include "fb.h"

// Since it creates these objects, it uses the local implementation of
// Objects.
// Create :	Font
//			FontMatchInfo
//			RenderingContext
//			FontObserver
#include "Mcf.h"
#include "Pcf.h"
#include "f.h"
#include "Mcfmi.h"
#include "Pcfmi.h"
#include "fmi.h"
#include "Mcrc.h"
#include "Pcrc.h"
#include "rc.h"
#include "Pcdoer.h"

// Uses:	DoneObserver
#include "Mnfdoer.h"

#include "net.h"		// libnet
#include "prio.h"		// for reading directories

FontBrokerObject::
FontBrokerObject() : fpPeers(NULL), fpPeersFromCatalog(NULL)
{
	// Enable webfonts by default
	pref.enableWebfonts = 1;
}

FontBrokerObject::
~FontBrokerObject()
{
}

//
// FontBrokerConsumer Interface specific
//
struct nff * FontBrokerObject::
LookupFont(struct nfrc *rc, struct nffmi *fmi, const char *accessor)
{
	struct nff *f = NULL;
	
	// Try to find a Font Object that we already created
	// Wondering why the accessor is not involved in the match.
	// All webfonts are NOT shared. Only local fonts are shared. And local
	// fonts can be accessed by any accessor.
	f = fontObjectCache.RcFmi2Font(rc, fmi);

	if (f)
	  {
		WF_TRACEMSG(("NF: Found font object 0x%x for fmi (%s) in cache.",
			f, nffmi_toString(fmi, NULL)));
		return (f);
	  }

	// On each enabled FontDisplayer, do a LookupFont() and use the
	// returned FontHandles to make a Font object.

	struct wfListElement *tmp = fpPeers.head;
	for(; tmp; tmp = tmp->next)
	  {
		FontDisplayerPeerObject *fpp = (FontDisplayerPeerObject *) tmp->item;
		void *fh = NULL;
		
		//
		// LookupFont() on a displayer will not be done if
		//		displayer's catalog does not support the {rc, fmi}
		//		and the displayer is not loaded.
		// If the fpp was already loaded, ask it anyway.
		// The logic is asking the catalog will save us from loading
		// the fpp. But if the fpp is already loaded and in memory,
		// then this operation is a relatively cheap operation. Webfonts
		// rely on this. Once a font is streamed, anymore lookup fonts
		// need to go the webfont displayer too as we dont know that facenames
		// the streamed fron contains.
		//
		// *** The native displayer will always be asked.
		//

		if (!fpp->isNative())
		{
			if (pref.enableWebfonts <= 0)
			{
				// If webfonts are disabled, always believe the catalog.
				if (!fpp->queryCatalog(rc, fmi))
				{
					continue;
				}
			}
			else
			{
				// If webfonts are enabled, then the special cases are
				// 1. fpp already loaded
				//		In this case, we always ask it. Catalog is not consulted.
				// 2. fpp gets loaded in the process of initializing the catalog
				//		In this case, we ask it since it got loaded.
				if (!fpp->isLoaded() && !fpp->queryCatalog(rc, fmi) && !fpp->isLoaded())
				{
					continue;
				}
			}
		}

		fh = fpp->LookupFont(rc, fmi, accessor);
		if (fh)
		  {
			if (!f)
			{
				f = (struct nff *) cfFactory_Create(NULL, rc);
				if (!f)
				{
					// Error.
					return (NULL);
				}
			}
			cfImpl *fimpl = cf2cfImpl(f);
			FontObject *fobj = (FontObject *)  fimpl->object;
			
			XP_ASSERT(fobj);
			fobj->addFontHandle(fpp, fh);
		  }
	  }

	if (f)
	  {
		WF_TRACEMSG(("NF: Created new font object 0x%x for fmi (%s).",
			f, nffmi_toString(fmi, NULL)));
		// The font needs to get added to the Font<-->Fmi map
		// We have to do this as the font object cache is a central list of
		// font objects.
		fontObjectCache.add(fmi, f);
	  }
	return (f);
}


struct nff *
/*ARGSUSED*/
FontBrokerObject::CreateFontFromUrl(struct nfrc* rc, const char* url_of_font,
									const char *url_of_page, jint faux,
									struct nfdoer* observer, MWContext *context)
{
	struct nff *f = NULL;

	// Decide if we want to pull the stream in
	// We will not pull the stream under these conditions
	// 1. webfonts is OFF in the preference
	// 2. a font of this name was already created by us
	
	if (pref.enableWebfonts <= 0)
	{
		// webfonts is off. Dont do font streaming.
		return (NULL);
	}

	// We should not reuse webfonts. This is because a webfont created for
	// one page should not be usable to another page according to
	// webfont security. So we will always create these fresh.
	
	//
	// Start a stream download of the url.
	//
	URL_Struct *url = NET_CreateURLStruct(url_of_font, NET_NORMAL_RELOAD);
	if (!url)
	{
		// Error. Cannot create url struct.
		WF_TRACEMSG(("NF: Error: Cannot create url struct."));
		return (NULL);
	}

	struct wf_new_stream_data *data = new wf_new_stream_data;
	if (!data)
	{
		// Error. No memory to create wf_new_stream_data.
		WF_TRACEMSG(("NF: Error: Cannot create wf_new_stream_data."));
		NET_FreeURLStruct(url);
		return (NULL);
	}

	// Create a web FontObject
	f = (struct nff *) cfFactory_Create(NULL, rc);
	if (!f)
	{
		// Error.
		WF_TRACEMSG(("NF: Error: Cannot create Font nff."));
		NET_FreeURLStruct(url);
		return (NULL);
	}
	// The fontobject that was created was wrong. We will destroy it and
	// put a new correct fontobject
	cfImpl *fimpl = cf2cfImpl(f);
	FontObject *fobj = (FontObject *) fimpl->object;
	delete fobj;
	fobj = new FontObject(f, rc, url->address);
	fimpl->object = fobj;

	nfdoer_addRef(observer, NULL);
	data->observer = observer;
	nff_addRef(f, NULL);
	data->f = f;
	nfrc_addRef(rc, NULL);
	data->rc = rc;
	data->url_of_page = CopyString(url_of_page);

	url->fe_data = data;

	//art NET_GetURL(url, FO_CACHE_AND_FONT, NULL, wfUrlExit);
	NET_GetURL(url, FO_CACHE_AND_FONT, context,(Net_GetUrlExitFunc*) wfUrlExit);

	return (f);
}

const char *
FontBrokerObject::GetMimetype(const char *imimetype, const char *address)
{
	// If the mimetype was not known, we will try to figure out the mimetype
	// from the extension. This is very tricky. We need to define two things:
	// 1. How do we know the mimetype was not known. Let us declare that all
	//		these mimetypes means the server doesn't know the mimetype
	//			application/x-unknown
	//			text/plain
	//			text/html
	//
	// 2. What is the extension ?
	//		We will define it as any string after the last dot in the url.
	//		That means for http://www.nescape.com/a.tar.gz, we will only
	//		recognize "gz" as the extension and not "tar.gz"
	const char *mimetype = imimetype;
	if (mimetype &&
		(!wf_strcasecmp(mimetype, "text/plain") ||
		 !wf_strcasecmp(mimetype, "text/html") ||
		 !wf_strncasecmp(mimetype, "application/x-unknown", 21)))
	{
		mimetype = NULL;
	}
	if (!mimetype)
	{
		const char *extension = wf_getExtension(address);
		struct wfListElement *tmp = fpPeers.head;
		for(; tmp; tmp = tmp->next)
		{
			FontDisplayerPeerObject *fpp = (FontDisplayerPeerObject *) tmp->item;
			mimetype = fpp->getMimetypeFromExtension(extension);
			if (mimetype && *mimetype)
			{
				break;
			}
		}
		if (!mimetype)
		{
			// Noway of finding a mimetype for this url.
			// Just use the original and see what happens.
			mimetype = imimetype;
		}
	}
	return (mimetype);
}


struct nff *
/*ARGSUSED*/
FontBrokerObject::CreateFontFromFile(struct nfrc* rc, const char *mimetype,
									 const char* fontFilename,
									 const char *url_of_page)
{
	// Decide if we want to pull the stream in
	// We will not pull the stream under these conditions
	// 1. webfonts is OFF in the preference
	// 2. a font of this name was already created by us
	
	if (pref.enableWebfonts <= 0)
	{
		// webfonts is off. Dont do font streaming.
		return (NULL);
	}
	struct nff *f = NULL;

	// We should not reuse webfonts. This is because a webfont created for
	// one page should not be usable to another page according to
	// webfont security. So we will always create these fresh.

	mimetype = GetMimetype(mimetype, fontFilename);
	if (!mimetype || !*mimetype)
	{
		return (NULL);
	}
	
	// Find which font Displayer implements the mimetype
	struct wfListElement *tmp = fpPeers.head;
	FontDisplayerPeerObject *fpp = NULL;
	void *fh = NULL;
	for(; tmp; tmp = tmp->next)
	{
		fpp = (FontDisplayerPeerObject *) tmp->item;
		if (fpp->isMimetypeEnabled(mimetype) > 0 &&
			(fh = fpp->CreateFontFromFile(rc, mimetype, fontFilename, url_of_page)))
		{
			f = (struct nff *) cfFactory_Create(NULL, rc);
			if (!f)
			{
				// Error.
				return (NULL);
			}

			// The fontobject that was created was wrong. We will destroy it and
			// put a new correct fontobject
			cfImpl *fimpl = cf2cfImpl(f);
			FontObject *fobj = (FontObject *) fimpl->object;
			delete fobj;
			fobj = new FontObject(f, rc, fontFilename);
			fimpl->object = fobj;
			
			XP_ASSERT(fobj);
			fobj->addFontHandle(fpp, fh);

			break;
		}
	}
	return (f);
}


//
// Ideally these catalog methods could be cached...
//
struct nffmi ** FontBrokerObject::
ListFonts(struct nfrc *rc, struct nffmi *fmi)
{
	struct nffmi ** cumulativeFmiList = NULL;
	int n = 0;
	struct wfListElement *tmp = fpPeers.head;
    for (; tmp; tmp = tmp->next)
	  {
		FontDisplayerPeerObject *fpp = (FontDisplayerPeerObject *) tmp->item;
		struct nffmi ** fmiList = fpp->ListFonts(rc, fmi);
		if (fmiList && fmiList[0])
		  {
			if (cumulativeFmiList == NULL)
			{
				// This is the first time. Just initialize the
				// cumulativeFmiList with the fmiList.
				cumulativeFmiList = fmiList;
				while (fmiList[n]) n++;
			}
			else
			{
				// Merge this list into the cumulativeFmiList
				merge(cumulativeFmiList, n, fmiList);

				// WF_FREE is nffbu::free(). So this is ok.
				// We dont have to release each fmi as they have been
				// copied into the cumulativeFmiList
				WF_FREE(fmiList);}
		  }
	  }	
	return(cumulativeFmiList);
}


jdouble * FontBrokerObject::
ListSizes(struct nfrc *rc, struct nffmi *fmi)
{
	jdouble * cumulativeSizeList = NULL;
	int maxSizes = 0;
	struct wfListElement *tmp = fpPeers.head;
    for (; tmp; tmp = tmp->next)
	  {
		FontDisplayerPeerObject * fpp = (FontDisplayerPeerObject *) tmp->item;
		jdouble* sizeList = fpp->ListSizes(rc, fmi);
		if (sizeList && (sizeList[0] >= 0))
		  {
			// Merge this list into the cumulativeFmiList
			MergeSizes(cumulativeSizeList, maxSizes, sizeList);

			// WF_FREE is nffbu::free(). So this is ok.
			WF_FREE(sizeList);
		  }
	  }	
	return(cumulativeSizeList);
}

/*ARGSUSED*/
struct nff *
FontBrokerObject::
GetBaseFont(struct nfrf *rf)
{
	return(fontObjectCache.Rf2Font(rf));
}

//
// FontBrokerDisplayer interface specific
//
jint FontBrokerObject::
RegisterFontDisplayer(struct nffp* fp)
{
	FontDisplayerPeerObject *fpp = new FontDisplayerPeerObject(fp);
	return (registerDisplayer(fpp));
}


//
// All displayer objects get created here. This checks the catalog and
// creates displayer only if the dlm_name isn't in the catalog (or) the
// dlm_name is newer than what we have in the catalog.
//
jint
FontBrokerObject::
CreateFontDisplayerFromDLM(const char* dlm_name)
{
	int ret = 0;
	FontDisplayerPeerObject * fpp = NULL;
	// Check the catalog to see if this dlm_name is present
	struct wfListElement *tmp = fpPeersFromCatalog.head;
    for (; tmp; tmp = tmp->next)
	{
		fpp = (FontDisplayerPeerObject *) tmp->item;
		int dlmchanged = fpp->dlmChanged(dlm_name);
		if (dlmchanged >= 0)
		{
			// Found the dlm in the catalog. Use it.
			// 1. remove the fpp from the fpPeersFromCatalog list
			fpPeersFromCatalog.remove(fpp);

			// 2. If the fpp has changed, resync it.
			if (dlmchanged > 0)
			{
				fpp->resync();
			}
			break;
		}
	}
	
	if (!tmp)
	{
		// Didn't find the dlm_name in the catalog. Create a new one.
		fpp = new FontDisplayerPeerObject(dlm_name);
	}

	// If the fpp was deleted, this dlm_name doesnt make a displayer
	if (fpp->isDeleted())
	{
		delete fpp;
		ret = -1;
	}
	else
	{
		ret = registerDisplayer(fpp);
	}
	return (ret);
}


#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#define WE_DEFINED_MAXPATHLEN
#endif

jint FontBrokerObject::
ScanForFontDisplayers(const char* dlmDirList)
{
	int ndisplayer = 0;
    const char *str = dlmDirList;
	char directoryName[MAXPATHLEN];
	
    while (*str != '\0')
	{
		str = wf_scanToken(str, directoryName, sizeof(directoryName), WF_PATH_SEP_STR, 0);

		wf_expandFilename(directoryName, MAXPATHLEN);

		if (directoryName[0] != '\0')
		{
			ndisplayer += scanDisplayersFromDir(directoryName);
		}

		if (*str != '\0')
		{
			str++;
		}
    }
	return (ndisplayer);
}

int FontBrokerObject::scanDisplayersFromDir(const char *directoryName)
{
	int ndisplayer = 0;

    PRDir *dir;
	PRDirEntry *dp;
    char filename[MAXPATHLEN];
    char *nameptr;

    if ((dir = PR_OpenDir(directoryName)) != NULL)
	{
		strcpy(filename, directoryName);
		nameptr = &filename[strlen(filename)];
		// The checking and addtion of '/' is OK on all platforms as the directoryName that
		// NSPR uses is all unix style.
		if (*(nameptr-1) != '/' )
		{
			*nameptr++ = '/';  
		}
		while ((dp = PR_ReadDir(dir, PR_SKIP_BOTH)) != NULL)
		{
			strcpy(nameptr, PR_DirName(dp));
			if (wf_isFileDirectory(filename))
			{
				ndisplayer += scanDisplayersFromDir(filename);
			}
			else if (wf_stringEndsWith(PR_DirName(dp), ".dll") ||
					 wf_stringEndsWith(PR_DirName(dp), ".so") ||
					 wf_stringEndsWith(PR_DirName(dp), ".sl") ||
					 wf_stringEndsWith(PR_DirName(dp), ".dlm"))
			{
				// Make a displayer out of this dll.
				jint ret = CreateFontDisplayerFromDLM(filename);
				if (ret >= 0)
				{
					ndisplayer++;
				}
			}
		}
		PR_CloseDir(dir);
    }
	return (ndisplayer);
}

#ifdef WE_DEFINED_MAXPATHLEN
#undef MAXPATHLEN
#undef WE_DEFINED_MAXPATHLEN
#endif

void FontBrokerObject::
RfDone(struct nfrf *rf)
{
	// Release all references to this rf in the Font
	fontObjectCache.releaseRf(rf);
}

//
// FontBrokerUtility interface specific
//  
struct nffmi *
FontBrokerObject::
CreateFontMatchInfo(const char* name, const char* charset,
					const char* encoding, jint weight, jint pitch, jint style,
					jint underline, jint strikeOut,
					jint resX, jint resY)
{
	// XXX We should implement fmi cache here.
	cfmi *fmi = cfmiFactory_Create(NULL, name, charset, encoding, weight,
								   pitch, style, underline, strikeOut, resX, resY);
	return ((struct nffmi *) fmi);
}

struct nfrc*
FontBrokerObject::
CreateRenderingContext(jint majorType, jint minorType,
					   void **args, jsize nargs)
{
	// WARNING: Dont ever think about caching rc
	crc *rc = crcFactory_Create(NULL, majorType, minorType, args, nargs);
	return ((struct nfrc *) rc);
}

struct nfdoer*
FontBrokerObject::
CreateFontObserver(nfFontObserverCallback callback, void *client_data)
{
	struct cdoer *doer = cdoerFactory_Create(NULL, callback, client_data);
	return ((struct nfdoer *) doer);
}

// Font preferences

jint
FontBrokerObject::IsWebfontsEnabled(void)
{
	return (pref.enableWebfonts);
}

jint
FontBrokerObject::EnableWebfonts(void)
{
	pref.enableWebfonts = 1;
	return (0);
}

jint
FontBrokerObject::DisableWebfonts(void)
{
	pref.enableWebfonts = 0;
	return (0);
}


const char **
FontBrokerObject::ListFontDisplayers()
{
	int ndisplayers = fpPeers.count();
	if (ndisplayers <= 0)
	{
		return (NULL);
	}

	const char **displayers =
	  (const char **) WF_ALLOC(sizeof(char *) * ndisplayers + 1);
	if (!displayers)
	{
		// No memory
		return (NULL);
	}
	
	int i = 0;
	struct wfListElement *tmp = fpPeers.head;
    for (; tmp; tmp = tmp->next)
	{
		FontDisplayerPeerObject * fpp = (FontDisplayerPeerObject *) tmp->item;
		if (fpp->name())
		{
			displayers[i++] = fpp->name();
		}
	}
	displayers[i] = NULL;

	return (displayers);
}

jint
FontBrokerObject::IsFontDisplayerEnabled(const char *displayer)
{
	FontDisplayerPeerObject *fpp = findDisplayer(displayer);
	if (!fpp)
	{
		return (-2);
	}
	return(fpp->isDisplayerEnabled());
}

const char **
FontBrokerObject::ListFontDisplayersForMimetype(const char *mimetype)
{
	int ndisplayers = fpPeers.count();
	if (ndisplayers <= 0)
	{
		return (NULL);
	}

	// 16 bit is unhappy to WF_FREE( const char **displayers )
	char **displayersBuffer = (char **)WF_ALLOC(sizeof(char *) * ndisplayers + 1);
	const char **displayers = (const char **)displayersBuffer;
	if (!displayers)
	{
		// No memory
		return (NULL);
	}
	
	int i = 0;
	struct wfListElement *tmp = fpPeers.head;
    for (; tmp; tmp = tmp->next)
	{
		FontDisplayerPeerObject * fpp = (FontDisplayerPeerObject *) tmp->item;
		if (fpp->isMimetypeEnabled(mimetype) > 0)
		{
			// this requests const char **displayers
			if (fpp->name())
			{
				displayers[i++] = fpp->name();
			}
		}
	}
	if (i == 0)
	{
		// There are no displayers that has this mimetype enabled.
		WF_FREE(displayersBuffer);
		displayers = NULL;
	}
	else
	{
		displayers[i] = NULL;
	}

	return (displayers);
}

const char *
FontBrokerObject::FontDisplayerForMimetype(const char *mimetype)
{
	const char *displayer = NULL;
	struct wfListElement *tmp = fpPeers.head;
	// The first fpp which has this mimetype disabled
	FontDisplayerPeerObject * first_fpp = NULL;
	FontDisplayerPeerObject * fpp;
	int ret;

    for (; tmp; tmp = tmp->next)
	{
		fpp = (FontDisplayerPeerObject *) tmp->item;
		int ret = fpp->isMimetypeEnabled(mimetype);
		if (ret > 0)
		{
			if (fpp->name())
			{
				displayer = fpp->name();
			}
			break;
		}
		else if (!first_fpp && ret == 0)
		{
			first_fpp = fpp;
		}
	}
	if (!displayer && first_fpp)
	{
		// No displayer has this mimetype enabled. Enable the first one.
		ret = first_fpp->enableMimetype(mimetype);
		if (ret >= 0)
		{
			displayer = first_fpp->name();
		}
		else
		{
			// The first fpp refused to enable this mimetype. Find more.
			tmp = fpPeers.head;
			for (; tmp; tmp = tmp->next)
			{
				fpp = (FontDisplayerPeerObject *) tmp->item;
				int ret = fpp->isMimetypeEnabled(mimetype);
				if (ret == 0)
				{
					// Here is a font Displayer who has this mimetype disabled. Enable it.
					ret = fpp->enableMimetype(mimetype);
					if (ret >= 0)
					{
						// Success
						displayer = fpp->name();
						break;
					}
				}
			}
		}
	}
	return (displayer);
}

jint
FontBrokerObject::EnableFontDisplayer(const char *displayer)
{
	FontDisplayerPeerObject *fpp = findDisplayer(displayer);
	if (!fpp)
	{
		return (-2);
	}
	return((jint)fpp->enableDisplayer());
}

jint
FontBrokerObject::DisableFontDisplayer(const char *displayer)
{
	FontDisplayerPeerObject *fpp = findDisplayer(displayer);
	if (!fpp)
	{
		return (-2);
	}
	return((jint)fpp->disableDisplayer());
}

jint
FontBrokerObject::EnableMimetype(const char *displayer, const char *mimetype)
{
	int ret;
	FontDisplayerPeerObject *fpp = findDisplayer(displayer);
	if (!fpp)
	{
		// Displayer not found
		ret = -2;
	}
	else if (fpp->enableMimetype(mimetype) < 0)
	{
		// Mimetype not in displayer.
		ret = -1;
	}
	else
	{
		// Disable this mimetype for all other displayers
		struct wfListElement *tmp = fpPeers.head;
		for (; tmp; tmp = tmp->next)
		{
			FontDisplayerPeerObject * fpp = (FontDisplayerPeerObject *) tmp->item;
			if (fpp->name() && strcmp(fpp->name(), displayer))
			{
				fpp->disableMimetype(mimetype);
			}
		}
		ret = 0;
	}
	return(ret);
}

jint
FontBrokerObject::DisableMimetype(const char *displayer, const char *mimetype)
{
	FontDisplayerPeerObject *fpp = findDisplayer(displayer);
	if (!fpp)
	{
		// Displayer not found
		return (-2);
	}
	return((jint)fpp->disableMimetype(mimetype));
}


jint
FontBrokerObject::LoadCatalog(const char *filename)
{
	const char *catfile = filename;
	if (!catfile || !*catfile)
	{
		catfile = catalogFilename;
	}

	if (!catfile || !*catfile)
	{
		// Catalog file doens't exist
		return (-1);
	}

	// Read from the catalog file
	FontCatalogFile fc(catfile);
	if (fc.status() < 0)
	{
		return (-1);
	}

    while (!fc.isEof())
	{
		FontDisplayerPeerObject * fpp = new FontDisplayerPeerObject (fc);
		if (!fpp->isDeleted())
		{
			wfList::ERROR_CODE err = fpPeersFromCatalog.add(fpp);
			if (err != wfList::SUCCESS)
			{
				// Ignore this.
				delete fpp;
			}
		}
		else
		{
			delete fpp;
		}
	}
	return (0);	
}

jint
FontBrokerObject::SaveCatalog(const char *filename)
{
	const char *catfile = filename;
	if (!catfile || !*catfile)
	{
		catfile = catalogFilename;
	}

	if (!catfile || !*catfile)
	{
		// Catalog file doens't exist
		return (-1);
	}

	FontCatalogFile fc(catfile, 1 /* write */);

	if (fc.status() < 0)
	{
		return (-1);
	}

	struct wfListElement *tmp = fpPeers.head;
    while (tmp)
	{
		FontDisplayerPeerObject * fpp = (FontDisplayerPeerObject *) tmp->item;
		fpp->describe(fc);
		tmp = tmp->next;
	}
	return (0);	
}

//
// Private methods
//
FontDisplayerPeerObject *
FontBrokerObject::findDisplayer(const char *displayerName)
{
	FontDisplayerPeerObject * wffpp = NULL;

	struct wfListElement *tmp = fpPeers.head;
    for (; tmp; tmp = tmp->next)
	{
		FontDisplayerPeerObject * fpp = (FontDisplayerPeerObject *) tmp->item;
		if (fpp->name() && !strcmp(fpp->name(), displayerName))
		{
			wffpp = fpp;
			break;
		}
	}
	return (wffpp);
}

// Merge fmiList together
int FontBrokerObject::
merge(struct nffmi ** &srcList, int &srcLen,
	  struct nffmi **newList)
{
	int newlen = 0;
	if (!newList || !newList[0])
	  {
		return -1;
	  }

	while (newList[newlen]) newlen++;

	// Allocate more space to hold 
	srcList = (struct nffmi **)
	  WF_REALLOC(srcList, sizeof(*srcList) * (srcLen + newlen + 1));
	if (!srcList)
	  {
		// No memory
		return -1;
	  }

	//
	// Copy the new list into the old one.
	//
	for(int i=0; i<newlen; i++)
	  {
		srcList[srcLen+i] = newList[i];
	  }
	srcLen += newlen;
	srcList[srcLen] = NULL;

	return 0;
}

jint FontBrokerObject::registerDisplayer(FontDisplayerPeerObject *fpp)
{
	//
	// we need to make sure that the native displayer is added to the
	// tail. The native displayer will be the last in the queue
	// for asking for a font. Reason:
	// - The native displayer might be implemented to always return
	//   a font by doing a closest match.
	//
	FontDisplayerPeerObject *tmp_fpp;
	wfList::ERROR_CODE err = fpPeers.add(fpp);
	int ret = 0;
	if (err != wfList::SUCCESS)
	{
		ret = -1;
	}
	else
	{
		// Reconfigure the fpPeers list so that the native
		// displayer is at the end of the list.
		struct wfListElement *tmp = fpPeers.head;
		while(tmp)
		{
			tmp_fpp = (FontDisplayerPeerObject *) tmp->item;
			if (tmp_fpp->isNative())
			{
				break;
			}
			tmp = tmp->next;
		}
		// Move native to the tail
		if (tmp && tmp != fpPeers.tail)
		{
			WF_TRACEMSG(("NF: Moving the native displayer to the end of the displayer list."));
			tmp_fpp = (FontDisplayerPeerObject *) tmp->item;
			fpPeers.remove((void *)tmp_fpp);
			// we are banking on the fact that add will add at end.
			fpPeers.add(tmp_fpp);
		}
	}
	return(ret);
}
