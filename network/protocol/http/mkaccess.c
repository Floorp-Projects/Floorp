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
/*
 *
 * Designed and Implemented by Lou Montulli '94
 * Heavily modified by Judson Valeski '97
 * Yada yada yada... Gagan Saksena '98
 *
 * This file implements HTTP access authorization
 * and HTTP cookies
 */

#if defined(CookieManagement)
/* #define TRUST_LABELS */
#endif

#if defined(SingleSignon)
#define EDITOR 1
#endif

#define alphabetize 1
#include "rosetta.h"
#include "xp.h"
#include "mkprefs.h"
#include "netutils.h"
#include "mkselect.h"
#include "mktcp.h"
#include "mkgeturl.h"
#include "mkparse.h"
#include "mkaccess.h"
#include "net_xp_file.h"
#include "cookies.h"
#include "httpauth.h"
#include "prefapi.h"
#include "shist.h"
#include "jscookie.h"
#include "prmon.h"
#include "edt.h" /* for EDT_SavePublishUsernameAndPassword */

#ifdef XP_MAC
#include "prpriv.h"             /* for NewNamedMonitor */
#else
#include "private/prpriv.h"
#endif

#include "secnav.h"
#include "sechash.h"
#include "libevent.h"
#include "pwcacapi.h"

/* for XP_GetString() */
#include "xpgetstr.h"

extern int XP_CONFIRM_AUTHORIZATION_FAIL;
extern int XP_ACCESS_ENTER_USERNAME;
extern int XP_CONFIRM_PROXYAUTHOR_FAIL;
extern int XP_CONNECT_PLEASE_ENTER_PASSWORD_FOR_HOST;
extern int XP_PROXY_REQUIRES_UNSUPPORTED_AUTH_SCHEME;
extern int XP_LOOPING_OLD_NONCES;
extern int XP_UNIDENTIFIED_PROXY_SERVER;
extern int XP_PROXY_AUTH_REQUIRED_FOR;
extern int XP_CONNECT_PLEASE_ENTER_PASSWORD_FOR_PROXY;
extern int XP_FORTEZZA_PROXY_AUTH;
extern int MK_ACCESS_COOKIES_THE_SERVER;
extern int MK_ACCESS_COOKIES_WISHES; 
#if defined(CookieManagement)
extern int MK_ACCESS_COOKIES_WISHES0;
extern int MK_ACCESS_COOKIES_WISHES1;
extern int MK_ACCESS_COOKIES_WISHESN;
extern int MK_ACCESS_COOKIES_WISHES_MODIFY;
extern int MK_ACCESS_COOKIES_REMEMBER;
extern int MK_ACCESS_COOKIES_ACCEPTED;
extern int MK_ACCESS_SITE_COOKIES_ACCEPTED;
extern int MK_ACCESS_COOKIES_PERMISSION;
#else
extern int MK_ACCESS_COOKIES_TOANYSERV; 
extern int MK_ACCESS_COOKIES_TOSELF;
extern int MK_ACCESS_COOKIES_NAME_AND_VAL;
extern int MK_ACCESS_COOKIES_COOKIE_WILL_PERSIST;
#endif
extern int MK_ACCESS_COOKIES_SET_IT;
extern int MK_ACCESS_YOUR_COOKIES;
extern int MK_ACCESS_NAME;
extern int MK_ACCESS_VALUE;
extern int MK_ACCESS_HOST;
extern int MK_ACCESS_DOMAIN;
extern int MK_ACCESS_PATH;
extern int MK_ACCESS_YES;
extern int MK_ACCESS_NO;
extern int MK_ACCESS_SECURE;
extern int MK_ACCESS_EXPIRES;
extern int MK_ACCESS_END_OF_SESSION;
extern int MK_ACCESS_VIEW_COOKIES;
extern int MK_ACCESS_VIEW_SITES;

#ifdef TRUST_LABELS
extern int MK_ACCESS_TL_PUR1; 
extern int MK_ACCESS_TL_PUR2; 
extern int MK_ACCESS_TL_PUR3; 
extern int MK_ACCESS_TL_PUR4; 
extern int MK_ACCESS_TL_PUR5; 
extern int MK_ACCESS_TL_PUR6; 
extern int MK_ACCESS_TL_PPH0; 
extern int MK_ACCESS_TL_PPH1; 
extern int MK_ACCESS_TL_PPH2; 
extern int MK_ACCESS_TL_PPH3; 
extern int MK_ACCESS_TL_PPH4; 
extern int MK_ACCESS_TL_PPH5; 
extern int MK_ACCESS_TL_ID1;  
extern int MK_ACCESS_TL_ID0;  
extern int MK_ACCESS_TL_BY;   
extern int MK_ACCESS_TL_RECP1;
extern int MK_ACCESS_TL_RECP2;
extern int MK_ACCESS_TL_RECP3;
extern int MK_ACCESS_TL_RPH0; 
extern int MK_ACCESS_TL_RPH1; 
extern int MK_ACCESS_TL_RPH2; 
extern int MK_ACCESS_TL_RPH3; 

#endif


#define MAX_NUMBER_OF_COOKIES  300
#define MAX_COOKIES_PER_SERVER 20
#define MAX_BYTES_PER_COOKIE   4096  /* must be at least 1 */

/*
 * Authentication information for servers and proxies is kept
 * on separate lists, but both lists consist of net_AuthStruct's.
 */

HG73943

PRIVATE XP_List * net_auth_list = NULL;
PRIVATE XP_List * net_proxy_auth_list = NULL;

PRIVATE Bool cookies_changed = FALSE;
#if defined(CookieManagement)
PRIVATE Bool cookie_permissions_changed = FALSE;
PRIVATE Bool cookie_remember_checked = FALSE;
#endif

PRIVATE NET_CookieBehaviorEnum net_CookieBehavior = NET_Accept;
PRIVATE Bool net_WarnAboutCookies = FALSE;
PRIVATE char *net_scriptName = (char *)0;

/*
 * Different schemes supported by the client.
 * Order means the order of preference between authentication schemes.
 *
 */
typedef enum _net_AuthType {
    AUTH_INVALID   = 0,
    AUTH_BASIC     = 1
#ifdef SIMPLE_MD5
	HG72621
#endif /* SIMPLE_MD5 */
    ,AUTH_FORTEZZA  = 3
} net_AuthType;


/*
 * This struct describes both Basic  authentication stuff,
 * for both HTTP servers and proxies.
 *
 */
HG10828


typedef struct _net_AuthStruct {
    net_AuthType	auth_type;
    char *			path;			/* For normal authentication only */
	char *			proxy_addr;		/* For proxy authentication only */
	char *			username;		/* Obvious */
	char *			password;		/* Not too cryptic either */
	char *			auth_string;	/* Storage for latest Authorization str */
	char *			realm;			/* For all auth schemes */
#ifdef SIMPLE_MD5
    HG82727
#endif
    char *                      challenge;
    HG26250
    char *                      signature;
    char *                      clientRan;
    PRBool                     oldChallenge;
    int                         oldChallenge_retries;
} net_AuthStruct;



/*----------------- Normal client-server authentication ------------------ */

PRIVATE net_AuthStruct *
net_CheckForAuthorization(char * address, Bool exact_match)
{

    char *atSign, *fSlash, *afterProto, *newAddress=NULL;
    char tmp;
    XP_List * list_ptr = net_auth_list;
    net_AuthStruct * auth_s;

	TRACEMSG(("net_CheckForAuthorization: checking for auth on: %s", address));

    /* Auth struct->path doesn't contain username/password info (such as
    * http://uname:pwd@host.com, so make sure we don't compare an address
    * passed in with one with an auth struct->path until we remove/reduce the 
    * address passed in. */
    if( (afterProto=PL_strstr(address, "://")) != 0 ) {
    afterProto=afterProto+3;
    tmp=*afterProto;
    *afterProto='\0';
    StrAllocCopy(newAddress, address);
    *afterProto=tmp;

    /* temporarily truncate after first slash, if any. */
    if( (fSlash=PL_strchr(afterProto, '/')) != 0)
      *fSlash='\0';
    atSign=PL_strchr(afterProto, '@');
    if(fSlash)
      *fSlash='/';
    if(atSign)
      StrAllocCat(newAddress, atSign+1);
    else
      StrAllocCat(newAddress, afterProto);
    }

    while((auth_s = (net_AuthStruct *) XP_ListNextObject(list_ptr))!=0)
      {
        XP_ASSERT(newAddress);
        if(!newAddress)
            return NULL;

		if(exact_match)
		  {
            if(!PL_strcmp(address, auth_s->path)) {
                XP_FREE(newAddress);       
                return(auth_s);
            }
		  }
		else
		  {
			/* shorter strings always come last so there can be no
			 * ambiquity
			 */
            if(!PL_strncasecmp(address, auth_s->path, PL_strlen(auth_s->path))) {
                XP_FREE(newAddress);       
                return(auth_s);
            }
		  }
      }
    XP_FREE(newAddress);
	HG25262
   
    return(NULL);
}

/* returns TRUE if authorization is required
 */
PUBLIC Bool
NET_AuthorizationRequired(char * address)
{
    net_AuthStruct * rv;
	char * last_slash = PL_strrchr(address, '/');

	if(last_slash)
		*last_slash = '\0';

	rv = net_CheckForAuthorization(address, FALSE);

	HG17993
	if(last_slash)
		*last_slash = '/';

	if(!rv)
		return(FALSE);
	else
		return(TRUE);
}

/* returns a authorization string if one is required, otherwise
 * returns NULL
 */
PUBLIC char *
NET_BuildAuthString(MWContext * context, URL_Struct *URL_s)
{
	char * address = URL_s->address;
	net_AuthStruct * auth_s = net_CheckForAuthorization(address, FALSE);

	if(!auth_s)
#if defined(XP_WIN) && defined(MOZILLA_CLIENT)
		return(WFE_BuildCompuserveAuthString(URL_s));
#else
		return(NULL);
#endif
	else
	  {
	  	static char * auth_header = 0;
		
		if(auth_header)
			PR_Free(auth_header);
		auth_header = PR_smprintf("Authorization: Basic %s"CRLF, auth_s->auth_string);
		return(auth_header);
	  }
}

PRIVATE net_AuthStruct *
net_ScanForHostnameRealmMatch(char * address, char * realm)
{
	char * proto_host = NET_ParseURL(address, GET_HOST_PART | GET_PROTOCOL_PART);
    XP_List * list_ptr = net_auth_list;
    net_AuthStruct * auth_s;

    while((auth_s = (net_AuthStruct *) XP_ListNextObject(list_ptr))!=0)
      {
        /* shorter strings always come last so there can be no
         * ambiquity
         */
        if(!PL_strncasecmp(proto_host, auth_s->path, PL_strlen(proto_host))
			&& !PL_strcasecmp(realm, auth_s->realm))
		{
			PR_Free(proto_host);
            return(auth_s);
		}
      }
	PR_Free(proto_host);
    return(NULL);
}

PRIVATE void
net_free_auth_struct(net_AuthStruct *auth)
{
    PR_Free(auth->path);
    PR_Free(auth->proxy_addr);
    PR_Free(auth->username);
    PR_Free(auth->password);
    PR_Free(auth->auth_string);
    PR_Free(auth->realm);
    /*FORTEZZA related stuff   */
    PR_FREEIF(auth->challenge);
    HG26763
    PR_FREEIF(auth->signature);
    PR_FREEIF(auth->clientRan);
    /*End FORTEZZA related stuff*/
    PR_Free(auth);
}

/* blows away all authorization structs currently in the list, and the list itself.
 * frees and nulls the list pointer itself (net_auth_list)
 */
PUBLIC void
NET_RemoveAllAuthorizations()
{
	net_AuthStruct * victim;
	if(XP_ListIsEmpty(net_auth_list)) /* XP_ListIsEmpty handles null list */
		return;

    while((victim = (net_AuthStruct *) XP_ListNextObject(net_auth_list)) != 0)
		net_free_auth_struct(victim);
	XP_ListDestroy(net_auth_list);
	net_auth_list = NULL;
}

PRIVATE void
net_remove_exact_auth_match_on_cancel(net_AuthStruct *prev_auth, char *cur_path)
{
	if(!prev_auth || !cur_path)
		return;

	if(!PL_strcmp(prev_auth->path, cur_path))
      {
        /* if the paths are exact and the user cancels
         * remove the mapping
         */
        XP_ListRemoveObject(net_auth_list, prev_auth);
		net_free_auth_struct(prev_auth);
	  }
}

#define HTTP_PW_MODULE_NAME  "http_pw"
#define HTTP_PW_NAME_TOKEN            "name"
#define HTTP_PW_PASSWORD_TOKEN        "pass"

PRIVATE char *
gen_http_key(char *address, char *realm)
{
	char *rv=NULL;

	StrAllocCopy(rv, address);
	StrAllocCat(rv, "\t");
	StrAllocCat(rv, realm);

	return rv;
}

PRIVATE void
separate_http_key(char *key, char **address, char **realm)
{
	char *tab;

	HG72294
		
	*address = NULL;
	*realm = NULL;

	if(!key)
		return;

	tab = PL_strchr(key, '\t');

	if(!tab)
		return;

	*address = key;
	*realm = tab+1;
}

PRIVATE void
net_store_http_password(char *address, char *realm, char *username, char *password)
{
	char *key;
	PCNameValueArray *array = PC_NewNameValueArray();

	if(!array)
		return;

	PC_AddToNameValueArray(array, HTTP_PW_NAME_TOKEN, username);	
	PC_AddToNameValueArray(array, HTTP_PW_PASSWORD_TOKEN, password);	

	key = gen_http_key(address, realm);

	if(!key)
		return;

	PC_StorePasswordNameValueArray(HTTP_PW_MODULE_NAME, key, array);

	PR_Free(key);

}

PRIVATE void
net_remove_stored_http_password(char *url)
{
	PC_DeleteStoredPassword(HTTP_PW_MODULE_NAME, url);
}

MODULE_PRIVATE void
net_http_password_data_interp(
        char *module,
        char *key,
        char *data, int32 data_len,
        char *type_buffer, int32 type_buffer_size,
        char *url_buffer, int32 url_buffer_size,
        char *username_buffer, int32 username_buffer_size,
        char *password_buffer, int32 password_buffer_size)
{
	PCNameValueArray * array;
	char *username, *password;
	char *address, *realm;

	array = PC_CharToNameValueArray(data, data_len);

	if(!array)
		return;

	username = PC_FindInNameValueArray(array, HTTP_PW_NAME_TOKEN);
	password = PC_FindInNameValueArray(array, HTTP_PW_PASSWORD_TOKEN);
	
	PL_strncpyz(type_buffer, "HTTP basic authorization", type_buffer_size);

	separate_http_key(key, &address, &realm);

	if(address)
	{
		PL_strncpyz(url_buffer, address, url_buffer_size);
	}

	if(username)
	{
		PL_strncpyz(username_buffer, username, username_buffer_size);
		PR_Free(username);
	}
	
	if(password)
	{
		PL_strncpyz(password_buffer, password, password_buffer_size);
		PR_Free(password);
	}
}

PRIVATE void
net_initialize_http_access(void)
{
	/* register PW cache interp function */
	
	PC_RegisterDataInterpretFunc(HTTP_PW_MODULE_NAME, 	
				     net_http_password_data_interp);
	HG21522
}

/* forward declaration */
PUBLIC XP_Bool 
stub_PromptUsernameAndPassword(MWContext *context,
                               const char *msg,
                               char **username,
                               char **password,
                               void *closure);

/* returns false if the user wishes to cancel authorization
 * and TRUE if the user wants to continue with a new authorization
 * string
 */
/* HARDTS: I took a whack at fixing up some of the strings leaked in this 
 * function.  All the PR_FREEIF()s are new. 
 */
PUBLIC Bool 
NET_AskForAuthString(MWContext *context,
					 URL_Struct * URL_s, 
					 char * authenticate, 
					 char * prot_template,
					 Bool   already_sent_auth,
                     void *closure)
{
	static PRBool first_time=TRUE;
	net_AuthStruct *prev_auth;
	char *address=URL_s->address;
	char *host=NET_ParseURL(address, GET_HOST_PART);
	char *new_address=NULL;
	char *username=NULL,*colon=NULL,*password=NULL,*unamePwd=NULL;
	char *u_pass_string=NULL;
	char *auth_string=NULL;
	char *realm;
	char *slash;
	char *authenticate_header_value;
	char *buf=NULL;
	int32 len=0;
	int status;
	PRBool re_authorize=FALSE;

	TRACEMSG(("Entering NET_AskForAuthString"));

	if(first_time)
	{
		net_initialize_http_access();
		first_time = FALSE;
	}

	if(authenticate)
	  {
		/* check for the compuserve Remote-Passphrase type of
	 	 * authentication
	 	 */
		authenticate_header_value = XP_StripLine(authenticate);
		HG03937

#define COMPUSERVE_HEADER_NAME "Remote-Passphrase"

		if(!PL_strncasecmp(authenticate_header_value, 
					 COMPUSERVE_HEADER_NAME, 
					 sizeof(COMPUSERVE_HEADER_NAME) - 1))
	  	{
	  		/* This is a compuserv style header 
	  	 	*/

		PR_FREEIF(host);
#if defined(XP_WIN) && defined(MOZILLA_CLIENT)
		return(WFE_DoCompuserveAuthenticate(context, URL_s, authenticate_header_value));
#else
		return(NET_AUTH_FAILED_DISPLAY_DOCUMENT);		
#endif	
	  	}			 
#define HTTP_BASIC_AUTH_TOKEN "BASIC"
		else if(PL_strncasecmp(authenticate_header_value, 
					 HTTP_BASIC_AUTH_TOKEN, 
					 sizeof(HTTP_BASIC_AUTH_TOKEN) - 1))
		{
			HG21632
			/* unsupported auth type */
			return(NET_AUTH_FAILED_DISPLAY_DOCUMENT);		
		}
	  }

	new_address = NET_ParseURL(address,	GET_PROTOCOL_PART | GET_HOST_PART | GET_PATH_PART);
	if(!new_address) {
		PR_FREEIF(host);
		return NET_AUTH_FAILED_DISPLAY_DOCUMENT;
	}

    /* get the username and password from the URL struct */
    if( URL_s->password && *URL_s->password ) {
        password = PL_strdup(URL_s->password);
    }

    if( URL_s->username && *URL_s->username ) {
        username = PL_strdup(URL_s->username);
    } else {
	    /* See if username and password are still in the address */
        unamePwd=NET_ParseURL(address, GET_USERNAME_PART | GET_PASSWORD_PART);
	    /* get the username & password out of the combo string */
	    if( (colon = PL_strchr(unamePwd, ':')) != NULL ) {
		    *colon='\0';
		    username=PL_strdup(unamePwd);
            /* Use password we found above? */
            if( password == NULL ) {
    		    password=PL_strdup(colon+1);
            }

		    *colon=':';
		    PR_Free(unamePwd);
	    } else {
		    username=unamePwd;
	    }
    }


	/*if last char is not a slash then */
	if (new_address[PL_strlen(new_address)-1] != '/')
	{
		/* remove everything after the last slash */
		slash = PL_strrchr(new_address, '/');
		if(++slash)
			*slash = '\0';
	}

	if(!authenticate)
	  {
		realm = "unknown";
	  }
	else
	  {
		realm = PL_strchr(authenticate, '"');
	
		if(realm)
		  {
			realm++;

			/* terminate at next quote */
			strtok(realm, "\"");

#define MAX_REALM_SIZE 128
			if(PL_strlen(realm) > MAX_REALM_SIZE)
				realm[MAX_REALM_SIZE] = '\0';
	
      }
		else
	      {
		    realm = "unknown";
	 	  }
		
    }

	/* no hostname/realm match search for exact match */
    prev_auth = net_CheckForAuthorization(new_address, FALSE);

    if(prev_auth && !already_sent_auth)
      {
		/* somehow the mapping changed since the time we sent
		 * the authorization.
		 * This happens sometimes because of the parrallel
		 * nature of the requests.
		 * In this case we want to just retry the connection
		 * since it will probably succede now.
		 */
		HG22220
		PR_FREEIF(host);
		PR_FREEIF(new_address);
		PR_FREEIF(username);
		PR_FREEIF(password);
		return(NET_RETRY_WITH_AUTH);
      }
    else if(prev_auth)
      {
		/* we sent the authorization string and it was wrong
		 */
        if(!FE_Confirm(context, XP_GetString(XP_CONFIRM_AUTHORIZATION_FAIL)))
		  {
        	TRACEMSG(("User canceled login!!!"));

			if(!PL_strcmp(prev_auth->path, new_address))
			  {
				/* if the paths are exact and the user cancels
				 * remove the mapping
				 */
				net_remove_exact_auth_match_on_cancel(prev_auth, new_address);
				PR_FREEIF(host);
				PR_FREEIF(new_address);
				PR_FREEIF(username);
				PR_FREEIF(password);
            	return(NET_AUTH_FAILED_DISPLAY_DOCUMENT);
			  }
		  }
		
		if (!username)
			username = PL_strdup(prev_auth->username);
		if (!password)
			password = PL_strdup(prev_auth->password);
		re_authorize = TRUE;
      }
	else
	  {
		char *ptr1, *ptr2;

        /* scan all the authorization strings to see if we
         * can find a hostname and realm match.  If we find
         * a match then reduce the authorization path to include
         * the current path as well.
         */
        prev_auth = net_ScanForHostnameRealmMatch(address, realm);
    
        if(prev_auth)
          {
		char *tmp;
		int skipProtoHost;

		net_remove_stored_http_password(prev_auth->path);

		tmp = NET_ParseURL(prev_auth->path, GET_HOST_PART | GET_PROTOCOL_PART);
		skipProtoHost = PL_strlen(tmp);
		PR_ASSERT(!PL_strncasecmp(
			tmp, 
			NET_ParseURL(new_address, GET_HOST_PART | GET_PROTOCOL_PART), 
			skipProtoHost));
		PR_Free(tmp);

            /* compare the two url paths until they deviate
             * once they deviate truncate.
             * skip ptr1, ptr2 past the host names 
             * which we have already compared
             */
            for(ptr1 = prev_auth->path + skipProtoHost, ptr2 = new_address + skipProtoHost; *ptr1 && *ptr2; ptr1++, ptr2++)
			  {
                if(*ptr1 != *ptr2)
                  {
					break;        /* end for */
                  }
			  }
			/* truncate at *ptr1 now since the new address may
			 * be just a subpath of the original address and
			 * the compare above will not handle the subpath correctly
			 */
			*ptr1 = '\0'; /* truncate */

			if(*(ptr1-1) == '/')
				*(ptr1-1) = '\0'; /* strip a trailing slash */

			/* make sure a path always has at least a slash
			 * at the end.
			 * If the slash isn't there then
			 * the password will be sent to ports on the
			 * same host since we use a first part match
			 */
			tmp = NET_ParseURL(prev_auth->path, GET_PATH_PART);
			if(!*tmp)
				StrAllocCat(prev_auth->path, "/");
			PR_Free(tmp);				

			TRACEMSG(("Truncated new auth path to be: %s", prev_auth->path));

			net_store_http_password(prev_auth->path, prev_auth->realm, prev_auth->username, prev_auth->password);

			PR_Free(host);
			PR_Free(new_address);
			return(NET_RETRY_WITH_AUTH);
          }
	  }
					 
	/* Use username and/or password specified in URL_struct if exists. */
	if (!username && URL_s->username && *URL_s->username) {
		username = PL_strdup(URL_s->username);
	}
	if (!password && URL_s->password && *URL_s->password) {
		password = PL_strdup(URL_s->password);
	}

	if(!username && !password)
	{
		/* look for a previously stored password in the pw cache */
		PCNameValueArray *array;

		array = PC_CheckForStoredPasswordArray(HTTP_PW_MODULE_NAME, URL_s->address);

		if(array)
		{
			username = PC_FindInNameValueArray(array, HTTP_PW_NAME_TOKEN);
			password = PC_FindInNameValueArray(array, HTTP_PW_PASSWORD_TOKEN);

			if(!username)
			{
				PR_FREEIF(password);
				password = NULL;
			}
		}
	}

	/* if the password is filled in then the username must
	 * be filled in already.  
	 */
    /* 4/6/99 -- with the new async dialogs, we will only enter this
       statement the first time through; afterward, we will have the
       user and pass via NET_ResumeWithAuth()
    */
	if(!password || re_authorize)
	  {
		XP_Bool remember_password = FALSE;
        char *loginString=XP_GetString(XP_ACCESS_ENTER_USERNAME);
	   	host = NET_ParseURL(address, GET_HOST_PART);

		/* malloc memory here to prevent buffer overflow */
		len = XP_STRLEN(loginString) + XP_STRLEN(realm) + XP_STRLEN(host) + 10;
		buf = (char *)PR_Malloc(len*sizeof(char));
		
		if(buf) {
            PR_snprintf( buf, len*sizeof(char), loginString, realm, host);


			NET_Progress(context, XP_GetString( XP_CONNECT_PLEASE_ENTER_PASSWORD_FOR_HOST) );
#if 1
#if defined(SingleSignon)
			/* prefill prompt with previous username/passwords if any 
             * this returns 1 if user pressed OK, or 0 if they Canceled
            */
			status = SI_PromptUsernameAndPassword
			    (context, buf, &username, &password, URL_s->address);

            /* Tell the Editor the correct username */
            if( status > 0 ) {
                EDT_SavePublishUsername(context, address, username);
            }
#else
			status = PC_PromptUsernameAndPassword
			    (context, buf, &username, &password,
			    &remember_password, NET_IsURLSecure(URL_s->address));
#endif
#else
            {
              NET_AuthClosure * authclosure = (NET_AuthClosure *) closure;

              /* the new multi-threaded case -- we return asynchronously */
              authclosure->msg = PL_strdup (buf);
              status = stub_PromptUsernameAndPassword
			    (context, buf, &username, &password, closure);
            }
#endif
	
			PR_Free(buf);
        } else {
	        status = 0;
		}

		PR_Free(host);

		if(!status) {
            /* XXX now we have FALSE == WAIT_FOR_AUTH */
            return(NET_WAIT_FOR_AUTH);

#ifdef notused /* with async dialogs, we never have a user/pass here */
			TRACEMSG(("User canceled login!!!"));

			/* if the paths are exact and the user cancels
			 * remove the mapping
			 */
			net_remove_exact_auth_match_on_cancel(prev_auth, new_address);

			PR_FREEIF(username);
			PR_FREEIF(password);
			PR_FREEIF(new_address);
			return(NET_AUTH_FAILED_DISPLAY_DOCUMENT);
#endif /* notused */
		  }
		else if(!username || !password)
		  {
			PR_FREEIF(username);
			PR_FREEIF(password);
			PR_FREEIF(new_address);
			return(NET_AUTH_FAILED_DISPLAY_DOCUMENT);
		  }
		else if(remember_password)
		{
			net_store_http_password(URL_s->address, realm, username, password);
		}
	  }

	StrAllocCopy(u_pass_string, username);
	StrAllocCat(u_pass_string, ":");
	StrAllocCat(u_pass_string, password);

	len = PL_strlen(u_pass_string);
	auth_string = (char*) PR_Malloc((((len+1)*4)/3)+10);

	if(!auth_string)
	  {
		PR_FREEIF(username);
		PR_FREEIF(password);
		PR_FREEIF(u_pass_string);
		PR_Free(new_address);
		return(NET_RETRY_WITH_AUTH);
	  }

	HG38932
	NET_UUEncode((unsigned char *)u_pass_string, (unsigned char*) auth_string, len);

	PR_Free(u_pass_string);

	if(prev_auth)
	  {
	    PR_FREEIF(prev_auth->auth_string);
        prev_auth->auth_string = auth_string;
		PR_FREEIF(prev_auth->username);
		prev_auth->username = username;
        PR_FREEIF(prev_auth->password);
        prev_auth->password = password;
	  }
	else
	  {
        char *atSign, *host, *fSlash;
		XP_List * list_ptr = net_auth_list;
		net_AuthStruct * tmp_auth_ptr;
		size_t new_len;

		/* construct a new auth_struct
		 */
		prev_auth = PR_NEWZAP(net_AuthStruct);
	    if(!prev_auth)
		  {
			PR_FREEIF(auth_string);
			PR_FREEIF(username);
			PR_FREEIF(password);
		    PR_Free(new_address);
		    return(NET_RETRY_WITH_AUTH);
		  }
		
        prev_auth->auth_string = auth_string;
		prev_auth->username = username;
        prev_auth->password = password;
		prev_auth->path = 0;
       /* Don't save username/password info in the auth struct path. */
        if( (atSign=PL_strchr(new_address, '@')) != 0) {
          if( (host=PL_strstr(new_address, "://")) != 0) {
            char tmp;
            host+=3;
            tmp=*host;
            *host='\0';
            StrAllocCopy(prev_auth->path, new_address);
            *host=tmp;
        
            fSlash=PL_strchr(host, '/');
            if(fSlash)
              *fSlash='\0';
            /* Do the atSign check again so we're sure to get one between the 
             * protocol part and the first slash. */
            atSign=PL_strchr(host, '@');
            if(fSlash)
              *fSlash='/';

            if(atSign) {
              StrAllocCat(prev_auth->path, atSign+1);
            } else {
              StrAllocCat(prev_auth->path, host);
            }
          }
        } else {
          StrAllocCopy(prev_auth->path, new_address);
        }

		prev_auth->realm = 0;
		StrAllocCopy(prev_auth->realm, realm);

		if(!net_auth_list)
		  {
			net_auth_list = XP_ListNew();
		    if(!net_auth_list)
			  {
          /* Maybe should free prev_auth here. */
		   		PR_Free(new_address);
				return(NET_RETRY_WITH_AUTH);
			  }
		  }		

		/* add it to the list so that it is before any strings of
		 * smaller length
		 */
		new_len = PL_strlen(prev_auth->path);
		while((tmp_auth_ptr = (net_AuthStruct *) XP_ListNextObject(list_ptr))!=0)
		  { 
			if(new_len > PL_strlen(tmp_auth_ptr->path))
			  {
				XP_ListInsertObject(net_auth_list, tmp_auth_ptr, prev_auth);
		   		PR_Free(new_address);
				return(NET_RETRY_WITH_AUTH);
			  }
		  }
		/* no shorter strings found in list */	
		XP_ListAddObjectToEnd(net_auth_list, prev_auth);
	  }

	PR_Free(new_address);
	return(NET_RETRY_WITH_AUTH);
}

/*--------------------------------------------------
 * The following routines support the 
 * Set-Cookie: / Cookie: headers
 */

PRIVATE XP_List * net_cookie_list=0;
#if defined(CookieManagement)
PRIVATE XP_List * net_cookie_permission_list=0;
PRIVATE XP_List * net_defer_cookie_list=0;
#endif

typedef struct _net_CookieStruct {
    char * path;
	char * host;
	char * name;
    char * cookie;
	time_t expires;
	time_t last_accessed;
	HG26237
	Bool   is_domain;   /* is it a domain instead of an absolute host? */
} net_CookieStruct;

#if defined(CookieManagement)
typedef struct _net_CookiePermissionStruct {
    char * host;
    Bool permission;
#ifdef TRUST_LABELS
    XP_List * TrustList; /* a list of trust label entries */
#endif
} net_CookiePermissionStruct;

typedef struct _net_DeferCookieStruct {
    MWContext * context;
    char * cur_url;
    char * set_cookie_header;
    time_t timeToExpire;
} net_DeferCookieStruct;

#ifdef TRUST_LABELS
void ParseTrustLabelInfo
	(char *Buf, net_CookiePermissionStruct * cookie_permission);
void SaveTrustLabelData
	( net_CookiePermissionStruct * cookie_permission, XP_File fp );
void DeleteCookieFromPermissions( char *CookieHost, char *CookieName );
void AddTrustLabelInfo( char *CookieName, TrustLabel *trustlabel );

typedef struct {
    char * CookieName;
    int purpose; /* the purpose rating has a series of bits set */
    int recipient; /* the recipient value */
    Bool bIdentifiable; /* true if the related cookie puts to identifiable information */
    char * by; /* the by field from the trust label */
    char * CookieSet; /* when the cookie was set into cookies.txt */
} TrustEntry;
#endif

#endif

/* Routines and data to protect the cookie list so it
**   can be accessed by mulitple threads
*/

#include "prthread.h"
#include "prmon.h"

static PRMonitor * cookie_lock_monitor = NULL;
static PRThread  * cookie_lock_owner = NULL;
static int cookie_lock_count = 0;

PRIVATE void
net_lock_cookie_list(void)
{
    if(!cookie_lock_monitor)
	cookie_lock_monitor = PR_NewNamedMonitor("cookie-lock");

    PR_EnterMonitor(cookie_lock_monitor);

    while(TRUE) {

	/* no current owner or owned by this thread */
	PRThread * t = PR_CurrentThread();
	if(cookie_lock_owner == NULL || cookie_lock_owner == t) {
	    cookie_lock_owner = t;
	    cookie_lock_count++;

	    PR_ExitMonitor(cookie_lock_monitor);
	    return;
	}

	/* owned by someone else -- wait till we can get it */
	PR_Wait(cookie_lock_monitor, PR_INTERVAL_NO_TIMEOUT);

    }
}

PRIVATE void
net_unlock_cookie_list(void)
{
   PR_EnterMonitor(cookie_lock_monitor);

#ifdef DEBUG
    /* make sure someone doesn't try to free a lock they don't own */
    PR_ASSERT(cookie_lock_owner == PR_CurrentThread());
#endif

    cookie_lock_count--;

    if(cookie_lock_count == 0) {
	cookie_lock_owner = NULL;
	PR_Notify(cookie_lock_monitor);
    }
    PR_ExitMonitor(cookie_lock_monitor);

}

#if defined(CookieManagement)
static PRMonitor * defer_cookie_lock_monitor = NULL;
static PRThread  * defer_cookie_lock_owner = NULL;
static int defer_cookie_lock_count = 0;

PRIVATE void
net_lock_defer_cookie_list(void)
{
    if(!defer_cookie_lock_monitor)
	defer_cookie_lock_monitor =
            PR_NewNamedMonitor("defer_cookie-lock");

    PR_EnterMonitor(defer_cookie_lock_monitor);

    while(TRUE) {

	/* no current owner or owned by this thread */
	PRThread * t = PR_CurrentThread();
	if(defer_cookie_lock_owner == NULL || defer_cookie_lock_owner == t) {
	    defer_cookie_lock_owner = t;
	    defer_cookie_lock_count++;

	    PR_ExitMonitor(defer_cookie_lock_monitor);
	    return;
	}

	/* owned by someone else -- wait till we can get it */
	PR_Wait(defer_cookie_lock_monitor, PR_INTERVAL_NO_TIMEOUT);

    }
}

PRIVATE void
net_unlock_defer_cookie_list(void)
{
   PR_EnterMonitor(defer_cookie_lock_monitor);

#ifdef DEBUG
    /* make sure someone doesn't try to free a lock they don't own */
    PR_ASSERT(defer_cookie_lock_owner == PR_CurrentThread());
#endif

    defer_cookie_lock_count--;

    if(defer_cookie_lock_count == 0) {
	defer_cookie_lock_owner = NULL;
	PR_Notify(defer_cookie_lock_monitor);
    }
    PR_ExitMonitor(defer_cookie_lock_monitor);

}

static PRMonitor * cookie_permission_lock_monitor = NULL;
static PRThread  * cookie_permission_lock_owner = NULL;
static int cookie_permission_lock_count = 0;

PRIVATE void
net_lock_cookie_permission_list(void)
{
    if(!cookie_permission_lock_monitor)
	cookie_permission_lock_monitor =
            PR_NewNamedMonitor("cookie_permission-lock");

    PR_EnterMonitor(cookie_permission_lock_monitor);

    while(TRUE) {

	/* no current owner or owned by this thread */
	PRThread * t = PR_CurrentThread();
	if(cookie_permission_lock_owner == NULL || cookie_permission_lock_owner == t) {
	    cookie_permission_lock_owner = t;
	    cookie_permission_lock_count++;

	    PR_ExitMonitor(cookie_permission_lock_monitor);
	    return;
	}

	/* owned by someone else -- wait till we can get it */
	PR_Wait(cookie_permission_lock_monitor, PR_INTERVAL_NO_TIMEOUT);

    }
}

PRIVATE void
net_unlock_cookie_permission_list(void)
{
   PR_EnterMonitor(cookie_permission_lock_monitor);

#ifdef DEBUG
    /* make sure someone doesn't try to free a lock they don't own */
    PR_ASSERT(cookie_permission_lock_owner == PR_CurrentThread());
#endif

    cookie_permission_lock_count--;

    if(cookie_permission_lock_count == 0) {
	cookie_permission_lock_owner = NULL;
	PR_Notify(cookie_permission_lock_monitor);
    }
    PR_ExitMonitor(cookie_permission_lock_monitor);

}

PRIVATE int
net_SaveCookiePermissions(char * filename);

PRIVATE void
net_FreeCookiePermission
    (net_CookiePermissionStruct * cookie_permission, Bool save)
{

    /*
     * This routine should only be called while holding the
     * cookie permission list lock
     */

    if(!cookie_permission) {
	return;
    }
    XP_ListRemoveObject(net_cookie_permission_list, cookie_permission);
    PR_FREEIF(cookie_permission->host);

#ifdef TRUST_LABELS
    /* free the entries on the trust list */
    if ( cookie_permission->TrustList ) {
        TrustEntry * TEntry;
        XP_List *Tptr = cookie_permission->TrustList;
        while( (TEntry = (TrustEntry *)XP_ListNextObject( Tptr )) ) {
            PR_FREEIF(TEntry->CookieName);
            PR_FREEIF(TEntry->by);
            PR_FREEIF(TEntry->CookieSet);
            /* before removing the current entry get the ptr to the next one */
            Tptr = Tptr->prev;
            XP_ListRemoveObject(cookie_permission->TrustList, TEntry);
        }
        XP_ListDestroy(cookie_permission->TrustList);
    }
#endif

    PR_Free(cookie_permission);
    if (save) {
	cookie_permissions_changed = TRUE;
	net_SaveCookiePermissions(NULL);
    }
}

/* blows away all cookie permissions currently in the list */
PRIVATE void
net_RemoveAllCookiePermissions()
{
    net_CookiePermissionStruct * victim;
    XP_List * cookiePermissionList;

    /* check for NULL or empty list */
    net_lock_cookie_permission_list();
    cookiePermissionList = net_cookie_permission_list;

    if(XP_ListIsEmpty(cookiePermissionList)) {
	net_unlock_cookie_permission_list();
	return;
    }
    while((victim =
	    (net_CookiePermissionStruct *)
	    XP_ListNextObject(cookiePermissionList)) != 0) {
	net_FreeCookiePermission(victim, FALSE);
	cookiePermissionList = net_cookie_permission_list;
    }
    XP_ListDestroy(net_cookie_permission_list);
    net_cookie_permission_list = NULL;
    net_unlock_cookie_permission_list();
}
#endif


/* This should only get called while holding the cookie-lock
**
*/
PRIVATE void
net_FreeCookie(net_CookieStruct * cookie)
{

	if(!cookie)
		return;
#ifdef TRUST_LABELS
        /*
         * delete the reference to the cookie in the cookie permission list
         * which contains the trust label for the cookie
         */
        DeleteCookieFromPermissions(cookie->host, cookie->name );
#endif
	XP_ListRemoveObject(net_cookie_list, cookie);

	PR_FREEIF(cookie->path);
	PR_FREEIF(cookie->host);
	PR_FREEIF(cookie->name);
	PR_FREEIF(cookie->cookie);

	PR_Free(cookie);

	cookies_changed = TRUE;
}


PUBLIC void
NET_DeleteCookie(char* cookieURL)
{
    net_CookieStruct * cookie;
    XP_List * list_ptr;
    char * cookieURL2 = 0;

    net_lock_cookie_list();
    list_ptr = net_cookie_list;
    while((cookie = (net_CookieStruct *) XP_ListNextObject(list_ptr))!=0) {
        StrAllocCopy(cookieURL2, "cookie:");
	StrAllocCat(cookieURL2, cookie->host);
        StrAllocCat(cookieURL2, "!");
	StrAllocCat(cookieURL2, cookie->path);
        StrAllocCat(cookieURL2, "!");
	StrAllocCat(cookieURL2, cookie->name);
	if (PL_strcmp(cookieURL, cookieURL2)==0) {
	    net_FreeCookie(cookie);
	    break;
	}
    }
    PR_FREEIF(cookieURL2);
    net_unlock_cookie_list();
}


/* blows away all cookies currently in the list, then blows away the list itself
 * nulling it after it's free'd
 */
PRIVATE void
net_RemoveAllCookies()
{
	net_CookieStruct * victim;
	XP_List * cookieList;

	/* check for NULL or empty list */
	net_lock_cookie_list();
	cookieList = net_cookie_list;
	if(XP_ListIsEmpty(cookieList)) {
		net_unlock_cookie_list();
		return;
	}

    while((victim = (net_CookieStruct *) XP_ListNextObject(cookieList)) != 0) {
		net_FreeCookie(victim);
		cookieList=net_cookie_list;
	}
	XP_ListDestroy(net_cookie_list);
	net_cookie_list = NULL;
	net_unlock_cookie_list();
}

PUBLIC void
NET_RemoveAllCookies()
{

#if defined(CookieManagement)
	net_RemoveAllCookiePermissions();
#endif
	net_RemoveAllCookies();
}

PRIVATE void
net_remove_oldest_cookie(void)
{
    XP_List * list_ptr;
    net_CookieStruct * cookie_s;
    net_CookieStruct * oldest_cookie;

	net_lock_cookie_list();
	list_ptr = net_cookie_list;

	if(XP_ListIsEmpty(list_ptr)) {
		net_unlock_cookie_list();
		return;
	}

	oldest_cookie = (net_CookieStruct*) list_ptr->next->object;
	
    while((cookie_s = (net_CookieStruct *) XP_ListNextObject(list_ptr))!=0)
      {
		if(cookie_s->last_accessed < oldest_cookie->last_accessed)
			oldest_cookie = cookie_s;
	  }

	if(oldest_cookie)
	  {
		TRACEMSG(("Freeing cookie because global max cookies has been exceeded"));
	    net_FreeCookie(oldest_cookie);
	  }
	net_unlock_cookie_list();
}

/* Remove any expired cookies from memory 
** This routine should only be called while holding the cookie list lock
*/
PRIVATE void
net_remove_expired_cookies(void)
{
    XP_List * list_ptr = net_cookie_list;
    net_CookieStruct * cookie_s;
	time_t cur_time = time(NULL);

	if(XP_ListIsEmpty(list_ptr))
		return;

    while((cookie_s = (net_CookieStruct *) XP_ListNextObject(list_ptr))!=0)
      {
		/* Don't get rid of expire time 0 because these need to last for 
		 * the entire session. They'll get cleared on exit.
		 */
		if( cookie_s->expires && (cookie_s->expires < cur_time) ) {
			net_FreeCookie(cookie_s);
			/* Reset the list_ptr to the beginning of the list.
			 * Do this because list_ptr's object was just freed
			 * by the call to net_FreeCookie struct, even
			 * though it's inefficient.
			 */
			list_ptr = net_cookie_list;
		}
	  }
}

PRIVATE XP_List * net_dormant_cookie_list=0;
Bool net_anonymous = FALSE;

PUBLIC void
NET_AnonymizeCookies()
{
    if (!net_anonymous) {
	net_lock_cookie_list();
	net_dormant_cookie_list = net_cookie_list;
	net_cookie_list = XP_ListNew();
	net_unlock_cookie_list();
	net_anonymous = TRUE;
    }
}

PUBLIC void
NET_UnanonymizeCookies()
{
    if (net_anonymous) {
	net_RemoveAllCookies();
	net_lock_cookie_list();
	net_cookie_list = net_dormant_cookie_list;
	net_unlock_cookie_list();
	net_dormant_cookie_list = 0;
	net_anonymous = FALSE;
    }
}

/*
 * Determine whether or not to suppress the referer field for anonymity.
 * The first time this function is called on behalf of a given window
 * after a mode change (into or out of anonymous mode), the returned value is
 * true.
 */
PUBLIC Bool
NET_SupressRefererForAnonymity(MWContext *window_id) {
   MWContext *outermost_window = window_id;
   Bool result;
   if (!window_id) {
      return FALSE;
   }
   while (outermost_window->grid_parent) {
      outermost_window = outermost_window->grid_parent;
   }
   result = (window_id->anonymous != net_anonymous);
   window_id->anonymous = net_anonymous;
   return result;
}

/* checks to see if the maximum number of cookies per host
 * is being exceeded and deletes the oldest one in that
 * case
 * This routine should only be called while holding the cookie lock
 */
PRIVATE void
net_CheckForMaxCookiesFromHost(const char * cur_host)
{
    XP_List * list_ptr = net_cookie_list;
    net_CookieStruct * cookie_s;
    net_CookieStruct * oldest_cookie = 0;
	int cookie_count = 0;

    if(XP_ListIsEmpty(list_ptr))
        return;

    while((cookie_s = (net_CookieStruct *) XP_ListNextObject(list_ptr))!=0)
      {
	    if(!PL_strcasecmp(cookie_s->host, cur_host))
		  {
			cookie_count++;
			if(!oldest_cookie 
				|| oldest_cookie->last_accessed > cookie_s->last_accessed)
                oldest_cookie = cookie_s;
		  }
      }

	if(cookie_count >= MAX_COOKIES_PER_SERVER && oldest_cookie)
	  {
		TRACEMSG(("Freeing cookie because max cookies per server has been exceeded"));
        net_FreeCookie(oldest_cookie);
	  }
}


/* search for previous exact match
** This routine should only be called while holding the cookie lock
*/
PRIVATE net_CookieStruct *
net_CheckForPrevCookie(char * path,
                   char * hostname,
                   char * name)
{

    XP_List * list_ptr = net_cookie_list;
    net_CookieStruct * cookie_s;

    while((cookie_s = (net_CookieStruct *) XP_ListNextObject(list_ptr))!=0) {
        if(path && hostname && cookie_s->path && cookie_s->host && cookie_s->name
                && !PL_strcmp(name, cookie_s->name)
                && !PL_strcmp(path, cookie_s->path)
                && !PL_strcasecmp(hostname, cookie_s->host))
            return(cookie_s);
    }
    return(NULL);
}

/* cookie utility functions */
PRIVATE void
NET_SetCookieBehaviorPref(NET_CookieBehaviorEnum x)
{
	net_CookieBehavior = x;

	HG83330
	if(net_CookieBehavior == NET_DontUse) {
		NET_XP_FileRemove("", xpHTTPCookie);
#if defined(CookieManagement)
		NET_XP_FileRemove("", xpHTTPCookiePermission);
#endif
	}
}

PRIVATE void
NET_SetCookieWarningPref(Bool x)
{
	net_WarnAboutCookies = x;
}

PRIVATE void
NET_SetCookieScriptPref(const char *name)
{
    PR_FREEIF(net_scriptName);
	if( name && *name )
		net_scriptName=PL_strdup(name);
	else
		net_scriptName=NULL;
}

PRIVATE NET_CookieBehaviorEnum
NET_GetCookieBehaviorPref(void)
{
	return net_CookieBehavior;
}

PRIVATE Bool
NET_GetCookieWarningPref(void)
{
	return net_WarnAboutCookies;
}

PRIVATE const char *
NET_GetCookieScriptPref(void)
{
    return (const char *)net_scriptName;
}

MODULE_PRIVATE int PR_CALLBACK
NET_CookieBehaviorPrefChanged(const char * newpref, void * data)
{
	PRInt32	n;
    if ( (PREF_OK != PREF_GetIntPref(pref_cookieBehavior, &n)) ) {
        n = DEF_COOKIE_BEHAVIOR;
    }
	NET_SetCookieBehaviorPref((NET_CookieBehaviorEnum)n);
	return PREF_NOERROR;
}

MODULE_PRIVATE int PR_CALLBACK
NET_CookieWarningPrefChanged(const char * newpref, void * data)
{
	PRBool	x;
    if ( (PREF_OK != PREF_GetBoolPref(pref_warnAboutCookies, &x)) ) {
        x = FALSE;
    }
	NET_SetCookieWarningPref(x);
	return PREF_NOERROR;
}

MODULE_PRIVATE int PR_CALLBACK
NET_CookieScriptPrefChanged(const char * newpref, void * data)
{
    char    s[64];
    int len = sizeof(s);
	*s='\0';
    PREF_GetCharPref(pref_scriptName, s, &len);
    NET_SetCookieScriptPref(s);
    return PREF_NOERROR;
}
 
#if defined(CookieManagement)

/*
 * search if permission already exists
 */
PRIVATE net_CookiePermissionStruct *
net_CheckForCookiePermission(char * hostname) {
    XP_List * list_ptr;
    net_CookiePermissionStruct * cookie_s;

    /* ignore leading period in host name */
    while (hostname && (*hostname == '.')) {
	hostname++;
    }

    net_lock_cookie_permission_list();
    list_ptr = net_cookie_permission_list;
    while((cookie_s = (net_CookiePermissionStruct *) XP_ListNextObject(list_ptr))!=0) {
	if(hostname && cookie_s->host
		&& !PL_strcasecmp(hostname, cookie_s->host)) {
	    net_unlock_cookie_permission_list();
	    return(cookie_s);
        }
    }
    net_unlock_cookie_permission_list();
    return(NULL);
}

/*
 * return:
 *  +1 if cookie permission exists for host and indicates host can set cookies
 *   0 if cookie permission does not exist for host
 *  -1 if cookie permission exists for host and indicates host can't set cookies
 */
PUBLIC int
NET_CookiePermission(char* URLName) {
    net_CookiePermissionStruct * cookiePermission;
    char * host;
    char * colon;

    if (!URLName || !(*URLName)) {
	return 0;
    }

    /* remove protocol from URL name */
    host = NET_ParseURL(URLName, GET_HOST_PART);

    /* remove port number from URL name */
    colon = PL_strchr(host, ':');
    if(colon) {
        *colon = '\0';
    }
    cookiePermission = net_CheckForCookiePermission(host);
    if(colon) {
        *colon = ':';
    }
    PR_Free(host);
    if (cookiePermission == NULL) {
	return 0;
    }
    if (cookiePermission->permission) {
	return 1;
    }
    return -1;
}

#endif

/* called from mkgeturl.c, NET_InitNetLib(). This sets the module local cookie pref variables
   and registers the callbacks */
PUBLIC void
NET_RegisterCookiePrefCallbacks(void)
{
	PRInt32	n;
	PRBool	x;
    char    s[64];
    int len = sizeof(s);
    *s = '\0';

    if ( (PREF_OK != PREF_GetIntPref(pref_cookieBehavior, &n)) ) {
        n = DEF_COOKIE_BEHAVIOR;
    }
	NET_SetCookieBehaviorPref((NET_CookieBehaviorEnum)n);
	PREF_RegisterCallback(pref_cookieBehavior, NET_CookieBehaviorPrefChanged, NULL);

    if ( (PREF_OK != PREF_GetBoolPref(pref_warnAboutCookies, &x)) ) {
        x = FALSE;
    }
	NET_SetCookieWarningPref(x);
	PREF_RegisterCallback(pref_warnAboutCookies, NET_CookieWarningPrefChanged, NULL);

	*s='\0';
	PREF_GetCharPref(pref_scriptName, s, &len);
	NET_SetCookieScriptPref(s);
	PREF_RegisterCallback(pref_scriptName, NET_CookieScriptPrefChanged, NULL);
}

/* returns TRUE if authorization is required
** 
**
** IMPORTANT:  Now that this routine is mutli-threaded it is up
**             to the caller to free any returned string
*/
PUBLIC char *
NET_GetCookie(MWContext * context, char * address)
{
	char *name=0;
	char *host = NET_ParseURL(address, GET_HOST_PART);
	char *path = NET_ParseURL(address, GET_PATH_PART);
    XP_List * list_ptr;
    net_CookieStruct * cookie_s;
	Bool first=TRUE;
	HG26748
	time_t cur_time = time(NULL);
	int host_length;
	int domain_length;

	/* return string to build */
	char * rv=0;

	/* disable cookie's if the user's prefs say so
	 */
	if(NET_GetCookieBehaviorPref() == NET_DontUse)
		return NULL;

	HG98476

	/* search for all cookies
	 */
	net_lock_cookie_list();
	list_ptr = net_cookie_list;
    while((cookie_s = (net_CookieStruct *) XP_ListNextObject(list_ptr))!=0)
      {
		if(!cookie_s->host)
			continue;

		/* check the host or domain first
		 */
		if(cookie_s->is_domain)
		  {
			char *cp;
			domain_length = PL_strlen(cookie_s->host);

			/* calculate the host length by looking at all characters up to
			 * a colon or '\0'.  That way we won't include port numbers
			 * in domains
			 */
			for(cp=host; *cp != '\0' && *cp != ':'; cp++)
				; /* null body */ 

			host_length = cp - host;
			if(domain_length > host_length 
				|| PL_strncasecmp(cookie_s->host, 
								&host[host_length - domain_length], 
								domain_length))
			  {
				HG20476
				/* no match. FAIL 
				 */
				continue;
			  }
			
		  }
		else if(PL_strcasecmp(host, cookie_s->host))
		  {
			/* hostname matchup failed. FAIL
			 */
			continue;
		  }

        /* shorter strings always come last so there can be no
         * ambiquity
         */
        if(cookie_s->path && !PL_strncmp(path,
                                         cookie_s->path,
                                         PL_strlen(cookie_s->path)))
          {

			/* if the cookie is secure and the path isn't
			 * dont send it
			 */
			HG83764
			
			/* check for expired cookies
			 */
			if( cookie_s->expires && (cookie_s->expires < cur_time) )
			  {
				/* expire and remove the cookie 
				 */
		   		net_FreeCookie(cookie_s);

				/* start the list parsing over :(
				 * we must also start the string over
				 */
				PR_FREEIF(rv);
				rv = NULL;
				list_ptr = net_cookie_list;
				first = TRUE; /* reset first */
				continue;
			  }

			if(first)
				first = FALSE;
			else
				StrAllocCat(rv, "; ");
			
			if(cookie_s->name && *cookie_s->name != '\0')
			  {
				cookie_s->last_accessed = cur_time;
            	StrAllocCopy(name, cookie_s->name);
            	StrAllocCat(name, "=");

#ifdef PREVENT_DUPLICATE_NAMES
				/* make sure we don't have a previous
				 * name mapping already in the string
				 */
				if(!rv || !PL_strstr(rv, name))
			      {	
            	    StrAllocCat(rv, name);
            	    StrAllocCat(rv, cookie_s->cookie);
				  }
#else
            	StrAllocCat(rv, name);
                StrAllocCat(rv, cookie_s->cookie);
#endif /* PREVENT_DUPLICATE_NAMES */
			  }
			else
			  {
            	StrAllocCat(rv, cookie_s->cookie);
			  }
          }
	  }

#if defined(MOZ_BRPROF)
    /*
     * Okay, this is a horrible hack. It looks for a URL with
     * 'iiop/BRPROF' in it (our little browsing profile reader), and
     * if it finds it, it uploads the browsing profile cookie.
     *
     * The _real_ way to do this would be to
     *
     * 1) See if the user has enabled the preference to even allow us
     *    to send out the cookie to _anyone_.
     *
     * 2) See if the specific site has asked for and received
     *    permission to be sent the cookie.
     */
    if (PL_strstr(address, "iiop/BRPROF")) {
      extern PRBool BP_GetProfile(char* *aProfileCookie);
      char* profile;
      if (BP_GetProfile(&profile)) {
        if (! first)
          StrAllocCat(rv, "; ");
        StrAllocCat(rv, "BP=");
        StrAllocCat(rv, profile);
        PL_strfree(profile);
      }
    }
#endif    

	  net_unlock_cookie_list();
	PR_FREEIF(name);
	PR_Free(path);
	PR_Free(host);

	/* may be NULL */
	return(rv);
}

#if defined(CookieManagement)
void
net_AddCookiePermission
	(net_CookiePermissionStruct * cookie_permission, Bool save ) {

    /*
     * This routine should only be called while holding the
     * cookie permission list lock
     */
    if (cookie_permission) {
	XP_List * list_ptr = net_cookie_permission_list;
	if(!net_cookie_permission_list) {
	    net_cookie_permission_list = XP_ListNew();
	    if(!net_cookie_permission_list) {
		PR_Free(cookie_permission->host);
		PR_Free(cookie_permission);
		return;
	    }
	    list_ptr = net_cookie_permission_list;
	}

#ifdef alphabetize
	/* add it to the list in alphabetical order */
	{
	    net_CookiePermissionStruct * tmp_cookie_permission;
	    Bool permissionAdded = FALSE;
	    while((tmp_cookie_permission = (net_CookiePermissionStruct *)
					   XP_ListNextObject(list_ptr))!=0) {
		if (PL_strcasecmp
			(cookie_permission->host,tmp_cookie_permission->host)<0) {
		    XP_ListInsertObject
			(net_cookie_permission_list,
			tmp_cookie_permission,
			cookie_permission);
		    permissionAdded = TRUE;
		    break;
		}
	    }
	    if (!permissionAdded) {
		XP_ListAddObjectToEnd
		    (net_cookie_permission_list, cookie_permission);
	    }
	}
#else
	/* add it to the end of the list */
	XP_ListAddObjectToEnd (net_cookie_permission_list, cookie_permission);
#endif

	if (save) {
	    cookie_permissions_changed = TRUE;
	    net_SaveCookiePermissions(NULL);
	}
    }
}

MODULE_PRIVATE PRBool
net_CookieIsFromHost(net_CookieStruct *cookie_s, char *host) {
    if (!cookie_s || !(cookie_s->host)) {
        return FALSE;
    }
    if (cookie_s->is_domain) {
        char *cp;
        int domain_length, host_length;

        domain_length = PL_strlen(cookie_s->host);

        /* calculate the host length by looking at all characters up to
         * a colon or '\0'.  That way we won't include port numbers
         * in domains
         */
        for(cp=host; *cp != '\0' && *cp != ':'; cp++) {
            ; /* null body */
        }
        host_length = cp - host;

        /* compare the tail end of host to cook_s->host */
        return (domain_length <= host_length &&
                PL_strncasecmp(cookie_s->host,
                    &host[host_length - domain_length],
                    domain_length) == 0);
    } else {
        return PL_strcasecmp(host, cookie_s->host) == 0;
    }
}

/* find out how many cookies this host has already set */
PRIVATE int
net_CookieCount(char * host) {
    int count = 0;
    XP_List * list_ptr;
    net_CookieStruct * cookie;

    net_lock_cookie_list();
    list_ptr = net_cookie_list;
    while((cookie = (net_CookieStruct *) XP_ListNextObject(list_ptr))!=0) {
	if (host && net_CookieIsFromHost(cookie, host)) {
	    count++;
	}
    }
    net_unlock_cookie_list();

    return count;
}

PUBLIC int
NET_CookieCount(char * URLName) {
    char * host;
    char * colon;
    int count = 0;

    if (!URLName || !(*URLName)) {
	return 0;
    }

    /* remove protocol from URL name */
    host = NET_ParseURL(URLName, GET_HOST_PART);

    /* remove port number from URL name */
    colon = PL_strchr(host, ':');
    if(colon) {
        *colon = '\0';
    }

    /* get count */
    count = net_CookieCount(host);

    /* free up allocated string and return */
    if(colon) {
        *colon = ':';
    }
    PR_Free(host);
    return count;
}

PRBool net_IntSetCookieStringInUse = FALSE;

PRIVATE void
net_DeferCookie(
		MWContext * context,
		char * cur_url,
		char * set_cookie_header,
		time_t timeToExpire) {
	net_DeferCookieStruct * defer_cookie = PR_NEW(net_DeferCookieStruct);
	defer_cookie->context = context;
	defer_cookie->cur_url = NULL;
	StrAllocCopy(defer_cookie->cur_url, cur_url);
	defer_cookie->set_cookie_header = NULL;
	StrAllocCopy(defer_cookie->set_cookie_header, set_cookie_header);
	defer_cookie->timeToExpire = timeToExpire;
	net_lock_defer_cookie_list();
	if (!net_defer_cookie_list) {
		net_defer_cookie_list = XP_ListNew();
		if (!net_cookie_list) {
			PR_FREEIF(defer_cookie->cur_url);
			PR_FREEIF(defer_cookie->set_cookie_header);
			PR_Free(defer_cookie);
		}
	}
	XP_ListAddObject(net_defer_cookie_list, defer_cookie);
	net_unlock_defer_cookie_list();
}

PRIVATE void
net_IntSetCookieString(MWContext * context,
					char * cur_url,
					char * set_cookie_header,
					time_t timeToExpire );

PRIVATE void
net_UndeferCookies() {
	net_DeferCookieStruct * defer_cookie;
	net_lock_defer_cookie_list();
	if(XP_ListIsEmpty(net_defer_cookie_list)) {
		net_unlock_defer_cookie_list();
		return;
	}
	defer_cookie = XP_ListRemoveEndObject(net_defer_cookie_list);
	net_unlock_defer_cookie_list();
	net_IntSetCookieString (
		defer_cookie->context,
		defer_cookie->cur_url,
		defer_cookie->set_cookie_header,
		defer_cookie->timeToExpire);
	PR_FREEIF(defer_cookie->cur_url);
	PR_FREEIF(defer_cookie->set_cookie_header);
	PR_Free(defer_cookie);
}
#endif

/* Java script is calling NET_SetCookieString, netlib is calling 
** this via NET_SetCookieStringFromHttp.
*/

PRIVATE void
net_IntSetCookieString(MWContext * context,
					char * cur_url,
					char * set_cookie_header,
					time_t timeToExpire )
{
	net_CookieStruct * prev_cookie;
	char *path_from_header=NULL, *host_from_header=NULL;
	char *host_from_header2=NULL;
	char *name_from_header=NULL, *cookie_from_header=NULL;
	time_t expires=0;
	char *cur_path = NET_ParseURL(cur_url, GET_PATH_PART);
	char *cur_host = NET_ParseURL(cur_url, GET_HOST_PART);
	char *semi_colon, *ptr, *equal;
	const char *script_name;
	PRBool HG83744 is_domain=FALSE, ask=FALSE, accept=FALSE;
	MWContextType type;
	Bool bCookieAdded;

	if(!context) {
		PR_Free(cur_path);
		PR_Free(cur_host);
		return;
	}

	/* Only allow cookies to be set in the listed contexts. We
	 * don't want cookies being set in html mail. 
	 */
	type = context->type;
	if(!( (type == MWContextBrowser)
		|| (type == MWContextHTMLHelp)
		|| (type == MWContextPane) )) {
		PR_Free(cur_path);
		PR_Free(cur_host);
		return;
	}
	
	if(NET_GetCookieBehaviorPref() == NET_DontUse) {
		PR_Free(cur_path);
		PR_Free(cur_host);
		return;
	}

#if defined(CookieManagement)
	/* Don't enter this routine if it is already in use by another
	   thread.  Otherwise the "remember this decision" result of the
	   other cookie (which came first) won't get applied to this cookie.
	 */
	if (net_IntSetCookieStringInUse) {
		PR_Free(cur_path);
		PR_Free(cur_host);
		net_DeferCookie(context, cur_url, set_cookie_header, timeToExpire);
		return;
	}
	net_IntSetCookieStringInUse = TRUE;
#endif

	HG87358
	/* terminate at any carriage return or linefeed */
	for(ptr=set_cookie_header; *ptr; ptr++)
		if(*ptr == LF || *ptr == CR) {
			*ptr = '\0';
			break;
		}

	/* parse path and expires attributes from header if
 	 * present
	 */
	semi_colon = PL_strchr(set_cookie_header, ';');

	if(semi_colon)
	  {
		/* truncate at semi-colon and advance 
		 */
		*semi_colon++ = '\0';

		/* there must be some attributes. (hopefully)
		 */
		HG83476

		/* look for the path attribute
		 */
		ptr = PL_strcasestr(semi_colon, "path=");

		if(ptr) {
			/* allocate more than we need */
			StrAllocCopy(path_from_header, XP_StripLine(ptr+5));
			/* terminate at first space or semi-colon
			 */
			for(ptr=path_from_header; *ptr != '\0'; ptr++)
				if(NET_IS_SPACE(*ptr) || *ptr == ';' || *ptr == ',') {
					*ptr = '\0';
					break;
				  }
		  }

		/* look for the URI or URL attribute
		 *
		 * This might be a security hole so I'm removing
		 * it for now.
		 */

		/* look for a domain */
        ptr = PL_strcasestr(semi_colon, "domain=");

        if(ptr) {
			char *domain_from_header=NULL;
			char *dot, *colon;
			int domain_length, cur_host_length;

            /* allocate more than we need */
			StrAllocCopy(domain_from_header, XP_StripLine(ptr+7));

            /* terminate at first space or semi-colon
             */
            for(ptr=domain_from_header; *ptr != '\0'; ptr++)
                if(NET_IS_SPACE(*ptr) || *ptr == ';' || *ptr == ',') {
                    *ptr = '\0';
                    break;
                  }

            /* verify that this host has the authority to set for
             * this domain.   We do this by making sure that the
             * host is in the domain
             * We also require that a domain have at least two
             * periods to prevent domains of the form ".com"
             * and ".edu"
			 *
			 * Also make sure that there is more stuff after
			 * the second dot to prevent ".com."
             */
            dot = PL_strchr(domain_from_header, '.');
			if(dot)
                dot = PL_strchr(dot+1, '.');

			if(!dot || *(dot+1) == '\0') {
				/* did not pass two dot test. FAIL
				 */
				PR_Free(domain_from_header);
				PR_Free(cur_path);
				PR_Free(cur_host);
				TRACEMSG(("DOMAIN failed two dot test"));
#if defined(CookieManagement)
				net_IntSetCookieStringInUse = FALSE;
				net_UndeferCookies();
#endif
				return;
			  }

			/* strip port numbers from the current host
			 * for the domain test
			 */
			colon = PL_strchr(cur_host, ':');
			if(colon)
			   *colon = '\0';

			domain_length   = PL_strlen(domain_from_header);
			cur_host_length = PL_strlen(cur_host);

			/* check to see if the host is in the domain
			 */
			if(domain_length > cur_host_length
				|| PL_strcasecmp(domain_from_header, 
							   &cur_host[cur_host_length-domain_length]))
			  {
				TRACEMSG(("DOMAIN failed host within domain test."
					  " Domain: %s, Host: %s", domain_from_header, cur_host));
				PR_Free(domain_from_header);
				PR_Free(cur_path);
				PR_Free(cur_host);
#if defined(CookieManagement)
				net_IntSetCookieStringInUse = FALSE;
				net_UndeferCookies();
#endif
				return;
              }

/*
 * check that portion of host not in domain does not contain a dot
 *    This satisfies the fourth requirement in section 4.3.2 of the cookie
 *    spec rfc 2109 (see www.cis.ohio-state.edu/htbin/rfc/rfc2109.html).
 *    It prevents host of the form x.y.co.nz from setting cookies in the
 *    entire .co.nz domain.  Note that this doesn't really solve the problem,
 *    it justs makes it more unlikely.  Sites such as y.co.nz can still set
 *    cookies for the entire .co.nz domain.
 */

			cur_host[cur_host_length-domain_length] = '\0';
			dot = XP_STRCHR(cur_host, '.');
			cur_host[cur_host_length-domain_length] = '.';
			if (dot) {
				TRACEMSG(("host minus domain failed no-dot test."
				  " Domain: %s, Host: %s", domain_from_header, cur_host));
				PR_Free(domain_from_header);
				PR_Free(cur_path);
				PR_Free(cur_host);
#if defined(CookieManagement)
				net_IntSetCookieStringInUse = FALSE;
				net_UndeferCookies();
#endif
				return;
			}

			/* all tests passed, copy in domain to hostname field
			 */
			StrAllocCopy(host_from_header, domain_from_header);
			is_domain = TRUE;

			TRACEMSG(("Accepted domain: %s", host_from_header));

			PR_Free(domain_from_header);
          }

		/* now search for the expires header 
		 * NOTE: that this part of the parsing
		 * destroys the original part of the string
		 */
		ptr = PL_strcasestr(semi_colon, "expires=");

		if(ptr) {
			char *date =  ptr+8;
			/* terminate the string at the next semi-colon
			 */
			for(ptr=date; *ptr != '\0'; ptr++)
				if(*ptr == ';') {
					*ptr = '\0';
					break;
				  }
			if(timeToExpire)
				expires = timeToExpire;
			else
				expires = NET_ParseDate(date);

			TRACEMSG(("Have expires date: %ld", expires));
		  }
	  }

	if(!path_from_header) {
        /* strip down everything after the last slash
         * to get the path.
         */
        char * slash = PL_strrchr(cur_path, '/');
        if(slash)
            *slash = '\0';

		path_from_header = cur_path;
	  } else {
		PR_Free(cur_path);
	  }

	if(!host_from_header)
		host_from_header = cur_host;
	else
		PR_Free(cur_host);

	/* keep cookies under the max bytes limit */
	if(PL_strlen(set_cookie_header) > MAX_BYTES_PER_COOKIE)
		set_cookie_header[MAX_BYTES_PER_COOKIE-1] = '\0';

	/* separate the name from the cookie */
	equal = PL_strchr(set_cookie_header, '=');

	if(equal) {
		*equal = '\0';
		StrAllocCopy(name_from_header, XP_StripLine(set_cookie_header));
		StrAllocCopy(cookie_from_header, XP_StripLine(equal+1));
	  } else {
		TRACEMSG(("Warning: no name found for cookie"));
		StrAllocCopy(cookie_from_header, XP_StripLine(set_cookie_header));
		StrAllocCopy(name_from_header, "");
	  }

    /* If there's a script, call it now */
    script_name = NET_GetCookieScriptPref();
    if( (const char *)0 != script_name ) {
        JSCFCookieData *cd;
        Bool changed = FALSE;
        JSCFResult result;

        cd = PR_NEWZAP(JSCFCookieData);
        if( (JSCFCookieData *)0 == cd ) {
            PR_FREEIF(path_from_header);
            PR_FREEIF(host_from_header);
            PR_FREEIF(name_from_header);
            PR_FREEIF(cookie_from_header);
            /* FREEIF(cur_path); */
            /* FREEIF(cur_host); */
#if defined(CookieManagement)
            net_IntSetCookieStringInUse = FALSE;
            net_UndeferCookies();
#endif
            return;
        }

        cd->path_from_header = path_from_header;
        cd->host_from_header = host_from_header;
        cd->name_from_header = name_from_header;
        cd->cookie_from_header = cookie_from_header;
        cd->expires = expires;
        cd->url = cur_url;
        HG84777
        cd->domain = is_domain;
        cd->prompt = NET_GetCookieWarningPref();
        cd->preference = NET_GetCookieBehaviorPref();

	/*
	 * This is only safe to do from the mozilla thread
	 *   since it might do file I/O and uses the preferences
	 *   global context + objects.
	 */
        /* XXX:  This is probably not correct, but it will fix the build for now... */
        result= JSCF_Execute(context, script_name, cd, &changed);
		if( result != JSCF_error) {
			if( changed ) {
				if( cd->path_from_header != path_from_header ) {
					PR_FREEIF(path_from_header);
					path_from_header = PL_strdup(cd->path_from_header);
				}
				if( cd->host_from_header != host_from_header ) {
					PR_FREEIF(host_from_header);
					host_from_header = PL_strdup(cd->host_from_header);
				}
				if( cd->name_from_header != name_from_header ) {
					PR_FREEIF(name_from_header);
					name_from_header = PL_strdup(cd->name_from_header);
				}
				if( cd->cookie_from_header != cookie_from_header ) {
					PR_FREEIF(cookie_from_header);
				  cookie_from_header = PL_strdup(cd->cookie_from_header);
			  }
			  if( cd->expires != expires )
				  expires = cd->expires;
			  if( cd->domain != is_domain )
				  /* lets hope the luser remembered to change the domain field */
				  is_domain = cd->domain;
			  HG27398
			}
			switch( result ) {
				case JSCF_reject:
					PR_FREEIF(path_from_header);
					PR_FREEIF(host_from_header);
					PR_FREEIF(name_from_header);
					PR_FREEIF(cookie_from_header);
					PR_Free(cd);
					/* FREEIF(cur_path); */
					/* FREEIF(cur_host); */
#if defined(CookieManagement)
					net_IntSetCookieStringInUse = FALSE;
					net_UndeferCookies();
#endif
					return;
				case JSCF_accept:
					accept=TRUE;
				case JSCF_error:
				case JSCF_ask:
					ask=TRUE;
				case JSCF_whatever:
				break;
			}
		}
		PR_Free(cd);
#if defined(CookieManagement)
    } else {
	net_CookiePermissionStruct * cookie_permission;
	cookie_permission = net_CheckForCookiePermission(host_from_header);
	if (cookie_permission != NULL) {
	    if (cookie_permission->permission == FALSE) {
		PR_FREEIF(path_from_header);
		PR_FREEIF(host_from_header);
		PR_FREEIF(name_from_header);
		PR_FREEIF(cookie_from_header);
		net_IntSetCookieStringInUse = FALSE;
		net_UndeferCookies();
		return;
	    } else {
		accept = TRUE;
            }
	}
#endif
    }
 

	if( (NET_GetCookieWarningPref() || ask) && !accept ) {
		/* the user wants to know about cookies so let them
		 * know about every one that is set and give them
		 * the choice to accept it or not
		 */
		char * new_string=0;
#if defined(CookieManagement)
		int count;
		char * remember_string = 0;
		StrAllocCopy
		    (remember_string, XP_GetString(MK_ACCESS_COOKIES_REMEMBER));

		/* find out how many cookies this host has already set */
		count = net_CookieCount(host_from_header);
		net_lock_cookie_list();
		prev_cookie = net_CheckForPrevCookie
			(path_from_header, host_from_header, name_from_header);
		net_unlock_cookie_list();
		if (prev_cookie) {
		    new_string = PR_smprintf(
			XP_GetString(MK_ACCESS_COOKIES_WISHES_MODIFY),
			host_from_header ? host_from_header : "");
		} else if (count>1) {
		    new_string = PR_smprintf(
			XP_GetString(MK_ACCESS_COOKIES_WISHESN),
			host_from_header ? host_from_header : "",
			count);
		} else if (count==1){
		    new_string = PR_smprintf(
			XP_GetString(MK_ACCESS_COOKIES_WISHES1),
			host_from_header ? host_from_header : "");
		} else {
		    new_string = PR_smprintf(
			XP_GetString(MK_ACCESS_COOKIES_WISHES0),
			host_from_header ? host_from_header : "");
		}
#else
		char * tmp_host = NET_ParseURL(cur_url, GET_HOST_PART);
		StrAllocCopy(new_string, XP_GetString(MK_ACCESS_COOKIES_THE_SERVER));
		StrAllocCat(new_string, tmp_host ? tmp_host : "");
		StrAllocCat(new_string, XP_GetString(MK_ACCESS_COOKIES_WISHES));
		PR_Free(tmp_host);

		if(is_domain) {
			StrAllocCat(new_string, XP_GetString(MK_ACCESS_COOKIES_TOANYSERV));
			StrAllocCat(new_string, host_from_header);
		  } else {
			StrAllocCat(new_string, XP_GetString(MK_ACCESS_COOKIES_TOSELF));
		  }

		StrAllocCat(new_string, XP_GetString(MK_ACCESS_COOKIES_NAME_AND_VAL));

		StrAllocCat(new_string, name_from_header);
		StrAllocCat(new_string, "=");
		StrAllocCat(new_string, cookie_from_header);
		StrAllocCat(new_string, "\n");

		if(expires) {
			StrAllocCat(new_string, XP_GetString(MK_ACCESS_COOKIES_COOKIE_WILL_PERSIST));
			StrAllocCat(new_string, ctime(&expires));
		  }

		StrAllocCat(new_string, XP_GetString(MK_ACCESS_COOKIES_SET_IT));
#endif

		/* 
		 * Who knows what thread we are on.  Only the mozilla thread
		 *   is allowed to post dialogs so, if need be, go over there
		 */
#if defined(CookieManagement)

	    {
		Bool old_cookie_remember_checked = cookie_remember_checked;
		XP_Bool userHasAccepted = ET_PostCheckConfirmBox
		    (context,
		     new_string,
		     remember_string,
		     0,0,
		     &cookie_remember_checked);
		PR_FREEIF(new_string);
		PR_FREEIF(remember_string);
		if (cookie_remember_checked) {
                    net_CookiePermissionStruct * cookie_permission;
                    cookie_permission = PR_NEW(net_CookiePermissionStruct);
                    if (cookie_permission) {
                        net_lock_cookie_permission_list();
                        StrAllocCopy(host_from_header2, host_from_header);
                        /* ignore leading periods in host name */
                        while (host_from_header2 && (*host_from_header2 == '.')) {
                            host_from_header2++;
                        }
                        cookie_permission->host = host_from_header2; /* set host string */
                        cookie_permission->permission = userHasAccepted;
#ifdef TRUST_LABELS
                        cookie_permission->TrustList = NULL;
#endif
                        net_AddCookiePermission(cookie_permission, TRUE);
                        net_unlock_cookie_permission_list();
                    }
                }

		if (old_cookie_remember_checked != cookie_remember_checked) {
		    cookie_permissions_changed = TRUE;
		    net_SaveCookiePermissions(NULL);
		}


		if (!userHasAccepted) {
#if defined(CookieManagement)
			net_IntSetCookieStringInUse = FALSE;
			net_UndeferCookies();
#endif
			return;
		}
	    }
#else
		if(!ET_PostMessageBox(context, new_string, TRUE)) {
			PR_FREEIF(new_string);
			return;
		}
		PR_FREEIF(new_string);
#endif
	  }

	TRACEMSG(("mkaccess.c: Setting cookie: %s for host: %s for path: %s",
					cookie_from_header, host_from_header, path_from_header));

	/* 
	 * We have decided we are going to insert a cookie into the list
	 *   Get the cookie lock so that we can munge on the list
	 */
	net_lock_cookie_list();

	/* limit the number of cookies from a specific host or domain */
	net_CheckForMaxCookiesFromHost(host_from_header);

	if(XP_ListCount(net_cookie_list) > MAX_NUMBER_OF_COOKIES-1)
		net_remove_oldest_cookie();


    prev_cookie = net_CheckForPrevCookie
        (path_from_header, host_from_header, name_from_header);

    if(prev_cookie) {
        prev_cookie->expires = expires;
        PR_FREEIF(prev_cookie->cookie);
        PR_FREEIF(prev_cookie->path);
        PR_FREEIF(prev_cookie->host);
        PR_FREEIF(prev_cookie->name);
        prev_cookie->cookie = cookie_from_header;
        prev_cookie->path = path_from_header;
        prev_cookie->host = host_from_header;
        prev_cookie->name = name_from_header;
        HG83263
        prev_cookie->is_domain = is_domain;
		prev_cookie->last_accessed = time(NULL);
      }	else {
		XP_List * list_ptr = net_cookie_list;
		net_CookieStruct * tmp_cookie_ptr;
		size_t new_len;

        /* construct a new cookie_struct
         */
        prev_cookie = PR_NEW(net_CookieStruct);
        if(!prev_cookie) {
			PR_FREEIF(path_from_header);
			PR_FREEIF(host_from_header);
			PR_FREEIF(name_from_header);
			PR_FREEIF(cookie_from_header);
			net_unlock_cookie_list();
#if defined(CookieManagement)
			net_IntSetCookieStringInUse = FALSE;
			net_UndeferCookies();
#endif
			return;
          }
    
        /* copy
         */
        prev_cookie->cookie  = cookie_from_header;
        prev_cookie->name    = name_from_header;
        prev_cookie->path    = path_from_header;
        prev_cookie->host    = host_from_header;
        prev_cookie->expires = expires;
        HG22730
        prev_cookie->is_domain = is_domain;
		prev_cookie->last_accessed = time(NULL);

		if(!net_cookie_list) {
			net_cookie_list = XP_ListNew();
		    if(!net_cookie_list) {
				PR_FREEIF(path_from_header);
				PR_FREEIF(name_from_header);
				PR_FREEIF(host_from_header);
				PR_FREEIF(cookie_from_header);
				PR_Free(prev_cookie);
				net_unlock_cookie_list();
#if defined(CookieManagement)
				net_IntSetCookieStringInUse = FALSE;
				net_UndeferCookies();
#endif
				return;
			  }
		  }		

		/* add it to the list so that it is before any strings of
		 * smaller length
		 */
		bCookieAdded = FALSE;
		new_len = PL_strlen(prev_cookie->path);
		while((tmp_cookie_ptr = (net_CookieStruct *) XP_ListNextObject(list_ptr))!=0) { 
			if(new_len > PL_strlen(tmp_cookie_ptr->path)) {
				XP_ListInsertObject(net_cookie_list, tmp_cookie_ptr, prev_cookie);
				bCookieAdded = TRUE;
				break;
			  }
		  }
		if ( !bCookieAdded ) {
			/* no shorter strings found in list */
			XP_ListAddObjectToEnd(net_cookie_list, prev_cookie);
		}
	  }

	/* At this point we know a cookie has changed. Write the cookies to file. */
	cookies_changed = TRUE;
	NET_SaveCookies(NULL);
	net_unlock_cookie_list();
#ifdef TRUST_LABELS
    {
        TrustLabel *trustlabel;
        /*
         * At this point the cookie was added to
         * the cookie list.  So see if there is a trust label that
         * matches the cookie and if so add the trust label to the
         * cookie permission list.  Existing trust label entries in 
		 * the permission list replaced with this latest entry.
         */
            if ( MatchCookieToLabel2(
                    cur_url, prev_cookie->name,
                    prev_cookie->path, prev_cookie->host, &trustlabel ) ) {
                /*
                 * found a match - add the trust info to the proper
                 * entry in the cookie permission list
                 */
                AddTrustLabelInfo( prev_cookie->name, trustlabel );
                /* update the disk file */
                cookie_permissions_changed = TRUE;
                net_SaveCookiePermissions(NULL);
            }
        }
#endif
#if defined(CookieManagement)
	net_IntSetCookieStringInUse = FALSE;
	net_UndeferCookies();
#endif
	return;
}

PUBLIC void
NET_SetCookieString(MWContext * context, 
					char * cur_url,
					char * set_cookie_header) {
    net_IntSetCookieString(context, cur_url, set_cookie_header, 0);
}

/* Determines whether the inlineHost is in the same domain as the currentHost. For use with rfc 2109
 * compliance/non-compliance. */
#ifdef TRUST_LABELS
PUBLIC int
#else
PRIVATE int
#endif
NET_SameDomain(char * currentHost, char * inlineHost)
{
	char * dot = 0;
	char * currentDomain = 0;
	char * inlineDomain = 0;

	if(!currentHost || !inlineHost)
		return 0;

	/* case insensitive compare */
	if(PL_strcasecmp(currentHost, inlineHost) == 0)
		return 1;

	currentDomain = PL_strchr(currentHost, '.');
	inlineDomain = PL_strchr(inlineHost, '.');

	if(!currentDomain || !inlineDomain)
		return 0;
	
	/* check for at least two dots before continuing, if there are
	   not two dots we don't have enough information to determine
	   whether or not the inlineDomain is within the currentDomain */
	dot = PL_strchr(inlineDomain, '.');
	if(dot)
		dot = PL_strchr(dot+1, '.');
	else
		return 0;

	/* handle .com. case */
	if(!dot || (*(dot+1) == '\0') )
		return 0;

	if(!PL_strcasecmp(inlineDomain, currentDomain))
		return 1;
	return 0;
}

/* This function wrapper wraps NET_SetCookieString for the purposes of 
** determining whether or not a cookie is inline (we need the URL struct, 
** and outputFormat to do so).  this is called from NET_ParseMimeHeaders 
** in mkhttp.c
** This routine does not need to worry about the cookie lock since all of
**   the work is handled by sub-routines
*/
PUBLIC void
NET_SetCookieStringFromHttp(FO_Present_Types outputFormat,
							URL_Struct * URL_s,
							MWContext * context, 
							char * cur_url,
							char * set_cookie_header)
{
	/* If the outputFormat is not PRESENT (the url is not going to the screen), and not
	*  SAVE AS (shift-click) then 
	*  the cookie being set is defined as inline so we need to do what the user wants us
	*  to based on his preference to deal with foreign cookies. If it's not inline, just set
	*  the cookie. */
	char *ptr=NULL, *date=NULL;
	time_t gmtCookieExpires=0, expires=0;

	if(CLEAR_CACHE_BIT(outputFormat) != FO_PRESENT && CLEAR_CACHE_BIT(outputFormat) != FO_SAVE_AS)
	{
		if (NET_GetCookieBehaviorPref() == NET_DontAcceptForeign)
		{
			/* the user doesn't want foreign cookies, check to see if its foreign */
			char * curSessionHistHost = 0;
			char * theColon = 0;
			char * curHost = NET_ParseURL(cur_url, GET_HOST_PART);
			History_entry * shistEntry = SHIST_GetCurrent(&context->hist);
			if (shistEntry) {
			curSessionHistHost = NET_ParseURL(shistEntry->address, GET_HOST_PART);
			}
			if(!curHost || !curSessionHistHost)
			{
				PR_FREEIF(curHost);
				PR_FREEIF(curSessionHistHost);
				return;
			}

			/* strip ports */
			theColon = PL_strchr(curHost, ':');
			if(theColon)
			   *theColon = '\0';
			theColon = PL_strchr(curSessionHistHost, ':');
			if(theColon)
				*theColon = '\0';

			/* if it's foreign, get out of here after a little clean up */
			if(!NET_SameDomain(curHost, curSessionHistHost))
			{
				PR_FREEIF(curHost);	
				PR_FREEIF(curSessionHistHost);
				return;
			}
			PR_FREEIF(curHost);	
			PR_FREEIF(curSessionHistHost);
		}
	}

	/* Determine when the cookie should expire. This is done by taking the difference between 
	   the server time and the time the server wants the cookie to expire, and adding that 
	   difference to the client time. This localizes the client time regardless of whether or
	   not the TZ environment variable was set on the client. */

	/* Get the time the cookie is supposed to expire according to the attribute*/
	ptr = PL_strcasestr(set_cookie_header, "expires=");
	if(ptr)
	{
		char *date =  ptr+8;
		char origLast = '\0';
		for(ptr=date; *ptr != '\0'; ptr++)
			if(*ptr == ';')
			  {
				origLast = ';';
				*ptr = '\0';
				break;
			  }
		expires = NET_ParseDate(date);
		*ptr=origLast;
	}
	if( URL_s->server_date && expires )
	{
		if( expires < URL_s->server_date )
		{
			gmtCookieExpires=1;
		}
		else
		{
			gmtCookieExpires = expires - URL_s->server_date + time(NULL);
			/* if overflow */
			if( gmtCookieExpires < time(NULL) )
				gmtCookieExpires = (((unsigned) (~0) << 1) >> 1); /* max int */
		}
	}

	net_IntSetCookieString(context, cur_url, set_cookie_header, gmtCookieExpires);
}

#if defined(CookieManagement)
/* saves the HTTP cookies permissions to disk
 * the parameter passed in on entry is ignored
 * returns 0 on success -1 on failure.
 */
PRIVATE int
net_SaveCookiePermissions(char * filename)
{
    XP_List * list_ptr;
    net_CookiePermissionStruct * cookie_permission_s;
    XP_File fp;
    int32 len = 0;

    if(NET_GetCookieBehaviorPref() == NET_DontUse) {
	return(-1);
    }

    if(!cookie_permissions_changed) {
	return(-1);
    }

    net_lock_cookie_permission_list();
    list_ptr = net_cookie_permission_list;

    if(!(net_cookie_permission_list)) {
	net_unlock_cookie_permission_list();
	return(-1);
    }

    if(!(fp = NET_XP_FileOpen(filename, xpHTTPCookiePermission, XP_FILE_WRITE))) {
	net_unlock_cookie_permission_list();
	return(-1);
    }
    len = NET_XP_FileWrite("# Netscape HTTP Cookie Permission File" LINEBREAK
                 "# http://www.netscape.com/newsref/std/cookie_spec.html"
                  LINEBREAK "# This is a generated file!  Do not edit."
		  LINEBREAK LINEBREAK, -1, fp);

    if (len < 0) {
	NET_XP_FileClose(fp);
	net_unlock_cookie_permission_list();
	return -1;
    }

    /* format shall be:
     * host \t permission <optional trust label information >
     */
    while((cookie_permission_s = (net_CookiePermissionStruct *)
	    XP_ListNextObject(list_ptr)) != NULL) {
	len = NET_XP_FileWrite(cookie_permission_s->host, -1, fp);
	if (len < 0) {
	    NET_XP_FileClose(fp);
	    net_unlock_cookie_permission_list();
	    return -1;
	}

        NET_XP_FileWrite("\t", 1, fp);

	if(cookie_permission_s->permission) {
            NET_XP_FileWrite("TRUE", -1, fp);
	} else {
            NET_XP_FileWrite("FALSE", -1, fp);
	}

#ifdef TRUST_LABELS
        /* save the trust label information */
        SaveTrustLabelData( cookie_permission_s, fp );
#endif
	len = NET_XP_FileWrite(LINEBREAK, -1, fp);
	if (len < 0) {
	    NET_XP_FileClose(fp);
	    net_unlock_cookie_permission_list();
	    return -1;
	}
    }

    /* save current state of cookie nag-box's checkmark */
    NET_XP_FileWrite("@@@@", -1, fp);
    NET_XP_FileWrite("\t", 1, fp);
    if (cookie_remember_checked) {
        NET_XP_FileWrite("TRUE", -1, fp);
    } else {
        NET_XP_FileWrite("FALSE", -1, fp);
    }
    NET_XP_FileWrite(LINEBREAK, -1, fp);

    cookie_permissions_changed = FALSE;
    NET_XP_FileClose(fp);
    net_unlock_cookie_permission_list();
    return(0);
}

/* reads the HTTP cookies permission from disk
 * the parameter passed in on entry is ignored
 * returns 0 on success -1 on failure.
 *
 */
#define PERMISSION_LINE_BUFFER_SIZE 4096

PRIVATE int
net_ReadCookiePermissions(char * filename)
{
    XP_File fp;
    char buffer[PERMISSION_LINE_BUFFER_SIZE];
    char *host, *p;
    Bool permission_value;
    net_CookiePermissionStruct * cookie_permission;
    char *host_from_header2;

    if(!(fp = NET_XP_FileOpen(filename, xpHTTPCookiePermission, XP_FILE_READ)))
	return(-1);

    /* format is:
     * host \t permission <optional trust label information>
     * if this format isn't respected we move onto the next line in the file.
     */

    net_lock_cookie_permission_list();
    while(NET_XP_FileReadLine(buffer, PERMISSION_LINE_BUFFER_SIZE, fp)) {
        if (*buffer == '#' || *buffer == CR || *buffer == LF || *buffer == 0) {
	    continue;
	}

        XP_StripLine(buffer); /* remove '\n' from end of the line */

        host = buffer;
        /* isolate the host field which is the first field on the line */
        if( !(p = PL_strchr(host, '\t')) ) {
            continue; /* no permission field */
        }
        *p++ = '\0';
        if(*p == CR || *p == LF || *p == 0) {
            continue; /* no permission field */
	}

        /* ignore leading periods in host name */
        while (host && (*host == '.')) {
            host++;
	}

	/*
         * the first part of the permission file is valid -
         * allocate a new permission struct and fill it in
         */
        cookie_permission = PR_NEW(net_CookiePermissionStruct);
        if (cookie_permission) {
            host_from_header2 = PL_strdup(host);
            cookie_permission->host = host_from_header2;

            /*
             *  Now handle the permission field.
             * a host value of "@@@@" is a special code designating the
             * state of the cookie nag-box's checkmark
	     */
            permission_value = (!PL_strncmp(p, "TRUE", sizeof("TRUE")-1));
            if (!PL_strcmp(host, "@@@@")) {
                cookie_remember_checked = permission_value;
            } else {
                cookie_permission->permission = permission_value;
			
#ifdef TRUST_LABELS
                /*
                 * pick up the trust label information from the line
                 * and add it to the permission struct
                 */
                cookie_permission->TrustList = NULL;
                ParseTrustLabelInfo(p, cookie_permission);
#endif

                /* add the permission entry */
                net_AddCookiePermission( cookie_permission, FALSE );
            }
        } /* end if (cookie_permission) */
    } /* while(NET_XP_FileReadLine( */

    net_unlock_cookie_permission_list();
    NET_XP_FileClose(fp);
    cookie_permissions_changed = FALSE;
    return(0);
}

#endif

/* saves out the HTTP cookies to disk
 *
 * on entry pass in the name of the file to save
 *
 * returns 0 on success -1 on failure.
 *
 */
PUBLIC int
NET_SaveCookies(char * filename)
{
    XP_List * list_ptr;
    net_CookieStruct * cookie_s;
	time_t cur_date = time(NULL);
	XP_File fp;
	int32 len = 0;
	char date_string[36];

	if(NET_GetCookieBehaviorPref() == NET_DontUse)
	  return(-1);

	if(!cookies_changed)
	  return(-1);

	if(net_anonymous) {
	    return(-1);
	}

	net_lock_cookie_list();
	list_ptr = net_cookie_list;
	if(!(list_ptr)) {
		net_unlock_cookie_list();
		return(-1);
	}

	if(!(fp = NET_XP_FileOpen(filename, xpHTTPCookie, XP_FILE_WRITE))) {
		net_unlock_cookie_list();
		return(-1);
	}

	len = NET_XP_FileWrite("# Netscape HTTP Cookie File" LINEBREAK
				 "# http://www.netscape.com/newsref/std/cookie_spec.html"
				 LINEBREAK "# This is a generated file!  Do not edit."
				 LINEBREAK LINEBREAK,
				 -1, fp);
	if (len < 0)
	{
		NET_XP_FileClose(fp);
		net_unlock_cookie_list();
		return -1;
	}

	/* format shall be:
 	 *
	 * host \t is_domain \t path \t secure \t expires \t name \t cookie
	 *
	 * is_domain is TRUE or FALSE
	 * secure is TRUE or FALSE  
	 * expires is a time_t integer
	 * cookie can have tabs
	 */
    while((cookie_s = (net_CookieStruct *) XP_ListNextObject(list_ptr)) != NULL)
      {
		if(cookie_s->expires < cur_date)
			continue;  /* don't write entry if cookie has expired 
						* or has no expiration date
						*/

		len = NET_XP_FileWrite(cookie_s->host, -1, fp);
		if (len < 0)
		{
			NET_XP_FileClose(fp);
			net_unlock_cookie_list();
			return -1;
		}
		NET_XP_FileWrite("\t", 1, fp);
		
		if(cookie_s->is_domain)
			NET_XP_FileWrite("TRUE", -1, fp);
		else
			NET_XP_FileWrite("FALSE", -1, fp);
		NET_XP_FileWrite("\t", 1, fp);

		NET_XP_FileWrite(cookie_s->path, -1, fp);
		NET_XP_FileWrite("\t", 1, fp);

		HG74640
		    NET_XP_FileWrite("FALSE", -1, fp);
		NET_XP_FileWrite("\t", 1, fp);

		PR_snprintf(date_string, sizeof(date_string), "%lu", cookie_s->expires);
		NET_XP_FileWrite(date_string, -1, fp);
		NET_XP_FileWrite("\t", 1, fp);

		NET_XP_FileWrite(cookie_s->name, -1, fp);
		NET_XP_FileWrite("\t", 1, fp);

		NET_XP_FileWrite(cookie_s->cookie, -1, fp);
 		len = NET_XP_FileWrite(LINEBREAK, -1, fp);
		if (len < 0)
		{
			NET_XP_FileClose(fp);
			net_unlock_cookie_list();
			return -1;
		}
      }

	cookies_changed = FALSE;

	NET_XP_FileClose(fp);
	net_unlock_cookie_list();
    return(0);
}

/* This isn't needed any more */
PRIVATE void
NET_InitRDFCookieResources (void)
{
}

/* reads HTTP cookies from disk
 *
 * on entry pass in the name of the file to read
 *
 * returns 0 on success -1 on failure.
 *
 *
 */
#define LINE_BUFFER_SIZE 4096

PUBLIC int
NET_ReadCookies(char * filename)
{
    XP_List * list_ptr;
    net_CookieStruct *new_cookie, *tmp_cookie_ptr;
	size_t new_len;
    XP_File fp;
	char buffer[LINE_BUFFER_SIZE];
	char *host, *is_domain, *path, *xxx, *expires, *name, *cookie;
	Bool added_to_list;

#if defined(CookieManagement)
    net_ReadCookiePermissions(NULL);
#endif

    if(!(fp = NET_XP_FileOpen(filename, xpHTTPCookie, XP_FILE_READ)))
        return(-1);

	net_lock_cookie_list();
	list_ptr = net_cookie_list;

    /* format is:
     *
     * host \t is_domain \t path \t xxx \t expires \t name \t cookie
     *
     * if this format isn't respected we move onto the next line in the file.
     * is_domain is TRUE or FALSE	-- defaulting to FALSE
     * xxx is TRUE or FALSE   -- should default to TRUE
     * expires is a time_t integer
     * cookie can have tabs
     */
    while(NET_XP_FileReadLine(buffer, LINE_BUFFER_SIZE, fp))
      {
		added_to_list = FALSE;

		if (*buffer == '#' || *buffer == CR || *buffer == LF || *buffer == 0)
		  continue;

		host = buffer;
		
		if( !(is_domain = PL_strchr(host, '\t')) )
			continue;
		*is_domain++ = '\0';
		if(*is_domain == CR || *is_domain == LF || *is_domain == 0)
			continue;
		
		if( !(path = PL_strchr(is_domain, '\t')) )
			continue;
		*path++ = '\0';
		if(*path == CR || *path == LF || *path == 0)
			continue;

		if( !(xxx = PL_strchr(path, '\t')) )
			continue;
		*xxx++ = '\0';
		if(*xxx == CR || *xxx == LF || *xxx == 0)
			continue;

		if( !(expires = PL_strchr(xxx, '\t')) )
			continue;
		*expires++ = '\0';
		if(*expires == CR || *expires == LF || *expires == 0)
			continue;

        if( !(name = PL_strchr(expires, '\t')) )
			continue;
		*name++ = '\0';
		if(*name == CR || *name == LF || *name == 0)
			continue;

        if( !(cookie = PL_strchr(name, '\t')) )
			continue;
		*cookie++ = '\0';
		if(*cookie == CR || *cookie == LF || *cookie == 0)
			continue;

		/* remove the '\n' from the end of the cookie */
		XP_StripLine(cookie);

        /* construct a new cookie_struct
         */
        new_cookie = PR_NEW(net_CookieStruct);
        if(!new_cookie)
          {
			net_unlock_cookie_list();
            return(-1);
          }

		memset(new_cookie, 0, sizeof(net_CookieStruct));
    
        /* copy
         */
        StrAllocCopy(new_cookie->cookie, cookie);
        StrAllocCopy(new_cookie->name, name);
        StrAllocCopy(new_cookie->path, path);
        StrAllocCopy(new_cookie->host, host);
        new_cookie->expires = atol(expires);

		HG87365

        if(!PL_strcmp(is_domain, "TRUE"))
        	new_cookie->is_domain = TRUE;
        else
        	new_cookie->is_domain = FALSE;

		if(!net_cookie_list)
		  {
			net_cookie_list = XP_ListNew();
		    if(!net_cookie_list)
			  {
				net_unlock_cookie_list();
				return(-1);
			  }
		  }		

		/* add it to the list so that it is before any strings of
		 * smaller length
		 */
		new_len = PL_strlen(new_cookie->path);
		while((tmp_cookie_ptr = (net_CookieStruct *) XP_ListNextObject(list_ptr)) != NULL)
		  { 
			if(new_len > PL_strlen(tmp_cookie_ptr->path))
			  {
				XP_ListInsertObject(net_cookie_list, tmp_cookie_ptr, new_cookie);
				added_to_list = TRUE;
				break;
			  }
		  }

		/* no shorter strings found in list */	
		if(!added_to_list)
		    XP_ListAddObjectToEnd(net_cookie_list, new_cookie);
	  }

    NET_XP_FileClose(fp);
	net_unlock_cookie_list();

	cookies_changed = FALSE;

    return(0);
}


/* --- New stuff: General auth utils (currently used only by proxy auth) --- */

/*
 * Figure out the authentication scheme used; currently supported:
 *
 *		* Basic
 *
 */
 HG73632

PRIVATE net_AuthType
net_auth_type(char *name)
{
	if (name) {
		while (*name && NET_IS_SPACE(*name))
			name++;
		if (!PL_strncasecmp(name, "basic", 5))
			return AUTH_BASIC;
#ifdef SIMPLE_MD5
		HG29383
#endif
		/*FORTEZZA checks*/
		else if (!PL_strncasecmp(name, "fortezzaproxy", 13))
			return AUTH_FORTEZZA;
	}
	return AUTH_INVALID;
}


/*
 * Figure out better of two {WWW,Proxy}-Authenticate headers;
 * SimpleMD5 is better than Basic.  Uses the order of AuthType
 * enum values.
 *
 */
MODULE_PRIVATE PRBool
net_IsBetterAuth(char *new_auth, char *old_auth)
{
	if (!old_auth || net_auth_type(new_auth) >= net_auth_type(old_auth))
		return PR_TRUE;
	else
		return PR_FALSE;
}


/*
 * Turns binary data of given lenght into a newly-allocated HEX string.
 *
 *
 */
PRIVATE char *
bin2hex(unsigned char *data, int len)
{
    char *buf = (char *)PR_Malloc(2 * len + 1);
    char *p = buf;

	if(!buf)
		return NULL;

    while (len-- > 0) {
		sprintf(p, "%02x", *data);
		p += 2;
		data++;
    }
    *p = '\0';
    return buf;
}


/*
 * Parse {WWW,Proxy}-Authenticate header parameters into a net_AuthStruct
 * structure.
 *
 */
#define SKIP_WS(p) while((*(p)) && NET_IS_SPACE(*(p))) p++

PRIVATE PRBool
next_params(char **pp, char **name, char **value)
{
    char *q, *p = *pp;

    SKIP_WS(p);
    if (!p || !(*p) || !(q = strchr(p, '=')))
		return FALSE;
    *name = p;
    *q++ = '\0';
    if (*q == '"') {
		*value = q + 1;
		q = strchr(*value, '"');
		if (q)
		  *q++ = '\0';
    }
    else {
		*value = q;
		while (*q && !NET_IS_SPACE(*q)) q++;
		if (*q)
		  *q++ = '\0';
    }

    *pp = q;
    return TRUE;
}

PRIVATE net_AuthStruct *
net_parse_authenticate_line(char *auth, net_AuthStruct *ret)
{
    char *name, *value, *p = auth;

	if (!auth || !*auth)
		return NULL;

	if (!ret)
		ret = PR_NEWZAP(net_AuthStruct);

	if(!ret)
		return NULL;

    SKIP_WS(p);
	ret->auth_type = net_auth_type(p);
    while (*p && !NET_IS_SPACE(*p)) p++;

    while (next_params(&p, &name, &value)) {
		if (!PL_strcasecmp(name, "realm"))
		  {
			  StrAllocCopy(ret->realm, value);
		  }
#ifdef SIMPLE_MD5
		else if (!PL_strcasecmp(name, "domain"))
		  {
			  StrAllocCopy(ret->domain, value);
		  }
		else if (!PL_strcasecmp(name, "nonce"))
		  {
			  StrAllocCopy(ret->nonce, value);
		  }
		else if (!PL_strcasecmp(name, "opaque"))
		  {
			  StrAllocCopy(ret->opaque, value);
		  }
		else if (!PL_strcasecmp(name, "oldnonce"))
		  {
			  ret->oldNonce = (!PL_strcasecmp(value, "TRUE")) ? TRUE : FALSE;
		  }
#endif /* SIMPLE_MD5 */
		/* Some FORTEZZA checks */
		else if (!PL_strcasecmp(name, "challenge"))
		  {
			  StrAllocCopy(ret->challenge, value);
		  }
		else if (!PL_strcasecmp(name, "oldchallenge"))
		  {
			  ret->oldChallenge = (!PL_strcasecmp(value, "TRUE")) ? TRUE : FALSE;
		  }
		/* End FORTEZZA checks  */
    }

#ifdef SIMPLE_MD5
	if (!ret->oldNonce)
		ret->oldNonce_retries = 0;
#endif /* SIMPLE_MD5 */
	/*Another FORTEZZA addition*/
	if (!ret->oldChallenge)
		ret->oldChallenge_retries = 0;
	/*End FORTEZZA  addition   */
    return ret;
}


#ifdef SIMPLE_MD5
/* ---------- New stuff: SimpleMD5 Authentication for proxies -------------- */

PRIVATE void do_md5(unsigned char *stuff, unsigned char digest[16])
{
    SECStatus rv;

    rv = HASH_HashBuf(HASH_AlgMD5, digest, stuff, strlen((char *)stuff));

    return;
}


/*
 * Generate a response for a SimpleMD5 challenge.
 *
 *	HEX( MD5("challenge password method url"))
 *
 */
char *net_generate_md5_challenge_response(char *challenge,
										  char *password,
										  int   method,
										  char *url)
{
    unsigned char digest[16];
    unsigned char *cookie =
	  (unsigned char *)PR_Malloc(strlen(challenge) + strlen(password) +
								strlen(url) + 10);

	if(!cookie)
		return NULL;

    sprintf((char *)cookie, "%s %s %s %s", challenge, password,
			(method==URL_POST_METHOD ? "POST" :
			 method==URL_HEAD_METHOD ? "HEAD" : "GET"),
			url);
    do_md5(cookie, digest);
    return bin2hex(digest, 16);
}


#define SIMPLEMD5_AUTHORIZATION_FMT "SimpleMD5\
 username=\"%s\",\
 realm=\"%s\",\
 nonce=\"%s\",\
 response=\"%s\",\
 opaque=\"%s\""

#endif /* SIMPLE_MD5 */

PRIVATE
char *net_generate_auth_string(URL_Struct *url_s,
							   net_AuthStruct *auth_s)
{
	if (!auth_s)
		return NULL;

	switch (auth_s->auth_type) {

	  case AUTH_INVALID:
		break;

	  case AUTH_BASIC:
		if (!auth_s->auth_string) {
			int len;
			char *u_pass_string = NULL;

			StrAllocCopy(u_pass_string, auth_s->username);
			StrAllocCat (u_pass_string, ":");
			StrAllocCat (u_pass_string, auth_s->password);

			len = PL_strlen(u_pass_string);
			if (!(auth_s->auth_string = (char*) PR_Malloc((((len+1)*4)/3)+20)))
			  {
				  return NULL;
			  }

			PL_strcpy(auth_s->auth_string, "Basic ");
			NET_UUEncode((unsigned char *)u_pass_string,
						 (unsigned char *)&auth_s->auth_string[6],
						 len);

			PR_Free(u_pass_string);
		}
		break;

#ifdef SIMPLE_MD5
	  case AUTH_SIMPLEMD5:
	    if (auth_s->username && auth_s->password &&
			auth_s->nonce    && auth_s->opaque   &&
			url_s            && url_s->address)
		  {
			  char *resp;

			  PR_FREEIF(auth_s->auth_string);
			  auth_s->auth_string = NULL;

			  if ((resp = net_generate_md5_challenge_response(auth_s->nonce,
															  auth_s->password,
															  url_s->method,
															  url_s->address)))
				{
					if ((auth_s->auth_string =
						 (char *)PR_Malloc(PL_strlen(auth_s->username) +
										  PL_strlen(auth_s->realm)    +
										  PL_strlen(auth_s->nonce)    +
										  PL_strlen(resp)             +
										  PL_strlen(auth_s->opaque)   +
										  100)))
					  {
						  sprintf(auth_s->auth_string,
								  SIMPLEMD5_AUTHORIZATION_FMT,
								  auth_s->username,
								  auth_s->realm,
								  auth_s->nonce,
								  resp,
								  auth_s->opaque);
					  }
					PR_Free(resp);
				}
		  }
		break;
#endif /* SIMPLE_MD5 */
		/* Handle the FORTEZZA case        */
	        case AUTH_FORTEZZA:
	       		HG26251
		  break;
		/* Done Handling the FORTEZZA case */
    }

	return auth_s->auth_string;
}


/* --------------- New stuff: client-proxy authentication --------------- */

PRIVATE net_AuthStruct *
net_CheckForProxyAuth(char * proxy_addr)
{
	XP_List * lp = net_proxy_auth_list;
	net_AuthStruct * s;

	while ((s = (net_AuthStruct *)XP_ListNextObject(lp)) != NULL)
      {
		  if (!PL_strcasecmp(s->proxy_addr, proxy_addr))
			  return s;
	  }

    return NULL;
}


/*
 * returns a proxy authorization string if one is required, otherwise
 * returns NULL
 */
PUBLIC char *
NET_BuildProxyAuthString(MWContext * context,
						 URL_Struct * url_s,
						 char * proxy_addr)
{
	net_AuthStruct * auth_s = net_CheckForProxyAuth(proxy_addr);

	return auth_s ? net_generate_auth_string(url_s, auth_s) : NULL;
}


/*
 * Returns FALSE if the user wishes to cancel proxy authorization
 * and TRUE if the user wants to continue with a new authorization
 * string.
 */
#define INVALID_AUTH_HEADER XP_GetString( XP_PROXY_REQUIRES_UNSUPPORTED_AUTH_SCHEME )

#define LOOPING_OLD_NONCES XP_GetString( XP_LOOPING_OLD_NONCES )

PUBLIC PRBool
NET_AskForProxyAuth(MWContext * context,
					char *   proxy_addr,
					char *   pauth_params,
					PRBool  already_sent_auth,
                    void * closure)
{
	net_AuthStruct * prev;
	PRBool new_entry = FALSE;
	char * username = NULL;
	char * password = NULL;
	char * buf;
	int32  len=0;

	TRACEMSG(("Entering NET_AskForProxyAuth"));

	if (!proxy_addr || !*proxy_addr || !pauth_params || !*pauth_params)
		return FALSE;

	prev = net_CheckForProxyAuth(proxy_addr);
    if (prev) {
		new_entry = FALSE;
		net_parse_authenticate_line(pauth_params, prev);
	}
	else {
		new_entry = TRUE;
		if (!(prev = net_parse_authenticate_line(pauth_params, NULL)))
		  {
			  FE_Alert(context, INVALID_AUTH_HEADER);
			  return FALSE;
		  }
		StrAllocCopy(prev->proxy_addr, proxy_addr);
	}

	if (!prev->realm || !*prev->realm)
		StrAllocCopy(prev->realm, XP_GetString( XP_UNIDENTIFIED_PROXY_SERVER ) );

    if (!new_entry) {
		if (!already_sent_auth)
		  {
			  /* somehow the mapping changed since the time we sent
			   * the authorization.
			   * This happens sometimes because of the parrallel
			   * nature of the requests.
			   * In this case we want to just retry the connection
			   * since it will probably succeed now.
			   */
			  return TRUE;
		  }
#ifdef SIMPLE_MD5
		else if (prev->oldNonce && prev->oldNonce_retries++ < 3)
		  {
			  /*
			   * We already sent the authorization string and the
			   * nonce was expired -- auto-retry.
			   */
			  if (!FE_Confirm(context, LOOPING_OLD_NONCES))
				  return FALSE;
		  }
#endif /* SIMPLE_MD5 */
		/* Do the good old FORTEZZA  stuff */
		else if (prev->oldChallenge && (prev->oldChallenge_retries++ > 3)) 
		  {
			  /*
			   * We already sent the authorization string and the
			   * nonce was expired -- auto-retry.
			   */
			  if (!FE_Confirm(context, XP_GetString(XP_CONFIRM_PROXYAUTHOR_FAIL)))
				  return FALSE;
		  }
		else if (prev->auth_type != AUTH_FORTEZZA)
		  {
			  /*
			   * We already sent the authorization string and it failed.
			   */
			  if (!FE_Confirm(context, XP_GetString(XP_CONFIRM_PROXYAUTHOR_FAIL)))
				  return FALSE;
		  }
	}

    
    if (prev->auth_type == AUTH_FORTEZZA) {
		SECStatus rv;
		rv = HG26252
		if ( rv != SECSuccess ) {
			return(FALSE);
		}
    } else
    {
	username = prev->username;
	password = prev->password;

	len = PL_strlen(prev->realm) + PL_strlen(proxy_addr) + 50;
	buf = (char*)PR_Malloc(len*sizeof(char));
	
	if(buf)
	  {
		PR_snprintf(buf, len*sizeof(char), XP_GetString( XP_PROXY_AUTH_REQUIRED_FOR ), prev->realm, proxy_addr);

		NET_Progress(context, XP_GetString( XP_CONNECT_PLEASE_ENTER_PASSWORD_FOR_PROXY ) );
#if 1
#if defined(SingleSignon)
		/* prefill prompt with previous username/passwords if any */
		len = SI_PromptUsernameAndPassword
		    (context, buf, &username, &password, proxy_addr);
#else
		len = FE_PromptUsernameAndPassword
		    (context, buf, &username, &password);
#endif
#else
        /* the new multi-thread case -- we return with out user/pass */
		len = stub_PromptUsernameAndPassword
		    (context, buf, &username, &password, closure);
#endif
		PR_Free(buf);
	  }
	else
	  {
		len = 0;
	  }

	if (!len)
	  {
          TRACEMSG(("Waiting for user auth"));
          return FALSE;
#if notused
		  TRACEMSG(("User canceled login!!!"));
		  return FALSE;
#endif /* notused */
	  }
	else if (!username || !password)
	  {
		  return FALSE;
	  }

	PR_FREEIF(prev->auth_string);
	prev->auth_string = NULL;		/* Generate a new one */
	PR_FREEIF(prev->username);
	prev->username = username;
	PR_FREEIF(prev->password);
	prev->password = password;
    }

	if (new_entry)
	  {
		  if (!net_proxy_auth_list)
			{
				net_proxy_auth_list = XP_ListNew();
				if (!net_proxy_auth_list)
				  {
					  return TRUE;
				  }
			}
		  XP_ListAddObjectToEnd(net_proxy_auth_list, prev);
	  }

	return TRUE;
}


#if defined(CookieManagement)

#include "htmldlgs.h"
extern int XP_CERT_PAGE_STRINGS;
extern int SA_REMOVE_BUTTON_LABEL;

/* return TRUE if "number" is in sequence of comma-separated numbers */
Bool net_InSequence(char* sequence, int number) {
    char* ptr;
    char* endptr;
    char* undo = NULL;
    Bool retval = FALSE;
    int i;

    /* not necessary -- routine will work even with null sequence */
    if (!*sequence) {
	return FALSE;
    }

    for (ptr = sequence ; ptr ; ptr = endptr) {

	/* get to next comma */
        endptr = PL_strchr(ptr, ',');

        /* if comma found, process it */
        if (endptr) {

            /* restore last comma-to-null back to comma */
            if (undo) {
                *undo = ',';
            }

            /* set the comma to a null */
            undo = endptr;
            *endptr++ = '\0';

            /* compare the number before the comma with "number" */
            if (*ptr) {
                i = atoi(ptr);
                if (i == number) {

                    /* "number" was in the sequence so return TRUE */
                    retval = TRUE;
                    break;
                }
            }
        }
    }

    /* restore last comma-to-null back to comma */
    if (undo) {
        *undo = ',';
    }
    return retval;
}

PR_STATIC_CALLBACK(PRBool)
net_AboutCookiesDialogDone(XPDialogState* state, char** argv, int argc,
						unsigned int button)
{
    XP_List *list;
    net_CookieStruct *cookie;
    net_CookiePermissionStruct *cookiePermission;
    char *buttonName;
    int cookieNumber;
    net_CookieStruct *cookieToDelete = 0;
    net_CookiePermissionStruct *cookiePermissionToDelete = 0;

    char* gone;

    buttonName = XP_FindValueInArgs("button", argv, argc);
    if (button != XP_DIALOG_OK_BUTTON) {
	/* OK button not pressed (must be cancel button that was pressed) */
	return PR_FALSE;
    }

    /* OK was pressed, do the deletions */

    /* get the comma-separated sequence of cookies to be deleted */
    gone = XP_FindValueInArgs("goneC", argv, argc);
    PR_ASSERT(gone);
    if (!gone) {
	return PR_FALSE;
    }

    /*
     * walk through the cookie list, deleting the designated cookies
     * Note: we can't delete cookie while "list" is pointing to it because
     * that would destroy "list".  So we do a lazy deletion
     */
    net_lock_cookie_list();
    list = net_cookie_list;
    cookieNumber = 0;
    while ( (cookie=(net_CookieStruct *) XP_ListNextObject(list)) ) {
	if (net_InSequence(gone, cookieNumber)) {
	    if (cookieToDelete) {
		net_FreeCookie(cookieToDelete);
	    }
	    cookieToDelete = cookie;
	}
	cookieNumber++;
    }

    if (cookieToDelete) {
	net_FreeCookie(cookieToDelete);
	cookies_changed = TRUE;
	NET_SaveCookies(NULL);
    }
    net_unlock_cookie_list();

    /* get the comma-separated sequence of permissions to be deleted */
    gone = XP_FindValueInArgs("goneP", argv, argc);
    PR_ASSERT(gone);
    if (!gone) {
	return PR_FALSE;
    }

    /*
     * walk through the cookie permission list, deleting the designated permissions
     * Note: we can't delete permissions while "list" is pointing to it because
     * that would destroy "list".  So we do a lazy deletion
     */
    net_lock_cookie_permission_list();
    list = net_cookie_permission_list;
    cookieNumber = 0;
    while ( (cookiePermission=(net_CookiePermissionStruct *) XP_ListNextObject(list)) ) {
	if (net_InSequence(gone, cookieNumber)) {
	    if (cookiePermissionToDelete) {
		net_FreeCookiePermission(cookiePermissionToDelete, TRUE);
	    }
	    cookiePermissionToDelete = cookiePermission;
	}
	cookieNumber++;
    }

    if (cookiePermissionToDelete) {
	net_FreeCookiePermission(cookiePermissionToDelete, TRUE);
	cookie_permissions_changed = TRUE;
	net_SaveCookiePermissions(NULL);
    }

    net_unlock_cookie_permission_list();
    return PR_FALSE;
}

PRIVATE Bool
CookieCompare (net_CookieStruct * cookie1, net_CookieStruct * cookie2) {
    char * host1 = cookie1->host;
    char * host2 = cookie2->host;

    /* get rid of leading period on host name, if any */
    if (*host1 == '.') {
	host1++;
    }
    if (*host2 == '.') {
	host2++;
    }

    /* make decision based on host name if they are unequal */
    if (PL_strcmp (host1, host2) < 0) {
	return -1;
    }
    if (PL_strcmp (host1, host2) > 0) {
	return 1;
    }

    /* if host names are equal, make decision based on cookie name */
    return (PL_strcmp (cookie1->name, cookie2->name));
}

/*
 * find the next cookie that is alphabetically after the specified cookie
 *    ordering is based on hostname and the cookie name
 *    if specified cookie is NULL, find the first cookie alphabetically
 */
PRIVATE net_CookieStruct *
NextCookieAfter(net_CookieStruct * cookie, int * cookieNum) {
    XP_List *cookie_list=net_cookie_list;
    net_CookieStruct *cookie_ptr;
    net_CookieStruct *lowestCookie = NULL;
    int localCookieNum = 0;
    int lowestCookieNum;

    while ( (cookie_ptr=(net_CookieStruct *) XP_ListNextObject(cookie_list)) ) {
	if (!cookie || (CookieCompare(cookie_ptr, cookie) > 0)) {
	    if (!lowestCookie ||
		    (CookieCompare(cookie_ptr, lowestCookie) < 0)) {
		lowestCookie = cookie_ptr;
		lowestCookieNum = localCookieNum;
	    }
	}
	localCookieNum++;
    }

    *cookieNum = lowestCookieNum;
    return lowestCookie;
}

typedef struct _CookieViewerDialog CookieViewerDialog;

struct _CookieViewerDialog {
    void *window;
    void *parent_window;
    PRBool dialogUp;
    XPDialogState *state;
};

#ifdef TRUST_LABELS
/* Purpose: given a cookie name look in the list containing the cookperm.txt
 * file and locate a matching trust label.  If a matching trust label
 * is found then format a simple text string that describes the trust label
 * information in a user friendly manner.
 *
 * If a matching label is not found return a string with a single space.
 * The caller must free the string.
 *
 * History:
 *  9/9/98 Paul Chek - switch recipient from a range to a single value 
 */
char *GetTrustLabelString(char *CookieName)
{
    XP_List * list_ptr, *Tptr;
    TrustEntry *TEntry;
    net_CookiePermissionStruct *cookperm;
    char *szTemp = NULL;
    char *szPurpose = NULL;
    char *szRecipient = NULL;
    char *szBy = NULL;
    int i, j;

/*
 * here's how we would like to write the code but it doesn't compile on the mac 
 *
 *   int PurposeIds[] = {MK_ACCESS_TL_PPH0, MK_ACCESS_TL_PPH1,
 *                       MK_ACCESS_TL_PPH2, MK_ACCESS_TL_PPH3,
 *                       MK_ACCESS_TL_PPH4, MK_ACCESS_TL_PPH5};
 *   int RecpIds[] = { MK_ACCESS_TL_RPH0, MK_ACCESS_TL_RPH1, 
 *                     MK_ACCESS_TL_RPH2, MK_ACCESS_TL_RPH3};
*/

#define NumPurposeBits 6 /* number of bits in the recipient value */
    /* array of msg Ids for the purpose values */
    int PurposeIds[NumPurposeBits];

#define NumRecpBits 4 /* number of bits in the recipient value */
    /* the list of msg Ids for the recipient value. */
    int RecpIds[NumRecpBits];

#if NumPurposeBits > NumRecpBits
#define NumStrs  NumPurposeBits
#else
#define NumStrs  NumRecpBits
#endif
    /* this array holds strings used in formatting the message */
    char *szTempStrs[ NumStrs ];

    PurposeIds[0] = MK_ACCESS_TL_PPH0;
    PurposeIds[1] = MK_ACCESS_TL_PPH1;
    PurposeIds[2] = MK_ACCESS_TL_PPH2;
    PurposeIds[3] = MK_ACCESS_TL_PPH3;
    PurposeIds[4] = MK_ACCESS_TL_PPH4;
    PurposeIds[5] = MK_ACCESS_TL_PPH5;

    RecpIds[0] = MK_ACCESS_TL_RPH0;
    RecpIds[1] = MK_ACCESS_TL_RPH1;
    RecpIds[2] = MK_ACCESS_TL_RPH2;
    RecpIds[3] = MK_ACCESS_TL_RPH3;



    if( CookieName ) {
        /* look thru the cookie permission list for a matching trust label */
        net_lock_cookie_permission_list();
        list_ptr = net_cookie_permission_list;
        while( szTemp == NULL &&
                (cookperm = (net_CookiePermissionStruct *) XP_ListNextObject(list_ptr))!=0) {
            /* look thru the trust list for this cookie */
            if (cookperm->TrustList ) {
                Tptr = cookperm->TrustList;
                while( (TEntry = (TrustEntry *)XP_ListNextObject(Tptr))) {
                    if( TEntry->CookieName
                            && !PL_strcmp(CookieName, TEntry->CookieName)) {
                        /* this permission entry matches the given cookie */
                        szTemp = (char*)PR_Malloc(BUFLEN);
                        break;
                    }
                }
            }
        }
        net_unlock_cookie_permission_list();

        /* was a match found?? */
        if (szTemp) {
            /* yes convert the ratings info into user strings */

            for( j=0; j<NumStrs; j++) {
                szTempStrs[j] = NULL;
            }

            /*
             * count the number of purpose bits set AND
             *  make a list of the phrases to use
             */
            for (j=0,i=0; i<NumPurposeBits; i++) {
               if ((TEntry->purpose>>i) & 1) {
                    /* this bit is set get the associated phrase */
                    szTempStrs[j++] = PL_strdup( XP_GetString( PurposeIds[i] ) );
                }
            }
            /*
             * build the purpose string 
             * For translation purposes I am using strings 
             * of the form "This information is used for %1$s, %2$s and %3$s."
             * and inserting the additional phrases in the formating.*/
            switch ( j ) {
                case 1:
                    PR_snprintf(szTemp, BUFLEN,
                        XP_GetString( MK_ACCESS_TL_PUR1 ),
                        szTempStrs[0]
                    );
                    break;
                case 2:
                    PR_snprintf(szTemp, BUFLEN,
                        XP_GetString( MK_ACCESS_TL_PUR2 ),
                        szTempStrs[0],
                        szTempStrs[1]
                    );
                    break;

                case 3:
                    PR_snprintf(szTemp, BUFLEN,
                        XP_GetString( MK_ACCESS_TL_PUR3 ),
                        szTempStrs[0],
                        szTempStrs[1],
                        szTempStrs[2]
                    );
                    break;

                case 4:
                    PR_snprintf(szTemp, BUFLEN,
                        XP_GetString( MK_ACCESS_TL_PUR4 ),
                        szTempStrs[0],
                        szTempStrs[1],
                        szTempStrs[2],
                        szTempStrs[3]
                    );
                    break;

                case 5:
                    PR_snprintf(szTemp, BUFLEN,
                        XP_GetString( MK_ACCESS_TL_PUR5 ),
                        szTempStrs[0],
                        szTempStrs[1],
                        szTempStrs[2],
                        szTempStrs[3],
                        szTempStrs[4]
                    );
                    break;

                case 6:
                    PR_snprintf(szTemp, BUFLEN,
                        XP_GetString( MK_ACCESS_TL_PUR6 ),
                        szTempStrs[0],
                        szTempStrs[1],
                        szTempStrs[2],
                        szTempStrs[3],
                        szTempStrs[4],
                        szTempStrs[5]
                    );
                    break;

            } /* switch ( j ) */
            /* I did things this way to avoid buffer overflow problems */
            szPurpose = PL_strdup( szTemp );
            /* free the temporary strings */
            for( j=0; j<NumStrs; j++) {
                PR_FREEIF(szTempStrs[j]);
                szTempStrs[j] = NULL;
            }

            /* second, build the recipient phrase list.  The recipients value as
			 * ossolated between allowing a range of values (0:2) and a single
			 * value, thats the reason for the ifdef'd code. */
#ifndef RECIPIENT_RANGE
			szTempStrs[0] = PL_strdup( XP_GetString( RecpIds[ TEntry->recipient] ) );
			PR_snprintf(szTemp, BUFLEN,
				XP_GetString( MK_ACCESS_TL_RECP1 ),
				szTempStrs[0]
			);
			PR_FREEIF(szTempStrs[0]);
            szRecipient = PL_strdup( szTemp );
#else
			/* is just a single bit set and is that bit 0 ?*/
			/* if  RECIPIENT_RANGE is defined you will need to 
			 * include these lines in allxpstr.h
			 *	ResDef(MK_ACCESS_TL_RECP2, (TRUST_LABEL_BASE + 17),
			 *	"It is %1$s and %2$s." )
			 *	ResDef(MK_ACCESS_TL_RECP3, (TRUST_LABEL_BASE + 18),
			 *	"It is %1$s, %2$s and %3$s." )
			 */
            if ( TEntry->recipient == 1 ) {
                szTempStrs[0] = PL_strdup( XP_GetString( MK_ACCESS_TL_RPH0 ) );
                PR_snprintf(szTemp, BUFLEN,
                    XP_GetString( MK_ACCESS_TL_RECP1 ),
                    szTempStrs[0]
                );
            } else {
                /*
                 * if bits other than bit 0 are set then dont 
                 * output the "used only by this site" text
                 *
                 * count the number of purpose bits set AND
                 *  make a list of the phrases to use
                 */
                for (j=0,i=1; i<NumRecpBits; i++) {
                    if ((TEntry->recipient>>i) & 1) {
                        /* this bit is set get the associated phrase */
                        szTempStrs[j++] = PL_strdup( XP_GetString( RecpIds[i] ) );
                    }
                }
                /*
                 * format the recipient string according to the 
                 * number of bits set
                 */
                switch ( j ) {
                    case 1:
                        PR_snprintf(szTemp, BUFLEN,
                            XP_GetString( MK_ACCESS_TL_RECP1 ),
                            szTempStrs[0]
                        );
                        break;

                    case 2:
                        PR_snprintf(szTemp, BUFLEN,
                            XP_GetString( MK_ACCESS_TL_RECP2 ),
                            szTempStrs[0],
                            szTempStrs[1]
                        );
                        break;

                    case 3:
                        PR_snprintf(szTemp, BUFLEN,
                            XP_GetString( MK_ACCESS_TL_RECP3 ),
                            szTempStrs[0],
                            szTempStrs[1],
                            szTempStrs[2]
                        );
                        break;

                } /* switch ( j ) */
            }
            /* I did things this way to avoid buffer overflow problems */
            szRecipient = PL_strdup( szTemp );
            /* free the temporary strings */
            for( j=0; j<NumStrs; j++) {
                PR_FREEIF(szTempStrs[j]);
                szTempStrs[j] = NULL;
            }
#endif

            /* construct the by string */
            if (TEntry->by) {
                PR_snprintf(szTemp, BUFLEN, XP_GetString(MK_ACCESS_TL_BY), TEntry->by);
                szBy = PL_strdup( szTemp );
            } else {
                szBy = PL_strdup( " " );
            }

            /*
             * construct the entire trust label statement.
             * WATCHOUT: you can only execute four XP_GetString calls
             * in this format string.
             */
            PR_snprintf(szTemp, BUFLEN,
                "%s " /* identifiable text */
                "%s " /* purpose string */
                "%s " /* receipent */
                "%s",  /* by string */
                (TEntry->bIdentifiable
                    ? XP_GetString(MK_ACCESS_TL_ID1)
                    : XP_GetString(MK_ACCESS_TL_ID0)),
                szPurpose, szRecipient, szBy);
        }
    }
    PR_FREEIF(szPurpose);
    PR_FREEIF(szRecipient);
    PR_FREEIF(szBy);

    return szTemp;
}


#define STR_TRUST_LABEL "(TrustLabel"
/* these structures describe the layout of the trust label information in 
 *  the cookperm.txt.  The structures are used for both reading and writing the 
 *  file.
 */
enum {TYPE_QUOTED_STRING, TYPE_INT};
typedef struct {
    char *szMatchStr; /* the string to look for coming in */
    int ValueType; /* what kind of thing the value is */
    int DataOffset; /* where in the TrustEntry struct to store the value */                    
} ParseItem;

static ParseItem ParseData[] = {
    {"cookie", TYPE_QUOTED_STRING, offsetof(TrustEntry, CookieName)},
    {"purpose", TYPE_INT, offsetof(TrustEntry, purpose)},
    {"id", TYPE_INT, offsetof(TrustEntry, bIdentifiable)},
    {"recipient", TYPE_INT, offsetof(TrustEntry, recipient)},
    {"by", TYPE_QUOTED_STRING, offsetof(TrustEntry, by)},
    /* the timestamp is the date and time when the cookie was set.   
     * It is in the form "14 Apr 89 03:20 GMT".   Use PR_ParseTimeString
     * to parse it into a PR_Exploded time.
     */
    {"timestamp", TYPE_QUOTED_STRING, offsetof(TrustEntry, CookieSet)}
};
#define NUM_ITEMS  sizeof(ParseData) / sizeof( ParseItem)

/******************************************************
 * Purpose:  parse all the trust label string from a line in
 * cookperm.txt and store the result in a net_CookiePermissionStruct.
 *
 * This is a simple table driven parser.  
 ******************************************************/
PRIVATE
void ParseTrustLabelInfo
    (char *Buf, net_CookiePermissionStruct * cookie_permission)
{
    char *pToken, *p, *pEnd, *pSub;
    int i, k, len;
    Bool Status = FALSE;
    char *pChar, *szTemp;
    char *pCopy = NULL;
    TrustEntry *TEntry;

    /*
     * Look for the trust label information which is in the form
     *   "(TrustLabel" - must be the first token
     *     "cookie:"<cookie name>
     *     "purpose:"<purpose value>
     *     "id:"<"0"|"1">
     *     "recipient:"<recipient value>
     *     "by:"<by value>
     *     "timestamp:"<timestamp>
     *    ")"								- must be the last token
     * purpose value == a hex value corresponding to the purpose value to set
     * recipient value == a hex value corresponding to the recipient value to set
     * by value == a text string enclosed in double quotes
     * timestamp == time when the cookie was set: format is quoted string in any of the 
     * formats that PR_Time will accept.
     * cookie name == the name of the cookie enclosed in quotes
     *
     * The middle tokens can be in any order.
     */

    /*
     * Do I have a well formed trust label string ??
     * - can I find a beginning and an end??
     */
    p = Buf;
    while ((p=strcasestr(p, (char *)STR_TRUST_LABEL)) && (pEnd=strchr(p,')'))) {
        p += PL_strlen(STR_TRUST_LABEL);

        /* Allocate a new trust Entry */
        TEntry = PR_NEW(TrustEntry);
        if (TEntry) {
            /* init the elements */
            TEntry->CookieName = NULL;
            TEntry->purpose = 0;
            TEntry->recipient = 0;
            TEntry->bIdentifiable = FALSE;
            TEntry->by = NULL;
            TEntry->CookieSet = NULL;

            /*
             * isolate this trust label string for parsing, I must make a copy
             * of the incoming string because the tokenizer will add nulls to it.
             * Also allocate one more byte than I really need so that the tokenizer
             * can write an extra Null at the end.
             */
            len = pEnd-p+1;
            pCopy = XP_ALLOC(len + 1);
            if (!pCopy) return;
            XP_MEMCPY(pCopy, p, len-1);
            pCopy[len] = '\0';

            /* get the first token from the copy */
            pToken = XP_STRTOK_R(pCopy, " :", &pSub);
            while (pToken) {
                if (pToken != ")") {
                    for ( i=0; i<NUM_ITEMS; i++ ) {
                        /* Is this token one that we are interested in ?? */
                        if (PL_strcasecmp(pToken, ParseData[i].szMatchStr) == 0) {
                            /* yes - extract its associated value and store it */
                            switch(ParseData[i].ValueType) {
                                case TYPE_QUOTED_STRING:
                                    /*
                                     * since I allow white space INSIDE the
                                     * quoted string, AND I want the next token
                                     * to contain all the stuff inside the
                                     * quoted string; I need to eat all the
                                     * whitespace between the : and the
                                     * next "
                                     */
                                    while(*pSub == ' ' || *pSub == '\t') {
                                        pSub++;
                                    }
                                    /* get next token which should be " */
                                    pToken = XP_STRTOK_R(nil, "\"", &pSub );
                                    if (pToken) {
                                        pChar = (char *)((void *)(TEntry))
                                            + ParseData[i].DataOffset;
                                        szTemp = PL_strdup(pToken);
                                        XP_MEMCPY((void *)pChar,
                                            (void *)&szTemp,
                                            sizeof(char *) );
                                    }
                                    break;
                                case TYPE_INT:
                                    /*
                                     * get the next token which should be some
                                     * number
                                     */
                                    pToken = XP_STRTOK_R(nil, " ", &pSub );
                                    if (pToken) {
                                        pChar = (char *)((void *)(TEntry))
                                            + ParseData[i].DataOffset;
                                        k = XP_ATOI( pToken );
                                        XP_MEMCPY((void *)pChar,
                                            (void *)&k, sizeof(int));
                                    }
                                    break;
                            }
                            break;           /* break out of the for loop */
                        }
                    }
                } else {
                    /* all done */
                    break;
                }
                pToken = XP_STRTOK_R(nil, " :", &pSub );     /* get the next token */
            } /* end while */
            PR_FREEIF( pCopy );

            /* Does the trust list exist in this entry, if not create it */
            if(!cookie_permission->TrustList) {
                cookie_permission->TrustList = XP_ListNew();
                if(!cookie_permission->TrustList) {
                    return;
                }
            }

            /* add the entry to the list in the permission entry */
            XP_ListAddObjectToEnd( cookie_permission->TrustList, TEntry );

        } /* end if (TEntry) */
        /* set up for the next entry */
        p = ++pEnd;
    } /* end while ((p=strcasestr( */
    return;
}

/****************************************************************
* Purpose: Write the trust label information from the permission
*		   list to cookperm.txt
*
* History:
*  Paul Chek - initial creation
****************************************************************/
PRIVATE
void SaveTrustLabelData
	( net_CookiePermissionStruct * cookie_permission, XP_File fp )
{
    XP_List *Tptr;
    TrustEntry *TEntry;
    char *szTemp;
    char *szTemp2;
    char **pChar;
    int *pInt;
    int T2Size = 50;
    int len, i;

    /*
     * do an initial allocation for szTemp2.  Whenever a quoted string 
     * is processed the size of szTemp2 is tested to make sure it is big enough,
     * if it is not it is resized.
     */
    szTemp2 = XP_ALLOC( T2Size );
    if ( fp && cookie_permission && cookie_permission->TrustList && szTemp2 ) {
        /* there is trust label information, write it */
        Tptr = cookie_permission->TrustList;
        while( (TEntry = (TrustEntry *)XP_ListNextObject(Tptr))) {
            /* build the string up one piece at a time, its slower but safer */
            szTemp = NULL;
            StrAllocCopy( szTemp, "  "STR_TRUST_LABEL);
            /* march thru the Parsing table and output the TrustLabel information */
            for ( i=0; i<NUM_ITEMS; i++ ) {
                switch(ParseData[i].ValueType) {
                    case TYPE_QUOTED_STRING:
                        pChar = (char **)(((char *)TEntry)+ ParseData[i].DataOffset);
                        if ( PL_strlen( *pChar ) ) {
                            /* make sure szTemp2 is big enough. */
                            len = PL_strlen( ParseData[i].szMatchStr ) + PL_strlen(*pChar) + 3;
                            if ( T2Size <= len ) {
                                T2Size = len +50;
                                szTemp2 = XP_REALLOC( szTemp2, T2Size );
                            }
                            if ( szTemp2 ) {
                                PR_snprintf( szTemp2, T2Size, " %s:\"%s\"",
                                ParseData[i].szMatchStr, *pChar );
                                StrAllocCat( szTemp, szTemp2 ); 
                            } else {
                               /* error reallocating szTemp2, blast out */
                               PR_FREEIF( szTemp );
                               return;
                            }
                        }
                        break;

                    case TYPE_INT:
                        pInt = (int *)(((char *)TEntry)+ ParseData[i].DataOffset);
                        PR_snprintf( szTemp2, T2Size, " %s:%d",
                            ParseData[i].szMatchStr, *pInt );
                        StrAllocCat( szTemp, szTemp2 ); 
                        break;
                }
            } /* end for */
            StrAllocCat( szTemp, " )" ); /* add the termination paren */

            /* write a single trust label string */
            NET_XP_FileWrite(szTemp, -1, fp);
            PR_FREEIF( szTemp );
        } /* end while( (TEntry =  */ 
        PR_FREEIF( szTemp2 );
    }
}

/****************************************************************
* Purpose:  search for a TrustEntry in the trust list of a cookie 
* permission entry.
*
* History:
*  Paul Chek - initial creation
****************************************************************/
PRIVATE
Bool FindTrustEntry( char *CookieName, XP_List *TList, TrustEntry **TheEntry )
{
    TrustEntry *TEntry;
    XP_List *TPtr;
    Bool Status = FALSE;
    if ( TList && !XP_ListIsEmpty( TList ) ) {
        TPtr = TList;
        while( (TEntry = (TrustEntry *)XP_ListNextObject(TPtr))) {
            if (PL_strcasecmp(CookieName, TEntry->CookieName) == 0) {
                /* the entry was found */
                *TheEntry = TEntry;
                Status = TRUE;
                break;
            }
        }
    }
    return Status;
}

/****************************************************************
* Purpose: given a TrustLabel struct sat up the corresponding TrustEntry
*
* History:
*  Paul Chek - initial creation
****************************************************************/
PRIVATE
void CopyTrustEntry( char *CookieName, TrustEntry *TEntry, TrustLabel *trustlabel )
{
    PRExplodedTime now;
    char line[100];

    if ( TEntry && trustlabel ) {
        PR_FREEIF(TEntry->CookieName);
        PR_FREEIF(TEntry->by);
        PR_FREEIF(TEntry->CookieSet);
        TEntry->CookieName  = PL_strdup( CookieName );
        TEntry->purpose = trustlabel->purpose;
        TEntry->recipient = trustlabel->recipients;
        TEntry->bIdentifiable = trustlabel->ID;
        TEntry->by = PL_strdup( trustlabel->szBy );

        /*
         * format an ASCII timestamp for when the cookie was set.  Use a 
         *format that PR_ParseTimeString supports. "22-AUG-1993 10:59 PM"
         */ 
        PR_ExplodeTime(PR_Now(), PR_LocalTimeParameters, &now);
        PR_FormatTimeUSEnglish(line, 400, "%d-%b-%Y %I:%M %p", &now);
        TEntry->CookieSet  = PL_strdup( line );
	}
}

/****************************************************************
* Purpose: look in the permissions list to see if there is 
* trust label information for the cookie.  If so delete it.
*
* History:
*  Paul Chek - initial creation
****************************************************************/
PRIVATE
void DeleteCookieFromPermissions( char *CookieHost, char *CookieName )
{
    XP_List *TmpList;
    TrustEntry *TEntry;
    net_CookiePermissionStruct * cookie_permission;

    /* lock the permission list */
    TmpList=net_cookie_permission_list;
    net_lock_cookie_permission_list();
    while ( (cookie_permission=(net_CookiePermissionStruct *)XP_ListNextObject(TmpList)) ) {
        /* is this the corect host for the cookie */
        if (PL_strcasecmp(CookieHost, cookie_permission->host) == 0) {
            /* yes - see if there is a trust label entry for the cookie */
            if ( FindTrustEntry( CookieName, cookie_permission->TrustList, &TEntry ) ) {
                /* yes there is - delete it */
                XP_ListRemoveObject( cookie_permission->TrustList, TEntry);
                /* free the strings in the entry */
                PR_FREEIF(TEntry->CookieName);
                PR_FREEIF(TEntry->by);
                PR_FREEIF(TEntry->CookieSet);
            }
            /* dont need to look in the list anymore cause there is only one entry per host */
            break;			
        }
    }
    net_unlock_cookie_permission_list();

}

/****************************************************************
* Purpose: given a TrustLabel struct containing the trust label
*			info that matches a particular cookie add the trust
*			label info to the cookie permission list
*
* History:
*  Paul Chek - initial creation
****************************************************************/
PRIVATE
void AddTrustLabelInfo( char *CookieName, TrustLabel *trustlabel )
{
    XP_List *TmpList;
    TrustEntry *TEntry;
	net_CookiePermissionStruct * cookie_permission;
	/* lock the permission list */
    TmpList=net_cookie_permission_list;
    net_lock_cookie_permission_list();
    while ( (cookie_permission=(net_CookiePermissionStruct *)XP_ListNextObject(TmpList)) ) {
		/* is this the corect host for the cookie */
		if (PL_strcasecmp(trustlabel->domainName, cookie_permission->host) == 0) {
			/* OK - we have found the entry for this host.  Look thru the trust list 
			   to see if we have an existing entry for this cookie.  If so replace it.
			   If not add a new entry
			*/
			if ( FindTrustEntry( CookieName, cookie_permission->TrustList, &TEntry ) ) {
				/* the entry exists modify it */
				CopyTrustEntry( CookieName, TEntry, trustlabel );
			} else {
				/* the entry does NOT exist - add a new one */
				TEntry = PR_NEWZAP( TrustEntry ); 
				if ( TEntry ) {
					/* yes - build the entry for the cookie */
					CopyTrustEntry( CookieName, TEntry, trustlabel );
					/* Does the trust list exist in this entry, if not create it */
					if(!cookie_permission->TrustList) {
						cookie_permission->TrustList = XP_ListNew();
						if(!cookie_permission->TrustList) {
							return;
						}
					}

					/* add it to the list */
					XP_ListAddObjectToEnd(cookie_permission->TrustList, TEntry);
					break;		  /* all done */	
				}
			}
		}
	}
    net_unlock_cookie_permission_list();
}

#endif

/*
 * return a string that has each " of the argument sting
 * replaced with \" so it can be used inside a quoted string
 */
PRIVATE char*
net_FixQuoted(char* s) {
    char * result;
    int count = PL_strlen(s);
    char *quote = s;
    unsigned int i, j;
    while (quote = PL_strchr(quote, '"')) {
        count = count++;
        quote++;
    }
    result = XP_ALLOC(count + 1);
    for (i=0, j=0; i<PL_strlen(s); i++) {
        if (s[i] == '"') {
            result[i+(j++)] = '\\';
        }
        result[i+j] = s[i];
    }
    result[i+j] = '\0';
    return result;
}

#ifdef XP_MAC
/* pinkerton - if we don't do this, it won't compile because it runs out of registers */
#pragma global_optimizer on
#endif

PRIVATE void
net_DisplayCookieInfoAsHTML(MWContext *context, char* host)
{
    char *buffer = (char*)PR_Malloc(BUFLEN);
    char *buffer2 = 0;
    int g = 0, count, cookieNum;
    XP_List *cookie_list;
    XP_List *cookie_permission_list;
    net_CookieStruct *cookie;
    net_CookiePermissionStruct *cookperm;
    CookieViewerDialog *dlg;
    int i;
    char * view_sites = NULL;
    char * view_cookies = NULL;
    char * heading = NULL;
#ifdef TRUST_LABELS
    char * szTrustLabel = NULL;
#endif

    static XPDialogInfo dialogInfo = {
	0,
	net_AboutCookiesDialogDone,
	600,
	400
    };

    XPDialogStrings* strings;
    strings = XP_GetDialogStrings(XP_CERT_PAGE_STRINGS);
    if (!strings) {
	return;
    }
    StrAllocCopy(buffer2, "");
    StrAllocCopy (view_cookies, XP_GetString(MK_ACCESS_VIEW_COOKIES));
    StrAllocCopy (view_sites, XP_GetString(MK_ACCESS_VIEW_SITES));

    /* generate initial section of html file */
    g += PR_snprintf(buffer+g, BUFLEN-g,
"<HTML>\n"
"<HEAD>\n"
"  <TITLE>Cookies</TITLE>\n"
"  <SCRIPT>\n"
"    index_frame = 0;\n"
"    title_frame = 1;\n"
"    spacer1_frame = 2;\n"
"    list_frame = 3;\n"
"    spacer2_frame = 4;\n"
"    prop_frame = 5;\n"
"    spacer3_frame = 6;\n"
"    button_frame = 7;\n"
"\n"
"    var cookie_mode;\n"
"    var goneC;\n"
"    var goneP;\n"
"    deleted_cookies = new Array (0"
	);
    FLUSH_BUFFER


    /* fill in initial 0's for deleted_cookies and deleted_sites arrays */
    count = XP_ListCount(net_cookie_list);
    for (i=1; i<count; i++) {
	g += PR_snprintf(buffer+g, BUFLEN-g,
",0"
	    );
	if ((i%50) == 0) {
	    g += PR_snprintf(buffer+g, BUFLEN-g,
"\n      "
	    );
	}
    }
    g += PR_snprintf(buffer+g, BUFLEN-g,
");\n"
"    deleted_sites = new Array (0"
	);
    count = XP_ListCount(net_cookie_permission_list);
    for (i=1; i<count; i++) {
	g += PR_snprintf(buffer+g, BUFLEN-g,
",0"
	    );
	if ((i%50) == 0) {
	    g += PR_snprintf(buffer+g, BUFLEN-g,
"\n      "
	    );
	}
    }
    FLUSH_BUFFER

    g += PR_snprintf(buffer+g, BUFLEN-g,
");\n"
"\n"
"    function DeleteItemSelected() {\n"
"      if (cookie_mode) {\n"
"        DeleteCookieSelected();\n"
"      } else {\n"
"        DeleteSiteSelected();\n"
"      }\n"
"    }\n"
"\n"
"    function DeleteCookieSelected() {\n"
"      selname = top.frames[list_frame].document.fSelectCookie.selname;\n"
"      goneC = top.frames[button_frame].document.buttons.goneC;\n"
"      var p;\n"
"      var i;\n"
"      for (i=selname.options.length; i>0; i--) {\n"
"        if (selname.options[i-1].selected) {\n"
"          selname.options[i-1].selected = 0;\n"
"          goneC.value = goneC.value + selname.options[i-1].value + \",\";\n"
"          deleted_cookies[selname.options[i-1].value] = 1;\n"
"          for (j=i ; j<selname.options.length ; j++) {\n"
"            selname.options[j-1] = selname.options[j];\n"
"          }\n"
"          selname.options[selname.options.length-1] = null;\n"
"        }\n"
"      }\n"
"      top.frames[prop_frame].document.open();\n"
"      top.frames[prop_frame].document.close();\n"
"    }\n"
"\n"
"    function DeleteSiteSelected() {\n"
"      selname = top.frames[list_frame].document.fSelectSite.selname;\n"
"      goneP = top.frames[button_frame].document.buttons.goneP;\n"
"      var p;\n"
"      var i;\n"
"      for (i=selname.options.length; i>0; i--) {\n"
"        if (selname.options[i-1].selected) {\n"
"          selname.options[i-1].selected = 0;\n"
"          goneP.value = goneP.value + selname.options[i-1].value + \",\";\n"
"          deleted_sites[selname.options[i-1].value] = 1;\n"
"          for (j=i; j < selname.options.length; j++) {\n"
"            selname.options[j-1] = selname.options[j];\n"
"          }\n"
"          selname.options[selname.options.length-1] = null;\n"
"        }\n"
"      }\n"
"      top.frames[prop_frame].document.open();\n"
"      top.frames[prop_frame].document.close();\n"
"    }\n"
"\n"
"    function ViewCookieSelected(selCookie) {\n"
"      selname = top.frames[list_frame].document.fSelectCookie.selname;\n"
"      top.frames[prop_frame].document.open();\n"
"      switch (selname.options[selCookie].value) {\n"
	);
    FLUSH_BUFFER

    /* Get rid of any expired cookies now so user doesn't
     * think/see that we're keeping cookies in memory.
     */
    net_lock_cookie_list();
    net_remove_expired_cookies();

    /* Generate the html for the cookie properties (alphabetical order) */
    cookie_list=net_cookie_list;
    cookie = NULL;
    while (cookie = NextCookieAfter(cookie, &cookieNum)) {
	char * name = NULL;
	char * value = NULL;
	char * domain_or_host = NULL;
	char * path = NULL;
	char * secure = NULL;
	char * yes_or_no = NULL;
	char * expires = NULL;
	char * expireDate = NULL;

	char *fixed_name = net_FixQuoted(cookie->name);
	char *fixed_value = net_FixQuoted(cookie->cookie);
	char *fixed_domain_or_host = net_FixQuoted(cookie->host);
	char *fixed_path = net_FixQuoted(cookie->path);

	StrAllocCopy (name, XP_GetString(MK_ACCESS_NAME));
	StrAllocCopy (value, XP_GetString(MK_ACCESS_VALUE));
	StrAllocCopy (domain_or_host,
	    cookie->is_domain
		? XP_GetString(MK_ACCESS_DOMAIN)
		: XP_GetString(MK_ACCESS_HOST));
	StrAllocCopy (path, XP_GetString(MK_ACCESS_PATH));
	StrAllocCopy (secure, XP_GetString(MK_ACCESS_SECURE));
	StrAllocCopy (yes_or_no,
	    HG78111
		? XP_GetString(MK_ACCESS_YES)
		: XP_GetString(MK_ACCESS_NO));
	StrAllocCopy (expires, XP_GetString(MK_ACCESS_EXPIRES));
	StrAllocCopy (expireDate,
	    cookie->expires
		? ctime(&(cookie->expires))
		: XP_GetString(MK_ACCESS_END_OF_SESSION));

	if (!expireDate) {
            StrAllocCopy (expireDate, "");
        } else if (expireDate[PL_strlen(expireDate)-1] == '\n') {
            expireDate[PL_strlen(expireDate)-1] = ' ';
	}

	g += PR_snprintf(buffer+g, BUFLEN-g,
"       case %d:\n"
"         top.frames[prop_frame].document.write(\n"
"            \"<NOBR><b>%s</b> %s</NOBR><BR>\" +\n"
"            \"<NOBR><b>%s</b> %s</NOBR><BR>\" +\n"
"            \"<NOBR><b>%s</b> %s</NOBR><BR>\" +\n"
"            \"<NOBR><b>%s</b> %s</NOBR><BR>\" +\n"
"            \"<NOBR><b>%s</b> %s</NOBR><BR>\" +\n"
"            \"<NOBR><b>%s</b> %s</NOBR><BR>\" ",
	    cookieNum,
	    name, fixed_name,
	    value, fixed_value,
	    domain_or_host, fixed_domain_or_host,
	    path, fixed_path,
	    secure, yes_or_no,
	    expires, expireDate
	    );
	FLUSH_BUFFER
	PR_FREEIF(fixed_name);
	PR_FREEIF(fixed_value);
	PR_FREEIF(fixed_domain_or_host);
	PR_FREEIF(fixed_path);
	PR_FREEIF(name);
	PR_FREEIF(value);
	PR_FREEIF(domain_or_host);
	PR_FREEIF(path);
	PR_FREEIF(secure);
	PR_FREEIF(yes_or_no);
	PR_FREEIF(expires);
	PR_FREEIF(expireDate);
	/* caveat: It would have been simpler to not allocate and free the
	 * above strings but rather just use the XP_GetString(...) parameters
	 * in the call to PR_snprintf.	However there are only four buffers
	 * that XP_GetString uses and it keeps recycling them.	Since the
	 * above would require more than four parameters which are calls to
	 * XP_GetString, some of the returned buffers would be overwritten
         * before they could be used.  Hence the simpler approach wouldn't work
	 */

#ifdef TRUST_LABELS
        /* add an entry that contains the trust label information */
        szTrustLabel = GetTrustLabelString( cookie->name );
        if ( szTrustLabel ) {
            g += PR_snprintf(buffer+g, BUFLEN-g,
"+\n"
"            \"<HR WIDTH=100%> <FONT SIZE=2> %s </FONT>\"",
                szTrustLabel);
            PR_FREEIF(szTrustLabel);
        }
#endif
        g += PR_snprintf(buffer+g, BUFLEN-g,
"\n"
"         );\n"
"         break;\n"
            );
        FLUSH_BUFFER
    }

    /* generate next section of html file */
    g += PR_snprintf(buffer+g, BUFLEN-g,
"      }\n"
"      top.frames[prop_frame].document.close();\n"
"    }\n"
"\n"
"    function loadCookies(){\n"
"      cookie_mode = 1;\n"
	);
    FLUSH_BUFFER

    /* generate index frame only if full viewer */

    if (!host) {
	g += PR_snprintf(buffer+g, BUFLEN-g,
"      top.frames[index_frame].document.open();\n"
"      top.frames[index_frame].document.write(\n"
"        \"<BODY BGCOLOR=#C0C0C0>\" +\n"
"          \"<TABLE BORDER=0 WIDTH=100%>\" +\n"
"            \"<TR>\" +\n"
"              \"<TD> <SPACER TYPE=HORIZONTAL SIZE=10> </TD>\" +\n"
"              \"<TD ALIGN=CENTER VALIGN=MIDDLE BGCOLOR=#FFFFFF>\" +\n"
"                \"<FONT SIZE=2 COLOR=#666666>\" +\n"
"                  \"<B>%s</B>\" +\n"
"                \"</FONT>\" +\n"
"              \"</TD>\" +\n"
"              \"<TD>&nbsp;&nbsp;&nbsp;</TD>\" +\n"
"              \"<TD ALIGN=CENTER VALIGN=MIDDLE BGCOLOR=#C0C0C0>\" +\n"
"                \"<A HREF=javascript:top.loadSites();>\" +\n"
"                  \"<FONT SIZE=2>%s</FONT>\" +\n"
"                \"</A>\" +\n"
"              \"</TD>\" +\n"
"              \"<TD>&nbsp;&nbsp;&nbsp;</TD>\" +\n"
"            \"</TR>\" +\n"
"          \"</TABLE>\" +\n"
"        \"</BODY>\"\n"
"      );\n"
"      top.frames[index_frame].document.close();\n"
"\n",
	    view_cookies, view_sites);
	FLUSH_BUFFER
    }

    /* generate next section of html file */
    if (!host) {
	StrAllocCopy (heading, XP_GetString(MK_ACCESS_COOKIES_ACCEPTED));
    } else {
	StrAllocCopy (heading, XP_GetString(MK_ACCESS_SITE_COOKIES_ACCEPTED));
    }
    g += PR_snprintf(buffer+g, BUFLEN-g,
"      top.frames[title_frame].document.open();\n"
"      top.frames[title_frame].document.write\n"
"        (\"&nbsp;%s\");\n"
"      top.frames[title_frame].document.close();\n"
"\n"
"      top.frames[list_frame].document.open();\n"
"      top.frames[list_frame].document.write(\n"
"        \"<FORM name=fSelectCookie>\" +\n"
"          \"<P>\" +\n"
"          \"<TABLE BORDER=0 WIDTH=100% HEIGHT=95%>\" +\n"
"            \"<TR>\" +\n"
"              \"<TD WIDTH=100% VALIGN=TOP>\" +\n"
"                \"<CENTER>\" +\n"
"                  \"<P>\" +\n"
"                  \"<SELECT NAME=selname \" +\n"
"                          \"SIZE=15 \" +\n"
"                          \"MULTIPLE \" +\n"
"                          \"onchange=top.ViewCookieSelected(selname.selectedIndex);>\"\n"
"      );\n",
	heading);
    FLUSH_BUFFER
    PR_FREEIF(heading);

    /* generate the html for the list of cookies in alphabetical order */
    cookie_list=net_cookie_list;
    cookie = NULL;
    while (cookie = NextCookieAfter(cookie, &cookieNum)) {
	if (!host || net_CookieIsFromHost(cookie, host)) {
            char *fixed_name = net_FixQuoted(cookie->name);
	    g += PR_snprintf(buffer+g, BUFLEN-g,
"      if (!deleted_cookies[%d]) {\n"
"        top.frames[list_frame].document.write(\n"
"                    \"<OPTION value=%d>\" +\n"
"                      \"%s:%s\" +\n"
"                    \"</OPTION>\"\n"
"        );\n"
"      }\n",
		cookieNum, cookieNum,
		(*(cookie->host)=='.') ? (cookie->host)+1: cookie->host,
		fixed_name
		);
	    FLUSH_BUFFER
            PR_FREEIF(fixed_name);
	}
    }
    net_unlock_cookie_list();

    /* generate next section of html file */
    StrAllocCopy (heading, XP_GetString(MK_ACCESS_COOKIES_PERMISSION));
    g += PR_snprintf(buffer+g, BUFLEN-g,
"      top.frames[list_frame].document.write(\n"
"                  \"</SELECT>\" +\n"
"                \"</CENTER>\" +\n"
"              \"</TD>\" +\n"
"            \"</TR>\" +\n"
"          \"</TABLE>\" +\n"
"        \"</FORM>\"\n"
"      );\n"
"      top.frames[list_frame].document.close();\n"
"\n"
"      top.frames[prop_frame].document.open();\n"
"      top.frames[prop_frame].document.close();\n"
"    }\n"
"\n"
"    function loadSites(){\n"
"      cookie_mode = 0;\n"
"      top.frames[index_frame].document.open();\n"
"      top.frames[index_frame].document.write(\n"
"        \"<BODY BGCOLOR=#C0C0C0>\" +\n"
"          \"<TABLE BORDER=0 WIDTH=100%>\" +\n"
"            \"<TR>\" +\n"
"              \"<TD> <SPACER TYPE=HORIZONTAL SIZE=10> </TD>\" +\n"
"              \"<TD ALIGN=CENTER VALIGN=MIDDLE BGCOLOR=#C0C0C0>\" +\n"
"                \"<A HREF=javascript:top.loadCookies();>\" +\n"
"                  \"<FONT SIZE=2>%s</FONT>\" +\n"
"                \"</A>\" +\n"
"              \"</TD>\" +\n"
"              \"<TD>&nbsp;&nbsp;&nbsp;</TD>\" +\n"
"              \"<TD ALIGN=CENTER VALIGN=MIDDLE BGCOLOR=#FFFFFF>\" +\n"
"                \"<FONT SIZE=2 COLOR=#666666>\" +\n"
"                  \"<B>%s</B>\" +\n"
"                \"</FONT>\" +\n"
"              \"</TD>\" +\n"
"              \"<TD>&nbsp;&nbsp;&nbsp;</TD>\" +\n"
"            \"</TR>\" +\n"
"          \"</TABLE>\" +\n"
"        \"</BODY>\"\n"
"      );\n"
"      top.frames[index_frame].document.close();\n"
"\n"
"      top.frames[title_frame].document.open();\n"
"      top.frames[title_frame].document.write\n"
"        (\"&nbsp;%s\");\n"
"      top.frames[title_frame].document.close();\n"
"\n"
"      top.frames[list_frame].document.open();\n"
"      top.frames[list_frame].document.write(\n"
"        \"<FORM name=fSelectSite>\" +\n"
"          \"<P>\" +\n"
"          \"<TABLE BORDER=0 WIDTH=100% HEIGHT=95%>\" +\n"
"            \"<TR>\" +\n"
"              \"<TD WIDTH=100% VALIGN=TOP>\" +\n"
"                \"<CENTER>\" +\n"
"                  \"<P>\" +\n"
"                  \"<SELECT NAME=selname SIZE=15 MULTIPLE> \"\n"
"      );\n",
	view_cookies, view_sites, heading);
    FLUSH_BUFFER
    PR_FREEIF(heading);
    FLUSH_BUFFER

    /* generate the html for the list of cookie permissions */
    cookieNum = 0;
    cookie_permission_list=net_cookie_permission_list;
    net_lock_cookie_permission_list();
    while ( (cookperm=(net_CookiePermissionStruct *)
		      XP_ListNextObject(cookie_permission_list)) ) {
	g += PR_snprintf(buffer+g, BUFLEN-g,
"      if (!deleted_sites[%d]) {\n"
"        top.frames[list_frame].document.write(\n"
"                    \"<OPTION value=\" +\n"
"                      \"%d>\" +\n"
"                      \"%s %s\" +\n"
"                    \"</OPTION>\"\n"
"        );\n"
"      }\n",
	    cookieNum, cookieNum,
            cookperm->permission ? "+" : "-",
	    cookperm->host);
	FLUSH_BUFFER
	cookieNum++;
    }
    net_unlock_cookie_permission_list();

    /* generate next section of html file */
    g += PR_snprintf(buffer+g, BUFLEN-g,
"      top.frames[list_frame].document.write(\n"
"                  \"</SELECT>\" +\n"
"                \"</CENTER>\" +\n"
"              \"</TD>\" +\n"
"            \"</TR>\" +\n"
"          \"</TABLE>\" +\n"
"        \"</FORM>\"\n"
"      );\n"
"      top.frames[list_frame].document.close();\n"
"\n"
"      top.frames[prop_frame].document.open();\n"
"      top.frames[prop_frame].document.close();\n"
"    }\n"
"\n"
"    function loadButtons(){\n"
"      top.frames[button_frame].document.open();\n"
"      top.frames[button_frame].document.write(\n"
"        \"<FORM name=buttons action=internal-cookieViewer-handler method=post>\" +\n"
"          \"<BR>\" +\n"
"          \"&nbsp;\" +\n"
"          \"<INPUT type=BUTTON \" +\n"
"                 \"value=Remove \" +\n"
"                 \"onclick=top.DeleteItemSelected();>\" +\n"
"          \"<DIV align=right>\" +\n"
"            \"<INPUT type=BUTTON value=OK width=80 onclick=parent.clicker(this,window.parent)>\" +\n"
"            \" &nbsp;&nbsp;\" +\n"
"            \"<INPUT type=BUTTON value=Cancel width=80 onclick=parent.clicker(this,window.parent)>\" +\n"
"          \"</DIV>\" +\n"
"          \"<INPUT type=HIDDEN name=xxxbuttonxxx>\" +\n"
"          \"<INPUT type=HIDDEN name=handle value="
	);
    FLUSH_BUFFER

    /* transfer what we have so far into strings->args[0] */
    if (buffer2) {
	XP_SetDialogString(strings, 0, buffer2);
	buffer2 = NULL;
    }

    /* Note: strings->args[1] will later be filled in with value of handle */

    /* generate remainder of html, it will go into strings->arg[2] */
    g += PR_snprintf(buffer+g, BUFLEN-g,
">\" +\n"
"          \"<INPUT TYPE=HIDDEN NAME=goneC SIZE=-1>\" +\n"
"          \"<INPUT TYPE=HIDDEN NAME=goneP SIZE=-1>\" +\n"
"        \"</FORM>\"\n"
"      );\n"
"      top.frames[button_frame].document.close();\n"
"    }\n"
"\n"
"    function loadFrames(){\n"
"      loadCookies();\n"
"      loadButtons();\n"
"    }\n"
"\n"
"    function clicker(but,win){\n"
#ifndef HTMLDialogs 
"      var goneC = top.frames[button_frame].document.buttons.goneC;\n"
"      var goneP = top.frames[button_frame].document.buttons.goneP;\n"
"      var expires = new Date();\n"
"      expires.setTime(expires.getTime() + 1000*60*60*24*365);\n"
"      document.cookie = \"htmldlgs=|\" + but.value +\n"
"        \"|goneC|\" + goneC.value + \"|goneP|\" + goneP.value + \"|\" +\n"
"        \"; expires=\" + expires.toGMTString();\n"
#endif
"      top.frames[button_frame].document.buttons.xxxbuttonxxx.value = but.value;\n"
"      top.frames[button_frame].document.buttons.xxxbuttonxxx.name = 'button';\n"
"      top.frames[button_frame].document.buttons.submit();\n"
"    }\n"
"\n"
"  </SCRIPT>\n"
"</HEAD>\n"
"<FRAMESET ROWS = 25,25,*,75\n"
"         BORDER=0\n"
"         FRAMESPACING=0\n"
"         onLoad=loadFrames()>\n"
"  <FRAME SRC=about:blank\n"
"        NAME=index_frame\n"
"        SCROLLING=NO\n"
"        MARGINWIDTH=1\n"
"        MARGINHEIGHT=1\n"
"        NORESIZE>\n"
"  <FRAME SRC=about:blank\n"
"        NAME=title_frame\n"
"        SCROLLING=NO\n"
"        MARGINWIDTH=1\n"
"        MARGINHEIGHT=1\n"
"        NORESIZE>\n"
"  <FRAMESET COLS=5,*,10,*,5\n"
"           BORDER=0\n"
"           FRAMESPACING=0>\n"
"    <FRAME SRC=about:blank\n"
"          NAME=spacer1_frame\n"
"          SCROLLING=AUTO\n"
"          MARGINWIDTH=0\n"
"          MARGINHEIGHT=0\n"
"          NORESIZE>\n"
"    <FRAME SRC=about:blank\n"
"          NAME=list_frame\n"
"          SCROLLING=AUTO\n"
"          MARGINWIDTH=0\n"
"          MARGINHEIGHT=0\n"
"          NORESIZE>\n"
"    <FRAME SRC=about:blank\n"
"          NAME=spacer2_frame\n"
"          SCROLLING=AUTO\n"
"          MARGINWIDTH=0\n"
"          MARGINHEIGHT=0\n"
"          NORESIZE>\n"
"    <FRAME SRC=about:blank\n"
"          NAME=prop_frame\n"
"          SCROLLING=AUTO\n"
"          MARGINWIDTH=0\n"
"          MARGINHEIGHT=0\n"
"          NORESIZE>\n"
"    <FRAME SRC=about:blank\n"
"          NAME=spacer3_frame\n"
"          SCROLLING=AUTO\n"
"          MARGINWIDTH=0\n"
"          MARGINHEIGHT=0\n"
"          NORESIZE>\n"
"  </FRAMESET>\n"
"  <FRAME SRC=about:blank\n"
"        NAME=button_frame\n"
"        SCROLLING=NO\n"
"        MARGINWIDTH=1\n"
"        MARGINHEIGHT=1\n"
"        NORESIZE>\n"
"</FRAMESET>\n"
"\n"
"<NOFRAMES>\n"
"  <BODY> <P> </BODY>\n"
"</NOFRAMES>\n"
"</HTML>\n"
	);
    FLUSH_BUFFER

    /* free some strings that are no longer needed */
    PR_FREEIF(view_cookies);
    PR_FREEIF(view_sites);
    PR_FREEIF(buffer);

    /* put html just generated into strings->arg[2] and invoke HTML dialog */
    if (buffer2) {
      XP_SetDialogString(strings, 2, buffer2);
      buffer2 = NULL;
    }
    dlg = PORT_ZAlloc(sizeof(CookieViewerDialog));
    if ( dlg == NULL ) {
	return;
    }
    dlg->parent_window = (void *)context;
    dlg->dialogUp = PR_TRUE;
    dlg->state =XP_MakeHTMLDialog(NULL, &dialogInfo, MK_ACCESS_YOUR_COOKIES,
		strings, (void *)dlg, PR_FALSE);

    return;
}

PUBLIC void
NET_CookieViewerReturn() {
}

PUBLIC void
NET_DisplayCookieInfoOfSiteAsHTML(MWContext * context, char * URLName)
{
    char * host;
    char * colon;

    /* remove protocol from URL name */
    host = NET_ParseURL(URLName, GET_HOST_PART);

    /* remove port number from URL name */
    colon = PL_strchr(host, ':');
    if(colon) {
        *colon = '\0';
    }

    net_DisplayCookieInfoAsHTML(context, host);

    /* free up allocated string and return */
    if(colon) {
        *colon = ':';
    }
    PR_Free(host);
}

PUBLIC void
NET_DisplayCookieInfoAsHTML(MWContext * context)
{
    net_DisplayCookieInfoAsHTML(context, NULL);
}


#ifdef XP_MAC
/* pinkerton - reset optimization state (see above) */
#pragma global_optimizer reset
#endif

#else
PUBLIC void
NET_DisplayCookieInfoAsHTML(MWContext * context)
{
}
#endif

