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
 * mkautocf.c: Proxy auto-config parser and evaluator
 * Ari Luotonen
 *
 * Updated and Documented by Judson Valeski 11/19/1997
 */
#include "mkutils.h"    /* LF */
#include "mktcp.h"
#include "netutils.h"
#include "prsystem.h"
#include "mkpadpac.h"
#include <time.h>
#include "mkautocf.h"
#include "xp_mem.h"     /* XP_NEW_ZAP() */
#ifndef XP_MAC
#include <sys/types.h>
#endif
#include "libi18n.h"
#include "mkgeturl.h"
#include "mkstream.h"
#include "mkgeturl.h"
#include "mkformat.h"
#include "mime.h"
#include "cvactive.h"
#include "gui.h"
#include "msgcom.h"
#include "prefapi.h"
#define WANT_ENUM_STRING_IDS
#include "allxpstr.h"
#undef WANT_ENUM_STRING_IDS
#include "xp_reg.h"

#ifndef NSPR20
#if defined(XP_UNIX) || defined(XP_WIN32)
#include "prnetdb.h"
#else
#define PRHostEnt struct hostent
#endif
#else
#include "prnetdb.h"
#endif

#include "net.h"
#include "libmocha.h"
#include "jsapi.h"
#include "jscompat.h"
#include "jspubtd.h"

/* for XP_GetString() */
#include "xpgetstr.h"
#ifdef NOT
extern int XP_BAD_KEYWORD_IN_PROXY_AUTOCFG;
extern int XP_RETRY_AGAIN_PROXY;
extern int XP_RETRY_AGAIN_SOCKS;
extern int XP_RETRY_AGAIN_PROXY_OR_SOCKS;
extern int XP_PROXY_UNAVAILABLE_TRY_AGAIN;
extern int XP_ALL_PROXIES_DOWN_TRY_AGAIN;
extern int XP_ALL_SOCKS_DOWN;
extern int XP_ALL_DOWN_MIX;
extern int XP_OVERRIDE_PROXY;
extern int XP_OVERRIDE_SOCKS;
extern int XP_OVERRIDE_MIX;
extern int XP_STILL_OVERRIDE_PROXY;
extern int XP_STILL_OVERRIDE_SOCKS;
extern int XP_STILL_OVERRIDE_MIX;
extern int XP_NO_CONFIG_RECIEVED;
extern int XP_NO_CONFIG_RECEIVED_NO_FAILOVER;
extern int XP_EMPTY_CONFIG_USE_PREV;
extern int XP_BAD_CONFIG_USE_PREV;
extern int XP_BAD_CONFIG_IGNORED;
extern int XP_BAD_TYPE_USE_PREV;
extern int XP_BAD_TYPE_CONFIG_IGNORED;
extern int XP_CONF_LOAD_FAILED_IGNORED;
extern int XP_CONF_LOAD_FAILED_NO_FAILOVER;
extern int XP_CONF_LOAD_FAILED_USE_PREV;
extern int XP_EVEN_SAVED_IS_BAD;
extern int XP_CONFIG_LOAD_ABORTED;
extern int XP_CONFIG_BLAST_WARNING;
extern int XP_RECEIVING_PROXY_AUTOCFG;
extern int XP_AUTOADMIN_MISSING;
#endif

#define PACF_MIN_RETRY_AFTER    300
#define PACF_MIN_RETRY_ASK       25
#define PACF_AUTO_RETRY_AFTER  1800
#define PACF_DIRECT_INTERVAL   1200
#define PACF_RETRY_OK_AFTER      30

/* Types of retrievals: retrieve directly, thru a proxy, or using socks. */
typedef enum _PACF_Type {
    PACF_TYPE_INVALID = 0,
    PACF_TYPE_DIRECT,
    PACF_TYPE_PROXY,
    PACF_TYPE_SOCKS
} PACF_Type;

/* Info about the proxy/socks server hostname/IP address and port. 
 * These nodes are kept in a list so we don't have to parse the pac file
 * every time to gather proxy/socks info. */
typedef struct _PACF_Node {
    PACF_Type   type;   /* direct | proxy | socks */
    char *      addr;
    u_long      socks_addr;
    short       socks_port;
    int         retries;
    time_t      retrying;
    time_t      last_try;
    time_t      down_since;
} PACF_Node;

/* Private object for the proxy config loader stream. */
typedef struct _PACF_Object {
   MWContext *  context;
   int          flag;
} PACF_Object;

/* A struct for holding queued state.
 *
 * The state is saved when NET_GetURL() is called for the first time,
 * and the proxy autoconfig retrieve has to be started (and finished)
 * before the first actual document can be loaded.
 *
 * A pre-exit function for the proxy autoconfig URL restarts the
 * original URL retrieve by calling NET_GetURL() again with the
 * same parameters. */
typedef struct _PACF_QueuedState {
    URL_Struct *        URL_s;
    FO_Present_Types    output_format;
    MWContext *         window_id;
    Net_GetUrlExitFunc* exit_routine;
} PACF_QueuedState;

typedef enum {
    PACF_SECONDARY_URL,
    PACF_BINDINGS
} pc_slot;

/* Declared in mkgeturl.c. NET_GetURL uses these variables to determine
 * whether or not the pac file has been loaded. */
extern XP_Bool NET_GlobalAcLoaded;
extern XP_Bool NET_ProxyAcLoaded;

/* Private proxy auto-config variables */
#ifdef MOCHA
PRIVATE Bool    pacf_do_failover  = TRUE;
PRIVATE Bool    pacf_loading      = FALSE;
PRIVATE Bool    pacf_ok           = FALSE;
PRIVATE char *  pacf_url          = NULL;
PRIVATE char *  pacf_src_buf      = NULL;
PRIVATE int     pacf_src_len      = 0;
PRIVATE XP_List*pacf_all_nodes    = NULL;
PRIVATE time_t  pacf_direct_until = (time_t)0;
PRIVATE int     pacf_direct_cnt   = 0;
PRIVATE PACF_QueuedState *queued_state = NULL;
PRIVATE XP_List*pacf_bad_keywords = NULL;

PRIVATE Bool	pacf_find_proxy_undefined = FALSE;

/* Javascript stuff. A javascript context does the interpretation and
 * compilation of the pac file. */
PRIVATE JSPropertySpec pc_props[] = {
    {"secondary", PACF_SECONDARY_URL},
    {"bindings",  PACF_BINDINGS},
    {0}
};

PRIVATE JSClass pc_class = {
    "ProxyConfig", 0,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

/* PRIVATE JSPropertySpec no_props[1] = { { NULL } }; */
PRIVATE JSObject *proxyConfig = NULL;
PRIVATE JSObject *globalConfig = NULL;
PRIVATE JSContext *configContext = NULL;

/* The javascript methods */
MODULE_PRIVATE JSBool PR_CALLBACK proxy_weekdayRange(JSContext *mc, JSObject *obj, unsigned int argc, jsval *argv, jsval *rval);
MODULE_PRIVATE JSBool PR_CALLBACK proxy_dateRange(JSContext *mc, JSObject *obj, unsigned int argc, jsval *argv, jsval *rval);
MODULE_PRIVATE JSBool PR_CALLBACK proxy_timeRange(JSContext *mc, JSObject *obj, unsigned int argc, jsval *argv, jsval *rval);
MODULE_PRIVATE JSBool PR_CALLBACK proxy_isPlainHostName(JSContext *mc, JSObject *obj, unsigned int argc, jsval *argv, jsval *rval);
MODULE_PRIVATE JSBool PR_CALLBACK proxy_dnsDomainLevels(JSContext *mc, JSObject *obj, unsigned int argc, jsval *argv, jsval *rval);
MODULE_PRIVATE JSBool PR_CALLBACK proxy_dnsDomainIs(JSContext *mc, JSObject *obj, unsigned int argc, jsval *argv, jsval *rval);
MODULE_PRIVATE JSBool PR_CALLBACK proxy_localHostOrDomainIs(JSContext *mc, JSObject *obj, unsigned int argc, jsval *argv, jsval *rval);
MODULE_PRIVATE JSBool PR_CALLBACK proxy_isResolvable(JSContext *mc, JSObject *obj, unsigned int argc, jsval *argv, jsval *rval);
MODULE_PRIVATE JSBool PR_CALLBACK proxy_dnsResolve(JSContext *mc, JSObject *obj, unsigned int argc, jsval *argv, jsval *rval);
MODULE_PRIVATE JSBool PR_CALLBACK proxy_isInNet(JSContext *mc, JSObject *obj, unsigned int argc, jsval *argv, jsval *rval);
MODULE_PRIVATE JSBool PR_CALLBACK proxy_myIpAddress(JSContext *mc, JSObject *obj, unsigned int argc, jsval *argv, jsval *rval);
MODULE_PRIVATE JSBool PR_CALLBACK proxy_regExpMatch(JSContext *mc, JSObject *obj, unsigned int argc, jsval *argv, jsval *rval);

PRIVATE JSFunctionSpec pc_methods[] = {
    { "weekdayRange",        proxy_weekdayRange,        3},
    { "dateRange",           proxy_dateRange,           7},
    { "timeRange",           proxy_timeRange,           7},
    { "isPlainHostName",     proxy_isPlainHostName,     1},
    { "dnsDomainLevels",     proxy_dnsDomainLevels,     1},
    { "dnsDomainIs",         proxy_dnsDomainIs,         2},
    { "localHostOrDomainIs", proxy_localHostOrDomainIs, 2},
    { "isResolvable",        proxy_isResolvable,        1},
    { "dnsResolve",          proxy_dnsResolve,          1},
    { "isInNet",             proxy_isInNet,             3},
    { "myIpAddress",         proxy_myIpAddress,         0},
    { "regExpMatch",         proxy_regExpMatch,         2},
    { "shExpMatch",          proxy_regExpMatch,         2},
    { NULL,                  NULL,                      0}
};

PRIVATE char *mkMethodString( int method ) {
  char *mstr= NULL;

  switch ( method ) {
	  case  URL_GET_METHOD: 
		mstr = XP_STRDUP("GET");
		break;
	  case URL_POST_METHOD:
		mstr = XP_STRDUP("POST");
		break;
	  case  URL_HEAD_METHOD:
		mstr = XP_STRDUP("HEAD");
		break;
	  case  URL_PUT_METHOD:
		mstr = XP_STRDUP("PUT");
		break;
	  case  URL_DELETE_METHOD:
		mstr = XP_STRDUP("DELETE");
		break;
	  case  URL_MKDIR_METHOD :
		mstr = XP_STRDUP("MKDIR");
		break;
	  case  URL_MOVE_METHOD:
		mstr = XP_STRDUP("MOVE");
		break;
	  case  URL_INDEX_METHOD:
		mstr = XP_STRDUP("INDEX");
		break;
	  case  URL_GETPROPERTIES_METHOD:
		mstr = XP_STRDUP("GETPROPERTIES");
		break;
  }
  return mstr;
}

PRIVATE char *msg2(char *fmt, char *prm) {
    char *msg = (char *)XP_ALLOC(XP_STRLEN(fmt) + 100 +
				 (prm ? XP_STRLEN(prm) : 0));
    if (msg)
	XP_SPRINTF(msg, fmt, prm ? prm : "-");

    return msg;
}

PRIVATE Bool confirm2(MWContext *context, char *fmt, char *prm) {
    Bool rv = TRUE;
    char *msg = msg2(fmt, prm);
    if (msg) {
	rv = FE_Confirm(context, msg);
	XP_FREE(msg);
    }
    return rv;
}

PRIVATE void alert2(MWContext *context, char *fmt, char *prm) {
    char *msg = msg2(fmt, prm);
    if (msg) {
	FE_Alert(context, msg);
	XP_FREE(msg);
    }
}

/* Returns a node representing a proxy/SOCKS server address or
 * direct access, and guarantees to return the same node if one
 * was already created for that address. */
PRIVATE PACF_Node *lookup_create_node(PACF_Type type, char *addr) {
    if (!pacf_all_nodes)
	pacf_all_nodes = XP_ListNew();

    /* Truncate at 64 characters -- gethostbyname() is evil */
    if (addr && XP_STRLEN(addr) > 64)
	addr[64] = '\0';

    if (!type || (!addr && (type == PACF_TYPE_PROXY ||
			    type == PACF_TYPE_SOCKS)))
      {
	  return NULL;
      }
    else
      {
	  XP_List *cur = pacf_all_nodes;
	  PACF_Node *node;

	  while ((node = (PACF_Node *)XP_ListNextObject(cur)) != NULL) {
	      if (node->type == type &&
		  ((!node->addr && !addr) ||
		   ( node->addr &&  addr  && !strcmp(node->addr, addr))))
		{
		    return node;
		}
	  }

	  node = XP_NEW_ZAP(PACF_Node);
      if (node) {
	      node->type = type;
	      node->addr = XP_STRDUP(addr);

	      XP_ListAddObject(pacf_all_nodes, node);
      }

	  return node;
      }
}

/* Called by netlib when it notices that a proxy is down. */
PRIVATE PACF_Node * pacf_proxy_is_down(MWContext *context,
				       char *proxy_addr,
				       u_long socks_addr,
				       short socks_port) {
    XP_List *cur = pacf_all_nodes;
    PACF_Node *node;

    while ((node = (PACF_Node *)XP_ListNextObject(cur)) != NULL) {
	if ((proxy_addr && node->addr && !XP_STRCMP(node->addr, proxy_addr)) ||
	    (socks_addr &&
	     node->socks_addr == socks_addr &&
	     node->socks_port == socks_port))
	  {
	      time_t now = time(NULL);

	      if (!node->down_since) {

		  node->down_since = now;
	      }
	      node->last_try = now;
	      node->retrying = (time_t)0;
	      return node;
	  }
    }
    return NULL;
}

PRIVATE Bool fill_return_values(PACF_Type   type,
				PACF_Node * node,
				char **     ret_proxy_addr,
				u_long *    ret_socks_addr,
				short *     ret_socks_port) {
    *ret_proxy_addr = NULL;
    *ret_socks_addr = 0;
    *ret_socks_port = 0;

	if(!node)
		return FALSE;

    if (type == PACF_TYPE_PROXY) {
	*ret_proxy_addr = node->addr;
    }
    else if (!node->socks_addr) {
	char *p, *host = NULL;

	StrAllocCopy(host, node->addr);
	p = XP_STRCHR(host, ':');
	if (p) {
	    *p++ = '\0';
	    node->socks_port = (short)atoi(p);
	}
	else {
	    node->socks_port = 1080;
	}

	if (isdigit(*host)) {
	    /*
	     * Some systems return a number instead of a struct
	     */
	    node->socks_addr = inet_addr(host);
	}
	else {
#ifdef NSPR20
		PRStatus rv;
	    PRHostEnt  *hp;
	    PRHostEnt  hpbuf;
	    char dbbuf[PR_NETDB_BUF_SIZE];
#else
	    struct hostent  *hp;
#if defined(XP_UNIX) || defined(XP_WIN32) 
	    struct hostent hpbuf;
	    char dbbuf[PR_NETDB_BUF_SIZE];
#endif
#endif /* NSPR20 */

	    NET_InGetHostByName++; /* global semaphore */
#ifdef NSPR20
	    rv = PR_GetHostByName(host, dbbuf, sizeof(dbbuf),  &hpbuf);
	    hp = (rv == PR_SUCCESS ? &hpbuf : NULL);
#elif defined(XP_UNIX) || defined(XP_WIN32)
	    hp = PR_gethostbyname(host, &hpbuf, dbbuf, sizeof(dbbuf), 0); 
#else
	    hp = gethostbyname(host); 
#endif
	    NET_InGetHostByName--; /* global semaphore */

	    if (!hp)
		return FALSE;  /* Fail? */

	    XP_MEMCPY(&node->socks_addr, hp->h_addr, hp->h_length);
	}
    }

    *ret_socks_addr = node->socks_addr;
    *ret_socks_port = node->socks_port;
    return TRUE;
}

#endif /* MOCHA */

/* NET_SetNoProxyFailover
 * Sets a flag that indicates that proxy failover should not
 * be done.  This is used by the Enterprise Kit code, where
 * the customer might configure the client to use specific
 * proxies.  In these cases, failover can cause different
 * proxies, or no proxies to be used. */
PUBLIC void
NET_SetNoProxyFailover(void) {
#ifdef MOCHA
  pacf_do_failover = FALSE;
#endif
}

PUBLIC XP_Bool
NET_LoadingPac(void) {
	return pacf_loading;
}

#ifdef MOZILLA_CLIENT
/*
 * NET_GetNoProxyFailover
 */
MODULE_PRIVATE Bool
NET_GetNoProxyFailover(void) {
  return ( pacf_do_failover == FALSE );
}
#endif /* MOZILLA_CLIENT */

/* Called by netlib to get a single string of "host:port" format,
 * given the string of possibilities returned by the Mocha script.
 *
 * This function will return an address to a proxy that is (to its
 * knowledge) up and running.  Netlib can later inform this module
 * using the function pacf_proxy_is_down() that a proxy is down
 * and should not be called for a few minutes.
 *
 * Returns FALSE if everything has failed, and an error should be
 * displayed to the user.
 *
 * Returns TRUE if there is hope.
 * If *ret is NULL, a direct connection should be attempted.
 * If *ret is not null, it is the proxy address to use. */
MODULE_PRIVATE Bool
pacf_get_proxy_addr(MWContext *context, char *list, char **ret_proxy_addr,
		    u_long *ret_socks_addr, short *ret_socks_port) {
#ifdef MOCHA
    Bool rv = FALSE;
    char *my_copy, *cur, *p, *addr;
    PACF_Type type = PACF_TYPE_INVALID;
    PACF_Node *node;
    PACF_Node *sm_ptr = NULL;
    PACF_Node *went_down = NULL;
    time_t now = time(NULL);
    time_t smallest = (time_t)0;
    int proxy_cnt = 0;
    int socks_cnt = 0;
    int retry_now = 0;

    if (!list || !*list || !ret_proxy_addr || !ret_socks_addr || !ret_socks_port)
	return FALSE;

    if (*ret_proxy_addr || *ret_socks_addr) {
	/*
	 * We get called here because this proxy/SOCKS server was down.
	 */
	went_down = pacf_proxy_is_down(context, *ret_proxy_addr,
				       *ret_socks_addr, *ret_socks_port);
    }

    *ret_proxy_addr = NULL;
    *ret_socks_addr = 0;
    *ret_socks_port = 0;

    cur = my_copy = XP_STRDUP(list);

    TRACEMSG(("Getting proxy addr from config list: %s", list));

	do {
	    p = XP_STRCHR(cur, ';');
		if (p) {
		    do {
				*p++ = '\0';
		} while (*p && XP_IS_SPACE(*p));
		}

		for (addr=cur; *addr && !XP_IS_SPACE(*addr); addr++)
			;
		if (*addr) {
		    do {
			*addr++ = '\0';
		    } while (*addr &&  XP_IS_SPACE(*addr));
		}

		type = ((!strcasecomp(cur, "DIRECT")) ? PACF_TYPE_DIRECT  :
			(!strcasecomp(cur, "PROXY"))  ? PACF_TYPE_PROXY   :
			(!strcasecomp(cur, "SOCKS"))  ? PACF_TYPE_SOCKS   :
							PACF_TYPE_INVALID );

		if (type == PACF_TYPE_DIRECT)           /* don't use a proxy */
		  {
		      rv = TRUE;
		      break;                            /* done */
		  }
		else if (type ==  PACF_TYPE_PROXY ||    /* use a proxy or... */       
			 type ==  PACF_TYPE_SOCKS )     /* use SOCKS */
		  {
		      if (type == PACF_TYPE_PROXY)
			  proxy_cnt++;
		      else
			  socks_cnt++;

	      node = lookup_create_node(type, addr);
		      if (*addr && node) {
			  if (node->down_since && node->retrying &&
			      now - node->retrying > PACF_RETRY_OK_AFTER)
			    {
				node->down_since = node->retrying = (time_t)0;
			    }

			  if (!node->down_since)
			    {
				if (fill_return_values(type, node,
						       ret_proxy_addr,
						       ret_socks_addr,
						       ret_socks_port))
				  {
				      rv = TRUE;
				      break;
				  }
			    }
			  else if (now - node->last_try >
				   (node->retries + 1) * PACF_AUTO_RETRY_AFTER)
			    {
				node->retries++;
				if (fill_return_values(type, node,
						       ret_proxy_addr,
						       ret_socks_addr,
						       ret_socks_port))
				  {
				      node->retrying = now;
				      rv = TRUE;
				      break;
				  }
			    }
			  else if (node != went_down &&
				   (!smallest || smallest > node->last_try))
			    {
				smallest = node->last_try;
				sm_ptr = node;
			    }
		      }
		  }
		else                                    /* invalid entry */
		  {
		    char *key = NULL;

		    if (!pacf_bad_keywords)
			{
			    pacf_bad_keywords = XP_ListNew();
			}
		    else
			{
			    XP_List *ptr = pacf_bad_keywords;

			    while ((key = (char *)XP_ListNextObject(ptr)) != NULL) {
				if (!strcmp(key, cur))
				    break;
			    }
			}

		      if (!key && XP_ListCount(pacf_bad_keywords) < 3)
			{
			    key = XP_STRDUP(cur);
			    XP_ListAddObject(pacf_bad_keywords, key);
			    alert2(context, XP_GetString(XP_BAD_KEYWORD_IN_PROXY_AUTOCFG), cur);
			}
		  }

		cur = p;

	    } while (cur && *cur);

    XP_FREE(my_copy);

    if (rv)
	  return TRUE;

	/* If we didn't return just above here, and we're using a pad pac file,
	 * something went wrong with it so disable it for this session and return
	 * true so we go direct. */
	if(NET_UsingPadPac()) {
		net_UsePadPac(FALSE);
		return TRUE;
	}

    if (pacf_do_failover && pacf_direct_until) {
	  if (now < pacf_direct_until)
	    {
		return TRUE;
	    }
	  else if (!FE_Confirm(context,
			       !socks_cnt ? XP_GetString(XP_RETRY_AGAIN_PROXY) :
			       !proxy_cnt ? XP_GetString(XP_RETRY_AGAIN_SOCKS) : XP_GetString(XP_RETRY_AGAIN_PROXY_OR_SOCKS)))
	    {
		pacf_direct_until = now + (++pacf_direct_cnt) * PACF_DIRECT_INTERVAL;
		return TRUE;
	    }
	  else
	    {
		pacf_direct_until = (time_t)0;
		retry_now = 1;
	    }
	}

    if (smallest &&
	(retry_now ||
	 now - smallest > PACF_MIN_RETRY_AFTER ||
	 (sm_ptr && !sm_ptr->retrying &&
	  now - smallest > PACF_MIN_RETRY_ASK &&
	  confirm2(context,
		   (!socks_cnt && proxy_cnt==1 ? XP_GetString(XP_PROXY_UNAVAILABLE_TRY_AGAIN) :
		    !socks_cnt && proxy_cnt >1 ? XP_GetString(XP_ALL_PROXIES_DOWN_TRY_AGAIN) :
		    !proxy_cnt                 ? XP_GetString(XP_ALL_SOCKS_DOWN) :
						 XP_GetString(XP_ALL_DOWN_MIX)),
		   sm_ptr->addr))) &&
	fill_return_values(type, sm_ptr,
			   ret_proxy_addr, ret_socks_addr, ret_socks_port))
      {
	  sm_ptr->retrying = now;
	  sm_ptr->retries++;
	  return TRUE;
      }
    else if (pacf_do_failover && FE_Confirm(context,
			!pacf_direct_cnt ?
			(!socks_cnt ? XP_GetString(XP_OVERRIDE_PROXY) :
			 !proxy_cnt ? XP_GetString(XP_OVERRIDE_SOCKS) : XP_GetString(XP_OVERRIDE_MIX)) :
			(!socks_cnt ? XP_GetString(XP_STILL_OVERRIDE_PROXY) :
			 !proxy_cnt ? XP_GetString(XP_STILL_OVERRIDE_SOCKS) : XP_GetString(XP_STILL_OVERRIDE_SOCKS))))
      {
	  pacf_direct_until = now + (++pacf_direct_cnt) * PACF_DIRECT_INTERVAL;
	  return TRUE;
      }
    else
      {
	  return FALSE;
      }
#else
	return FALSE;
#endif /* MOCHA */
}

#ifdef MOCHA


/* Saves out the proxy autoconfig file to disk, in case the server
 * is down the next time.
 *
 * Returns 0 on success, -1 on failure. */
PRIVATE int pacf_save_config(void) {
    XP_File fp;
	int32 len = 0;

    if (!pacf_do_failover || !pacf_src_buf || !*pacf_src_buf || pacf_src_len <= 0)
	return -1;

    if(!(fp = XP_FileOpen("", xpProxyConfig, XP_FILE_WRITE)))
	return -1;

    len = XP_FileWrite(pacf_src_buf, pacf_src_len, fp);
    XP_FileClose(fp);
	if (len != pacf_src_len)
		return -1;

    return 0;
}

/* Reads the proxy autoconfig file from disk.
 * This is called if the config server is not responding.
 *
 * returns 0 on success -1 on failure. */
PRIVATE int pacf_read_config(void) {
    XP_StatStruct st;
    XP_File fp;

    if (XP_Stat("", &st, xpProxyConfig) == -1)
	return -1;

    if (!(fp = XP_FileOpen("", xpProxyConfig, XP_FILE_READ)))
	return -1;

    pacf_src_len = st.st_size;
    pacf_src_buf = (char *)XP_ALLOC(pacf_src_len + 1);
    if (!pacf_src_buf) {
	XP_FileClose(fp);
	pacf_src_len = 0;
	return -1;
    }

    if ((pacf_src_len = XP_FileRead(pacf_src_buf, pacf_src_len, fp)) > 0)
      {
	  pacf_src_buf[pacf_src_len] = '\0';
      }
    else
      {
	  XP_FREE(pacf_src_buf);
	  pacf_src_buf = NULL;
	  pacf_src_len = 0;
      }

    XP_FileClose(fp);

    return 0;
}

/* Private stream object methods for receiving the proxy autoconfig file. */
PRIVATE int pacf_write(NET_StreamClass *stream, CONST char *buf, int32 len) {
	PACF_Object *obj=stream->data_object;	
    if (len > 0) {
	if (!pacf_src_buf)
	    pacf_src_buf = (char*)XP_ALLOC(len + 1);
	else
	    pacf_src_buf = (char*)XP_REALLOC(pacf_src_buf,
					     pacf_src_len + len + 1);

	if (!pacf_src_buf) {    /* Out of memory */
	    pacf_src_len = 0;
	    return MK_DATA_LOADED;
	}

	XP_MEMCPY(pacf_src_buf + pacf_src_len, buf, len);
	pacf_src_len += len;
	pacf_src_buf[pacf_src_len] = '\0';
    }
    return MK_DATA_LOADED;
}

PRIVATE unsigned int pacf_write_ready(NET_StreamClass *stream) {	
    return MAX_WRITE_READY;
}

PRIVATE void pacf_complete(NET_StreamClass *stream) {
    PACF_Object *obj=stream->data_object;
	jsval result;
    XP_StatStruct st;
    JSBool ok;	

retry:
    if (!obj->flag || obj->flag == 2) {
	pacf_loading = FALSE;


	/*  JONM -- Check me over please!
	  proxyConfig =
	  MOCHA_DefineNewObject(decoder->js_context, decoder->window_object,
				"ProxyConfig", &pc_class, NULL, NULL, 0,
				pc_props, pc_methods); */
	PREF_GetConfigContext(&configContext);
	PREF_GetGlobalConfigObject(&globalConfig);

	proxyConfig = JS_DefineObject(configContext, globalConfig, 
						"ProxyConfig",
						&pc_class, 
						NULL, 
						JSPROP_ENUMERATE);
	if (proxyConfig) {
		if (!JS_DefineProperties(configContext,
					 proxyConfig,
					 pc_props)) {
		return;
		}

		if (!JS_DefineFunctions(configContext,
					proxyConfig,
					pc_methods)) {
		return;
		}

	}

	JS_DefineObject(configContext, proxyConfig, 
					"bindings",
					&pc_class, 
					NULL, 
					0);
	/*MOCHA_DefineNewObject(decoder->js_context, proxyConfig, "bindings",
			      &pc_class, NULL, NULL, 0, no_props, 0); */

    }

    if (!pacf_src_buf) {
	if ( pacf_do_failover == FALSE && !NET_UsingPadPac() ) {
	  /* Don't failover to using no proxies */
	     FE_Alert(obj->context, XP_GetString(XP_NO_CONFIG_RECEIVED_NO_FAILOVER));
	}
	else if (obj->flag != 2 && (XP_Stat("", &st, xpProxyConfig) == -1) && !NET_UsingPadPac())
	  {
			FE_Alert(obj->context, XP_GetString(XP_NO_CONFIG_RECIEVED));
	  }
	else
	{
		if(NET_UsingPadPac()) {
			pacf_read_config();
			obj->flag = 1;
			goto retry;
		} else if (obj->flag == 2 ||
			confirm2(obj->context, XP_GetString(XP_EMPTY_CONFIG_USE_PREV), pacf_url)) {
			pacf_read_config();
			obj->flag = 1;
			goto retry;
		}
	}
	pacf_ok = FALSE;
	goto out;
    }

		ok = JS_EvaluateScript(configContext, proxyConfig, 
			   pacf_src_buf, pacf_src_len, pacf_url, 0,
			   &result);

    if (!ok) {
		/* Something went wrong with the js evaluation. If we're using a
		 * proxy auto-discovery pac file, don't use it again. */
		if(NET_UsingPadPac()) {
			net_UsePadPac(FALSE);
			goto out;
		}
		if (obj->flag) {
				FE_Alert(obj->context, XP_GetString(XP_EVEN_SAVED_IS_BAD));
		} else if (XP_Stat("", &st, xpProxyConfig) == -1) {
				alert2(obj->context, XP_GetString(XP_BAD_CONFIG_IGNORED), pacf_url);
		} else if (confirm2(obj->context, XP_GetString(XP_BAD_CONFIG_USE_PREV), pacf_url)) {
			  XP_FREE(pacf_src_buf);
			  pacf_src_buf = NULL;
			  pacf_src_len = 0;
			  pacf_read_config();
			  obj->flag = 1;            /* Uh (this is a horrid hack) */
			  goto retry;
		}
		pacf_ok = FALSE;
		} else {
		pacf_ok = TRUE;
		if (!obj->flag) {
			pacf_save_config();
		}
    }

    XP_FREE(obj);

out:
	;
}

PRIVATE void pacf_abort(NET_StreamClass *stream, int status) {
	PACF_Object *obj=stream->data_object;	
    pacf_loading = FALSE;
		FE_Alert(obj->context, XP_GetString(XP_GLOBAL_CONFIG_LOAD_ABORTED));
    XP_FREE(obj);
}

#endif /* MOCHA */

/* A stream constructor function for application/x-ns-proxy-autoconfig. 
 * This is used by cvmime.c; it's registered as the stream converter for
 * pac files. */
MODULE_PRIVATE NET_StreamClass *
NET_ProxyAutoConfig(int fmt, void *data_obj, URL_Struct *URL_s, 
					MWContext *w) {
#ifdef MOCHA
    PACF_Object         *obj;
    NET_StreamClass     *stream;

    if (!pacf_loading) {
	/*
	 * The Navigator didn't start this config retrieve
	 * intentionally.  Discarding the config.
	 */
	if(!URL_s)
		return NULL;
	alert2(w, XP_GetString(XP_CONFIG_BLAST_WARNING), URL_s->address);
	return NULL;
    }
    else {
	NET_Progress(w, XP_GetString( XP_RECEIVING_PROXY_AUTOCFG ) ); 
    }

    if (pacf_src_buf) {
	XP_FREE(pacf_src_buf);
	pacf_src_buf = NULL;
	pacf_src_len = 0;
    }

    if (!(stream = XP_NEW_ZAP(NET_StreamClass)))
	return NULL;

    if (!(obj = XP_NEW_ZAP(PACF_Object))) {
	XP_FREE(stream);
	return NULL;
    }

    obj->context = w;

    stream->data_object         = obj;
    stream->name                = "ProxyAutoConfigLoader";
    stream->complete            = (MKStreamCompleteFunc)  pacf_complete;
    stream->abort               = (MKStreamAbortFunc)     pacf_abort;
    stream->put_block           = (MKStreamWriteFunc)     pacf_write;
    stream->is_write_ready      = (MKStreamWriteReadyFunc)pacf_write_ready;
    stream->window_id           = w;

    return stream;
#else   /* ! MOCHA */
    return NULL;
#endif  /* ! MOCHA */
}

#ifdef MOCHA


/* calls NET_GetURL() to get the url that had been queued up behind the 
 * actual pac file url. */
static void pacf_restart_queued(URL_Struct *URL_s, int status,
				MWContext *window_id) {
    XP_StatStruct st;

	/* We want to fail silently if we're loading a pad pac and fail,
	 * so fall through to bottom and load the queued url. */
    if (pacf_loading && !(NET_UsingPadPac() || foundPADPAC) ) {
	
	if ( pacf_do_failover == FALSE ) {
	  /* Don't failover to using no proxies */
	     FE_Alert(window_id, XP_GetString(XP_CONF_LOAD_FAILED_NO_FAILOVER));
	}
	else if (XP_Stat("", &st, xpProxyConfig) == -1)
	  {
	      if (status < 0)
		  FE_Alert(window_id, XP_GetString(XP_CONF_LOAD_FAILED_IGNORED));
	      else {
				alert2(window_id, XP_GetString(XP_BAD_TYPE_CONFIG_IGNORED), pacf_url);
		  }
	  }
	else if (status == MK_INTERRUPTED)
	  {
		  /* silently fail and retry later */
		  NET_ProxyAcLoaded = FALSE;
		  NET_GlobalAcLoaded = FALSE;
	  }
	else if (status < 0
		 ? FE_Confirm(window_id, XP_GetString(XP_CONF_LOAD_FAILED_USE_PREV))
		 : confirm2(window_id, XP_GetString(XP_BAD_TYPE_USE_PREV), pacf_url))
	  {
	      PACF_Object *obj = XP_NEW_ZAP(PACF_Object);
		  NET_StreamClass stream;
		  stream.data_object=obj;

	      if (obj) {
		  obj->context = window_id;
		  obj->flag = 2;
		  pacf_complete(&stream);
	      }
	  }

	pacf_loading = FALSE;
    } else {
		/* Call this only if everything succeeded -- otherwise
		   the netlib has already freed the URL_s, and there isn't
		   a clean fix, so for now we'll just forget the URL load
		   if proxy config load went foul.
		 */
		if(queued_state) {
			NET_GetURL(queued_state->URL_s,
				   queued_state->output_format,
				   queued_state->window_id,
				   queued_state->exit_routine);
		}
	}
	XP_FREEIF(queued_state);
	queued_state = NULL;
}

#endif /* MOCHA */


/* Called by mkgeturl.c to originally retrieve, and re-retrieve
 * the proxy autoconfig file.
 *
 * autoconfig_url is the URL pointing to the autoconfig.
 *
 * The rest of the parameters are what was passed to NET_GetURL(),
 * and when the autoconfig load finishes NET_GetURL() will be called
 * with those exact same parameters.
 *
 * This is because the proxy config is loaded when NET_GetURL() is
 * called for the very first time, and the actual request must be put
 * on hold when the proxy config is being loaded.
 *
 * When called from the explicit proxy config RE-load function
 * NET_ReloadProxyConfig, the four last parameters are all zero,
 * and no request gets restarted. */
MODULE_PRIVATE int NET_LoadProxyConfig(char *autoconf_url,
				       URL_Struct *URL_s,
				       FO_Present_Types output_format,
				       MWContext *window_id,
				       Net_GetUrlExitFunc *exit_routine) {
    URL_Struct *my_url_s = NULL;
	char * global_url = NULL;

    if (!autoconf_url)
	return -1;

	if (!XP_STRCMP(autoconf_url,"BAD-NOAUTOADMNLIB")) {
	    FE_Alert(window_id, XP_GetString( XP_AUTOADMIN_MISSING ));
		return -1;
	}

    StrAllocCopy(pacf_url, autoconf_url);
    my_url_s = NET_CreateURLStruct(autoconf_url, NET_SUPER_RELOAD);

    
	if (exit_routine) {

	queued_state = XP_NEW_ZAP(PACF_QueuedState);
	if(!queued_state)
		return -1;
	queued_state->URL_s = URL_s;
	queued_state->output_format = output_format;
	queued_state->window_id = window_id;
	queued_state->exit_routine = exit_routine;

	if(!my_url_s)
		return -1;

	my_url_s->pre_exit_fn = pacf_restart_queued;
    }

    /* Alert the proxy autoconfig module that config is coming */
    pacf_loading = TRUE;
    pacf_find_proxy_undefined = FALSE;

    return NET_GetURL(my_url_s, FO_PRESENT, window_id, NULL);
}

/* Returns a pointer to a NULL-terminted buffer which contains
 * the text of the proxy autoconfig file. */
PUBLIC char * NET_GetProxyConfigSource(void) {
#ifdef MOCHA
    return pacf_src_buf;
#else
    return 0;
#endif
}


 /* Given a URL, returns the proxy that should be used.
 * The proxy is determined by the proxy autoconfig file,
 * which is a JavaScript routine. */
MODULE_PRIVATE char *pacf_find_proxies_for_url(MWContext *context, 
											   URL_Struct *URL_s ) {
#ifdef MOCHA
    jsval rv;
    char *buf = NULL;
    char *host = NULL;
    char *p, *q, *r;
    int i, len = 0;
    char *safe_url = NULL;
    char *orig_url = NULL;
    char *method = NULL;
    char *bad_url = NULL;
    char *result = NULL;
    JSBool ok;

	if(!URL_s)
		return NULL;

	orig_url=URL_s->address;
	method=mkMethodString(URL_s->method);

    /* If proxy failover is not allowed, and we weren't
     * able to autoload the proxy, return a string that
     * pacf_get_proxy_addr will always fail with. */
    if ( !pacf_do_failover && !pacf_loading && !pacf_ok ) {
      return "";
    }

    if (!orig_url || !pacf_ok || pacf_loading || pacf_find_proxy_undefined)
	return NULL;

    if (!(bad_url = XP_STRDUP(orig_url)))
	goto out;

    len = NET_UnEscapeCnt(bad_url);

    if (!(safe_url = XP_ALLOC(2 * len + 1)))    /* worst case */
	goto out;

    p = bad_url;
    q = safe_url;
    for(i=0; i<len; i++, p++) {
	switch (*p) {
	  case '\n':
	    *q++ = '\\';
	    *q++ = 'n';
	    break;
	  case '\r':
	    *q++ = '\\';
	    *q++ = 'r';
	    break;
	  case '\0':
	    *q++ = '\\';
	    *q++ = '0';
	    break;
	  case '"':
	    *q++ = '\\';
	    *q++ = '"';
	    break;
	  case '\\':
	    *q++ = '\\';
	    *q++ = '\\';
	    break;
	  default:
	    *q++ = *p;
	}
    }
    *q = '\0';

    len  = (int)(q - safe_url);

    if (!(host = XP_ALLOC(len + 1)))
	goto out;

    if (!(buf  = XP_ALLOC(len*2 + 50)))
	goto out;

    host[0] = '\0';

    p = XP_STRSTR(safe_url, "://");
    if (p) {
	p += 3;
	q = XP_STRCHR(p, '/');
	if (q)
	    *q = '\0';
	r = XP_STRCHR(p, '@');
	if (r)
		p = r + 1;
	XP_STRCPY(host, p);
	if (q)
	    *q = '/';
	p = XP_STRCHR(host, ':');
	if (p)
	    *p = '\0';
    }
	    XP_SPRINTF(buf, "FindProxyForURL(\"%s\",\"%s\",\"%s\")", safe_url, host,
	       method ? method : "" );

    if (!JS_AddRoot(configContext, &rv))
	goto out;

		ok = JS_EvaluateScript(configContext, proxyConfig,
			   buf, strlen(buf), 0, 0, &rv);

    if (ok) {
	if (JSVAL_IS_STRING(rv)) {
	    const char *name =
		JS_GetStringBytes(JSVAL_TO_STRING(rv));
	    if (*name)
		result = XP_STRDUP(name);
	}
    }

    JS_RemoveRoot(configContext, &rv);
out:
    FREEIF(method);
    FREEIF(buf);
    FREEIF(host);
    FREEIF(safe_url);
    FREEIF(bad_url);
    return result;

#else   /* ! MOCHA */

    return NULL;
#endif  /* ! MOCHA */
}

#ifdef MOCHA

/* Utility functions to be called from Javascript (aka Mocha).
 * These are the actual implementations of the javascript fucntions that
 * are in the javascript pac file.
 *
 * Get the number of DNS domain levels (number of dots):
 *
 *      dnsDomainLevels(host)
 *
 *
 * Host/URL based conditions:
 *
 *      isPlainHostName(host)
 *      dnsDomainIs(host, name)
 *      localHostOrDomainIs(host, name)
 *      isResolvable(host)
 *      regExpMatch(host or URL, regexp)
 *
 *
 * Date/time based conditions (limits inclusive):
 *
 *      weekdayRange(wkday)
 *      weekdayRange(wkday, wkday)
 *
 *      dateRange(day)
 *      dateRange(day, day)
 *      dateRange(mon)
 *      dateRange(mon, mon)
 *      dateRange(year)
 *      dateRange(year, year)
 *      dateRange(day, mon, day, mon)
 *      dateRange(mon, year, mon, year)
 *      dateRange(day, mon, year, day, mon, year)
 *
 *      timeRange(hour)
 *      timeRange(hour, hour)
 *      timeRange(hour, min, hour, min)
 *      timeRange(hour, min, sec, hour, min, sec)
 *
 *
 *
 */

/* Returns true if the hostname doesn't contain a dot
 * (is a plain hostname, not an FQDN).
 *
 * Just a string operation, doesn't consult DNS. */
MODULE_PRIVATE JSBool PR_CALLBACK
proxy_isPlainHostName(JSContext *mc, JSObject *obj, unsigned int argc, 
					  jsval *argv, jsval *rval) {
    if (argc >= 1 && JSVAL_IS_STRING(argv[0])) {
	const char *h = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));

	if (h && !XP_STRCHR(h, '.')) {
	    *rval = JSVAL_TRUE;
	    return JS_TRUE;
	}
    }

    *rval = JSVAL_FALSE;
    return JS_TRUE;
}

/* Returns the number of domain levels in the hostname
 * (the number of dots, actually).
 *
 * Just a string operation, doesn't consult DNS. */
MODULE_PRIVATE JSBool PR_CALLBACK
proxy_dnsDomainLevels(JSContext *mc, JSObject *obj, unsigned int argc, 
					  jsval *argv, jsval *rval) {
    int i = 0;

    if (argc >= 1 && JSVAL_IS_STRING(argv[0])) {
	const char *h = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));

	if (h) {
	    while (*h) {
		if (*h == '.')
		    i++;
		h++;
	    }
	}
    }

    *rval = INT_TO_JSVAL(i);
    return JS_TRUE;
}

/* Checks if the hostname contains the given domain. */
MODULE_PRIVATE JSBool PR_CALLBACK
proxy_dnsDomainIs(JSContext *mc, JSObject *obj, unsigned int argc, 
				  jsval *argv, jsval *rval) {
    if (argc >= 2 && JSVAL_IS_STRING(argv[0]) && JSVAL_IS_STRING(argv[1])) {
	const char *h = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	const char *p = JS_GetStringBytes(JSVAL_TO_STRING(argv[1]));
	int len1, len2;

	if (h && p && (len1 = XP_STRLEN(h)) >= (len2 = XP_STRLEN(p)) &&
	    (!strcasecomp(h + len1 - len2, p))) {
	    *rval = JSVAL_TRUE;
	    return JS_TRUE;
	}
    }

    *rval = JSVAL_FALSE;
    return JS_TRUE;
}

/* Returns true if the hostname matches exactly the given
 * pattern hostname, or if the hostname is just a local hostname
 * and it matches the hostname in the pattern FQDN hostname. */
MODULE_PRIVATE JSBool PR_CALLBACK
proxy_localHostOrDomainIs(JSContext *mc, JSObject *obj, unsigned int argc, 
						  jsval *argv, jsval *rval) {
    if (argc >= 2 && JSVAL_IS_STRING(argv[0]) && JSVAL_IS_STRING(argv[1])) {
	const char *h = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	const char *p = JS_GetStringBytes(JSVAL_TO_STRING(argv[1]));
	char *hp, *pp;

	if (h && p) {
	    hp = XP_STRCHR(h, '.');
	    pp = XP_STRCHR(p, '.');

	    if (hp || !pp) {
		if (!strcasecomp(h,p)) {
		    *rval = JSVAL_TRUE;
		    return JS_TRUE;
		}
	    }
	    else if (!strncasecomp(h, p, pp - p)) {
		*rval = JSVAL_TRUE;
		return JS_TRUE;
	    }
	}
    }

    *rval = JSVAL_FALSE;
    return JS_TRUE;
}

/* Attempts to resolve the DNS name, and returns TRUE if resolvable. */
MODULE_PRIVATE JSBool PR_CALLBACK
proxy_isResolvable(JSContext *mc, JSObject *obj, unsigned int argc, 
				   jsval *argv, jsval *rval) {
    if (argc >= 1 && JSVAL_IS_STRING(argv[0])) {
	const char *h = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	PRHostEnt *hp = NULL;
#ifdef NSPR20
	PRStatus rv;
    PRHostEnt hpbuf;
    char dbbuf[PR_NETDB_BUF_SIZE];
#elif defined(XP_UNIX) || defined(XP_WIN32)
	PRHostEnt hpbuf;
	char dbbuf[PR_NETDB_BUF_SIZE];
#endif

	if (h) {
	    char *safe = XP_STRDUP(h);
	    if (XP_STRLEN(safe) > 64)
		safe[64] = '\0';
#ifdef NSPR20
	    rv = PR_GetHostByName(safe, dbbuf, sizeof(dbbuf),  &hpbuf);
	    hp = (rv == PR_SUCCESS ? &hpbuf : NULL);
#elif defined(XP_UNIX) || defined(XP_WIN32)
	    hp = PR_gethostbyname(safe, &hpbuf, dbbuf, sizeof(dbbuf), 0);
#else
	    hp = gethostbyname(safe);
#endif
	    XP_FREE(safe);
	}

	if (hp) {
	    TRACEMSG(("~~~~~~~~~~~ isResolvable(%s) returns TRUE\n", h));
	    TRACEMSG(("~~~~~~~~~~~ hp = %p, hp->h_name = %s\n", hp, hp->h_name ? hp->h_name : "(null)"));
	    *rval = JSVAL_TRUE;
	    return JS_TRUE;
	}
    }

    TRACEMSG(("~~~~~~~~~~~ isResolvable(%s) returns FALSE\n", argv[0]));
    *rval = JSVAL_FALSE;
    return JS_TRUE;
}

/* Resolves a DNS name, and returns the IP address string.
 *
 * Maintains a private cache for the last resolved address (so this
 * function can be called multiple times with the same host argument
 * without doing actual DNS queries every time). */
PRIVATE char *proxy_dns_resolve(const char *host) {
    static char *cache_host = NULL;
    static char *cache_ip = NULL;
#ifdef NSPR20
	PRStatus rv;
    PRHostEnt *hp = NULL;
    PRHostEnt hpbuf;
    char dbbuf[PR_NETDB_BUF_SIZE];
#else
    struct hostent *hp = NULL;
#if defined(XP_UNIX) || defined(XP_WIN32)
    struct hostent hpbuf;
    char dbbuf[PR_NETDB_BUF_SIZE];
#endif
#endif /* xNSPR20 */

    if (host) {
	const char *p;
	char *safe = NULL;
	XP_Bool is_numeric_ip = TRUE;

	for(p=host; *p; p++) {
	    if (!XP_IS_DIGIT(*p) && *p != '.') {
		is_numeric_ip = FALSE;
		break;
	    }
	}
	if (is_numeric_ip) {
	    return XP_STRDUP(host);
	}

	if (cache_host && cache_ip && !XP_STRCMP(cache_host, host)) {
	    return XP_STRDUP(cache_ip);
	}
	safe = XP_STRDUP(host);
	if (safe) {
	    if (XP_STRLEN(safe) > 64)
		safe[64] = '\0';

#ifdef NSPR20
	    rv = PR_GetHostByName(safe, dbbuf, sizeof(dbbuf),  &hpbuf);
	    hp = (rv == PR_SUCCESS ? &hpbuf : NULL);
#elif defined(XP_UNIX) || defined(XP_WIN32)
	    hp = PR_gethostbyname(safe, &hpbuf, dbbuf, sizeof(dbbuf), 0);
#else
	    hp = gethostbyname(safe);
#endif
	    XP_FREE(safe);
	}
	if (hp) {
	    char *ip = NULL;
	    struct in_addr in;

	    XP_MEMCPY(&in.s_addr, hp->h_addr, hp->h_length);

	    ip = inet_ntoa(in);
	    if (ip) {
		StrAllocCopy(cache_host, host);
		StrAllocCopy(cache_ip, ip);

		return XP_STRDUP(ip);
	    }
	}
    }

    return NULL;
}

MODULE_PRIVATE JSBool PR_CALLBACK
proxy_dnsResolve(JSContext *mc, JSObject *obj, unsigned int argc, 
				 jsval *argv, jsval *rval) {
    if (argc >= 1 && JSVAL_IS_STRING(argv[0])) {
	const char *host = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	char *ip = proxy_dns_resolve(host);

	if (ip) {
	    JSString * str = JS_NewString(mc, ip, strlen(ip));
	    if (!str) {
		XP_FREE(ip);
		return JS_FALSE;
	    }
	    *rval = STRING_TO_JSVAL(str);
	    return JS_TRUE;
	}
    }

    *rval = JSVAL_NULL;
    return JS_TRUE;
}

/* Returns the IP address of the host machine as a string. */
MODULE_PRIVATE JSBool PR_CALLBACK
proxy_myIpAddress(JSContext *mc, JSObject *obj, unsigned int argc, 
				  jsval *argv, jsval *rval) {
    static XP_Bool initialized = FALSE;
    static char *my_address = NULL;

    if (!initialized) {
	char name[100];

	initialized = TRUE;
#ifndef NSPR20
	if (gethostname(name, sizeof(name)) == 0) {
	    my_address = proxy_dns_resolve(name);
	}
#else
	if (PR_GetSystemInfo(PR_SI_HOSTNAME, name, sizeof(name)) == PR_SUCCESS) {
	    my_address = proxy_dns_resolve(name);
	}
#endif
    }

    TRACEMSG(("~~~~~~~~~~~~~~~~~~ myIpAddress() returns %s\n", my_address ? my_address : "(null)"));

    if (my_address) {
        JSString *str;
	str = JS_NewStringCopyZ(mc, my_address);
	if (!str)
	    return JS_FALSE;
	*rval = STRING_TO_JSVAL(str);
	return JS_TRUE;
    }

    *rval = JSVAL_NULL;
    return JS_TRUE;
}

/* Determines if the host IP address belongs to the given network.
 * Uses SOCKS style address pattern and mask:
 *
 *      isInNet(hostname, "111.222.33.0", "255.255.255.0"); */
PRIVATE unsigned long convert_addr(const char *ip) {
    char *p, *q, *buf = NULL;
    int i;
    unsigned char b[4];
    unsigned long addr = 0L;

	p = buf = XP_STRDUP(ip);
    if (ip && p) {
	for(i=0; p && i<4; i++) {
	    q = XP_STRCHR(p, '.');
	    if (q) {
		*q = '\0';
	    }
	    b[i] = XP_ATOI(p) & 0xff;
	    if (q) {
		p = q+1;
	    }
	}
	addr = (((unsigned long)b[0] << 24) |
		((unsigned long)b[1] << 16) |
		((unsigned long)b[2] <<  8) |
		((unsigned long)b[3]));

	XP_FREE(buf);
    }

    return htonl(addr);
}

MODULE_PRIVATE JSBool PR_CALLBACK
proxy_isInNet(JSContext *mc, JSObject *obj, unsigned int argc, 
			  jsval *argv, jsval *rval) {
    if (argc >= 3 &&
	JSVAL_IS_STRING(argv[0]) &&
	JSVAL_IS_STRING(argv[1]) &&
	JSVAL_IS_STRING(argv[2]))
      {
	  const char *ipstr = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	  char *ip = proxy_dns_resolve(ipstr);
	  const char *patstr = JS_GetStringBytes(JSVAL_TO_STRING(argv[1]));
	  const char *maskstr = JS_GetStringBytes(JSVAL_TO_STRING(argv[2]));

	  if (ip) {
	      unsigned long host = convert_addr(ip);
	      unsigned long pat  = convert_addr(patstr);
	      unsigned long mask = convert_addr(maskstr);

	      XP_FREE(ip);

	      if ((mask & host) == (mask & pat)) {
		  TRACEMSG(("~~~~~~~~~~~~ isInNet(%s(%s), %s, %s) returns TRUE\n",
			    ipstr, ip, patstr, maskstr));
		    *rval = JSVAL_TRUE;
		    return JS_TRUE;
	      }
	  }
      }

    TRACEMSG(("~~~~~~~~~~~~ isInNet() returns FALSE\n"));

    *rval = JSVAL_FALSE;
    return JS_TRUE;
}

/* Does a regular expression match between the host/URL and the pattern.
 *
 * Returns true on match. */
MODULE_PRIVATE JSBool PR_CALLBACK
proxy_regExpMatch(JSContext *mc, JSObject *obj, unsigned int argc, 
				  jsval *argv, jsval *rval) {
    if (argc >= 2 && JSVAL_IS_STRING(argv[0]) && JSVAL_IS_STRING(argv[1])) {
	const char *url = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	const char *pat = JS_GetStringBytes(JSVAL_TO_STRING(argv[1]));

	if (url && pat && XP_RegExpValid((char *) pat) &&       /* XXX */
	    !XP_RegExpMatch((char *) url, (char *) pat, TRUE)) {/* XXX */
	    *rval = JSVAL_TRUE;
	    return JS_TRUE;
	}
    }

    *rval = JSVAL_FALSE;
    return JS_TRUE;
}

PRIVATE
struct tm * get_struct_tm(JSContext *mc,
			  unsigned int *argc, jsval *argv) {
    time_t now = time(NULL);

    if (*argc > 0 &&  JSVAL_IS_STRING(argv[*argc-1])) {
	const char *laststr = JS_GetStringBytes(JSVAL_TO_STRING(argv[*argc-1]));
	if (!strcasecomp(laststr, "GMT")) {
	    (*argc)--;
	    return gmtime(&now);
	}
    }
    return localtime(&now);
}

char *weekdays = "SUNMONTUEWEDTHUFRISAT";
char *monnames = "JANFEBMARAPRMAYJUNJULAUGSEPOCTNOVDEC";

PRIVATE int get_no(const char *nam, char *arr) {
    char *p = strcasestr(arr, nam);
    return p ? (((int)(p - arr)) / 3) : -1;
}

PRIVATE int get_em(JSContext *mc, int argc, jsval *argv,
		   int *d, int *m, int *y) {
    int i=0;

    *m = *d = *y = -1;

    for (i=0; i<argc; i++) {
	if (JSVAL_IS_STRING(argv[i])) {
	    *m = get_no(JS_GetStringBytes(JSVAL_TO_STRING(argv[i])), monnames);
	} 
	else if (JSVAL_IS_NUMBER(argv[i])) {
	    if (JSVAL_TO_INT(argv[i]) > 1900)
		*y = JSVAL_TO_INT(argv[i]) - 1900;
	    else
		*d = JSVAL_TO_INT(argv[i]);
	} 
	else {
	    assert(0);
	}
    }
    return ((*d > -1 ? 1 : 0) +
	    (*m > -1 ? 1 : 0) +
	    (*y > -1 ? 1 : 0));
}

PRIVATE int are_different(JSContext *mc, jsval *argv) {
    int d1, d2, m1, m2, y1, y2;

    get_em(mc, 1,  argv,    &d1, &m1, &y1);
    get_em(mc, 1, &argv[1], &d2, &m2, &y2);

    return ((d1 == -1 || d2 == -1) &&
	    (m1 == -1 || m2 == -1) &&
	    (y1 == -1 || y2 == -1));
}

PRIVATE long get_rel(int sel_d, int sel_m, int sel_y, int d, int m, int y) {
    return ((sel_d > -1 ? d : 0) +
	    (sel_m > -1 ? m : 0) * 31 +
	    (sel_y > -1 ? y : 0) * 372);
}

PRIVATE int cmp_properly(int sel_y, long rel_lo, long rel, long rel_hi) {
    if (sel_y || rel_lo < rel_hi)
	return rel_lo <= rel && rel <= rel_hi;
    else
	return rel_lo <= rel || rel <= rel_hi;
}

MODULE_PRIVATE JSBool PR_CALLBACK
proxy_dateRange(JSContext *mc, JSObject *obj, unsigned int argc, 
				jsval *argv, jsval *rval) {
    int d1, d2, m1, m2, y1, y2;
    struct tm *tms = get_struct_tm(mc, &argc, argv);

    d1 = d2 = m1 = m2 = y1 = y2 = -1;

    if (argc == 1 || argc == 3 || (argc==2 && are_different(mc, argv)))
      {
	  *rval = ((get_em(mc, argc, argv, &d1, &m1, &y1) &&
		   (d1 == -1 || d1 == tms->tm_mday)  &&
		   (m1 == -1 || m1 == tms->tm_mon)   &&
		   (y1 == -1 || y1 == tms->tm_year))
		  ? JSVAL_TRUE : JSVAL_FALSE);
      }
    else
      {
	  *rval = (((get_em(mc, argc/2,  argv,         &d1, &m1, &y1) ==
		     get_em(mc, argc/2, &argv[argc/2], &d2, &m2, &y2))   &&
		   cmp_properly(y1,
				get_rel(d1, m1, y1, d1, m1, y1),
				get_rel(d1, m1, y1, tms->tm_mday, tms->tm_mon, tms->tm_year),
				get_rel(d1, m1, y1, d2, m2, y2)))
		  ? JSVAL_TRUE : JSVAL_FALSE);
      }
    return JS_TRUE;
}

MODULE_PRIVATE JSBool PR_CALLBACK
proxy_weekdayRange(JSContext *mc, JSObject *obj, unsigned int argc, 
				   jsval *argv, jsval *rval) {
    struct tm *tms = get_struct_tm(mc, &argc, argv);
    int i=0, j=0;

    if (argc >= 1)
	i = get_no(JS_GetStringBytes(JSVAL_TO_STRING(argv[0])), weekdays);
    if (argc >= 2)
	j = get_no(JS_GetStringBytes(JSVAL_TO_STRING(argv[1])), weekdays);

    *rval = (((argc == 1 && i == tms->tm_wday) ||
	      (argc == 2 && ((i <= j && (i <= tms->tm_wday && j >= tms->tm_wday)) ||
			    (i >  j && (i <= tms->tm_wday || j >= tms->tm_wday)))))
	    ? JSVAL_TRUE : JSVAL_FALSE);
    return JS_TRUE;
}

/* timeRange(<hr>)
 * timeRange(<hr>, <hr>)
 * timeRange(<hr>, <min>, <hr>, <min>)
 * timeRange(<hr>, <min>, <sec>, <hr>, <min>, <sec>) */
MODULE_PRIVATE JSBool PR_CALLBACK
proxy_timeRange(JSContext *mc, JSObject *obj, unsigned int argc, 
				jsval *argv, jsval *rval) {
	int32 secondsA=0, secondsB=0, secondsC=0;
    struct tm *tms = get_struct_tm(mc, &argc, argv);

	if(argc == 1) {
		secondsA = JSVAL_TO_INT(argv[0])*60*60;
		secondsB = tms->tm_hour*60*60;
		*rval = (secondsA == secondsB) ? JSVAL_TRUE : JSVAL_FALSE;
		return JS_TRUE;
	}
	if(argc == 2) {
		secondsA = JSVAL_TO_INT(argv[0])*60*60;
		secondsB = tms->tm_hour*60*60;
		secondsC = JSVAL_TO_INT(argv[1])*60*60;
	}
	if(argc == 4) {
		secondsA = JSVAL_TO_INT(argv[0])*60*60 + JSVAL_TO_INT(argv[1])*60;
		secondsB = tms->tm_hour*60*60 + tms->tm_min*60;
		secondsC = JSVAL_TO_INT(argv[2])*60*60 + JSVAL_TO_INT(argv[3])*60;
	}
	if(argc == 6) {
		secondsA = JSVAL_TO_INT(argv[0])*60*60 + JSVAL_TO_INT(argv[1])*60 + JSVAL_TO_INT(argv[2]);
		secondsB = tms->tm_hour*60*60 + tms->tm_min*60 + tms->tm_sec;
		secondsC = JSVAL_TO_INT(argv[3])*60*60 + JSVAL_TO_INT(argv[4])*60 + JSVAL_TO_INT(argv[5]);
	}
	*rval = (secondsA <= secondsB && secondsB < secondsC) ? JSVAL_TRUE : JSVAL_FALSE;
	return JS_TRUE;
}

#endif /* MOCHA */
