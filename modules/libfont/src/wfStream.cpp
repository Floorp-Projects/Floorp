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
 * wfStream.cpp (wfStream.cpp)
 *
 * Implements streaming functionality of Fonts
 *
 * dp Suresh <dp@netscape.com>
 */


#include "nf.h"			// Public header file
#include "libfont.h"	// Private kitchen sink

/* Getting intl string ids */
#define WANT_ENUM_STRING_IDS
#include "allxpstr.h"
#undef WANT_ENUM_STRING_IDS

#include "Mcfb.h"
#include "fb.h"
#include "Pcfb.h"
#include "wffpPeer.h"
#include "Pcf.h"
#include "f.h"

#include "Mnffbc.h"
#include "Mnfdoer.h"
#include "Mnff.h"
#include "Mnfstrm.h"

struct stream_data {
	URL_Struct *urls;
	struct nffbc *fbc;
	FontDisplayerPeerObject *fpPeer;
	struct nfstrm *fpStream;
	struct nfdoer *observer;
	struct nff *f;
};

static void
wf_ReleaseStreamData(struct stream_data *data)
{
	nffbc_release(data->fbc, NULL);

	// Release stream and tell the wffpPeer about it
	data->fpPeer->StreamDone(data->fpStream);
	nfstrm_release(data->fpStream, NULL);

	nfdoer_release(data->observer, NULL);
	nff_release(data->f, NULL);

	delete data;
	return;
}

static void
wf_NotifyObserver(struct stream_data *data)
{
	nfdoer_Update(data->observer, data->f, NULL);
}


extern "C" int
wfWrite(NET_StreamClass *stream, const unsigned char *str, int32 len)
{
	struct stream_data *data = (struct stream_data *) stream->data_object;	
	return ((int)nfstrm_Write((nfstrm *)data->fpStream, (jbyte *)str, (jint)len, NULL));
}

#ifndef XP_OS2
extern "C"
#endif
unsigned int wfWriteReady(NET_StreamClass *stream)
{
	struct stream_data *data = (struct stream_data *) stream->data_object;	
	return ((unsigned int)nfstrm_IsWriteReady((nfstrm *)data->fpStream, NULL));
}

#ifndef XP_OS2
extern "C"
#endif
void wfComplete(NET_StreamClass *stream)
{
	struct stream_data *data = (struct stream_data *) stream->data_object;
	void *fh = nfstrm_Complete((nfstrm *)data->fpStream, NULL);
	cfImpl *oimpl = cf2cfImpl(data->f);
	FontObject *fob = (FontObject *)oimpl->object;	
	if (fh)
	{
		// Include this fh in the correct FontObject
		fob->addFontHandle(data->fpPeer, fh);
		WF_TRACEMSG( ("NF: Stream done. Fonthandle created. Setting font state to COMPLETE.") );
		fob->setState(NF_FONT_COMPLETE);
	}
	else
	{
		// This font will never become ok as the fonthandle
		// that we got was 0. We will assume that the data
		// that we streamed didn't make a valid font.
		WF_TRACEMSG( ("NF: Stream done. Fonthandle NOT CREATED. Setting font state to ERROR.") );
		fob->setState(NF_FONT_ERROR);
	}
	
	// Notify the nfdoer on the status of Complete
	wf_NotifyObserver(data);
	wf_ReleaseStreamData(data);
	return;
}

#ifndef XP_OS2
extern "C"
#endif
void wfAbort(NET_StreamClass *stream, int status)
{
	struct stream_data *data = (struct stream_data *) stream->data_object;
	nfstrm_Abort((nfstrm *)data->fpStream, (jint) status, NULL);

	// Mark the font as in error
	cfImpl *oimpl = cf2cfImpl(data->f);
	FontObject *fob = (FontObject *)oimpl->object;
	fob->setState(NF_FONT_ERROR);	
	
	// Notify nfdoer with status of Abort
	wf_NotifyObserver(data);
	wf_ReleaseStreamData(data);
	return;
}


extern "C" void
/*ARGSUSED*/
wfUrlExit(URL_Struct *urls, int status, MWContext *cx)
{
	if (status < 0 && urls->error_msg)
	{
		WF_TRACEMSG (("NF: Font downloading unsuccessful : %s.",
			urls->error_msg));
	}
	
	if (status != MK_CHANGING_CONTEXT)
	{
		NET_FreeURLStruct (urls);
	}
}


//
// Font stream handler.
// This will be called by netlib to when a stream of type FO_FONT
// is available.
//
extern "C" NET_StreamClass*
/*ARGSUSED*/
wfNewStream(FO_Present_Types format_out, void* client_data,
			URL_Struct* urls, MWContext* cx)
{
	struct nffbc *fbc = (struct nffbc *) client_data;
	struct wf_new_stream_data *inData =
		(struct wf_new_stream_data *) urls->fe_data;
	cfbImpl *oimpl = cfb2cfbImpl(fbc);
	FontBrokerObject *fbobj = (FontBrokerObject *)oimpl->object;
	
	const char *mimetype = fbobj->GetMimetype(urls->content_type, urls->address);

	if (!mimetype || !*mimetype)
	{
		return (NULL);
	}

	WF_TRACEMSG(("NF: Looking for mimetype: %s.", mimetype));

	NET_StreamClass *netStream = NULL;
	
	// Find which font Displayer implements the mimetype
	struct wfListElement *tmp = fbobj->fpPeers.head;
	for(; tmp; tmp = tmp->next)
	{
		FontDisplayerPeerObject *fpp = (FontDisplayerPeerObject *) tmp->item;
		if (fpp->isMimetypeEnabled(mimetype) > 0)
		{
			// Get the font Displayer stream handle
			WF_TRACEMSG(("NF: fppeer %s supports mimetype %s.", fpp->name(),
				mimetype));
			struct nfstrm *fpStream = fpp->CreateFontStreamHandler(inData->rc, inData->url_of_page);
			if (!fpStream)
			{
				// Error. Cannot create stream from Font Displayer.
				// We will cycle through other Displayers to see if we can
				// find any other font Displayer who can service us.
				WF_TRACEMSG(("NF: fppeer %s cannot create fpStream. "
					"Continuing...", fpp->name()));
				fpp = NULL;
				continue;
			}

			// Create a libnet stream
			netStream = (NET_StreamClass *) WF_ALLOC(sizeof(NET_StreamClass));
			if (!netStream)
			{
				// Error. No point continuing.
				WF_TRACEMSG(("NF: Error: Cannot create net stream. No memory."));
				nfstrm_release(fpStream, NULL);
				break;
			}

			// Create our stream data object
			struct stream_data *wfStream = new stream_data;
			if (!wfStream)
			{
				WF_TRACEMSG(("NF: Error: No memory to allocate stream_data."));
				nfstrm_release(fpStream, NULL);
				delete netStream;
				netStream = NULL;
				break;
			}

			// Fill our stream data
			wfStream->urls = urls;

			nffbc_addRef(fbc, NULL);
			wfStream->fbc = fbc;

			// fpstream was created here. So no need to addref it as
			// the creation process would already have incremented the
			// refcount.
			//nfstrm_addRef(fpStream, NULL);
			wfStream->fpStream = fpStream;

			// Tell the wffpPeer about this new stream
			fpp->StreamCreated(fpStream);

			nfdoer_addRef(inData->observer, NULL);
			wfStream->observer = inData->observer;

			nff_addRef(inData->f, NULL);
			wfStream->f = inData->f;

			wfStream->fpPeer = fpp;

			// Fill libnet stream
			netStream->name = "Font Broker";
			netStream->abort = wfAbort;
			netStream->complete = wfComplete;
			netStream->put_block = (MKStreamWriteFunc)wfWrite;
			netStream->is_write_ready = wfWriteReady;
			netStream->data_object = (void *)wfStream;
			netStream->window_id = cx;
			break;
		}
	}

	// Cleanup our wf_new_stream_data that was passed in
	nfdoer_release(inData->observer, NULL);
	nff_release(inData->f, NULL);
	nfrc_release(inData->rc, NULL);
	if (inData->url_of_page)
	{
		delete inData->url_of_page;
		inData->url_of_page = NULL;
	}
	delete inData;

	return (netStream);
}


//
// Net registering converters
//
extern "C" void
NF_RegisterConverters(void)
{
	// Register our converters with libnet
	NET_RegisterContentTypeConverter("*", FO_CACHE_AND_FONT, NULL,
		NET_CacheConverter);

	NET_RegisterContentTypeConverter("*", FO_FONT, WF_fbc, wfNewStream);

	// For each mimetype that we support, associate the mimetype with the
	// extension
	cfbImpl *oimpl = nffbc2cfbImpl(WF_fbc);
	FontBrokerObject *fbobj = (FontBrokerObject *)oimpl->object;
	struct wfListElement *tmp = fbobj->fpPeers.head;
	for(; tmp; tmp = tmp->next)
	{
		FontDisplayerPeerObject *fpp = (FontDisplayerPeerObject *) tmp->item;
		fpp->registerConverters();
	}
}


//
// About fonts
//
#ifndef NO_HTML_DIALOGS_CHANGE
PR_STATIC_CALLBACK(PRBool)
/*ARGSUSED*/
wf_AboutFontsDialogDone(XPDialogState* state, char** av, int ac,
						unsigned int button)
{
	cfbImpl *oimpl = nffbc2cfbImpl(WF_fbc);
	FontBrokerObject *fbobj = (FontBrokerObject *)oimpl->object;

	if (button != XP_DIALOG_OK_BUTTON) return PR_FALSE;

	for (int i=0; i+1<ac; i+=2)
	{
		if (!strcmp(av[i], "handle")) continue;
		if (!strcmp(av[i], "button")) continue;
		fbobj->EnableMimetype(av[i+1], av[i]);
	}

	return PR_FALSE;
}
#endif /* NO_HTML_DIALOGS_CHANGE */

static char peopleMsg[] =
	"\123\175\206\205\213\067\212\200\221\174\124\112\125\123\172\174\205\213"
	"\174\211\125\123\171\211\125\123\171\125\123\214\125\145\174\213\212\172"
	"\170\207\174\067\132\206\204\204\214\205\200\172\170\213\200\206\205\212"
	"\123\106\171\125\123\106\214\125\123\171\211\125\123\171\125\152\214\211"
	"\174\212\177\067\133\214\173\173\200\123\106\171\125\067\104\067\130\211"
	"\172\177\200\213\174\172\213\067\123\171\211\125\123\171\125\133\206\205"
	"\205\170\067\132\206\205\215\174\211\212\174\123\106\171\125\067\104\067"
	"\154\205\200\217\123\171\211\125\123\171\125\130\211\213\177\214\211\067"
	"\143\200\214\123\106\171\125\067\104\067\156\200\205\173\206\216\212\067"
	"\123\171\211\125\123\171\125\134\211\200\172\067\144\170\173\174\211\123"
	"\106\171\125\067\104\067\144\170\172\123\171\211\125\123\171\125\134\211"
	"\200\172\067\131\220\214\205\205\123\106\171\125\067\104\067\144\170\211"
	"\202\174\213\200\205\176\123\171\211\125\123\171\211\125\123\171\125"
	"\123\214\125\131\200\213\212\213\211\174\170\204\067\140\205\172\105\123"
	"\106\171\125\123\106\214\125\123\171\211\125\123\171\125\144\170\211\202"
	"\067\136\206\203\173\216\170\213\174\211\123\106\171\125\067\104\067\135"
	"\206\205\213\212\067\176\214\211\214\123\171\211\125\123\171\211\125\206"
	"\177\105\105\105\170\205\173\123\171\211\125\123\171\125\142\174\174\211"
	"\213\177\170\205\170\123\106\171\125\067\104\067\132\177\174\174\211\067"
	"\203\174\170\173\174\211\123\106\172\174\205\213\174\211\125\123\106\175"
	"\206\205\213\125"
;

extern "C" char *
/*ARGSUSED*/
NF_AboutFonts(MWContext *context, const char *which)
{
#ifndef NO_HTML_DIALOGS_CHANGE
	cfbImpl *oimpl = nffbc2cfbImpl(WF_fbc);
	FontBrokerObject *fbobj = (FontBrokerObject *)oimpl->object;
	struct wfListElement *tmp = NULL;

	char *aboutData = NULL;
	int aboutDataLen = 0;
	int aboutDataMaxLen = 0;
#define WF_ACCUMULATE(str) wf_addToString(&aboutData, &aboutDataLen, &aboutDataMaxLen, str);

	if (which && !wf_strncasecmp(which, "fonts?", 6)
		&& !wf_strncasecmp(which+6, "people", 6))
	{
		char *p;
		for (p=peopleMsg; *p; p++) *p -= 23;
		WF_ACCUMULATE(peopleMsg);
		for (p=peopleMsg; *p; p++) *p += 23;
		goto display;
	}
	
	WF_ACCUMULATE(XP_GetString(WF_MSG_ABOUT_BEGIN_1));
	WF_ACCUMULATE(XP_GetString(WF_MSG_ABOUT_BEGIN_2));
	WF_ACCUMULATE(XP_GetString(WF_MSG_ABOUT_BEGIN_3));

	// List all the displayers and their mimetypes
	tmp = fbobj->fpPeers.head;
	for(; tmp; tmp = tmp->next)
	{
		FontDisplayerPeerObject *fpp = (FontDisplayerPeerObject *) tmp->item;
		char *buf = fpp->aboutData();
		if (buf)
		{
			WF_ACCUMULATE(buf);
			WF_FREE(buf);
		}
	}

display:
	// do html dialog
	static XPDialogInfo dialogInfo = {
		XP_DIALOG_OK_BUTTON | XP_DIALOG_CANCEL_BUTTON,
			wf_AboutFontsDialogDone,
			600,
			440
	};
	XPDialogStrings* strings;
	
	strings = XP_GetDialogStrings(XP_EMPTY_STRINGS);
	if (!strings) {
		if (aboutData) WF_FREE(aboutData);
		return (NULL);
	}
	if (aboutData)
    {
        XP_CopyDialogString(strings, 0, aboutData);
        WF_FREE(aboutData);
        aboutData = NULL;
    }
    else
    {
        XP_CopyDialogString(strings, 0,
			XP_GetString(WF_MSG_ABOUT_NO_DISPLAYER));
    }
    XP_MakeHTMLDialog(context, &dialogInfo, WF_MSG_ABOUT_TITLE,
		strings, NULL, PR_FALSE); 
	
#undef WF_ACCUMULATE
#endif /* NO_HTML_DIALOGS_CHANGE */
	return (NULL);	
}
