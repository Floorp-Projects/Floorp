/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef MKACCESS_H
#define MKACCESS_H

#ifndef MKGETURL_H
#include "mkgeturl.h"
#endif
/* returns TRUE if authorization is required
 */
extern Bool NET_AuthorizationRequired(char * address);

/* returns false if the user wishes to cancel authorization
 * and TRUE if the user wants to continue with a new authorization
 * string
 */
extern Bool NET_AskForAuthString(MWContext * context,
                     				URL_Struct *URL_s,
                     				char * authenticate,
                     				char * prot_template,
									Bool already_sent_auth,
                                    void * closure);

/* returns a authorization string if one is required, otherwise
 * returns NULL
 */
extern char * NET_BuildAuthString(MWContext * context, URL_Struct *URL_s);

/*
 * Builds the Proxy-authorization string
 */
extern char *
NET_BuildProxyAuthString(MWContext * context,
			 URL_Struct * url_s,
			 char * proxy_addr);

/*
 * Returns FALSE if the user wishes to cancel proxy authorization
 * and TRUE if the user wants to continue with a new authorization
 * string.
 */
PUBLIC PRBool
NET_AskForProxyAuth(MWContext * context,
		    char *   proxy_addr,
		    char *   pauth_params,
		    PRBool  already_sent_auth,
            void *   closure);

MODULE_PRIVATE int PR_CALLBACK
NET_CookieBehaviorPrefChanged(const char * newpref, void * data);

MODULE_PRIVATE int PR_CALLBACK
NET_CookieWarningPrefChanged(const char * newpref, void * data);

MODULE_PRIVATE int PR_CALLBACK
NET_CookieScriptPrefChanged(const char * newpref, void * data);

MODULE_PRIVATE void
net_http_password_data_interp(
        char *module,
        char *key,
        char *data, int32 data_len,
        char *type_buffer, int32 type_buffer_size,
        char *url_buffer, int32 url_buffer_size,
        char *username_buffer, int32 username_buffer_size,
        char *password_buffer, int32 password_buffer_size);

PUBLIC void NET_DeleteCookie(char* cookieURL);
PUBLIC void NET_DisplayCookieInfoAsHTML(MWContext * context);
PUBLIC void NET_CookieViewerReturn();


/*============================================================================================
    THIS IS THE BEGINNING OF TRUST.H WHICH IDEALLY SHOULD BE A SEPERATE FILE
  ============================================================================================*/
#ifndef TRUSTLABEL_H
/* #define TRUSTLABEL_H 1 */
#ifdef TRUST_LABELS

/* the structure that the iterators use to pass back the processed trust label
 * associated with this struct is a constructor and destructor and some methods */
typedef struct {
	short purpose;			/* the purpose range encoded in bits 0-5 */
	short ID;			/* the id value */
	short recipients;		/* the Recipients value */
	PRBool  isGeneric;		/* true if this is a generic label i.e. applies to all cookies */
	PRBool  isSigned;			/* true if this label was signed and the signature verified */ 
	PRTime ExpDate;			/* the expiration date in local time */
	char  *signatory;		/* who signed the label if it was signed */
	char  *domainName;		/* the applicable domain for the label */
	char  *path;			/* the path for the label */
	char  *szTrustAuthority;	/* the trust authority rating service  */
	XP_List  *nameList;		/* the list of names of the specific cookies that this label is for */
	char  *szBy;			/* the by field from the PICS label */
} TrustLabel;

TrustLabel *TL_Construct();
void TL_Destruct( TrustLabel *ALabel );
void TL_ProcessForAttrib( TrustLabel *ALabel, char *szFor);
void TL_SetSignatory( TrustLabel *ALabel, char *Signatory );
void TL_SetTrustAuthority( TrustLabel *ALabel, char *TrustAuthority );
void TL_SetByField( TrustLabel *ALabel, char *ByField );

/* utility function that use TrustLabel */
void PICS_ExtractTrustLabel( URL_Struct *URL_s, char *value );
PRBool IsTrustLabelsEnabled(void);
PUBLIC PRBool MatchCookieToLabel2( char *TargetURL,  char *CookieName,
	char *CookiePath, char *CookieHost, 
	TrustLabel **TheLabel );

#endif

#endif




#endif /* MKACCESS_H */
