/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

