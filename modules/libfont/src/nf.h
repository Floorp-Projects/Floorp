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
 *
 * This contains definitions the are outside the scope of the interfaces
 * defined that need to be exported out of the broker (libfont).
 *
 * dp Suresh <dp@netscape.com>
 */


#ifndef _nf_H_
#define _nf_H_

#include "jritypes.h"
#include "ntypes.h"
#ifdef MOZILLA_CLIENT
#include "client.h"
#endif /* MOZILLA_CLIENT */

#ifdef __cplusplus
#define NF_PUBLIC_API_DEFINE(returnType) extern "C" returnType
#define NF_PUBLIC_API_IMPLEMENT(returnType) extern "C" returnType
#else
#define NF_PUBLIC_API_DEFINE(returnType) extern returnType
#define NF_PUBLIC_API_IMPLEMENT(returnType) returnType
#endif

/*
 * Initialize routine for the FontBroker.
 */
NF_PUBLIC_API_DEFINE(struct nffbc *) NF_FontBrokerInitialize(void);

#ifdef MOZILLA_CLIENT
/*
 * Display about fonts in html
 */
NF_PUBLIC_API_DEFINE(char *) NF_AboutFonts(MWContext *context, const char *which);
#endif /* MOZILLA_CLIENT */

/*
 * Registering of font converters for font streaming
 */
NF_PUBLIC_API_DEFINE(void) NF_RegisterConverters(void);

#ifndef NO_PERFORMANCE_HACK
/*
 * For improving measure and draw performance.
 */
struct nfrc;
struct rc_data;
NF_PUBLIC_API_DEFINE(struct rc_data *) NF_GetRCNativeData(struct nfrc* rc);
#endif /* NO_PERFORMANCE_HACK */

/*
 * The native font displayer name. This is specific to netscape navigator.
 */
#define NF_NATIVE_FONT_DISPLAYER "Netscape Default Font Displayer"

/*
 * Global variables that we create. These are the font broker
 * interface variables that is available after font broker init.
 */
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
	extern struct nffbc *WF_fbc;
	extern struct nffbu *WF_fbu;
	extern struct nffbp *WF_fbp;
#ifdef __cplusplus
}
#endif /* __cplusplus */


/* The bounding box of a string. We are using a structure instead of a JMC object
 * because we expect this to be called a lot and returning a JMC object is going to be
 * performance expensive.
 */
struct nf_bounding_box {
	int width;
	int ascent;
	int descent;
	int lbearing;
	int rbearing;
};

/*
 * For faster drawing, we are exposing the rendering context data to the
 * Displayer. Here are the definitions the Displayer will need to be able
 * to do this.
 */

/* Rendering context majorTypes. */
#define NF_RC_INVALID 0
#define NF_RC_DIRECT 1
#define NF_RC_BUFFER 2

/* Rendering context minorTypes. 
 * Valid minor types are
 * any negative number : Rc's are always different.
 * 0 (or) any positive number : Enable rc checking
 */
#define NF_RC_ALWAYS_DIFFERENT -1


struct rc_direct {
#if defined(XP_UNIX)
  /* Display * */ void *display;
  /* Drawable */ void *d;
  /* GC */ void *gc;
  jint	mask;
#elif defined(XP_WIN)
  /* DC */ void *dc;
#elif defined(XP_OS2)
  /* DC */ void *dc;
#elif defined(XP_MAC)
  /* CGragPtr */ void *port;
#endif
};

#if defined(XP_UNIX)
   /* mask bit assignments */
#define NF_DrawImageTextMask	(1<<0)
#endif

struct rc_buffer {
  /* IMPLEMENT_ME */
  int implement_me;
};

struct rc_data {
  jint majorType;
  jint minorType;
  /* Displayers store state information here */
  void *displayer_data;
  union {
	struct rc_direct directRc;
	struct rc_buffer bufferRc;
  } t;
};

/*
 * State values for NF_FONT
 */
#define NF_FONT_COMPLETE	1
#define NF_FONT_INCOMPLETE	0
#define NF_FONT_ERROR		-1

/* Font stream notification callback. */
struct nff;	/* Forward declaration */
#ifndef XP_OS2
typedef void (*nfFontObserverCallback)(struct nff *, void *client_data);
#else
typedef void (* _Optlink nfFontObserverCallback)(struct nff *, void *client_data);
#endif

/* Font stream: Maximum that can be returned from nfstrm::WriteReady() */
#define NF_MAX_WRITE_READY 0X0FFFFFFF

/* Values for nfFmiWeight */
#define nfWeightDontCare  0
/* Meaningful values of weight: 100, 200, 300, 400, 500, 600, 700, 800, 900 */

/* Values for nfFmiPitch */
#define nfSpacingDontCare      0
#define nfSpacingProportional  1
#define nfSpacingMonospaced    2

/* Values for nfFmiStyle */
#define nfStyleDontCare   0
#define nfStyleNormal     1
#define nfStyleItalic     2
#define nfStyleOblique    3

/* Values for nfFmiUnderline */
#define nfUnderlineDontCare   0
#define nfUnderlineYes        1
#define nfUnderlineNo         2

/* Values for nfFmiStrikeOut */
#define nfStrikeOutDontCare   0
#define nfStrikeOutYes        1
#define nfStrikeOutNo         2

/* Values for nfFmiResolutionX and nfFmiResolutionY */
#define nfResolutionDontCare 0

/*
 * List of predefined attributes for the FontMatchInfo
 */
#define nfFmiName				"nfFmiName"
#define nfFmiCharset			"nfFmiCharset"
#define nfFmiEncoding			"nfFmiEncoding"
#define nfFmiWeight				"nfFmiWeight"
#define nfFmiPitch				"nfFmiPitch"
#define nfFmiStyle				"nfFmiStyle"
#define nfFmiUnderline			"nfFmiUnderline"
#define nfFmiStrikeOut			"nfFmiStrikeOut"
/***
#define nfFmiPanose				"nfFmiPanose"
***/
#define nfFmiResolutionX		"nfFmiResolutionX"
#define nfFmiResolutionY		"nfFmiResolutionY"

#endif /* _nf_H_ */
