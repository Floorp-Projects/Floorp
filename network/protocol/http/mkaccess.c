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
#include "xp.h"
#include "netutils.h"
#include "mkselect.h"
#include "mktcp.h"
#include "mkgeturl.h"
#include "mkparse.h"
#include "mkaccess.h"
#include "cookies.h"
#include "httpauth.h"
#include "prefapi.h"
#include "shist.h"
#include "jscookie.h"
#include "prmon.h"

#ifdef XP_MAC
#include "prpriv.h"             /* for NewNamedMonitor */
#else
#include "private/prpriv.h"
#endif

#include "secnav.h"
#include "libevent.h"
#include "pwcacapi.h"

/* for XP_GetString() */
#include "xpgetstr.h"
extern int XP_CONFIRM_AUTHORIZATION_FAIL;
extern int XP_ACCESS_ENTER_USERNAME;
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
extern int MK_ACCESS_COOKIES_TOANYSERV; 
extern int MK_ACCESS_COOKIES_TOSELF;
extern int MK_ACCESS_COOKIES_NAME_AND_VAL;
extern int MK_ACCESS_COOKIES_COOKIE_WILL_PERSIST;
extern int MK_ACCESS_COOKIES_SET_IT;
extern int MK_ACCESS_YOUR_COOKIES;
extern int MK_ACCESS_MAXIMUM_COOKS;
extern int MK_ACCESS_COOK_COUNT;
extern int MK_ACCESS_MAXIMUM_COOKS_PER_SERV;
extern int MK_ACCESS_MAXIMUM_COOK_SIZE;
extern int MK_ACCESS_NO_COOKIES;
extern int MK_ACCESS_NAME;
extern int MK_ACCESS_VALUE;
extern int MK_ACCESS_HOST;
extern int MK_ACCESS_SEND_TO_HOST;
extern int MK_ACCESS_IS_DOMAIN;
extern int MK_ACCESS_IS_NOT_DOMAIN;
extern int MK_ACCESS_SEND_TO_PATH;
extern int MK_ACCESS_AND_BELOW;
extern int MK_ACCESS_SECURE;
extern int MK_ACCESS_EXPIRES;
extern int MK_ACCESS_END_OF_SESSION;

#define MAX_NUMBER_OF_COOKIES  300
#define MAX_COOKIES_PER_SERVER 20
#define MAX_BYTES_PER_COOKIE   4096  /* must be at least 1 */

/*
 * Authentication information for servers and proxies is kept
 * on separate lists, but both lists consist of net_AuthStruct's.
 */

PRIVATE XP_List * net_auth_list = NULL;
PRIVATE XP_List * net_proxy_auth_list = NULL;

PRIVATE Bool cookies_changed = FALSE;

PRIVATE NET_CookieBehaviorEnum net_CookieBehavior = NET_Accept;
PRIVATE Bool net_WarnAboutCookies = FALSE;
PRIVATE char *net_scriptName = (char *)0;

static const char *pref_cookieBehavior = "network.cookie.cookieBehavior";
static const char *pref_warnAboutCookies = "network.cookie.warnAboutCookies";
static const char *pref_scriptName = "network.cookie.filterName";


/*
 * Different schemes supported by the client.
 * Order means the order of preference between authentication schemes.
 *
 */
typedef enum _net_AuthType {
    AUTH_INVALID   = 0,
    AUTH_BASIC     = 1
#ifdef SIMPLE_MD5
	, AUTH_SIMPLEMD5 = 2		/* Much better than "Basic" */
#endif /* SIMPLE_MD5 */
    ,AUTH_FORTEZZA  = 3
} net_AuthType;


/*
 * This struct describes both Basic and SimpleMD5 authentication stuff,
 * for both HTTP servers and proxies.
 *
 */
typedef struct _net_AuthStruct {
    net_AuthType	auth_type;
    char *			path;			/* For normal authentication only */
	char *			proxy_addr;		/* For proxy authentication only */
	char *			username;		/* Obvious */
	char *			password;		/* Not too cryptic either */
	char *			auth_string;	/* Storage for latest Authorization str */
	char *			realm;			/* For all auth schemes */
#ifdef SIMPLE_MD5
    char *			domain;			/* SimpleMD5 only */
    char *			nonce;			/* SimpleMD5 only */
    char *			opaque;			/* SimpleMD5 only */
    XP_Bool			oldNonce;		/* SimpleMD5 only */
	int				oldNonce_retries;
#endif
    char *                      challenge;
    char *                      certChain;
    char *                      signature;
    char *                      clientRan;
    XP_Bool                     oldChallenge;
    int                         oldChallenge_retries;
} net_AuthStruct;



/*----------------- Normal client-server authentication ------------------ */

PRIVATE net_AuthStruct *
net_CheckForAuthorization(char * address, Bool exact_match)
{

    XP_List * list_ptr = net_auth_list;
    net_AuthStruct * auth_s;

	TRACEMSG(("net_CheckForAuthorization: checking for auth on: %s", address));

    while((auth_s = (net_AuthStruct *) XP_ListNextObject(list_ptr))!=0)
      {
		if(exact_match)
		  {
		    if(!XP_STRCMP(address, auth_s->path))
			    return(auth_s);
		  }
		else
		  {
			/* shorter strings always come last so there can be no
			 * ambiquity
			 */
		    if(!strncasecomp(address, auth_s->path, XP_STRLEN(auth_s->path)))
			    return(auth_s);
		  }
      }
   
    return(NULL);
}

/* returns TRUE if authorization is required
 */
PUBLIC Bool
NET_AuthorizationRequired(char * address)
{
    net_AuthStruct * rv;
	char * last_slash = XP_STRRCHR(address, '/');

	if(last_slash)
		*last_slash = '\0';

	rv = net_CheckForAuthorization(address, FALSE);

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
			XP_FREE(auth_header);
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
        if(!strncasecomp(proto_host, auth_s->path, XP_STRLEN(proto_host))
			&& !strcasecomp(realm, auth_s->realm))
		{
			XP_FREE(proto_host);
            return(auth_s);
		}
      }
	XP_FREE(proto_host);
    return(NULL);
}

PRIVATE void
net_free_auth_struct(net_AuthStruct *auth)
{
    XP_FREE(auth->path);
    XP_FREE(auth->proxy_addr);
    XP_FREE(auth->username);
    XP_FREE(auth->password);
    XP_FREE(auth->auth_string);
    XP_FREE(auth->realm);
    /*FORTEZZA related stuff   */
    XP_FREEIF(auth->challenge);
    XP_FREEIF(auth->certChain);
    XP_FREEIF(auth->signature);
    XP_FREEIF(auth->clientRan);
    /*End FORTEZZA related stuff*/
    XP_FREE(auth);
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

	if(!XP_STRCMP(prev_auth->path, cur_path))
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

	*address = NULL;
	*realm = NULL;

	if(!key)
		return;

	tab = XP_STRCHR(key, '\t');

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

	XP_FREE(key);

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
	
	XP_STRNCPY_SAFE(type_buffer, "HTTP basic authorization", type_buffer_size);

	separate_http_key(key, &address, &realm);

	if(address)
	{
		XP_STRNCPY_SAFE(url_buffer, address, url_buffer_size);
	}

	if(username)
	{
		XP_STRNCPY_SAFE(username_buffer, username, username_buffer_size);
		XP_FREE(username);
	}
	
	if(password)
	{
		XP_STRNCPY_SAFE(password_buffer, password, password_buffer_size);
		XP_FREE(password);
	}
}

PRIVATE void
net_initialize_http_access()
{
	/* register PW cache interp function */
	
	PC_RegisterDataInterpretFunc(HTTP_PW_MODULE_NAME, 	
				     net_http_password_data_interp);
}

/* returns false if the user wishes to cancel authorization
 * and TRUE if the user wants to continue with a new authorization
 * string
 */
/* HARDTS: I took a whack at fixing up some of the strings leaked in this 
 * function.  All the XP_FREEIF()s are new. 
 */
PUBLIC Bool 
NET_AskForAuthString(MWContext *context,
					 URL_Struct * URL_s, 
					 char * authenticate, 
					 char * prot_template,
					 Bool   already_sent_auth)
{
	static XP_Bool first_time=TRUE;
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
	XP_Bool re_authorize=FALSE;

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

#define COMPUSERVE_HEADER_NAME "Remote-Passphrase"

		if(!strncasecomp(authenticate_header_value, 
					 COMPUSERVE_HEADER_NAME, 
					 sizeof(COMPUSERVE_HEADER_NAME) - 1))
	  	{
	  		/* This is a compuserv style header 
	  	 	*/

		XP_FREEIF(host);
#if defined(XP_WIN) && defined(MOZILLA_CLIENT)
		return(WFE_DoCompuserveAuthenticate(context, URL_s, authenticate_header_value));
#else
		return(NET_AUTH_FAILED_DISPLAY_DOCUMENT);		
#endif	
	  	}			 
#define HTTP_BASIC_AUTH_TOKEN "BASIC"
		else if(strncasecomp(authenticate_header_value, 
					 HTTP_BASIC_AUTH_TOKEN, 
					 sizeof(HTTP_BASIC_AUTH_TOKEN) - 1))
		{
			/* unsupported auth type */
			return(NET_AUTH_FAILED_DISPLAY_DOCUMENT);		
		}
	  }

	new_address = NET_ParseURL(address,	GET_PROTOCOL_PART | GET_HOST_PART | GET_PATH_PART);
	if(!new_address) {
		XP_FREEIF(host);
		return NET_AUTH_FAILED_DISPLAY_DOCUMENT;
	}

	unamePwd=NET_ParseURL(address, GET_USERNAME_PART | GET_PASSWORD_PART);
	/* get the username & password out of the combo string */
	if( (colon = XP_STRCHR(unamePwd, ':')) != NULL ) {
		*colon='\0';
		username=XP_STRDUP(unamePwd);
		password=XP_STRDUP(colon+1);
		*colon=':';
		XP_FREE(unamePwd);
	} else {
		username=unamePwd;
	}

	if(username && !(*username) )
	{
		XP_FREEIF(username);
		username = NULL;
	}
	if(password && !(*password) )
	{
		XP_FREEIF(password);
		password = NULL;
	}

	/*if last char is not a slash then */
	if (new_address[XP_STRLEN(new_address)-1] != '/')
	{
		/* remove everything after the last slash */
		slash = XP_STRRCHR(new_address, '/');
		if(++slash)
			*slash = '\0';
	}

	if(!authenticate)
	  {
		realm = "unknown";
	  }
	else
	  {
		realm = XP_STRCHR(authenticate, '"');
	
		if(realm)
		  {
			realm++;

			/* terminate at next quote */
			XP_STRTOK(realm, "\"");

#define MAX_REALM_SIZE 128
			if(XP_STRLEN(realm) > MAX_REALM_SIZE)
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
		XP_FREEIF(host);
		XP_FREEIF(new_address);
		XP_FREEIF(username);
		XP_FREEIF(password);
		return(NET_RETRY_WITH_AUTH);
      }
    else if(prev_auth)
      {
		/* we sent the authorization string and it was wrong
		 */
        if(!FE_Confirm(context, XP_GetString(XP_CONFIRM_AUTHORIZATION_FAIL)))
		  {
        	TRACEMSG(("User canceled login!!!"));

			if(!XP_STRCMP(prev_auth->path, new_address))
			  {
				/* if the paths are exact and the user cancels
				 * remove the mapping
				 */
				net_remove_exact_auth_match_on_cancel(prev_auth, new_address);
				XP_FREEIF(host);
				XP_FREEIF(new_address);
				XP_FREEIF(username);
				XP_FREEIF(password);
            	return(NET_AUTH_FAILED_DISPLAY_DOCUMENT);
			  }
		  }
		
		if (!username)
			username = XP_STRDUP(prev_auth->username);
		if (!password)
			password = XP_STRDUP(prev_auth->password);
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

			net_remove_stored_http_password(prev_auth->path);

            /* compare the two url paths until they deviate
             * once they deviate truncate
             */
            for(ptr1 = prev_auth->path, ptr2 = new_address; *ptr1 && *ptr2; ptr1++, ptr2++)
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
			XP_FREE(tmp);				

			TRACEMSG(("Truncated new auth path to be: %s", prev_auth->path));

			net_store_http_password(prev_auth->path, prev_auth->realm, prev_auth->username, prev_auth->password);

			XP_FREE(host);
			XP_FREE(new_address);
			return(NET_RETRY_WITH_AUTH);
          }
	  }
					 
	/* Use username and/or password specified in URL_struct if exists. */
	if (!username && URL_s->username && *URL_s->username) {
		username = XP_STRDUP(URL_s->username);
	}
	if (!password && URL_s->password && *URL_s->password) {
		password = XP_STRDUP(URL_s->password);
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
				XP_FREEIF(password);
				password = NULL;
			}
		}
	}

	/* if the password is filled in then the username must
	 * be filled in already.  
	 */
	if(!password || re_authorize)
	  {
		XP_Bool remember_password;
	   	host = NET_ParseURL(address, GET_HOST_PART);

		/* malloc memory here to prevent buffer overflow */
		len = XP_STRLEN(XP_GetString(XP_ACCESS_ENTER_USERNAME));
		len += XP_STRLEN(realm) + XP_STRLEN(host) + 10;
		
		buf = (char *)XP_ALLOC(len*sizeof(char));
		
		if(buf)
		  {
			PR_snprintf( buf, len*sizeof(char), 
						XP_GetString(XP_ACCESS_ENTER_USERNAME), 
						realm, host);


			NET_Progress(context, XP_GetString( XP_CONNECT_PLEASE_ENTER_PASSWORD_FOR_HOST) );
			if (username && !(*username))
				XP_FREE(username);
			XP_FREEIF(password);
			status = PC_PromptUsernameAndPassword(context, buf, 
											  	  &username, &password, 
												  &remember_password,
												  NET_IsURLSecure(URL_s->address));
	
			XP_FREE(buf);
		  }
		else
		  {
			status = 0;
		  }

		XP_FREE(host);

		if(!status)
		  {
			TRACEMSG(("User canceled login!!!"));

			/* if the paths are exact and the user cancels
			 * remove the mapping
			 */
			net_remove_exact_auth_match_on_cancel(prev_auth, new_address);

			XP_FREEIF(username);
			XP_FREEIF(password);
			XP_FREEIF(new_address);
			return(NET_AUTH_FAILED_DISPLAY_DOCUMENT);
		  }
		else if(!username || !password)
		  {
			XP_FREEIF(username);
			XP_FREEIF(password);
			XP_FREEIF(new_address);
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

	len = XP_STRLEN(u_pass_string);
	auth_string = (char*) XP_ALLOC((((len+1)*4)/3)+10);

	if(!auth_string)
	  {
		XP_FREEIF(username);
		XP_FREEIF(password);
		XP_FREEIF(u_pass_string);
		XP_FREE(new_address);
		return(NET_RETRY_WITH_AUTH);
	  }

	NET_UUEncode((unsigned char *)u_pass_string, (unsigned char*) auth_string, len);

	XP_FREE(u_pass_string);

	if(prev_auth)
	  {
	    XP_FREEIF(prev_auth->auth_string);
        prev_auth->auth_string = auth_string;
		XP_FREEIF(prev_auth->username);
		prev_auth->username = username;
        XP_FREEIF(prev_auth->password);
        prev_auth->password = password;
	  }
	else
	  {
		XP_List * list_ptr = net_auth_list;
		net_AuthStruct * tmp_auth_ptr;
		size_t new_len;

		/* construct a new auth_struct
		 */
		prev_auth = XP_NEW_ZAP(net_AuthStruct);
	    if(!prev_auth)
		  {
			XP_FREEIF(auth_string);
			XP_FREEIF(username);
			XP_FREEIF(password);
		    XP_FREE(new_address);
		    return(NET_RETRY_WITH_AUTH);
		  }
		
        prev_auth->auth_string = auth_string;
		prev_auth->username = username;
        prev_auth->password = password;
		prev_auth->path = 0;
		StrAllocCopy(prev_auth->path, new_address);
		prev_auth->realm = 0;
		StrAllocCopy(prev_auth->realm, realm);

		if(!net_auth_list)
		  {
			net_auth_list = XP_ListNew();
		    if(!net_auth_list)
			  {
          /* Maybe should free prev_auth here. */
		   		XP_FREE(new_address);
				return(NET_RETRY_WITH_AUTH);
			  }
		  }		

		/* add it to the list so that it is before any strings of
		 * smaller length
		 */
		new_len = XP_STRLEN(prev_auth->path);
		while((tmp_auth_ptr = (net_AuthStruct *) XP_ListNextObject(list_ptr))!=0)
		  { 
			if(new_len > XP_STRLEN(tmp_auth_ptr->path))
			  {
				XP_ListInsertObject(net_auth_list, tmp_auth_ptr, prev_auth);
		   		XP_FREE(new_address);
				return(NET_RETRY_WITH_AUTH);
			  }
		  }
		/* no shorter strings found in list */	
		XP_ListAddObjectToEnd(net_auth_list, prev_auth);
	  }

	XP_FREE(new_address);
	return(NET_RETRY_WITH_AUTH);
}

/*--------------------------------------------------
 * The following routines support the 
 * Set-Cookie: / Cookie: headers
 */

PRIVATE XP_List * net_cookie_list=0;

typedef struct _net_CookieStruct {
    char * path;
	char * host;
	char * name;
    char * cookie;
	time_t expires;
	time_t last_accessed;
	Bool   secure;      /* only send for https connections */
	Bool   is_domain;   /* is it a domain instead of an absolute host? */
} net_CookieStruct;

/* Routines and data to protect the cookie list so it
**   can be accessed by mulitple threads
*/

#include "prthread.h"
#include "prmon.h"

static PRMonitor * cookie_lock_monitor = NULL;
static PRThread  * cookie_lock_owner = NULL;
static int cookie_lock_count = 0;

PRIVATE void
net_lock_cookie_list()
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
net_unlock_cookie_list()
{
   PR_EnterMonitor(cookie_lock_monitor);

#ifdef DEBUG
    /* make sure someone doesn't try to free a lock they don't own */
    XP_ASSERT(cookie_lock_owner == PR_CurrentThread());
#endif

    cookie_lock_count--;

    if(cookie_lock_count == 0) {
	cookie_lock_owner = NULL;
	PR_Notify(cookie_lock_monitor);
    }
    PR_ExitMonitor(cookie_lock_monitor);

}

/* This should only get called while holding the cookie-lock
**
*/
PRIVATE void
net_FreeCookie(net_CookieStruct * cookie)
{

	if(!cookie)
		return;

	XP_ListRemoveObject(net_cookie_list, cookie);

	XP_FREEIF(cookie->path);
	XP_FREEIF(cookie->host);
	XP_FREEIF(cookie->name);
	XP_FREEIF(cookie->cookie);

	XP_FREE(cookie);

	cookies_changed = TRUE;
}



/* blows away all cookies currently in the list, then blows away the list itself
 * nulling it after it's free'd
 */
PUBLIC void
NET_RemoveAllCookies()
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
	    if(!strcasecomp(cookie_s->host, cur_host))
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

    while((cookie_s = (net_CookieStruct *) XP_ListNextObject(list_ptr))!=0)
      {
		if(path 
			&& hostname 
				&& cookie_s->path
				   && cookie_s->host
					  && cookie_s->name
					 	 && !XP_STRCMP(name, cookie_s->name)
							&& !XP_STRCMP(path, cookie_s->path)
								&& !XP_STRCASECMP(hostname, cookie_s->host))
                return(cookie_s);
			
      }

    return(NULL);
}

/* cookie utility functions */
PRIVATE void
NET_SetCookieBehaviorPref(NET_CookieBehaviorEnum x)
{
	net_CookieBehavior = x;

	if(net_CookieBehavior == NET_DontUse)
		XP_FileRemove("", xpHTTPCookie);
}

PRIVATE void
NET_SetCookieWarningPref(Bool x)
{
	net_WarnAboutCookies = x;
}

PRIVATE void
NET_SetCookieScriptPref(const char *name)
{
    XP_FREEIF(net_scriptName);
	if( name && *name )
		net_scriptName=XP_STRDUP(name);
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
	int32	n;
	PREF_GetIntPref(pref_cookieBehavior, &n);
	NET_SetCookieBehaviorPref((NET_CookieBehaviorEnum)n);
	return PREF_NOERROR;
}

MODULE_PRIVATE int PR_CALLBACK
NET_CookieWarningPrefChanged(const char * newpref, void * data)
{
	Bool	x;
	PREF_GetBoolPref(pref_warnAboutCookies, &x);
	NET_SetCookieWarningPref(x);
	return PREF_NOERROR;
}

MODULE_PRIVATE int PR_CALLBACK
NET_CookieScriptPrefChanged(const char * newpref, void * data)
{
    char    s[64];
    int len = sizeof(s);
    PREF_GetCharPref(pref_scriptName, s, &len);
    NET_SetCookieScriptPref(s);
    return PREF_NOERROR;
}
 

/* called from mkgeturl.c, NET_InitNetLib(). This sets the module local cookie pref variables
   and registers the callbacks */
PUBLIC void
NET_RegisterCookiePrefCallbacks(void)
{
	int32	n;
	Bool	x;
    char    s[64];
    int len = sizeof(s);

	PREF_GetIntPref(pref_cookieBehavior, &n);
	NET_SetCookieBehaviorPref((NET_CookieBehaviorEnum)n);
	PREF_RegisterCallback(pref_cookieBehavior, NET_CookieBehaviorPrefChanged, NULL);

	PREF_GetBoolPref(pref_warnAboutCookies, &x);
	NET_SetCookieWarningPref(x);
	PREF_RegisterCallback(pref_warnAboutCookies, NET_CookieWarningPrefChanged, NULL);

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
	Bool secure_path=FALSE;
	time_t cur_time = time(NULL);
	int host_length;
	int domain_length;

	/* return string to build */
	char * rv=0;

	/* disable cookie's if the user's prefs say so
	 */
	if(NET_GetCookieBehaviorPref() == NET_DontUse)
		return NULL;

	if(!strncasecomp(address, "https", 5))
		secure_path = TRUE;

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
			domain_length = XP_STRLEN(cookie_s->host);

			/* calculate the host length by looking at all characters up to
			 * a colon or '\0'.  That way we won't include port numbers
			 * in domains
			 */
			for(cp=host; *cp != '\0' && *cp != ':'; cp++)
				; /* null body */ 

			host_length = cp - host;
			if(domain_length > host_length 
				|| strncasecomp(cookie_s->host, 
								&host[host_length - domain_length], 
								domain_length))
			  {
				/* no match. FAIL 
				 */
				continue;
			  }
			
		  }
		else if(strcasecomp(host, cookie_s->host))
		  {
			/* hostname matchup failed. FAIL
			 */
			continue;
		  }

        /* shorter strings always come last so there can be no
         * ambiquity
         */
        if(cookie_s->path && !XP_STRNCMP(path,
                                         cookie_s->path,
                                         XP_STRLEN(cookie_s->path)))
          {

			/* if the cookie is secure and the path isn't
			 * dont send it
			 */
			if(cookie_s->secure && !secure_path)
				continue;  /* back to top of while */
			
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
				XP_FREEIF(rv);
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
				if(!rv || !XP_STRSTR(rv, name))
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

	  net_unlock_cookie_list();
	XP_FREEIF(name);
	XP_FREE(path);
	XP_FREE(host);

	/* may be NULL */
	return(rv);
}

/* Java script is calling NET_SetCookieString, netlib is calling 
** this via NET_SetCookieStringFromHttp.
*/
PRIVATE void
net_IntSetCookieString(MWContext * context, 
					char * cur_url,
					char * set_cookie_header,
					time_t timeToExpire)
{
	net_CookieStruct * prev_cookie;
	char *path_from_header=NULL, *host_from_header=NULL;
	char *name_from_header=NULL, *cookie_from_header=NULL;
	time_t expires=0;
	char *cur_path = NET_ParseURL(cur_url, GET_PATH_PART);
	char *cur_host = NET_ParseURL(cur_url, GET_HOST_PART);
	char *semi_colon, *ptr, *equal;
	const char *script_name;
	XP_Bool set_secure=FALSE, is_domain=FALSE, ask=FALSE, accept=FALSE;
	MWContextType type;

	if(!context) {
		XP_FREE(cur_path);
		XP_FREE(cur_host);
		return;
	}

	/* Only allow cookies to be set in the listed contexts. We
	 * don't want cookies being set in html mail. 
	 */
	type = context->type;
	if(!( (type == MWContextBrowser)
		|| (type == MWContextHTMLHelp)
		|| (type == MWContextPane) )) {
		XP_FREE(cur_path);
		XP_FREE(cur_host);
		return;
	}
	
	if(NET_GetCookieBehaviorPref() == NET_DontUse) {
		XP_FREE(cur_path);
		XP_FREE(cur_host);
		return;
	}

	/* terminate at any carriage return or linefeed */
	for(ptr=set_cookie_header; *ptr; ptr++)
		if(*ptr == LF || *ptr == CR) {
			*ptr = '\0';
			break;
		}

	/* parse path and expires attributes from header if
 	 * present
	 */
	semi_colon = XP_STRCHR(set_cookie_header, ';');

	if(semi_colon)
	  {
		/* truncate at semi-colon and advance 
		 */
		*semi_colon++ = '\0';

		/* there must be some attributes. (hopefully)
		 */
		if(strcasestr(semi_colon, "secure"))
			set_secure = TRUE;

		/* look for the path attribute
		 */
		ptr = strcasestr(semi_colon, "path=");

		if(ptr) {
			/* allocate more than we need */
			StrAllocCopy(path_from_header, XP_StripLine(ptr+5));
			/* terminate at first space or semi-colon
			 */
			for(ptr=path_from_header; *ptr != '\0'; ptr++)
				if(XP_IS_SPACE(*ptr) || *ptr == ';' || *ptr == ',') {
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
        ptr = strcasestr(semi_colon, "domain=");

        if(ptr) {
			char *domain_from_header=NULL;
			char *dot, *colon;
			int domain_length, cur_host_length;

            /* allocate more than we need */
			StrAllocCopy(domain_from_header, XP_StripLine(ptr+7));

            /* terminate at first space or semi-colon
             */
            for(ptr=domain_from_header; *ptr != '\0'; ptr++)
                if(XP_IS_SPACE(*ptr) || *ptr == ';' || *ptr == ',') {
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
            dot = XP_STRCHR(domain_from_header, '.');
			if(dot)
                dot = XP_STRCHR(dot+1, '.');

			if(!dot || *(dot+1) == '\0') {
				/* did not pass two dot test. FAIL
				 */
				XP_FREE(domain_from_header);
				XP_FREE(cur_path);
				XP_FREE(cur_host);
				TRACEMSG(("DOMAIN failed two dot test"));
				return;
			  }

			/* strip port numbers from the current host
			 * for the domain test
			 */
			colon = XP_STRCHR(cur_host, ':');
			if(colon)
			   *colon = '\0';

			domain_length   = XP_STRLEN(domain_from_header);
			cur_host_length = XP_STRLEN(cur_host);

			/* check to see if the host is in the domain
			 */
			if(domain_length > cur_host_length
				|| strcasecomp(domain_from_header, 
							   &cur_host[cur_host_length-domain_length]))
			  {
				TRACEMSG(("DOMAIN failed host within domain test."
					  " Domain: %s, Host: %s", domain_from_header, cur_host));
				XP_FREE(domain_from_header);
				XP_FREE(cur_path);
				XP_FREE(cur_host);
				return;
              }

			/* all tests passed, copy in domain to hostname field
			 */
			StrAllocCopy(host_from_header, domain_from_header);
			is_domain = TRUE;

			TRACEMSG(("Accepted domain: %s", host_from_header));

			XP_FREE(domain_from_header);
          }

		/* now search for the expires header 
		 * NOTE: that this part of the parsing
		 * destroys the original part of the string
		 */
		ptr = strcasestr(semi_colon, "expires=");

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
        char * slash = XP_STRRCHR(cur_path, '/');
        if(slash)
            *slash = '\0';

		path_from_header = cur_path;
	  } else {
		XP_FREE(cur_path);
	  }

	if(!host_from_header)
		host_from_header = cur_host;
	else
		XP_FREE(cur_host);

	/* keep cookies under the max bytes limit */
	if(XP_STRLEN(set_cookie_header) > MAX_BYTES_PER_COOKIE)
		set_cookie_header[MAX_BYTES_PER_COOKIE-1] = '\0';

	/* separate the name from the cookie */
	equal = XP_STRCHR(set_cookie_header, '=');

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

        cd = XP_NEW_ZAP(JSCFCookieData);
        if( (JSCFCookieData *)0 == cd ) {
            XP_FREEIF(path_from_header);
            XP_FREEIF(host_from_header);
            XP_FREEIF(name_from_header);
            XP_FREEIF(cookie_from_header);
            /* FREEIF(cur_path); */
            /* FREEIF(cur_host); */
            return;
        }

        cd->path_from_header = path_from_header;
        cd->host_from_header = host_from_header;
        cd->name_from_header = name_from_header;
        cd->cookie_from_header = cookie_from_header;
        cd->expires = expires;
        cd->url = cur_url;
        cd->secure = set_secure;
        cd->domain = is_domain;
        cd->prompt = NET_GetCookieWarningPref();
        cd->preference = NET_GetCookieBehaviorPref();

	/*
	 * This probably is only safe to do from the mozilla thread
	 *   since it might do file I/O and uses the preferences
	 *   global context + objects
	 * XXX chouck
	 */
        result = JSCF_Execute(context, script_name, cd, &changed);
		if( result != JSCF_error) {
			if( changed ) {
				if( cd->path_from_header != path_from_header ) {
					XP_FREEIF(path_from_header);
					path_from_header = XP_STRDUP(cd->path_from_header);
				}
				if( cd->host_from_header != host_from_header ) {
					XP_FREEIF(host_from_header);
					host_from_header = XP_STRDUP(cd->host_from_header);
				}
				if( cd->name_from_header != name_from_header ) {
					XP_FREEIF(name_from_header);
					name_from_header = XP_STRDUP(cd->name_from_header);
				}
				if( cd->cookie_from_header != cookie_from_header ) {
					XP_FREEIF(cookie_from_header);
				  cookie_from_header = XP_STRDUP(cd->cookie_from_header);
			  }
			  if( cd->expires != expires )
				  expires = cd->expires;
			  if( cd->domain != is_domain )
				  /* lets hope the luser remembered to change the domain field */
				  is_domain = cd->domain;
			  if( cd->secure != set_secure )
				  set_secure = cd->secure;
			}
			switch( result ) {
				case JSCF_reject:
					XP_FREEIF(path_from_header);
					XP_FREEIF(host_from_header);
					XP_FREEIF(name_from_header);
					XP_FREEIF(cookie_from_header);
					XP_FREE(cd);
					/* FREEIF(cur_path); */
					/* FREEIF(cur_host); */
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
		XP_FREE(cd);
    }
 

	if( (NET_GetCookieWarningPref() || ask) && !accept ) {
		/* the user wants to know about cookies so let them
		 * know about every one that is set and give them
		 * the choice to accept it or not
		 */
		char * new_string=0;
		char * tmp_host = NET_ParseURL(cur_url, GET_HOST_PART);

        StrAllocCopy(new_string, XP_GetString(MK_ACCESS_COOKIES_THE_SERVER));
        StrAllocCat(new_string, tmp_host ? tmp_host : "");
        StrAllocCat(new_string, XP_GetString(MK_ACCESS_COOKIES_WISHES));

		StrAllocCopy(new_string, XP_GetString(MK_ACCESS_COOKIES_THE_SERVER));
		StrAllocCat(new_string, tmp_host ? tmp_host : "");
		StrAllocCat(new_string, XP_GetString(MK_ACCESS_COOKIES_WISHES));

		XP_FREE(tmp_host);

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

		/* 
		 * Who knows what thread we are on.  Only the mozilla thread
		 *   is allowed to post dialogs so, if need be, go over there
		 */
		if(!ET_PostMessageBox(context, new_string, TRUE)) {
			XP_FREEIF(new_string);
			return;
		}
		XP_FREEIF(new_string);
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


    prev_cookie = net_CheckForPrevCookie(path_from_header, 
								   		 host_from_header, 
								   		 name_from_header);

    if(prev_cookie) {
        prev_cookie->expires = expires;
        XP_FREEIF(prev_cookie->cookie);
        XP_FREEIF(prev_cookie->path);
        XP_FREEIF(prev_cookie->host);
        XP_FREEIF(prev_cookie->name);
        prev_cookie->cookie = cookie_from_header;
        prev_cookie->path = path_from_header;
        prev_cookie->host = host_from_header;
        prev_cookie->name = name_from_header;
        prev_cookie->secure = set_secure;
        prev_cookie->is_domain = is_domain;
		prev_cookie->last_accessed = time(NULL);
      }	else {
		XP_List * list_ptr = net_cookie_list;
		net_CookieStruct * tmp_cookie_ptr;
		size_t new_len;

        /* construct a new cookie_struct
         */
        prev_cookie = XP_NEW(net_CookieStruct);
        if(!prev_cookie) {
			XP_FREEIF(path_from_header);
			XP_FREEIF(host_from_header);
			XP_FREEIF(name_from_header);
			XP_FREEIF(cookie_from_header);
			net_unlock_cookie_list();
            return;
          }
    
        /* copy
         */
        prev_cookie->cookie  = cookie_from_header;
        prev_cookie->name    = name_from_header;
        prev_cookie->path    = path_from_header;
        prev_cookie->host    = host_from_header;
        prev_cookie->expires = expires;
        prev_cookie->secure  = set_secure;
        prev_cookie->is_domain = is_domain;
		prev_cookie->last_accessed = time(NULL);

		if(!net_cookie_list) {
			net_cookie_list = XP_ListNew();
		    if(!net_cookie_list) {
				XP_FREEIF(path_from_header);
				XP_FREEIF(name_from_header);
				XP_FREEIF(host_from_header);
				XP_FREEIF(cookie_from_header);
				XP_FREE(prev_cookie);
				net_unlock_cookie_list();
				return;
			  }
		  }		

		/* add it to the list so that it is before any strings of
		 * smaller length
		 */
		new_len = XP_STRLEN(prev_cookie->path);
		while((tmp_cookie_ptr = (net_CookieStruct *) XP_ListNextObject(list_ptr))!=0) { 
			if(new_len > XP_STRLEN(tmp_cookie_ptr->path)) {
				XP_ListInsertObject(net_cookie_list, tmp_cookie_ptr, prev_cookie);
				net_unlock_cookie_list();
				cookies_changed = TRUE;
				return;
			  }
		  }
		/* no shorter strings found in list */	
		XP_ListAddObjectToEnd(net_cookie_list, prev_cookie);
	  }

	/* At this point we know a cookie has changed. Write the cookies to file. */
	cookies_changed = TRUE;
	NET_SaveCookies(NULL);
	net_unlock_cookie_list();
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
PRIVATE int
NET_SameDomain(char * currentHost, char * inlineHost)
{
	char * dot = 0;
	char * currentDomain = 0;
	char * inlineDomain = 0;

	if(!currentHost || !inlineHost)
		return 0;

	/* case insensitive compare */
	if(XP_STRCASECMP(currentHost, inlineHost) == 0)
		return 1;

	currentDomain = XP_STRCHR(currentHost, '.');
	inlineDomain = XP_STRCHR(inlineHost, '.');

	if(!currentDomain || !inlineDomain)
		return 0;
	
	/* check for at least two dots before continuing, if there are
	   not two dots we don't have enough information to determine
	   whether or not the inlineDomain is within the currentDomain */
	dot = XP_STRCHR(inlineDomain, '.');
	if(dot)
		dot = XP_STRCHR(dot+1, '.');
	else
		return 0;

	/* handle .com. case */
	if(!dot || (*(dot+1) == '\0') )
		return 0;

	if(!XP_STRCASECMP(inlineDomain, currentDomain))
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
				XP_FREEIF(curHost);
				XP_FREEIF(curSessionHistHost);
				return;
			}

			/* strip ports */
			theColon = XP_STRCHR(curHost, ':');
			if(theColon)
			   *theColon = '\0';
			theColon = XP_STRCHR(curSessionHistHost, ':');
			if(theColon)
				*theColon = '\0';

			/* if it's foreign, get out of here after a little clean up */
			if(!NET_SameDomain(curHost, curSessionHistHost))
			{
				XP_FREEIF(curHost);	
				XP_FREEIF(curSessionHistHost);
				return;
			}
			XP_FREEIF(curHost);	
			XP_FREEIF(curSessionHistHost);
		}
	}

	/* Determine when the cookie should expire. This is done by taking the difference between 
	   the server time and the time the server wants the cookie to expire, and adding that 
	   difference to the client time. This localizes the client time regardless of whether or
	   not the TZ environment variable was set on the client. */

	/* Get the time the cookie is supposed to expire according to the attribute*/
	ptr = strcasestr(set_cookie_header, "expires=");
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
		/* If the cookie has expired, don't waste any more time. */
		if( expires < URL_s->server_date )
		{
			return;
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

	net_lock_cookie_list();
	list_ptr = net_cookie_list;
	if(XP_ListIsEmpty(list_ptr)) {
		net_unlock_cookie_list();
		return(-1);
	}

	if(!(fp = XP_FileOpen(filename, xpHTTPCookie, XP_FILE_WRITE))) {
		net_unlock_cookie_list();
		return(-1);
	}

	len = XP_FileWrite("# Netscape HTTP Cookie File" LINEBREAK
				 "# http://www.netscape.com/newsref/std/cookie_spec.html"
				 LINEBREAK "# This is a generated file!  Do not edit."
				 LINEBREAK LINEBREAK,
				 -1, fp);
	if (len < 0)
	{
		XP_FileClose(fp);
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

		len = XP_FileWrite(cookie_s->host, -1, fp);
		if (len < 0)
		{
			XP_FileClose(fp);
			net_unlock_cookie_list();
			return -1;
		}
		XP_FileWrite("\t", 1, fp);
		
		if(cookie_s->is_domain)
			XP_FileWrite("TRUE", -1, fp);
		else
			XP_FileWrite("FALSE", -1, fp);
		XP_FileWrite("\t", 1, fp);

		XP_FileWrite(cookie_s->path, -1, fp);
		XP_FileWrite("\t", 1, fp);

		if(cookie_s->secure)
		    XP_FileWrite("TRUE", -1, fp);
		else
		    XP_FileWrite("FALSE", -1, fp);
		XP_FileWrite("\t", 1, fp);

		PR_snprintf(date_string, sizeof(date_string), "%lu", cookie_s->expires);
		XP_FileWrite(date_string, -1, fp);
		XP_FileWrite("\t", 1, fp);

		XP_FileWrite(cookie_s->name, -1, fp);
		XP_FileWrite("\t", 1, fp);

		XP_FileWrite(cookie_s->cookie, -1, fp);
 		len = XP_FileWrite(LINEBREAK, -1, fp);
		if (len < 0)
		{
			XP_FileClose(fp);
			net_unlock_cookie_list();
			return -1;
		}
      }

	cookies_changed = FALSE;

	XP_FileClose(fp);
	net_unlock_cookie_list();
    return(0);
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
	char *host, *is_domain, *path, *secure, *expires, *name, *cookie;
	Bool added_to_list;

    if(!(fp = XP_FileOpen(filename, xpHTTPCookie, XP_FILE_READ)))
        return(-1);

	net_lock_cookie_list();
	list_ptr = net_cookie_list;

    /* format is:
     *
     * host \t is_domain \t path \t secure \t expires \t name \t cookie
     *
	 * if this format isn't respected we move onto the next line in the file.
     * is_domain is TRUE or FALSE	-- defaulting to FALSE
     * secure is TRUE or FALSE   -- should default to TRUE
     * expires is a time_t integer
     * cookie can have tabs
     */
    while(XP_FileReadLine(buffer, LINE_BUFFER_SIZE, fp))
      {
		added_to_list = FALSE;

		if (*buffer == '#' || *buffer == CR || *buffer == LF || *buffer == 0)
		  continue;

		host = buffer;
		
		if( !(is_domain = XP_STRCHR(host, '\t')) )
			continue;
		*is_domain++ = '\0';
		if(*is_domain == CR || *is_domain == LF || *is_domain == 0)
			continue;
		
		if( !(path = XP_STRCHR(is_domain, '\t')) )
			continue;
		*path++ = '\0';
		if(*path == CR || *path == LF || *path == 0)
			continue;

		if( !(secure = XP_STRCHR(path, '\t')) )
			continue;
		*secure++ = '\0';
		if(*secure == CR || *secure == LF || *secure == 0)
			continue;

		if( !(expires = XP_STRCHR(secure, '\t')) )
			continue;
		*expires++ = '\0';
		if(*expires == CR || *expires == LF || *expires == 0)
			continue;

        if( !(name = XP_STRCHR(expires, '\t')) )
			continue;
		*name++ = '\0';
		if(*name == CR || *name == LF || *name == 0)
			continue;

        if( !(cookie = XP_STRCHR(name, '\t')) )
			continue;
		*cookie++ = '\0';
		if(*cookie == CR || *cookie == LF || *cookie == 0)
			continue;

		/* remove the '\n' from the end of the cookie */
		XP_StripLine(cookie);

        /* construct a new cookie_struct
         */
        new_cookie = XP_NEW(net_CookieStruct);
        if(!new_cookie)
          {
			net_unlock_cookie_list();
            return(-1);
          }

		XP_MEMSET(new_cookie, 0, sizeof(net_CookieStruct));
    
        /* copy
         */
        StrAllocCopy(new_cookie->cookie, cookie);
        StrAllocCopy(new_cookie->name, name);
        StrAllocCopy(new_cookie->path, path);
        StrAllocCopy(new_cookie->host, host);
        new_cookie->expires = atol(expires);
		if(!XP_STRCMP(secure, "FALSE"))
        	new_cookie->secure = FALSE;
		else
        	new_cookie->secure = TRUE;
        if(!XP_STRCMP(is_domain, "TRUE"))
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
		new_len = XP_STRLEN(new_cookie->path);
		while((tmp_cookie_ptr = (net_CookieStruct *) XP_ListNextObject(list_ptr)) != NULL)
		  { 
			if(new_len > XP_STRLEN(tmp_cookie_ptr->path))
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

    XP_FileClose(fp);
	net_unlock_cookie_list();

	cookies_changed = FALSE;

    return(0);
}



/* --- New stuff: General auth utils (currently used only by proxy auth) --- */

/*
 * Figure out the authentication scheme used; currently supported:
 *
 *		* Basic
 *		* SimpleMD5
 *
 */
PRIVATE net_AuthType
net_auth_type(char *name)
{
	if (name) {
		while (*name && XP_IS_SPACE(*name))
			name++;
		if (!strncasecomp(name, "basic", 5))
			return AUTH_BASIC;
#ifdef SIMPLE_MD5
		else if (!strncasecomp(name, "simplemd5", 9))
			return AUTH_SIMPLEMD5;
#endif
		/*FORTEZZA checks*/
		else if (!strncasecomp(name, "fortezzaproxy", 13))
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
MODULE_PRIVATE XP_Bool
net_IsBetterAuth(char *new_auth, char *old_auth)
{
	if (!old_auth || net_auth_type(new_auth) >= net_auth_type(old_auth))
		return TRUE;
	else
		return FALSE;
}


/*
 * Turns binary data of given lenght into a newly-allocated HEX string.
 *
 *
 */
PRIVATE char *
bin2hex(unsigned char *data, int len)
{
    char *buf = (char *)XP_ALLOC(2 * len + 1);
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
#define SKIP_WS(p) while((*(p)) && XP_IS_SPACE(*(p))) p++

PRIVATE XP_Bool
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
		while (*q && !XP_IS_SPACE(*q)) q++;
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
		ret = XP_NEW_ZAP(net_AuthStruct);

	if(!ret)
		return NULL;

    SKIP_WS(p);
	ret->auth_type = net_auth_type(p);
    while (*p && !XP_IS_SPACE(*p)) p++;

    while (next_params(&p, &name, &value)) {
		if (!strcasecomp(name, "realm"))
		  {
			  StrAllocCopy(ret->realm, value);
		  }
#ifdef SIMPLE_MD5
		else if (!strcasecomp(name, "domain"))
		  {
			  StrAllocCopy(ret->domain, value);
		  }
		else if (!strcasecomp(name, "nonce"))
		  {
			  StrAllocCopy(ret->nonce, value);
		  }
		else if (!strcasecomp(name, "opaque"))
		  {
			  StrAllocCopy(ret->opaque, value);
		  }
		else if (!strcasecomp(name, "oldnonce"))
		  {
			  ret->oldNonce = (!strcasecomp(value, "TRUE")) ? TRUE : FALSE;
		  }
#endif /* SIMPLE_MD5 */
		/* Some FORTEZZA checks */
		else if (!strcasecomp(name, "challenge"))
		  {
			  StrAllocCopy(ret->challenge, value);
		  }
		else if (!strcasecomp(name, "oldchallenge"))
		  {
			  ret->oldChallenge = (!strcasecomp(value, "TRUE")) ? TRUE : FALSE;
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
	/*End FPRTEZZA  addition   */
    return ret;
}


#ifdef SIMPLE_MD5
/* ---------- New stuff: SimpleMD5 Authentication for proxies -------------- */

PRIVATE void do_md5(unsigned char *stuff, unsigned char digest[16])
{
	MD5Context *cx = MD5_NewContext();
	unsigned int len;

	if (!cx)
		return;

	MD5_Begin(cx);
	MD5_Update(cx, stuff, strlen((char*)stuff));
	MD5_End(cx, digest, &len, 16);	/* len had better be 16 when returned! */

	MD5_DestroyContext(cx, (DSBool)TRUE);
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
	  (unsigned char *)XP_ALLOC(strlen(challenge) + strlen(password) +
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

			len = XP_STRLEN(u_pass_string);
			if (!(auth_s->auth_string = (char*) XP_ALLOC((((len+1)*4)/3)+20)))
			  {
				  return NULL;
			  }

			XP_STRCPY(auth_s->auth_string, "Basic ");
			NET_UUEncode((unsigned char *)u_pass_string,
						 (unsigned char *)&auth_s->auth_string[6],
						 len);

			XP_FREE(u_pass_string);
		}
		break;

#ifdef SIMPLE_MD5
	  case AUTH_SIMPLEMD5:
	    if (auth_s->username && auth_s->password &&
			auth_s->nonce    && auth_s->opaque   &&
			url_s            && url_s->address)
		  {
			  char *resp;

			  XP_FREEIF(auth_s->auth_string);
			  auth_s->auth_string = NULL;

			  if ((resp = net_generate_md5_challenge_response(auth_s->nonce,
															  auth_s->password,
															  url_s->method,
															  url_s->address)))
				{
					if ((auth_s->auth_string =
						 (char *)XP_ALLOC(XP_STRLEN(auth_s->username) +
										  XP_STRLEN(auth_s->realm)    +
										  XP_STRLEN(auth_s->nonce)    +
										  XP_STRLEN(resp)             +
										  XP_STRLEN(auth_s->opaque)   +
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
					XP_FREE(resp);
				}
		  }
		break;
#endif /* SIMPLE_MD5 */
		/* Handle the FORTEZZA case        */
	        case AUTH_FORTEZZA:
		  if (auth_s->signature && auth_s->challenge && 
		      auth_s->certChain && auth_s->clientRan) {
		          int len;

			  XP_FREEIF(auth_s->auth_string);
			  auth_s->auth_string = NULL;

			  len = XP_STRLEN(auth_s->signature) + XP_STRLEN(auth_s->challenge)
			        + XP_STRLEN(auth_s->certChain) + XP_STRLEN(auth_s->clientRan) + 100;
			  auth_s->auth_string = (char *)XP_ALLOC(len);
			  if (auth_s->auth_string) {
				sprintf(auth_s->auth_string,"signature=\"%s\" challenge=\"%s\" "
					"clientRan=\"%s\" certChain=\"%s\"",auth_s->signature,
					auth_s->challenge, auth_s->clientRan, auth_s->certChain);
			  }
		  }
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
		  if (!strcasecomp(s->proxy_addr, proxy_addr))
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

PUBLIC XP_Bool
NET_AskForProxyAuth(MWContext * context,
					char *   proxy_addr,
					char *   pauth_params,
					XP_Bool  already_sent_auth)
{
	net_AuthStruct * prev;
	XP_Bool new_entry = FALSE;
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
		rv = SECNAV_ComputeFortezzaProxyChallengeResponse(context,
														  prev->challenge,
														  &prev->signature,
														  &prev->clientRan,
														  &prev->certChain);
		if ( rv != SECSuccess ) {
			return(FALSE);
		}
    } else
    {
	username = prev->username;
	password = prev->password;

	len = XP_STRLEN(prev->realm) + XP_STRLEN(proxy_addr) + 50;
	buf = (char*)XP_ALLOC(len*sizeof(char));
	
	if(buf)
	  {
		PR_snprintf(buf, len*sizeof(char), XP_GetString( XP_PROXY_AUTH_REQUIRED_FOR ), prev->realm, proxy_addr);

		NET_Progress(context, XP_GetString( XP_CONNECT_PLEASE_ENTER_PASSWORD_FOR_PROXY ) );
		len = FE_PromptUsernameAndPassword(context, buf, 
									       &username, &password);
		XP_FREE(buf);
	  }
	else
	  {
		len = 0;
	  }

	if (!len)
	  {
		  TRACEMSG(("User canceled login!!!"));
		  return FALSE;
	  }
	else if (!username || !password)
	  {
		  return FALSE;
	  }

	XP_FREEIF(prev->auth_string);
	prev->auth_string = NULL;		/* Generate a new one */
	XP_FREEIF(prev->username);
	prev->username = username;
	XP_FREEIF(prev->password);
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

#define BUFLEN 2048
/* create an HTML stream and push a bunch of HTML about cookies. Such as:
 * GENERAL INFO
 * The number of cookies you have in mem (expired cookies don't show up).
 * The maximum number allowed.
 * The maximum allowed per server.
 * The maximum allowable size of a cookie (in bytes).
 *
 * PER COOKIE INFO
 * Cookie name & Value.
 * The host it came from.
 * Whether or not there's a domain.
 * The path the cookie will be sent to.
 * Whether or not the cookie is sent secure.
 * When the cookie expires.
 * 
 */
MODULE_PRIVATE void 
NET_DisplayCookieInfoAsHTML(ActiveEntry * cur_entry) {
	char *buffer=(char*)XP_ALLOC(BUFLEN), *expireDate=NULL;
   	NET_StreamClass *stream;
	int i, g, numOfCookies;
	XP_List *list=net_cookie_list;
	net_CookieStruct *cookie;

	if(!buffer) {
		cur_entry->status = MK_UNABLE_TO_CONVERT;
		return;
	  }

	/* The the current entry's content type */
	StrAllocCopy(cur_entry->URL_s->content_type, TEXT_HTML);

	cur_entry->format_out = CLEAR_CACHE_BIT(cur_entry->format_out);
	stream = NET_StreamBuilder(cur_entry->format_out, 
							   cur_entry->URL_s, 
							   cur_entry->window_id);

	if(!stream) {
		cur_entry->status = MK_UNABLE_TO_CONVERT;
		XP_FREE(buffer);
		return;
	  }


/* define a macro to push a string up the stream and handle errors */
#define PUT_PART(part)													\
cur_entry->status = (*stream->put_block)(stream,			\
										part ? part : "Unknown",		\
										part ? XP_STRLEN(part) : 7);	\
	if(cur_entry->status < 0)												\
	  goto END;
/* End PUT_PART macro */

	/* Get rid of any expired cookies now so user doesn't
	 * think/see that we're keeping cookies in mem.
	 */
	net_remove_expired_cookies();
	numOfCookies=XP_ListCount(net_cookie_list);

	/* Write out the initial statistics. */
	g = PR_snprintf(buffer, BUFLEN,
"<TITLE>%s</TITLE>\n"
"<h2>%s</h2>\n"
"<TABLE>\n"
"<TR>\n",
		XP_GetString(MK_ACCESS_YOUR_COOKIES),
		XP_GetString(MK_ACCESS_YOUR_COOKIES));

	g += PR_snprintf(buffer+g, BUFLEN-g,
"<TD ALIGN=RIGHT><b>%s</TD>\n"
"<TD>%ld</TD>\n"
"</TR>\n"
"<TR>\n",
		XP_GetString(MK_ACCESS_MAXIMUM_COOKS),
		MAX_NUMBER_OF_COOKIES);

	g += PR_snprintf(buffer+g, BUFLEN-g,
"<TD ALIGN=RIGHT><b>%s</TD>\n"
"<TD>%ld</TD>\n"
"</TR>\n"
"<TR>\n",
		XP_GetString(MK_ACCESS_COOK_COUNT),
		numOfCookies);

	g += PR_snprintf(buffer+g, BUFLEN-g,
"<TD ALIGN=RIGHT><b>%s</TD>\n"
"<TD>%ld</TD>\n"
"</TR>\n"
"<TR>\n",
		XP_GetString(MK_ACCESS_MAXIMUM_COOKS_PER_SERV),
		MAX_COOKIES_PER_SERVER);

	g += PR_snprintf(buffer+g, BUFLEN-g,
"<TD ALIGN=RIGHT><b>%s</TD>\n"
"<TD>%ld</TD>\n"
"</TR>\n"
"</TABLE>\n"
"<HR>",
		XP_GetString(MK_ACCESS_MAXIMUM_COOK_SIZE),
		MAX_BYTES_PER_COOKIE);


	PUT_PART(buffer);

	if(!numOfCookies) {
		XP_STRCPY(buffer, XP_GetString(MK_ACCESS_NO_COOKIES));
		PUT_PART(buffer);
		goto END;
	  }

/* define some macros to help us output HTML */
#define HEADING(arg1)					\
	XP_STRCPY(buffer, "<tt>");			\
	for(i=XP_STRLEN(arg1); i < 16; i++)	\
		XP_STRCAT(buffer, "&nbsp;");	\
	XP_STRCAT(buffer, arg1);			\
	XP_STRCAT(buffer, " </tt>");		\
	PUT_PART(buffer);

#define BRCRLF					\
	XP_STRCPY(buffer, "<BR>\n");		\
	PUT_PART(buffer);
/* End html macros */

	/* Write out each cookie */
	while ( (cookie=(net_CookieStruct *) XP_ListNextObject(list)) ) {

		HEADING(XP_GetString(MK_ACCESS_NAME));
		XP_STRCPY(buffer, cookie->name);
		PUT_PART(buffer);
		BRCRLF;

		HEADING(XP_GetString(MK_ACCESS_VALUE));
		XP_STRCPY(buffer, cookie->cookie);
		PUT_PART(buffer);
		BRCRLF;

		HEADING(XP_GetString(MK_ACCESS_HOST));
		XP_STRCPY(buffer, cookie->host);
		PUT_PART(buffer);
		BRCRLF;

		HEADING(XP_GetString(MK_ACCESS_SEND_TO_HOST));
		if(cookie->is_domain)
			XP_STRCPY(buffer, XP_GetString(MK_ACCESS_IS_DOMAIN));
		else
			XP_STRCPY(buffer, XP_GetString(MK_ACCESS_IS_NOT_DOMAIN));
		PUT_PART(buffer);
		BRCRLF;

		HEADING(XP_GetString(MK_ACCESS_SEND_TO_PATH));
		XP_STRCPY(buffer, cookie->path);
		PUT_PART(buffer);
		XP_STRCPY(buffer, XP_GetString(MK_ACCESS_AND_BELOW));
		PUT_PART(buffer);
		BRCRLF;

		HEADING(XP_GetString(MK_ACCESS_SECURE));
		if(cookie->secure)
			XP_STRCPY(buffer, "Yes");
		else
			XP_STRCPY(buffer, "No");
		PUT_PART(buffer);
		BRCRLF;

		HEADING(XP_GetString(MK_ACCESS_EXPIRES));
		if(cookie->expires) {
			expireDate=ctime(&(cookie->expires));
			if(expireDate)
				XP_STRCPY(buffer, expireDate);
			else
				XP_STRCPY(buffer, "NULL");
		} else {
			XP_STRCPY(buffer, XP_GetString(MK_ACCESS_END_OF_SESSION));
		}
		PUT_PART(buffer);
		XP_STRCPY(buffer, " GMT");
		PUT_PART(buffer);
		BRCRLF;

		XP_STRCPY(buffer, "\n<P>\n");
		PUT_PART(buffer);
	}
	/* End each cookie */

END:
	XP_FREE(buffer);
	if(cur_entry->status < 0)
		(*stream->abort)(stream, cur_entry->status);
	else
		(*stream->complete)(stream);
	return;
}

