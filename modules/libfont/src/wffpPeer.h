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
 * wffppeer.h (FontDisplayerPeerObject.h)
 *
 * This object is local to the FontDisplayer. One of these objects
 * exist for every FontDisplayer that exists. All calls to the FontDisplayer
 * are routed through this peer object. This takes care of loading and
 * unloading the FontDisplayers as neccessary.
 *
 * dp Suresh <dp@netscape.com>
 */


#ifndef _wffpPeer_H_
#define _wffpPeer_H_

#include "libfont.h"
#include "nf.h"

#include "Mnfrf.h"
#include "Mnffmi.h"
#include "Mnfrc.h"
#include "Mnffp.h"
#include "Mnfstrm.h"

#include "wfList.h"
#include "wfMime.h"
#include "wfDlm.h"
#include "wffpCat.h"

extern "C" void wf_unloadTimer(void *closure);

class FontDisplayerPeerObject {
public:
  typedef enum {
    WF_FP_STATIC = 0,
    WF_FP_DYNAMIC
#if defined(XP_OS2)
	/* insure that we know the size of this enum (4 bytes!) */
	, WF_FP_TYPE_MAX=70000 
#endif
  } WF_FP_TYPE;

protected:

  // Whether this Displayer was loaded dynamically or not
  WF_FP_TYPE fpType;
  
  // These are valid only if the fpType is 'WF_FP_DYNAMIC'
  wfDlm dlm;

  // Name and description of the Displayer
  char *displayerName;
  char *displayerDescription;

  // Mimetypes that this displayer can support
  wfMimeList mimeList;

  // Access Contol
  // deleted : means this fp will no longer be used for this session and
  //			is a candidate for garbage collection.
  char deleted;
  // disabled : means this fp will not be used unless enabled across session.
  char disabled;

  // The actual displayer object that implements the nffp interface
  struct nffp *fontDisplayer;

  int native;

  // Catalog
  FontDisplayerCatalogObject catalog;

  // We need to refcount fh and stream opened by this displayer
  // to decide when to unload the displayer
  int streamCount;
  wfList fhList;
  void *unloadTimerId;

protected:
  int load(void);
  int unload(void);
  int finalize(void);
  int finalizeExceptDlmAndDisabled(void);

public:
  //
  // Constructor and Destructor
  //

  FontDisplayerPeerObject(struct nffp *fp);
  FontDisplayerPeerObject(const char *dlmName);
  FontDisplayerPeerObject(FontCatalogFile &fc);
  ~FontDisplayerPeerObject();

  //
  // Fontdisplayer routines
  //

  jdouble *EnumerateSizes(struct nfrc *rc, void *fh);
  struct nfrf * CreateRenderableFont(struct nfrc *rc, void *fh, jdouble pointsize);
  void * LookupFont(struct nfrc *rc, struct nffmi *fmi, const char *accessor);
  void * CreateFontFromFile(struct nfrc *rc, const char *mimetype,
	  const char *fontFilename, const char *urlOfPage);
  struct nfstrm * CreateFontStreamHandler(struct nfrc *rc, const char *urlOfPage);
  struct nffmi *GetMatchInfo(void *fh);
  private:
  jint ReleaseFontHandle(void *fh);

  //
  // Catalogue routines
  //
  public:
  struct nffmi ** ListFonts(struct nfrc *rc, struct nffmi *fmi);
  jdouble * ListSizes(struct nfrc *rc, struct nffmi *fmi);

  //
  // mime handling routines
  //
  const char * getMimetypeFromExtension(const char *ext);
  
  int countMimetypes();

  int isMimetypeEnabled(const char *mimetype);
  int disableMimetype(const char *mimetype);

  // If this wffpPeer matches the dlm_name and
  //	if the dlm has changed, returns 1.
  //	else returns 0
  // If dlm_name doesn't match the dlm this wffpPeer is associated with, returns -1.
  int dlmChanged(const char *dlm_name);

  // resync:
  // If the dlm changed, then this will reload the dlm and reinitialize
  //
  // WARNING: before calling this, the CALLER needs to ensure that all references
  // to this wffpPeer is removed as after resync(), the kind of fonts this displayer
  // could serve could change and worse, this displayer may get deleted too.
  //
  void resync(void);
  int enableMimetype(const char *mimetype);

  //
  // Catalog routines
  //
  jint describe(FontCatalogFile &fc);
  jint reconstruct(FontCatalogFile &fc);
  int queryCatalog(struct nfrc *rc, struct nffmi *fmi);

  //
  // Additional routines
  //

  const char *name(void);
  int isNative(void);
  int isDeleted(void);
  int isLoaded(void);

  // For refcounting and deciding when to unload displayers
  void StreamCreated(struct nfstrm *strm);
  void StreamDone(struct nfstrm *strm);
  void FontHandleCreated(void *fh);
  void FontHandleDone(void *fh);
  void decideToUnload(void);

  //
  // For display in the about:fonts information, this will generate html
  //
  char *aboutData(void);

  // These return -1 if error; 0 if success
  int disableDisplayer(void);
  int enableDisplayer(void);
  int isDisplayerEnabled(void);

  // Register a converter to libnet/ so that the association between mimetype
  // and extensions that are supported is recognized.
  int registerConverters(void);

  friend void wf_unloadTimer(void *closure);
};

#endif /* _wffpPeer_H_ */
