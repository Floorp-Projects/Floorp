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
 * General include that contain all general junk that is in the system.
 *
 * dp Suresh <dp@netscape.com>
 */


#ifndef _libfont_H_
#define _libfont_H_

#include "stdio.h"			/* Use C stdio to print errors */
#include "string.h"

/*
 * XXX Technically a lot of these includes should be ifdef MOZILLA_CLIENT
 * XXX as the make sense only when libfont/ is used with communicator.
 * XXX
 * XXX Eventually when we want java applications to use libfont/ then we will take
 * XXX we might want to enforce this rule strict.
 */

#include "xp_mcom.h"
#include "jritypes.h"		/* For jri types jint, jsize */
#include "net.h"			/* URL_Struct */
#include "merrors.h"
#include "proto.h"
#include "shist.h"		/* For SHIST_GetCurrent() */

#include "prlink.h"			/* NSPR PR_LoadLibrary */

/* Some of our interfaces use MWContext * and this is defined in
 * the mozilla client ns/include/structs.h
 *
 * By doing this we will have problems working with other consumers
 * like java application.
 */
#include "structs.h"	/* For MWContext */ 

#ifndef NO_PREF_CHANGE
#ifdef MOZILLA_CLIENT
#include "prefapi.h"	/* For mozilla prefs */
#endif
#endif /* NO_PREF_CHANGE */

#ifndef NO_HTML_DIALOGS_CHANGE
#include "htmldlgs.h"	/* For NF_AboutData()::XP_ConvertStringToArgVec() */
#include "xpgetstr.h"
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* NO_HTML_DIALOGS_CHANGE */

#include "libi18n.h"	/* For unicode conversion routines */

/*
 * Cross module memory allocator and deallocator.
 * Until jmc works this out, we will use these.
 */
#define WF_ALLOC(n) XP_ALLOC(n)
#define WF_FREE(p) XP_FREE(p)
#define WF_REALLOC(p, n) XP_REALLOC(p, n)

/*
 * Debug tracing
 */
extern int wf_trace_flag;
#define WF_TRACEMSG(msg) XP_LTRACE(wf_trace_flag, 1, msg)

/*
 * Forward declaration of all interface structures.
 */
struct nfdoer;
struct nff;
struct nffbc;
struct nffbp;
struct nffbu;
struct nffmi;
struct nffp;
struct nfrc;
struct nfrf;
struct nfstrm;
struct nfdlm;

/*
 * new_stream_data
 * This structure is passed in as client_data to NewStream
 */
struct wf_new_stream_data {
	struct nfdoer *observer;
	struct nff *f;
	struct nfrc *rc;
	char *url_of_page;
};

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
	void wfUrlExit(URL_Struct *urls, int status, MWContext *cx);
	NET_StreamClass* wfNewStream(FO_Present_Types format_out,
		void* client_data,	URL_Struct* urls, MWContext* cx);
#ifdef __cplusplus
}
#endif /* __cplusplus */



#ifdef __cplusplus
#include "wfMisc.h"
#endif /* __cplusplus */

#define WF_CATALOG_FILE_VERSION "1.0"

#define	WF_PATH_SEP_STR	";"

#endif /* _libfont_H_ */
