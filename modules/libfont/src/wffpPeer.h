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
