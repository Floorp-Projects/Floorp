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
 * wffppeer.cpp (FontDisplayerPeerObject.cpp)
 *
 * This object is local to the FontDisplayer. One of these objects
 * exist for every FontDisplayer that exists. All calls to the FontDisplayer
 * are routed through this peer object. This takes care of loading and
 * unloading the FontDisplayers as neccessary.
 *
 * dp Suresh <dp@netscape.com>
 */


#include "wffpPeer.h"

#include "fe_proto.h"
#define WF_ONE_MILLESEC 1
#define WF_ONE_MINUTE (60 * 1000 * WF_ONE_MILLESEC)

/* Getting intl string ids */
#define WANT_ENUM_STRING_IDS
#include "allxpstr.h"
#undef WANT_ENUM_STRING_IDS

FontDisplayerPeerObject::
FontDisplayerPeerObject(struct nffp *fp)
	: dlm(NULL), deleted(0), disabled(0), native(-1),
	  streamCount(0), fhList(NULL), unloadTimerId(NULL)
{
	XP_ASSERT(fp);
	fpType = FontDisplayerPeerObject::WF_FP_STATIC;

	fontDisplayer = fp;

	// Although there we aren't expected to free the returned
	// Name and Description, we could unload this dynamic modules
	// and the name and decription could be in their data space.
	// So to prevent losing these, we take a copy.
	displayerName = CopyString(nffp_Name(fontDisplayer, NULL));
	displayerDescription = CopyString(nffp_Description(fontDisplayer, NULL));
	
#if defined(XP_WIN) && !defined(WIN32)
    // Win16 It seems the const char * assignment doesn't work.
	char mimeString[256];
	*mimeString = '\0';
	strcpy(mimeString, nffp_EnumerateMimeTypes(fontDisplayer, NULL));
#else
	const char *mimeString = nffp_EnumerateMimeTypes(fontDisplayer, NULL);
#endif
	if (mimeString && *mimeString )
	{
		mimeList.reconstruct(mimeString);
		registerConverters();
	}
}


FontDisplayerPeerObject::
FontDisplayerPeerObject(const char *dlmName)
	: dlm(dlmName), mimeList(NULL), deleted(0), disabled(0), native(-1),
	  streamCount(0), fhList(NULL), unloadTimerId(NULL)
{
	fpType = FontDisplayerPeerObject::WF_FP_DYNAMIC;

	fontDisplayer = NULL;
	displayerName = NULL;
	displayerDescription = NULL;

	if (dlm.status() < 0)
	{
		deleted = 1;
		return;
	}

#ifdef DEBUG
	// Don't load in purified versions of libraries.
	if(XP_STRSTR(dlmName, "_pure_")) {
		XP_ASSERT(0);
		deleted = 1;
		return;
	}
#endif

	if (load() < 0)
	{
		deleted = 1;
		return;
	}
	displayerName = CopyString(nffp_Name(fontDisplayer, NULL));
	displayerDescription = CopyString(nffp_Description(fontDisplayer,NULL));
	
#if defined(XP_WIN) && !defined(WIN32)
    // Win16 It seems the const char * assignment doesn't work.
	char mimeString[256];
	*mimeString = '\0';
	strcpy(mimeString, nffp_EnumerateMimeTypes(fontDisplayer, NULL));
#else
	const char *mimeString = nffp_EnumerateMimeTypes(fontDisplayer, NULL);
#endif
	if (mimeString && *mimeString )
	{
		mimeList.reconstruct(mimeString);
		registerConverters();
	}
}

FontDisplayerPeerObject::
FontDisplayerPeerObject(FontCatalogFile &fc)
	: dlm(NULL), deleted(0), disabled(0), native(-1),
	  streamCount(0), fhList(NULL), unloadTimerId(NULL)
{
	fpType = FontDisplayerPeerObject::WF_FP_DYNAMIC;
	fontDisplayer = NULL;
	displayerName = NULL;
	displayerDescription = NULL;

	reconstruct(fc);
}


FontDisplayerPeerObject::
~FontDisplayerPeerObject()
{
	finalize();
}

jint FontDisplayerPeerObject::
describe(FontCatalogFile &fc)
{
	char *s;
	
	// Print out the displayer information
	
	if (fpType != WF_FP_DYNAMIC)
	{
		return (0);
	}
	fc.output("displayer = {");
	fc.indentIn();

	s = "dynamic";
	fc.output("type", s);

	s = dlm.describe();
	fc.output("dlm", s);
	if (s)
	{
		XP_FREE(s);
	}

	fc.output("name", displayerName);
	fc.output("description", displayerDescription);

	s = mimeList.describe();
	fc.output("mimeString", s);
	if (s)
	{
		XP_FREE(s);
	}

	fc.output("deleted", deleted);
	fc.output("disabled", disabled);

	fc.output("catalog = {");
	fc.indentIn();
	catalog.describe(fc);
	fc.indentOut();
	fc.output("} // End of catalog");

	fc.indentOut();
	fc.output("} // End of displayer");

	return (0);
}

jint FontDisplayerPeerObject::
reconstruct(FontCatalogFile &fc)
{
	char buf[1024];
	int buflen;

	int over = 0;
	char *variable = NULL;
	char *value = NULL;
	char *ret;

	finalize();
	
	while (!over)
	{
		ret = fc.readline(buf, sizeof(buf));
		if (!ret)
		{
			over = 1;
			continue;
		}
		buflen = strlen((const char *)buf);
		if (buf[buflen-1] == '\n')
		{
			buf[buflen-1] = '\0';
			buflen--;
		}
		if (!strncmp(buf, "#", 1))
		{
			// Ignore comments
			continue;
		}
		wf_scanVariableAndValue(buf, buflen, variable, value);
		if (!wf_strcasecmp(variable, "displayer"))
		{
			continue;
		}
		else if (!wf_strcasecmp(variable, "type"))
		{
			if (!wf_strcasecmp(value, "dynamic"))
			{
				fpType = WF_FP_DYNAMIC;
			}
			else
			{
				deleted = 1;
				over = 1;
				continue;
			}
		}
		else if (!wf_strcasecmp(variable, "dlm"))
		{
			fontDisplayer = NULL;
			dlm.reconstruct(value);
			if (dlm.status() < 0)
			{
				deleted = 1;
				over = 1;
				continue;
			}
		}
		else if (!wf_strcasecmp(variable, "disabled"))
		{
			disabled = atoi(value);
		}
		else if (!wf_strcasecmp(variable, "name"))
		{
			if (displayerName) delete displayerName;
			displayerName = CopyString(value);
		}

		else if (!wf_strcasecmp(variable, "description"))
		{
			if (displayerDescription) delete displayerDescription;
			displayerDescription = CopyString(value);
		}
		else if (!wf_strcasecmp(variable, "mimeString"))
		{
			mimeList.reconstruct(value);
			registerConverters();
		}
		else if (!wf_strcasecmp(variable, "deleted"))
		{
			deleted = atoi(value);
		}
		else if (!wf_strcasecmp(variable, "catalog"))
		{
			catalog.reconstruct(fc);
		}
		else if (!strncmp(variable, "}", 1))
		{
			over = 1;
			continue;
		}
	}

	// sanity check the fpp
	if (dlm.filename() == NULL || dlm.status() <= 0 || fpType != WF_FP_DYNAMIC)
		deleted = 1;
	return (0);
}


jdouble *
FontDisplayerPeerObject::EnumerateSizes(struct nfrc *rc, void *fh)
{
	// Check for generic preconditions
	if (deleted || disabled)
	{
		return NULL;
	}

	// Check for call specific preconditions
	if (load() < 0)
	{
		deleted = 1;
		return (NULL);
	}
	return((jdouble *)nffp_EnumerateSizes(fontDisplayer, rc, fh, NULL));
}

struct nffmi * 
FontDisplayerPeerObject::GetMatchInfo(void *fh)
{
	// Check for generic preconditions
	if (deleted || disabled)
	{
		return (NULL);
	}

	// Check for call specific preconditions
	if (load() < 0)
	{
		deleted = 1;
		return (NULL);
	}
	return(nffp_GetMatchInfo(fontDisplayer, fh, NULL));
}

struct nfrf *
FontDisplayerPeerObject::
CreateRenderableFont(struct nfrc *rc, void *fh, jdouble pointsize)
{
	// Check for generic preconditions
	if (deleted || disabled)
	{
		return (NULL);
	}

	// Check for call specific preconditions
	if (load() < 0)
	{
		deleted = 1;
		return (NULL);
	}
	return(nffp_GetRenderableFont(fontDisplayer, rc, fh, pointsize, NULL));
}

void *
FontDisplayerPeerObject::LookupFont(struct nfrc *rc, struct nffmi *fmi, const char *accessor)
{
	// Check for generic preconditions
	if (deleted || disabled)
	{
		return (NULL);
	}

	// Check for call specific preconditions
	if (load() < 0)
	{
		deleted = 1;
		return (NULL);
	}
	return(nffp_LookupFont(fontDisplayer, rc, fmi, accessor, NULL));
}

void *
FontDisplayerPeerObject::CreateFontFromFile(struct nfrc *rc, const char *mimetype,
										   const char *filename, const char *urlOfPage)
{
	// Check for generic preconditions
	if (deleted || disabled)
	{
		return (NULL);
	}

	// Check for call specific preconditions
	if (load() < 0)
	{
		deleted = 1;
		return (NULL);
	}
	return(nffp_CreateFontFromFile(fontDisplayer, rc, mimetype, filename, urlOfPage,
								   NULL));
}

struct nfstrm *
FontDisplayerPeerObject::CreateFontStreamHandler(struct nfrc *rc, const char *urlOfPage)
{
	// Check for generic preconditions
	if (deleted || disabled)
	{
		return (NULL);
	}

	// Check for call specific preconditions
	if (load() < 0)
	{
		deleted = 1;
		return (NULL);
	}
	return((struct nfstrm *)
			nffp_CreateFontStreamHandler(fontDisplayer, rc, urlOfPage, NULL));
}


jint
FontDisplayerPeerObject::ReleaseFontHandle(void *fh)
{
	// Check for generic preconditions
	if (deleted || disabled)
	{
		return (-1);
	}

	// Check for call specific preconditions
	if (load() < 0)
	{
		deleted = 1;
		return (-1);
	}
	return(nffp_ReleaseFontHandle(fontDisplayer, fh, NULL));
}

//
// Catalogue routines
//

struct nffmi **
FontDisplayerPeerObject::ListFonts(struct nfrc *rc, struct nffmi *fmi)
{
	// Check for generic preconditions
	if (deleted || disabled)
	{
		return (NULL);
	}

	// Check for call specific preconditions
	if (load() < 0)
	{
		deleted = 1;
		return (NULL);
	}
	return((struct nffmi **)
			 nffp_ListFonts(fontDisplayer, rc, fmi, NULL));
}

jdouble *
FontDisplayerPeerObject::ListSizes(struct nfrc *rc, struct nffmi *fmi)
{
	// Check for generic preconditions
	if (deleted || disabled)
	{
		return (NULL);
	}

	// Check for call specific preconditions
	if (load() < 0)
	{
		deleted = 1;
		return (NULL);
	}
	return((jdouble *)
			 nffp_ListSizes(fontDisplayer, rc, fmi, NULL));
}

int
FontDisplayerPeerObject::queryCatalog(struct nfrc *rc, struct nffmi *fmi)
{
	int ret = catalog.supportsFmi(rc, fmi);

	if (ret < 0)
	{
		// Catalog may not have been initialized.
		if (!catalog.isInitialized(rc))
		{
			// Now we need to load the dll to initialize the catalog.
			struct nffmi **fmis = ListFonts(rc, NULL);
			catalog.update(rc, fmis);
			
			if (fmis)
			{
				// XXX We need to free the fmis. We may get away
				// XXX doing that if we make catalog.update() not
				// XXX take a copy of the fmi.
				wf_releaseFmis(fmis);
				WF_FREE(fmis);
			}
			ret = catalog.supportsFmi(rc, fmi);
		}
	}
	return (ret);
}


//
// Displayer loading/unloading management routines
//
int
FontDisplayerPeerObject::dlmChanged(const char *dlm_name)
{
	int ret = -1;
	// if this is the dlm that we are associcated with
	if (!strcmp(dlm.filename(), dlm_name))
	{
		if (dlm.isChanged() == 0)
		{
			// Unchanged
			ret = 0;
		}
		else
		{
			// This mean the dlm has either changed or may have been deleted.
			ret = 1;
		}
	}

	return (ret);
}

void
FontDisplayerPeerObject::resync(void)
{
	native = -1;

	finalizeExceptDlmAndDisabled();

	// This might cause a core dump as usually a resync() is done only
	// when we know that the dlm has changed. And if it was loaded in
	// memory and the disk copy was changed whew!
	// Some platforms wont let the copy on disk to change. So we are kind of
	// safe.
	//
	// XXX A better solution would be to not unload it at all but just clear our
	// internal state. Let us see.
	dlm.unload(/*force*/ 1);

	dlm.sync();

	if (dlm.status() < 0)
	{
		deleted = 1;
		return;
	}
	if (load() < 0)
	{
		deleted = 1;
		return;
	}
	displayerName = CopyString(nffp_Name(fontDisplayer, NULL));
	displayerDescription = CopyString(nffp_Description(fontDisplayer,NULL));
	
#if defined(XP_WIN) && !defined(WIN32)
    // Win16 It seems the const char * assignment doesn't work.
	char mimeString[256];
	*mimeString = '\0';
	strcpy(mimeString, nffp_EnumerateMimeTypes(fontDisplayer, NULL));
#else
	const char *mimeString = nffp_EnumerateMimeTypes(fontDisplayer, NULL);
#endif
	if (mimeString && *mimeString )
	{
		mimeList.reconstruct(mimeString);
		registerConverters();
	}
}

//
// mime handling routines
//

int
FontDisplayerPeerObject::countMimetypes()
{
	// Check for generic preconditions
	if (deleted || disabled)
	{
		return (-1);
	}
	return (mimeList.count());
}

int
FontDisplayerPeerObject::isMimetypeEnabled(const char *mimetype)
{
	// Check for generic preconditions
	if (deleted || disabled)
	{
		return (0);
	}

	return (mimeList.isEnabled(mimetype));
}

const char *
FontDisplayerPeerObject::getMimetypeFromExtension(const char *ext)
{
	// Check for generic preconditions
	if (deleted || disabled)
	{
		return (0);
	}

	return (mimeList.getMimetypeFromExtension(ext));
}


int
FontDisplayerPeerObject::disableMimetype(const char *mimetype)
{
	// Check for generic preconditions
	if (deleted)
	{
		return (-1);
	}

	return (mimeList.setEnabledStatus(mimetype, 0));
}

int
FontDisplayerPeerObject::enableMimetype(const char *mimetype)
{
	// Check for generic preconditions
	if (deleted)
	{
		return (-1);
	}

	return (mimeList.setEnabledStatus(mimetype, 1));
}

//
// Additional routines
//

const char *
FontDisplayerPeerObject::name(void)
{
	// Check for generic preconditions
	if (deleted || disabled)
	{
		return (NULL);
	}

	return (displayerName);
}

int
FontDisplayerPeerObject::isNative(void)
{
	if (native < 0)
	{
		native = 0;	// not native
		if (displayerName)
		{
			native = (strcmp(displayerName, NF_NATIVE_FONT_DISPLAYER) == 0);
		}
	}
	// Check if this is the native font displayer
	return (native);
}

int
FontDisplayerPeerObject::isDeleted(void)
{
	return (deleted);
}

int
FontDisplayerPeerObject::isLoaded(void)
{
	int ret = 0;

	// Check for generic preconditions
	if (deleted || disabled)
	{
		return (ret);
	}

	if (fontDisplayer || fpType == FontDisplayerPeerObject::WF_FP_STATIC)
	{
		ret = 1;
	}
	return (ret);
}


void
FontDisplayerPeerObject::StreamCreated(struct nfstrm *strm)
{
	streamCount++;
}

void
FontDisplayerPeerObject::StreamDone(struct nfstrm *strm)
{
	streamCount--;

	decideToUnload();
}

void
FontDisplayerPeerObject::FontHandleCreated(void *fh)
{
	fhList.add(fh);
}

void
FontDisplayerPeerObject::FontHandleDone(void *fh)
{
	if (fhList.remove(fh) == wfList::SUCCESS)
	{
		// If there was no other copy of the fh in our fhlist
		// then this was the last fh and can be released.
		if (fhList.isExist(fh) != wfList::SUCCESS)
		{
			ReleaseFontHandle(fh);
			
			if (fpType == FontDisplayerPeerObject::WF_FP_DYNAMIC)
			{
				// Static displayers need not be unloaded.
				decideToUnload();
			}
		}
	}
}


int
FontDisplayerPeerObject::disableDisplayer(void)
{
	if (deleted)
	{
		return (-1);
	}

	disabled = 1;
	return (0);
}

int
FontDisplayerPeerObject::enableDisplayer(void)
{
	if (deleted)
	{
		return (-1);
	}

	disabled = 0;
	return (0);
}

int
FontDisplayerPeerObject::isDisplayerEnabled(void)
{
	if (deleted)
	{
		return (0);
	}
	return ((disabled ? 0 : 1));
}


int
FontDisplayerPeerObject::registerConverters(void)
{
	if (deleted)
	{
		return (-1);
	}

	if (!mimeList.isEmpty())
	{
		// Foreach mime
		struct wfListElement *tmp = mimeList.head;
		for (; tmp; tmp = tmp->next)
		{
			struct mime_store *ele = (struct mime_store *) tmp->item;
			NET_cdataCommit(ele->mimetype, ele->extensions);
		}
	}
	
	return (0);
}


char *
FontDisplayerPeerObject::aboutData(void)
{
	char *aboutData = NULL;
	int aboutDataLen = 0;
	int aboutDataMaxLen = 0;
#ifndef NO_HTML_DIALOGS_CHANGE
#define WF_ABOUT_DATA_ALLOCATION_STEP 64
#define WF_ACCUMULATE(str) wf_addToString(&aboutData, &aboutDataLen, &aboutDataMaxLen, str);

	// Dont show deleted font displayers.
	if (isDeleted())
	{
		return (NULL);
	}


	// Displayer data
    char *fmtstring = NULL;
    char *buf;
    if (fpType == FontDisplayerPeerObject::WF_FP_STATIC)
    {
        fmtstring = XP_GetString(WF_MSG_ABOUT_DISPLAYER_STATIC);
        buf = PR_smprintf(fmtstring,
			(displayerName ? displayerName : ""),
			(displayerDescription ? displayerDescription : ""));
    }
    else
    {
        fmtstring = XP_GetString(WF_MSG_ABOUT_DISPLAYER_DYNAMIC);
        buf = PR_smprintf(fmtstring,
			(displayerName ? displayerName : ""),
			(displayerDescription ? displayerDescription : ""),
			dlm.filename());
    }
    if (buf)
    {
        WF_ACCUMULATE(buf);
        XP_FREE(buf);
        buf = NULL;
    }
	if (!mimeList.isEmpty())
	{
		// Foreach mime
		struct wfListElement *tmp = mimeList.head;
		for (; tmp; tmp = tmp->next)
		{
			struct mime_store *ele = (struct mime_store *) tmp->item;
			buf = PR_smprintf(XP_GetString(WF_MSG_ABOUT_DISPLAYER_MIME),
				ele->mimetype,
				(displayerName ? displayerName : ""),
				(ele->isEnabled ? "checked" : ""), /* NO I18N */
				ele->mimetype, ele->description,
				ele->extensions);
			if (buf)
			{
				WF_ACCUMULATE(buf);
				XP_FREE(buf);
				buf = NULL;
			}
		}
	}
	WF_ACCUMULATE(XP_GetString(WF_MSG_ABOUT_DISPLAYER_END));
#endif /* NO_HTML_DIALOGS_CHANGE */
	return (aboutData);
}
	
//
// Private method implementations
//

int
FontDisplayerPeerObject::load(void)
{
	// Check for generic preconditions
	if (deleted || disabled)
	{
		return (0);
	}

	if (fontDisplayer || fpType == FontDisplayerPeerObject::WF_FP_STATIC)
	{
		return (0);
	}

	dlm.load();
	if (dlm.status() < 0)
	{
		return (-1);
	}

	fontDisplayer = dlm.createDisplayerObject(WF_fbp);
	if (!fontDisplayer)
	{
		WF_TRACEMSG(("NF: dlm (%s) Couldn't create fontdisplayer object. Skipping dlm.",
				  dlm.filename()));
		dlm.unload();
		return (-1);
	}
	return (0);
}

extern "C" void wf_unloadTimer(void *closure)
{
	FontDisplayerPeerObject *fpp = (FontDisplayerPeerObject *)closure;
	if (fpp->unloadTimerId)
	{
		// One last sanity check
		if (fpp->streamCount == 0 && fpp->fhList.isEmpty())
		{
			fpp->unload();
		}
		fpp->unloadTimerId = 0;
	}
}

void
FontDisplayerPeerObject::decideToUnload(void)
{
	if (streamCount == 0 && fhList.isEmpty())
	{
		// This displayer can be unloaded.
		// I really want to do something like unloading the displayer
		// after say 1 min. because say the navigator moves from one page
		// to another and both had use for this displayer, then this displayer
		// will get loaded twice and unloaded once causing thrashing.
		//
		// The problem in implementing this is how to we ensure we get
		// back control after this many seconds. We should use the
		// FE Timer callback.
		
		// Clear any previous timers
		if (unloadTimerId)
		{
			FE_ClearTimeout(unloadTimerId);
		}

		// Set the new timer
		unloadTimerId = FE_SetTimeout(wf_unloadTimer, this, WF_ONE_MINUTE);
	}
	else
	{
		// Unload should not happen
		if (unloadTimerId)
		{
			FE_ClearTimeout(unloadTimerId);
		}
	}
}

int
FontDisplayerPeerObject::unload(void)
{
	if (fpType == FontDisplayerPeerObject::WF_FP_STATIC)
	{
		// Static displayer need not be unloaded.
		return (0);
	}
	if (!fontDisplayer)
	{
		// We were never loaded.
		return (-1);
	}
	nffp_release(fontDisplayer, NULL);
	fontDisplayer = NULL;
	return (dlm.unload());
}

int FontDisplayerPeerObject::
finalizeExceptDlmAndDisabled()
{
	// Release memory
	if (displayerName)
	{
		delete displayerName;
		displayerName = NULL;
	}
	if (displayerDescription)
	{
		delete displayerDescription;
		displayerDescription = NULL;
	}

	// Clear native flag
	native = -1;
	// Clear deleted flag
	deleted = 0;
	
	// Clean mimelist
	mimeList.finalize();

	// Clean catalog
	catalog.finalize();

	return (0);
}

int FontDisplayerPeerObject::
finalize()
{
	finalizeExceptDlmAndDisabled();

	// Clean dlm
	unload();
	dlm.finalize();
	
	// Clear disabled flag
	disabled = 0;

	return (0);
}

