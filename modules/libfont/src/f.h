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
 * f.h (FontObject.h)
 *
 * C++ implementation of the (f) FontObject
 *
 * dp Suresh <dp@netscape.com>
 */


#ifndef _f_H_
#define _f_H_

#include "libfont.h"

// Uses :	RenderingContext
//			RenderableFont
//			FontDisplayer
//			Font
#include "Mnfrc.h"
#include "Mnfrf.h"
#include "Mnffp.h"
#include "Mnff.h"

#include "wffpPeer.h"
#include "wfList.h"
#include "wfSzList.h"

struct fh_store {
  FontDisplayerPeerObject *fppeer;
  void *fh;
  wfSizesList sizesList;
};

class FontObject : public wfList {
private:
  // The nff that has this FontObject. This is reverse mapping
  // for the purpose of accessing refcount.
  // WARNING: Holding this will not increment the refcount of the
  //			nff. This is there for the purpose of elements of the
  //			list to refcount the FontObject.
  struct nff *self;

  // While the GarbageColletor for the Font object is running it
  // will set this member. This will ensure that when the GC is
  // releasing references to the Font object, it will not step over
  // itself.
  int inGC;
  
  // These are valid only for webfonts. If this font object
  // reflects a webfont, then isWebFont will be 1 and
  // {urlOffont} will indicate where this was created
  // from.
  int iswebfont;
  const char *urlOfFont;

  // For webfonts this specifies the state of the font. For
  // non-webfonts, this state will always be in the complete state.
  int state;

  // Specified if this font object can be shared or not.
  // All font objects that just have the native displayer serving rfs
  // are sharable across different accessors. Webfonts are not.
  int shared;

  // The rc this font was created for
  jint m_rcMajorType;
  jint m_rcMinorType;
  void computeSizes(struct nfrc *rc, struct fh_store *ele);
  
public:
  FontObject(struct nff *self, struct nfrc *rc,
	  const char *urlOfFont = NULL);
  ~FontObject();

  int GC();

  jdouble *EnumerateSizes(struct nfrc *rc);
  struct nfrf *GetRenderableFont(struct nfrc *rc, jdouble pointsize);

  struct nffmi *GetMatchInfo(struct nfrc *rc, jdouble pointsize);

  // Rc type that this font was created with
  jint GetRcMajorType();
  jint GetRcMinorType();

  //
  // FontBroker specific
  //
  int addFontHandle(FontDisplayerPeerObject *fp_peer, void *font_handle);

  // Returns 1 is rf exists in the Font. 0 otherwise.
  int isRfExist(struct nfrf *rf);

  // Returns the number of rf's that were released
  int releaseRf(struct nfrf *rf);

  int isWebFont();
  //const char *name();
  const char *url();

  //
  // Webfont specific
  //
  // getState() returns the status of the webfont. Valid status's are
  // NF_FONT_INCOMPLETE
  //	: Not yet complete. The font is in the process of being made.
  //		XXX we might want to distinguish this from the
  //		XXX NOT-YET-ACTIVE state.
  // NF_FONT_COMPLETE
  //	: complete. The font is ready for use. For non-webfonts, this
  //		will be the state on creation.
  // NF_FONT_ERROR
  //	: Error. There was some error in creation of this font.
  //		This font will never be complete.
  int GetState();
  int setState(int completion_state);

  //
  // Query if this font is shared
  //
  int isShared(void);
  int setShared(int sharedState);

  friend void free_fh_store(wfList *object, void *item);
};
#endif /* _f_H_ */

