/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/* 
 * fb.h (FontBrokerObject.h)
 *
 * C++ implementation of the (fb) FontBrokerObject
 *
 * dp Suresh <dp@netscape.com>
 */


#ifndef _fb_H_
#define _fb_H_

#include "libfont.h"
#include "nf.h"

// Uses :	FontMatchInfo
//			Font
//			RenderingContext
//			Observer
#include "Mnffmi.h"
#include "Mnff.h"
#include "Mnfrc.h"
#include "Mnfdoer.h"


#include "wfFCache.h"
#include "wffpPeer.h"

struct wf_font_broker_pref {
	int enableWebfonts;
};

class FontBrokerObject {
private:
  struct wf_font_broker_pref pref;
  int merge(struct nffmi ** &srcList, int &srcMax,
			struct nffmi **newList);
  FontDisplayerPeerObject *findDisplayer(const char *DisplayerName);
  int scanDisplayersFromDir(const char* dlm_dir);
  jint registerDisplayer(FontDisplayerPeerObject *fpp);

public:
  // Cache of Font Objects. Font <--> {RC,Fmi} mapping is done by this.
  wfFontObjectCache fontObjectCache;

  // A list of all FontDisplayers that we know about.
  wfList fpPeers;

  // The catalog filename
  const char *catalogFilename;
  wfList fpPeersFromCatalog;

  // Constructor and Destructor
  FontBrokerObject(void);
  ~FontBrokerObject(void);

  //
  // FontBrokerConsumer Interface specific
  //
  struct nff *LookupFont(struct nfrc *rc, struct nffmi *fmi, const char *accessor);

  struct nff *CreateFontFromUrl(struct nfrc* rc, const char* url_of_font,
								const char *url_of_page, jint faux,
								struct nfdoer* completion_callback,
								MWContext *context);
  struct nff *CreateFontFromFile(struct nfrc* rc, const char *mimetype,
								 const char* fontfilename,
								 const char *url_of_page);

  //
  // Catalog specific routines
  struct nffmi **ListFonts(struct nfrc *rc, struct nffmi *fmi);

  jdouble *ListSizes(struct nfrc *rc, struct nffmi *fmi);

  struct nff *GetBaseFont(struct nfrf *rf);

  //
  // FontBrokerDisplayer interface specific
  //
  jint RegisterFontDisplayer(struct nffp* fp);

  jint CreateFontDisplayerFromDLM(const char* dlm_name);

  // dlm_dir is a semicolon separated list of directory names
  //	eg.
  //	~/.netscape/fonts;/usr/lib/netscape/fonts
  // Each directory tree in this list searched for displayer DLMs
  // The number of displayers that were loaded is returned.
  //
  // Expansion happens for
  // ~	HOME
  //
  jint ScanForFontDisplayers(const char* dlm_dir);

  void RfDone(struct nfrf *rf);

  //
  // FontBrokerUtility interface specific
  //
  struct nffmi *CreateFontMatchInfo(const char* name, const char* charset,
									const char* encoding, jint weight,
									jint pitch, jint style, jint underline,
									jint strikeOut, jint resX, jint resY);

  struct nfrc *CreateRenderingContext(jint majorType, jint minorType,
	  void ** args, jsize nargs);

  struct nfdoer *CreateFontObserver(nfFontObserverCallback callback,
	  void *call_data);

  //
  // Preferences
  //
  jint IsWebfontsEnabled(void);
  jint EnableWebfonts(void);
  jint DisableWebfonts(void);

  const char **ListFontDisplayers();
  jint IsFontDisplayerEnabled(const char *displayer);
  const char **ListFontDisplayersForMimetype(const char *mimetype);
  const char *FontDisplayerForMimetype(const char *mimetype);

  jint EnableFontDisplayer(const char *displayer);
  jint DisableFontDisplayer(const char *displayer);
  jint EnableMimetype(const char *displayer, const char *mimetype);
  jint DisableMimetype(const char *displayer, const char *mimetype);

  jint LoadCatalog(const char *filename);
  jint SaveCatalog(const char *filename);

  const char *GetMimetype(const char *imimetype, const char *address);
};
#endif /* _fb_H_ */

