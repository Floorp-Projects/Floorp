/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * generalized portable TCP routines
 *
 * Handles calls to async connect and dns routines.
 *
 * Designed and originally implemented by Lou Montulli '94
 * Modified by Judson Valeski '97
 */

#include "rosetta.h"
#include "mkutils.h"
#include "netutils.h"
#include "prerror.h"
#include "prsystem.h"
#include "prefapi.h"
#include "mkpadpac.h"
#include "mkprefs.h"

#if defined(XP_WIN)
#define ASYNC_DNS
#endif

#include "mktcp.h"
#include "mkparse.h"
#include "mkgeturl.h"  /* for error codes, and some util functions */
#include "fe_proto.h" /* for externs */
#ifdef MOZILLA_CLIENT
#include "secnav.h"
#endif /* MOZILLA_CLIENT */
#include "merrors.h"

#include "ssl.h"
#include "xp_error.h"

#if defined(XP_OS2) /*DSR072196 - use os2sock.h*/
#include "os2sock.h"
#endif /* XP_OS2 */

#include "timing.h"

#if defined(SMOOTH_PROGRESS)
#include "progress.h"
#endif

#ifdef XP_UNIX
/* #### WARNING, this is duplicated in mksockrw.c
 */
# include <sys/ioctl.h>
/*
 * mcom_db.h is only included here to set BYTE_ORDER !!!
 * MAXDNAME is pilfered right out of arpa/nameser.h.
 */
# include "mcom_db.h"

# if defined(__hpux) || defined(_HPUX_SOURCE)
#  define BYTE_ORDER BIG_ENDIAN
#  define MAXDNAME        256             /* maximum domain name */
# else
#  include <arpa/nameser.h>
# endif

#include <resolv.h>

#if defined(HAVE_SYS_FILIO_H)
#include <sys/filio.h>
#endif

#endif /* XP_UNIX */

/* for XP_GetString() */
#include "xpgetstr.h"
extern int MK_CONNECTION_REFUSED;
extern int MK_CONNECTION_TIMED_OUT;
extern int MK_OUT_OF_MEMORY;
extern int MK_UNABLE_TO_CONNECT;
extern int MK_UNABLE_TO_CREATE_SOCKET;
extern int MK_UNABLE_TO_LOCATE_HOST;
extern int MK_UNABLE_TO_LOCATE_SOCKS_HOST;
extern int XP_ERRNO_EALREADY;
extern int XP_ERRNO_ECONNREFUSED;
extern int XP_ERRNO_EINPROGRESS;
extern int XP_ERRNO_EISCONN;
extern int XP_ERRNO_ETIMEDOUT;
extern int XP_ERRNO_EWOULDBLOCK;
extern int XP_PROGRESS_CONTACTHOST;
extern int XP_PROGRESS_LOOKUPHOST;
extern int XP_PROGRESS_NOCONNECTION;
extern int XP_PROGRESS_UNABLELOCATE;
extern int MK_PORT_ACCESS_NOT_ALLOWED;

/* An awful hack. setupSocks gets set to true after the first call to BeginConnect.
 * We don't want to be setting up the socks host/port (a call to gethostbyname) 
 * until we've gone all the way through the netscape initialization.
 */
static setupSocks=FALSE;

typedef enum {
	NET_TCP_FINISH_DNS_LOOKUP,
	NET_TCP_FINISH_CONNECT
} TCPStatesEnum;

struct _TCP_ConData {
	TCPStatesEnum       next_state;  /* states of the machine */
	PRNetAddr           net_addr;
	HG93765
	time_t              begin_time;
};

/* MAX_DNS_LIST_SIZE controls the cache of resolved hosts.
 * If it is undefined, the cache will grow without bound.
 * If it is defined and >0, then the cache will be limited
 * to that many entries.  If it is 0, there will be no cache.
 *
 * also needed for DNS failover...
 */
# define MAX_DNS_LIST_SIZE 10

/* Global TCP connect variables
 */
PUBLIC unsigned long NET_SocksHost=0;
PUBLIC short NET_SocksPort=0;
PUBLIC char * NET_SocksHostName=0;

PUBLIC PRBool socksFailure=FALSE;

PUBLIC int NET_InGetHostByName = FALSE; /* global semaphore */

/* The struct used to define a DNS cache entry */
typedef struct _DNSEntry {
    char      *hostname;
	PRUint32  *ips;
	int32      addressCount;
	int        h_length;
	time_t     expirationTime;
} DNSEntry;
   
PRIVATE XP_List * dns_list=0;

PRIVATE int32 dnsCacheExpiration=0;

#define DEFAULT_TCP_CONNECT_TIMEOUT 35  /* seconds */
PRIVATE uint32 net_tcp_connect_timeout = DEFAULT_TCP_CONNECT_TIMEOUT;/*seconds*/

/* set the number of seconds for the tcp connect timeout
 *
 * this timeout is used to end connections that do not
 * timeout on there own */
PUBLIC void
NET_SetTCPConnectTimeout(uint32 seconds) {
	net_tcp_connect_timeout = seconds;
}

/* socks support function
 *
 * if set to NULL socks is off.  If set to non null string the string will be
 * used as the socks host and socks will be used for all connections.
 *
 * returns 0 if the numonic hostname given is invalid or gethostbyname
 * returns host unknown
 *
 */
int NET_SetSocksHost(char * host)
{
	PRBool is_numeric_ip;
	char *host_cp;

#ifdef MOZ_OFFLINE
	if (NET_IsOffline()
		|| !setupSocks) {
		/* Either we're offline, or this is the very first call ot setsockshost. */
		socksFailure=FALSE;
		return 1;
	}
#endif /* MOZ_OFFLINE */

	if(host && *host)
	  {
		char *cp;
		TRACEMSG(("Trying to set Socks host: %s\n", host));

		/* If there's no port or it's zero, fail out so user gets
		 * an error and checks his configuration.
		 */
		if ( ((cp = PL_strrchr(host, ':')) != NULL) 
			&& (*(cp+1) != '\0') 
			&& (*(cp+1) != '0') ) {
			*cp = 0;
			NET_SocksPort = atoi(cp+1);
		} else {
			NET_SocksHost = 0;
			NET_SocksPort = 0;
			PR_FREEIF(NET_SocksHostName);
			NET_SocksHostName = 0;
		    TRACEMSG(("Couldn't find a socks port. :(\n"));
			socksFailure=TRUE;
            return 0;  /* Fail? */
		}

		is_numeric_ip = TRUE; /* init positive */
		for(host_cp = host; *host_cp; host_cp++)
		    if(!NET_IS_DIGIT(*host_cp) && *host_cp != '.')
			  {
				is_numeric_ip = FALSE;
				break;
			  }

        if (is_numeric_ip)  /* Numeric node address */
          {
            PRNetAddr netAddr;
            PRStatus status;
		    TRACEMSG(("Socks host is numeric"));

            /* some systems return a number instead of a struct */
            status = PR_StringToNetAddr(host, &netAddr);
            if (PR_SUCCESS == status) {
                NET_SocksHost = netAddr.inet.ip;
            }
			if (NET_SocksHostName) PR_Free (NET_SocksHostName);
            NET_SocksHostName = PL_strdup(host);
          }
        else      /* name not number */
          {
    	    PRHostEnt *hp;
	    	PRHostEnt hpbuf;
			char dbbuf[PR_NETDB_BUF_SIZE];
			PRStatus rv;

			NET_InGetHostByName++; /* global semaphore */
		    rv = PR_GetHostByName(host, dbbuf, sizeof(dbbuf),  &hpbuf);
		    hp = (rv == PR_SUCCESS ? &hpbuf : NULL);
			NET_InGetHostByName--; /* global semaphore */

            if (!hp)
              {
                TRACEMSG(("mktcp.c: Can't find Socks host name `%s'\n", host));
                NET_SocksHost = 0;
				if (NET_SocksHostName) PR_Free (NET_SocksHostName);
				NET_SocksHostName = 0;
		        TRACEMSG(("Socks host is bad. :(\n"));
				if (cp) {
				  *cp = ':';
				}
				socksFailure=TRUE;
                return 0;  /* Fail? */
              }
			memcpy(&NET_SocksHost, hp->h_addr_list[0], hp->h_length);
          }
		if (cp) {
			*cp = ':';
		}
	  }
	else
	  {
        NET_SocksHost = 0;
		if (NET_SocksHostName) PR_Free (NET_SocksHostName);
		NET_SocksHostName = NULL;
		NET_SocksPort = 0;
		TRACEMSG(("Clearing Socks Host\n"));
	  }
	socksFailure=FALSE;
    return(1);
}

/* Used internally as a utility function. */
PRIVATE void
NET_FreeDNSStruct(DNSEntry * theVictim)
{
	if(theVictim)
	  {
		FREE(theVictim->ips);
	    FREEIF(theVictim->hostname);
	    FREE(theVictim);
	  }
}

/* Used when user turns the cache off (dnsCacheExpiration is set to 0). */
PRIVATE void
NET_DeleteDNSList(void)
{
	XP_List * list_obj = dns_list;
	DNSEntry * dns_entry;

	if(!list_obj)
		return;
    while((dns_entry = (DNSEntry *)XP_ListNextObject(list_obj)) != 0)
    {
		XP_ListRemoveObject(dns_list, dns_entry);
		NET_FreeDNSStruct(dns_entry);
	}
	XP_ListDestroy(dns_list);
	return;
}

PUBLIC void
NET_SetDNSExpirationPref(int32 n)
{
	if((dnsCacheExpiration = n) <= 0)
		NET_DeleteDNSList();
}

MODULE_PRIVATE int PR_CALLBACK
NET_DNSExpirationPrefChanged(const char * newpref, void * data)
{
	PRInt32 n;
    if (PREF_OK != PREF_GetIntPref(pref_dnsExpiration, &n) )
        n = DEF_DNS_EXPIRATION;
	NET_SetDNSExpirationPref((int32)n);
	return PREF_NOERROR;
}

/* the dnsCacheExpiration units are seconds */
PRIVATE int32
net_GetDNSExpiration(void)
{
	return dnsCacheExpiration;
}

/* net_CacheDNSEntry
 *
 * caches the results of a dns lookup for fast
 * retrieval
 */
PRIVATE void
net_CacheDNSEntry(char * hostname, 
				  PRHostEnt * host_pointer,
				  int32 addrCount)
{
#if defined(MAX_DNS_LIST_SIZE) && (MAX_DNS_LIST_SIZE == 0)

    /* If MAX_DNS_LIST_SIZE is defined, and 0, don't cache anything. */
    return;

#else /* !defined(MAX_DNS_LIST_SIZE) || (MAX_DNS_LIST_SIZE > 0) */

	/* Otherwise, MAX_DNS_LIST_SIZE is either the number of entries to
	   cache, or is undefined, meaning "unlimited." */

	int32 i;
	DNSEntry * new_entry;

	/* Make sure incomming data is valid. */
	if(!hostname || !host_pointer)
		return;
	
	/* Determine whether or not we've got a list going yet. If not, get one */
	if(!dns_list)
		dns_list = XP_ListNew();
    if(!dns_list)
		return;

	/* Create our DNSEntry object for caching. */
    if(!(new_entry = PR_NEW(DNSEntry)))
		return;

	/* Copy the host name. */
	new_entry->hostname = 0;
	StrAllocCopy(new_entry->hostname, hostname);

	if(!new_entry->hostname)
	{
		PR_Free(new_entry);
		return;
	}

	/* Copy all the ip addresses. */
	if( !(new_entry->ips = (PRUint32 *) PR_Malloc(sizeof(PRUint32) * addrCount)) )
	{
		PR_Free(new_entry->hostname);
		PR_Free(new_entry);
		return;
	}

	PR_ASSERT(host_pointer->h_length == 4);
    for(i=0; i < addrCount; i++)
    {
		memcpy(&new_entry->ips[i], host_pointer->h_addr_list[i], 4);
    }

	/* Copy all the other data. */
	new_entry->addressCount = addrCount;
    new_entry->h_length = host_pointer->h_length;
	new_entry->expirationTime = time(NULL) + (net_GetDNSExpiration()); /* Current time plus expiration */

    XP_ListAddObject(dns_list, new_entry);

# ifdef MAX_DNS_LIST_SIZE
    /* check to make sure the list is not overflowing the maximum size. */
    if(XP_ListCount(dns_list) > MAX_DNS_LIST_SIZE)
      {
	    DNSEntry * first_entry = (DNSEntry *) XP_ListRemoveEndObject(dns_list);
	    NET_FreeDNSStruct(first_entry);
      }
# endif /* defined(MAX_DNS_LIST_SIZE) */

#endif  /* !defined(MAX_DNS_LIST_SIZE) || (MAX_DNS_LIST_SIZE > 0) */
    return;
}

/* net_CheckDNSCache
 *
 * checks the list of cached dns entries and returns
 * the dns info in the form of struct hostent if a match is
 * found for the passed in hostname
 */
PRIVATE DNSEntry *    
net_CheckDNSCache(CONST char * hostname)	
{
    XP_List *list_obj = dns_list;
    DNSEntry * dns_entry;

    if(!hostname || !dns_list)
		return(0);

    while((dns_entry = (DNSEntry *)XP_ListNextObject(list_obj)) != 0)
      {
		TRACEMSG(("net_CheckDNSCache: comparing %s and %s", hostname, dns_entry->hostname));
		if(dns_entry->hostname && !PL_strcasecmp(hostname, dns_entry->hostname))
		  {
			/* See if the dns entry has expired, if so, get rid of it */
			if(dns_entry->expirationTime < time(NULL))
			{
				XP_ListRemoveObject(dns_list, dns_entry);
				NET_FreeDNSStruct(dns_entry);
				return 0;
			}
		   	return(dns_entry);
		  }
      }
    return(0);
}

/* a list of dis-allowed ports for gopher connections for security reasons */
PRIVATE int net_bad_ports_table[] = {
1, 7, 9, 11, 13, 15, 17, 19, 20,
21, 23, 25, 37, 42, 43, 53, 77, 79, 87, 95, 101, 102,
103, 104, 109, 110, 111, 113, 115, 117, 119,
135, 143, 389, 512, 513, 514, 515, 526, 530, 531, 532,
540, 556, 601, 6000, 0 };

/* Lookup a host name. If ASYNC_DNS is defined a platform specific async
 * lookup will occur.
 * 
 * return: 0 on sucess, -1 on failure. MK_WAITING_FOR_LOOKUP is always
 *   returned after first call when ASYNC_DNS is defined (you have to call
 *   it again).
 * 
 * On success hostEnt is not null. */
#if defined(XP_WIN)
extern int NET_AsyncDNSLookup(void* aContext,
                              const char* aHostPort,
                              PRHostEnt** aHoststructPtrPtr,
                              PRFileDesc* aSocket);
#endif

PRIVATE int
#ifndef ASYNC_DNS
net_dns_lookup(char *host,
			   PRHostEnt **hostEnt,
			   char *dbbuf,
			   PRHostEnt *hpbuf)
#else
net_dns_lookup(MWContext *windowID,
			   char *host,
			   PRHostEnt **hostEnt,
			   PRFileDesc *socket)
#endif /* ASYNC_DNS */


{
	int status;
	NET_InGetHostByName++;
	PR_ASSERT(host);
	PR_ASSERT(hostEnt);
#ifndef ASYNC_DNS
	PR_ASSERT(dbbuf);
	PR_ASSERT(hpbuf);

	/* Not asyncronus, completes a full lookup before returing. */
	status = PR_GetHostByName(host, dbbuf, PR_NETDB_BUF_SIZE, hpbuf);

	if(status == PR_SUCCESS) {
		/* Success, hpbuf points to a valid hostent. */
		NET_InGetHostByName--;
		*hostEnt=hpbuf;
		return status;
	}
#else  /* ASYNC_DNS */
	PR_ASSERT(windowID);
	PR_ASSERT(socket);
	/* FE_StartAsyncDNSLookup  should fill in the hoststruct
	 * or leave zero the pointer if not found
	 * it can also return MK_WAITING_FOR_LOOKUP
	 * if it's not done yet.
	 *
	 * dbbuf and hpbuf are not needed in ASYNC_DNS case. */
	status = NET_AsyncDNSLookup(windowID, host, hostEnt, socket);

	if(status == MK_WAITING_FOR_LOOKUP)	{
		/* Always get here after first call to FE_Async.. for a
		 * particular host name. This ensures execution doesn't 
		 * block. */
		NET_InGetHostByName--;
		return status;
	}
	else if(status == 0) {
		/* Success, hostEnt points to a valid hostent. */
		NET_InGetHostByName--;
		return status;
	}
#endif /* ASYNC_DNS */
	/* FAIL */
	NET_InGetHostByName--;
	/* must set this pointer to NULL if we fail. FE_AsyncDNSLookup does
	 * this but PR_GetHostByName doesn't, so we cover our bases here. */
	*hostEnt = NULL;
	return -1;
}

/*  find the ip address of a host and set sin->sin_addr to that address
 * return 0 on success */

PRIVATE int 
net_FindAddress (const char *host_ptr, 
				 PRNetAddr  *net_addr,
				 MWContext  *window_id, 
				 PRFileDesc *sock) {
	PRHostEnt *hoststruct_pointer; /* Pointer to host - See netdb.h */
	DNSEntry * cache_pointer;
	char *port, *host_port=0, *host_cp;

	/* acts as a flag to determine whether or not this is the first failure. If it is, we want to sanityCheckDNS
	   which determines whether or not we've got a good net connection, sort of */
	static XP_Bool first_dns_failure=TRUE;
	static int random_host_number = -1;
	static time_t random_host_expiration = 0;
	static XP_Bool tryPAD=TRUE;
	XP_Bool is_numeric_ip;

	if(!host_ptr || !*host_ptr)
		return -1;

	/* Proxy Auto-Discovery (PAD) */
	if(tryPAD && MK_PadEnabled && MK_padPacURL && *MK_padPacURL) {
		PRHostEnt *hostEnt=NULL;
		static PRFileDesc *socket=NULL;
		if(!PL_strncasecmp(MK_padPacURL, "file:", 5)) {
			/* Don't allow file urls because they're hard to figure out in an xp way. */
			tryPAD=FALSE;
			foundPADPAC=FALSE;
		} else {
			if(socket == NULL)
				socket=PR_NewTCPSocket();
			if(socket != NULL) {
				char *padHost=NET_ParseURL(MK_padPacURL, GET_HOST_PART);
				if(padHost && *padHost) {
					char *colon=PL_strchr(padHost, ':');
					int status;
#ifndef ASYNC_DNS
					char dbbuf[PR_NETDB_BUF_SIZE];
					PRHostEnt dpbuf;
#endif
					if(colon)
						*colon='\0';
#ifndef ASYNC_DNS
					status = net_dns_lookup(padHost, &hostEnt, dbbuf, &dpbuf);
#else
					status = net_dns_lookup(window_id, padHost, &hostEnt, socket);
#endif /* ASYNC_DNS */
					if(status != MK_WAITING_FOR_LOOKUP) {
						PR_Close(socket);
						socket=NULL;
						if(hostEnt != NULL)
							/* We found the padpac */
							foundPADPAC=TRUE;
						tryPAD=FALSE;
					}
					if(colon)
						*colon=':';
				} else {
					PR_Close(socket);
					socket=NULL;
					tryPAD=FALSE;
				}
				PR_FREEIF(padHost);
			} /* End socket not null */
		} 
	} /* End tryPAD */

    StrAllocCopy(host_port, host_ptr);      /* Make a copy we can mess with */
	if (!host_port)
		return -1;

    /* Parse port number if present */  
	port = PL_strchr(host_port, ':');  
    if (port) {
        *port++ = 0;       
        if (NET_IS_DIGIT(*port)) {
			unsigned short port_num = (unsigned short) atol(port);
			int i;

			/* check for illegal ports */
			/* if the explicit port number equals
			 * the default port number let it through
			 */
			if(port_num != PR_ntohs(net_addr->inet.port)) {
            	/* disallow well known ports */
            	for(i=0; net_bad_ports_table[i]; i++)
                	if(port_num == net_bad_ports_table[i]) {
                    	char *error_msg = PL_strdup(XP_GetString(MK_PORT_ACCESS_NOT_ALLOWED));
                    	if(error_msg) {
                        	FE_Alert(window_id, error_msg);
							PR_Free(error_msg);
					  	  }

						/* return special error code
						 * that NET_BeginConnect knows
						 * about.
						 */
						PR_Free(host_port);
                    	return(MK_UNABLE_TO_CONNECT);
                  	  }

            	net_addr->inet.port = PR_htons(port_num);
			  }

		  }
      }

    /* see if this host entry is already cached */
    if( (cache_pointer = net_CheckDNSCache(host_port)) != NULL ) {
		/* call FE_ClearDNSSelect to catch the case of a cache hit before 
		 * a successfull lookup response for this particular socket.
		 * This happens when a previous socket lookup for the same host
		 * succeeded and propagated the cache entry before this one
		 * finished.
		 */

        NET_ClearDNSSelect(window_id, sock);
		
	    net_addr->inet.ip = cache_pointer->ips[0];

	    PR_Free(host_port);
	    return(0);  /* FOUND OK */
    }

	/* Determine whether or not we're dealing with an ip address as the host */
	is_numeric_ip = TRUE; /* init positive */
	for(host_cp = host_port; *host_cp; host_cp++)
		if(!NET_IS_DIGIT(*host_cp) && *host_cp != '.') {
			is_numeric_ip = FALSE;
			break;
		  }

    /* Parse host number if present. */  
    if (is_numeric_ip) {
		PRUint16 port = net_addr->inet.port;  /* save a copy */
		if(PR_SUCCESS != PR_StringToNetAddr(host_port, net_addr)) {
			PR_ASSERT(0);
			PR_Free(host_port);
			return(-1); /* fail */
		}
		net_addr->inet.port = port;  /* StringToNetAddr overites the port num */
	/* name not number */
    } else {
		int32 addressCount=0;
	    char *remapped_host_port=0;

		/* randomly remap home.netscape.com to home1.netscape.com through
		 * home32.netscape.com
		 * cache the original name not the new one.
		 */
		if((!PL_strncasecmp(host_port, "home.", 5)
		    || !PL_strncasecmp(host_port, "rl.", 3))
			&& (PL_strcasestr(host_port+2, ".netscape.com")
				|| PL_strcasestr(host_port+2, ".mcom.com"))) {
			time_t cur_time = time(NULL);
			char temp_string[32];
			XP_Bool is_rl_host;

			*temp_string = '\0';

			is_rl_host = !PL_strncasecmp(host_port, "rl.", 3);

			if(random_host_number == -1 || random_host_expiration < cur_time) {
				/* pick a new random number */
				random_host_expiration = cur_time + (60 * 5); /* five min */
				random_host_number = (XP_RANDOM() & 31);
			  }
				
			if(is_rl_host)
				PR_snprintf(temp_string, sizeof(temp_string), "rl%d%s",
					   		random_host_number+1, host_port+2);
			else
				PR_snprintf(temp_string, sizeof(temp_string), "home%d%s",
					   		random_host_number+1, host_port+4);

			StrAllocCopy(remapped_host_port, temp_string);

		    TRACEMSG(("Remapping %s to %s", host_port, remapped_host_port));
		  } 
		
#ifdef XP_UNIX
		  {
		    /* Implement a terrible hack just for unix. If the environment
		     * variable SOCKS_NS is defined, stomp on the host that the DNS
		     * code uses for host lookup to be a specific ip address. */
		    static char firstTime = 1;
		    if (firstTime) {
                PRNetAddr netAddr;
                PRStatus status;
			    char *ns = getenv("SOCKS_NS");
			    firstTime = 0;
			    if (ns && ns[0]) {
				    /* Use a specific host for dns lookups */
					extern int res_init (void);
				    res_init();
                    status = PR_StringToNetAddr(ns, &netAddr);
                    if (PR_SUCCESS == status) {
                        _res.nsaddr_list[0].sin_addr.s_addr = netAddr.inet.ip;
                    }
				    _res.nscount = 1;
			      }
		      }
		  }
#endif

		{
			int status;
#ifndef ASYNC_DNS
			char dbbuf[PR_NETDB_BUF_SIZE];
			PRHostEnt dpbuf;
#endif
			/* malloc the string to prevent overflow */
			char *msg = PR_smprintf(XP_GetString(XP_PROGRESS_LOOKUPHOST), host_port);

			if(msg) {
#if defined(SMOOTH_PROGRESS)
                PM_Status(window_id, NULL, msg);
#else
        		NET_Progress(window_id, msg);
#endif
				PR_Free(msg);
			  }

            TIMING_STARTCLOCK_NAME("dns:lookup", (remapped_host_port ? remapped_host_port : host_port));

#ifndef ASYNC_DNS
			status = net_dns_lookup( 
						   (remapped_host_port ?
							remapped_host_port :
							host_port),
							&hoststruct_pointer,
							dbbuf,
							&dpbuf);
#else
			status = net_dns_lookup(window_id, 
							   (remapped_host_port ?
								remapped_host_port :
								host_port),
								&hoststruct_pointer,
								sock);
#endif /* ASYNC_DNS */            
			/* sucess if hoststruct_pointer is not null */
			if(status == MK_WAITING_FOR_LOOKUP) {
				PR_Free(host_port);
				return status;
			}
		}

        TIMING_STOPCLOCK_NAME("dns:lookup", (remapped_host_port ? remapped_host_port : host_port),
                              window_id, (hoststruct_pointer ? "ok" : "error"));

        if (!hoststruct_pointer) {
			if(first_dns_failure) {
				first_dns_failure = FALSE;
				/* On the proxy causes confusing messages.
				 * This function is only implemented on for XP_UNIX. */
				NET_SanityCheckDNS(window_id);
			  }
            TRACEMSG(("mktcp.c: Can't find host name `%s'.  Errno #%d\n",
									host_port, PR_GetError()));
			PR_Free(host_port);
			TRACEMSG(("gethostbyname failed with error: %d\n", PR_GetError()));
            return -1;  /* Fail? */
        }

		/* Count the number of addresses returned in the A record. */
		for (addressCount=0; hoststruct_pointer->h_addr_list[addressCount]; addressCount++) {;}

		/* If the addressCount is zero we've got a problem */
		if (addressCount == 0) {
			PR_ASSERT(0);
			PR_Free(host_port);
			return -1;
		}

		/* Copy the first address in the list to the sin. char ** h_addr_list */
		PR_ASSERT(hoststruct_pointer->h_length == 4);
		memcpy(&net_addr->inet.ip, hoststruct_pointer->h_addr_list[0], 4);
	
    	/* if NET_GetDNSExpiration() returns 0 we are considering the cache disabled */
		if(net_GetDNSExpiration() > 0)
			net_CacheDNSEntry(host_port, hoststruct_pointer, addressCount);
      } /* end name not number else*/

#ifdef DEBUG
	{
		char str[64];
		PR_NetAddrToString(net_addr, str, sizeof(str));
		TRACEMSG(("TCP.c: Found address %s and port %d", str, (int)PR_ntohs(net_addr->inet.port)));
	}
#endif

	PR_Free(host_port);
    return(0);   /* OK, we found an address */
}

/* FREE left over tcp connection data if there is any
 */
MODULE_PRIVATE void 
NET_FreeTCPConData(TCP_ConData * data)
{
	TRACEMSG(("Freeing TCPConData...Done with connect (i hope)"));
	FREEIF(data)
}

PRIVATE int
net_start_first_connect(const char   *host, 
						PRFileDesc   *sock, 
						MWContext    *window_id, 
						TCP_ConData  *tcp_con_data,
						char        **error_msg,
                        PRUint32     localIP)
{

    /* malloc the string to prevent overflow
     */
    int32 len = PL_strlen(XP_GetString(XP_PROGRESS_CONTACTHOST));
    char * buf;

    len += PL_strlen(host);

    buf = (char *)PR_Malloc((len+10)*sizeof(char));
    if(buf)
      {
        PR_snprintf(buf, (len+10)*sizeof(char),
                XP_GetString(XP_PROGRESS_CONTACTHOST), host);
#if defined(SMOOTH_PROGRESS) 
        PM_Status(window_id, NULL, buf);
#else
        NET_Progress(window_id, buf);
#endif
        FREE(buf);
      }

	HG26300
	/* set the begining time to be the current time.
	 * the timeout value will be  compared to this
	 * later
	 */
	tcp_con_data->begin_time = time(NULL);

#define CONNECT_TIMEOUT  0
    
    /* Bind to a user specified local IP address if specified.
     * Netlib does not check the validity of this address.
     * The user is guaranteing it's existence and usability. */
    if ( localIP > 0 ) {
        PRStatus status;
        PRErrorCode errorCode; /* see http://www.mozilla.org/docs/refList/refNSPR/prerr.htm#1027954 */
        PRNetAddr *addr = (PRNetAddr *)PR_Malloc(sizeof(PRNetAddr));
        if (!addr)
            return MK_UNABLE_TO_CONNECT;
        status = PR_InitializeNetAddr(PR_IpAddrNull, 0, addr);
        if (status != PR_SUCCESS) {
            errorCode = PR_GetError();
        } else {
            addr->inet.ip = localIP;
            status = PR_Bind(sock, addr);
            if (status != PR_SUCCESS) {
                errorCode = PR_GetError();
            }
        }
        PR_Free(addr);
    }

    /* if it's not equal to PR_SUCCESS something went wrong
	 *
	 * PRNetAddr is binary compatible with struct sockaddr
     */
    if(PR_SUCCESS != PR_Connect (sock,
								 &tcp_con_data->net_addr,
								 CONNECT_TIMEOUT))
      {

        int rv = PR_GetError();

#if !defined(XP_MAC) && !defined(XP_WIN16)
		PR_ASSERT(rv != PR_WOULD_BLOCK_ERROR); /* should never happen */
#endif
        if (rv == PR_IN_PROGRESS_ERROR || rv == PR_WOULD_BLOCK_ERROR)
          {
            tcp_con_data->next_state = NET_TCP_FINISH_CONNECT;
            return(MK_WAITING_FOR_CONNECTION);  /* not connected yet */
          }
        else if (rv == PR_IS_CONNECTED_ERROR)
          {
            return(MK_CONNECTED);  /* already connected */
          }
        else
          {
            PR_Close(sock);
            if(rv == PR_CONNECT_REFUSED_ERROR)
              {
	    		*error_msg = NET_ExplainErrorDetails(MK_CONNECTION_REFUSED, host);

                TRACEMSG(("connect: refused\n"));

                return(MK_CONNECTION_REFUSED);
              }
            else if(rv == PR_CONNECT_TIMEOUT_ERROR)
              {
	    		*error_msg = NET_ExplainErrorDetails(MK_CONNECTION_TIMED_OUT);

                TRACEMSG(("connect: timed out\n"));

                return(MK_CONNECTION_TIMED_OUT);
              }
			else 
              {
			    *error_msg = NET_ExplainErrorDetails(MK_UNABLE_TO_CONNECT, rv);

                TRACEMSG(("connect: unable to connect %i\n", rv));

                return (MK_UNABLE_TO_CONNECT);
              }
          }
      }

    /* else  good connect */

    return(MK_CONNECTED);
}

/* Finds the DNSEntry with hostname in it, removes it if it only has one address, otherwise
   removes the first address and shifts the other(s) forward so they can be tried. */
PRIVATE XP_Bool
net_connection_failed(CONST char *hostname)
{
	DNSEntry *dns_entry = 0;
	char *hostCopy=NULL, *port=NULL;

	if(!hostname)
		return FALSE;

	StrAllocCopy(hostCopy, hostname);
	if(!hostCopy)
		return FALSE;

	/* Look for a port */
	if( (port = PL_strchr(hostCopy, ':')) != NULL )
		*port = '\0';

	dns_entry = net_CheckDNSCache(hostCopy);
	FREE(hostCopy);
	/* If we find the cached dns entry pull the address off the top because it is the one 
		that failed. Then shift the others up to the front of the list. */
	if (dns_entry)
	{
		/* If there is only one (most common case) ip address associated with this host,
		   blow the entire entry away */
		if (dns_entry->addressCount == 1)		  
		{
			XP_ListRemoveObject(dns_list, dns_entry);
			NET_FreeDNSStruct(dns_entry);
			return FALSE;
		}
		else /* Failover case */
		{
			dns_entry->addressCount--;
			/* Shift addresses up one, overwriting the first one */
			memmove(dns_entry->ips,
						&dns_entry->ips[1],
						sizeof(PRUint32) * dns_entry->addressCount );
			/* Null terminate the array */
			dns_entry->ips[dns_entry->addressCount] = 0;
			return TRUE;
		}
	}
	/* If the host wasn't in the cache then caching is off or an ip address was sent in here
	   and we don't want to keep trying this address so return false. */
	return FALSE;
}


/*
 * Non blocking connect begin function.  It will most likely
 * return negative. If it does, NET_ConnectFinish() will
 * need to be called repeatably until a connect is established.
 *
 * return's negative on error
 * return's MK_WAITING_FOR_CONNECTION when continue is neccessary
 * return's MK_CONNECTED on true connect
 */ 

MODULE_PRIVATE int 
NET_BeginConnect (CONST char   *url, 
				  char   *ip_address_string,
          	      char         *prot_name, 
          	      int           def_port, 
          	      PRFileDesc     **sock, 
			      HG92743
			      TCP_ConData **tcp_con_data,
          	      MWContext    *window_id,
			      char        **error_msg,
				  unsigned long		socks_host,
				  short			socks_port,
                  PRUint32 localIP)
{

	char * proxy=NULL;
	PRInt32 iPort=0;
	char text[MAXHOSTNAMELEN + 8];
    CONST char *host; 
    char *althost=0; 
   	int status;
    PRFileDesc *fd;
    char *host_string=0;
    PRSocketOptionData opt;
#ifdef MOZ_OFFLINE
	if (NET_IsOffline())	/* if we're offline, ret error - dmb 11/25/96 */
		return MK_OFFLINE;	
#endif /* MOZ_OFFLINE */
	TRACEMSG(("NET_BeginConnect called for url: %s", url));

	/* One time startup flag. If this is the first time in BeginConnect,
	 * make sure the socks is setup if it needs to be.
	 */
	if(!setupSocks) {
		setupSocks=TRUE;
		if (NET_GetProxyStyle() == PROXY_STYLE_MANUAL) {
            if ( (PREF_OK != PREF_CopyCharPref(pref_socksServer,&proxy))
                || !proxy || !*proxy ) {
                NET_SetSocksHost(NULL); /* NULL is ok */
            } else {
                if ( PREF_OK == PREF_GetIntPref(pref_socksPort,&iPort) ) {
				    PR_snprintf(text, sizeof(text), "%s:%d", proxy, iPort);  
				    NET_SetSocksHost(text);
                }
			}
		}
	}

	/* in case finish connect calls us 
	 */
	if(*tcp_con_data)
		NET_FreeTCPConData(*tcp_con_data);
	
	/* construct state table data
	 */	
	*tcp_con_data = PR_NEW(TCP_ConData);
	
	if(!*tcp_con_data)
	  {
	    *error_msg = NET_ExplainErrorDetails(MK_OUT_OF_MEMORY);
		return(MK_OUT_OF_MEMORY);
	  }

	memset(*tcp_con_data, 0, sizeof(TCP_ConData));

    /* Set up Internet defaults and port*/
	PR_InitializeNetAddr(PR_IpAddrNull, (PRUint16) def_port, &(*tcp_con_data)->net_addr);

    if(NET_URL_Type(url)) 
      {
        host_string = NET_ParseURL(url, GET_HOST_PART);
		host = host_string;
      }
    else
      {
        /* assume that a hostname was passed directly 
         * instead of a URL
         */
        host = url;
      }

    /* build a socket
     */
    *sock = PR_NewTCPSocket();

HG59609

    if(*sock == NULL)
      {                                                
        int error = PR_GetError();
        TRACEMSG(("NETSOCKET call returned INVALID_SOCKET: %d", error));
        NET_FreeTCPConData(*tcp_con_data);
        *tcp_con_data = 0;
        *error_msg = NET_ExplainErrorDetails(MK_UNABLE_TO_CREATE_SOCKET, error);
        FREE_AND_CLEAR(host_string);
        return(MK_UNABLE_TO_CREATE_SOCKET);
      }

HG40252

	/* make the socket non-blocking */
	opt.option = PR_SockOpt_Nonblocking;
	opt.value.non_blocking = PR_TRUE;
	PR_SetSocketOption(*sock, &opt);

#if defined(XP_WIN16) || (defined(XP_OS2) && !defined(XP_OS2_DOUGSOCK))
	opt.option = PR_SockOpt_Linger;
	opt.value.linger.polarity = PR_TRUE;
	opt.value.linger.linger = PR_INTERVAL_NO_WAIT;
	PR_SetSocketOption(*sock, &opt);
#endif /* XP_WIN16 */

	/* copy the sock no
	 */
    fd = *sock;

HG28879
	{
		/* if the socks host is invalid */
		if ( socksFailure && (NET_GetProxyStyle() == PROXY_STYLE_MANUAL) )
		{
			char * prefSocksHost = NULL;
			/* Tell the FE about the failure */
            int32 len = PL_strlen(XP_GetString(XP_PROGRESS_UNABLELOCATE));
			char * buf;
			if( (PREF_OK == PREF_CopyCharPref(pref_socksServer,&prefSocksHost))
                && prefSocksHost)
			{
				len += PL_strlen(prefSocksHost);

				buf = (char *)PR_Malloc((len+10)*sizeof(char));
				if(buf)
				  {
					PR_snprintf(buf, (len+10)*sizeof(char),
							XP_GetString(XP_PROGRESS_UNABLELOCATE), prefSocksHost);
#if defined(SMOOTH_PROGRESS)
                    PM_Status(window_id, NULL, buf);
#else
					NET_Progress(window_id, buf);
#endif
					FREE(buf);
				  }

				/* Tell the user about the failure */
				*error_msg = NET_ExplainErrorDetails(MK_UNABLE_TO_LOCATE_SOCKS_HOST, prefSocksHost);

				/* clean up prior to leaving */
				NET_FreeTCPConData(*tcp_con_data);
				*tcp_con_data = 0;
				PR_Close(*sock);
				*sock = NULL;
				FREE_AND_CLEAR(host_string);
				PR_FREEIF(prefSocksHost);
			}
			return MK_UNABLE_TO_LOCATE_HOST;
		}
	}

HG71089

	/* if an IP address string was passed in, then use it instead of the
	 * hostname to do the DNS lookup.
	 */
	if ( ip_address_string ) {
		char *port;

		StrAllocCopy(althost, ip_address_string);

		port = PL_strchr(host, ':');
		if ( port ) {
			StrAllocCat(althost, port);
		}
	}
	
    status = net_FindAddress(althost?althost:host, 
				 &((*tcp_con_data)->net_addr), 
				 window_id, 
				 *sock);

	if (althost) {
		PR_Free(althost);
	}
	
	if(status == MK_WAITING_FOR_LOOKUP)
	  {
		(*tcp_con_data)->next_state = NET_TCP_FINISH_DNS_LOOKUP;
        FREE_AND_CLEAR(host_string);
       	return(MK_WAITING_FOR_CONNECTION);  /* not connected yet */
	  }
    else if (status < 0)
      {
        {
            /* malloc the string to prevent overflow
             */
            int32 len = PL_strlen(XP_GetString(XP_PROGRESS_UNABLELOCATE));
            char * buf;

            len += PL_strlen(host);

            buf = (char *)PR_Malloc((len+10)*sizeof(char));
            if(buf)
              {
                PR_snprintf(buf, (len+10)*sizeof(char),
                        XP_GetString(XP_PROGRESS_UNABLELOCATE), host);
#if defined(SMOOTH_PROGRESS)
                PM_Status(window_id, NULL, buf);
#else
                NET_Progress(window_id, buf);
#endif
                FREE(buf);
              }
        }

		NET_FreeTCPConData(*tcp_con_data);
		*tcp_con_data = 0;
		PR_Close(*sock);
	    *sock = NULL;

		/* @@@@@ HACK!  If we detect an illegal port in
		 * net_FindAddress we put up an error message
		 * and return MK_UNABLE_TO_CONNECT.  Don't
		 * print another message
		 */
		if(status != MK_UNABLE_TO_CONNECT)
	    	*error_msg = NET_ExplainErrorDetails(MK_UNABLE_TO_LOCATE_HOST, 
												*host ? host : "(no name specified)");
        FREE_AND_CLEAR(host_string);
        return MK_UNABLE_TO_LOCATE_HOST;
      }

    TIMING_STARTCLOCK_NAME("tcp:connect", url);

    status = net_start_first_connect(host, *sock, window_id, 
									 *tcp_con_data, error_msg, localIP);

	if(status != MK_WAITING_FOR_CONNECTION)
	  {
        TIMING_STOPCLOCK_NAME("tcp:connect", url, window_id, "error");
		NET_FreeTCPConData(*tcp_con_data);
		*tcp_con_data = 0;
	  }
	else
	  {
       	(*tcp_con_data)->next_state = NET_TCP_FINISH_CONNECT;
		/* save in case we need it */
       	HG83665 
	  }

	if(status < 0)
	  {
		net_connection_failed(host);
		PR_Close(*sock);
	    *sock = NULL;
	  }

    FREE_AND_CLEAR(host_string);
	
    return(status);
}


/*
 * Non blocking connect finishing function.  This routine polls
 * the socket until a connection finally takes place or until
 * an error occurs. NET_ConnectFinish() will need to be called 
 * repeatably until a connect is established.
 *
 * return's negative on error
 * return's MK_WAITING_FOR_CONNECTION when continue is neccessary
 * return's MK_CONNECTED on true connect
 */
PUBLIC int 
NET_FinishConnect (CONST char   *url,
                   char         *prot_name,
                   int           def_port,
                   PRFileDesc   **sock, 
			       TCP_ConData **tcp_con_data,
                   MWContext    *window_id,
				   char        **error_msg,
                   PRUint32 localIP)
{
	int status;
	char *host=NULL;

	TRACEMSG(("NET_FinishConnect called for url: %s", url));

	if(!*tcp_con_data)  /* safty valve */
	  {
	    *error_msg = NET_ExplainErrorDetails(MK_OUT_OF_MEMORY);
		return(MK_OUT_OF_MEMORY);
	  }

	switch((*tcp_con_data)->next_state)
	{
	case NET_TCP_FINISH_DNS_LOOKUP:
	  {
		char * host_string=NULL;	
		const char * host = NULL;

        if(NET_URL_Type(url))
          {
            host_string = NET_ParseURL(url, GET_HOST_PART);
            host = host_string;
          }
        else
          {
            /* assume that a hostname was passed directly
             * instead of a URL
             */
            host = url;
          }

		status = net_FindAddress(host, 
					&((*tcp_con_data)->net_addr),
					window_id, 
					*sock);

        if(status == MK_WAITING_FOR_LOOKUP)
          {
            (*tcp_con_data)->next_state = NET_TCP_FINISH_DNS_LOOKUP;
            FREE_AND_CLEAR(host_string);
            return(MK_WAITING_FOR_CONNECTION);  /* not connected yet */
          }
        else if (status < 0)
          {
        	{
            	/* malloc the string to prevent overflow
             	 */
            	int32 len = PL_strlen(XP_GetString(XP_PROGRESS_UNABLELOCATE));
            	char * buf;
	
            	len += PL_strlen(host);
	
            	buf = (char *)PR_Malloc((len+10)*sizeof(char));
            	if(buf)
              	  {
                	PR_snprintf(buf, (len+10)*sizeof(char),
                        	XP_GetString(XP_PROGRESS_UNABLELOCATE), host);
#if defined(SMOOTH_PROGRESS)
                    PM_Status(window_id, NULL, buf);
#else
                	NET_Progress(window_id, buf);
#endif
                	FREE(buf);
              	  }
        	}

            NET_FreeTCPConData(*tcp_con_data);
            *tcp_con_data = 0;
	        *error_msg = NET_ExplainErrorDetails(MK_UNABLE_TO_LOCATE_HOST, host);
            FREE_AND_CLEAR(host_string);
            return MK_UNABLE_TO_LOCATE_HOST;
          }

        TIMING_STARTCLOCK_NAME("tcp:connect", url);

        status = net_start_first_connect(host, *sock, window_id, 
										 *tcp_con_data, error_msg, localIP);

        if(status != MK_WAITING_FOR_CONNECTION)
          {
            TIMING_STOPCLOCK_NAME("tcp:connect", url, window_id, "error");
            NET_FreeTCPConData(*tcp_con_data);
            *tcp_con_data = 0;
          }
		else
		  {
        	(*tcp_con_data)->next_state = NET_TCP_FINISH_CONNECT;
		  }

        if(status < 0)
          {
			net_connection_failed(host);
			TRACEMSG(("mktcp.c: Error during connect %d\n", PR_GetError()));
            PR_Close(*sock);
            *sock = NULL;
          }  

        FREE_AND_CLEAR(host_string);

        return(status);

	  } /* end of this part of the case */
	
	case NET_TCP_FINISH_CONNECT:
	{
		PRPollDesc pd;

		pd.fd = *sock;
		pd.in_flags = PR_POLL_WRITE | PR_POLL_EXCEPT; 

        /* do another connect here to see if we are actually connected
		 *
		 * PRNetAddr is binary compatible with struct sockaddr
         */
		if(PR_Poll(&pd, 1, PR_INTERVAL_NO_TIMEOUT) < 1)
		{
			PR_ASSERT(0);
	        return MK_WAITING_FOR_CONNECTION;  /* perhaps we should error here */
		}

#if defined(XP_WIN16)
        /* Bind to a user specified local IP address if specified.
         * Netlib does not check the validity of this address.
         * The user is guaranteing it's existence and usability. */
        if ( localIP > 0 ) {
            PRStatus status;
            PRErrorCode errorCode; /* see http://www.mozilla.org/docs/refList/refNSPR/prerr.htm#1027954 */
            PRNetAddr *addr = (PRNetAddr *)PR_Malloc(sizeof(PRNetAddr));
            if (!addr)
                return MK_UNABLE_TO_CONNECT;
            status = PR_InitializeNetAddr(PR_IpAddrNull, 0, addr);
            if (status != PR_SUCCESS) {
                errorCode = PR_GetError();
            } else {
                addr->inet.ip = localIP;
                status = PR_Bind(sock, addr);
                if (status != PR_SUCCESS) {
                    errorCode = PR_GetError();
                }
            }
            PR_Free(addr);
        }

    	if(PR_SUCCESS != PR_Connect (*sock,
								 &(*tcp_con_data)->net_addr,
								 CONNECT_TIMEOUT))
#else
        if(PR_SUCCESS != PR_GetConnectStatus(&pd))
#endif 
        {
		    int error;
    
		    error = PR_GetError();
		
#if !defined(XP_MAC) && !defined(XP_WIN16)
			PR_ASSERT(error != PR_WOULD_BLOCK_ERROR); /* should never happen */
#endif
			if (error == PR_IN_PROGRESS_ERROR || error == PR_WOULD_BLOCK_ERROR)
		      {

				/* check the begin time against the current time minus
				 * the timeout value.  Error out if past the timeout
				 */
				if((time_t)(time(NULL)-net_tcp_connect_timeout) > (*tcp_con_data)->begin_time)
				  {
					error = XP_ERRNO_ETIMEDOUT;
					goto error_out;
				  }

			    /* still waiting
			     */
                return MK_WAITING_FOR_CONNECTION;
		      }
		    else if(error !=  PR_IS_CONNECTED_ERROR)
		      {
error_out:
			
		        /* if EISCONN then we actually are connected
		         * otherwise return an error
		         */

				/* At this point we know there was a problem with the ip address we tried. */
			    TRACEMSG(("mktcp.c: Error during connect: %d\n", error));

				host = NET_ParseURL(url, GET_HOST_PART);
				if(!host)
					return -1;

				/* In the case when the url is a host, no protocol, NET_ParseURL returns null byte.
				   We run up against this case when a proxy was placed into the url instead of 
				   the url itself.*/
				if(*host == '\0') {
					FREE(host);
					host = NULL;
					StrAllocCopy(host, url);
				}

				/* If we should try other addresses with this tcpConnection, do it */
				if (net_connection_failed(host))
				{
					int status;

					if(*host == '\0')
					  {
						/* the url is a hostname */
						FREE(host);
						if( !(host = PL_strdup(url)) )
							return -1;
					  }

					/* Go get another of the ip addresses */
    				status = net_FindAddress(host, 
				 				&((*tcp_con_data)->net_addr), 
				 				window_id, 
				 				*sock);

					FREE(host);
					if(status == 0)
        				return(NET_BeginConnect (url,
												NULL,
                                 				prot_name,
                                 				def_port,
                                 				sock,
                                 				HG98376
                                 				tcp_con_data,
                                 				window_id,
								 				error_msg,
												0,
												0,
                                                localIP));
					/* else fall through to the error */
				}
				else
					FREE(host);

                TIMING_STOPCLOCK_NAME("tcp:connect", url, window_id, "error");

				HG92362
                if (error == PR_CONNECT_REFUSED_ERROR)
					{
							char * host = NET_ParseURL(url, GET_HOST_PART);
	        			    *error_msg = NET_ExplainErrorDetails(
												MK_CONNECTION_REFUSED,
												host);
							FREE(host);
		    			    NET_FreeTCPConData(*tcp_con_data);
		    			    *tcp_con_data = 0;
                        	return MK_CONNECTION_REFUSED;
					}
                else if (error == PR_CONNECT_TIMEOUT_ERROR)
					{
		    			NET_FreeTCPConData(*tcp_con_data);
		    			*tcp_con_data = 0;
	        			*error_msg = NET_ExplainErrorDetails(MK_CONNECTION_TIMED_OUT);
                        return MK_CONNECTION_TIMED_OUT;
					}
                else
					{
		    			NET_FreeTCPConData(*tcp_con_data);
		    			*tcp_con_data = 0;
	        			*error_msg = NET_ExplainErrorDetails(MK_UNABLE_TO_CONNECT, error);
                        return MK_UNABLE_TO_CONNECT;
					}
		      }
        }

        TIMING_STOPCLOCK_NAME("tcp:connect", url, window_id, "ok");
		TRACEMSG(("mktcp.c: Successful connection (message 1)"));
		NET_FreeTCPConData(*tcp_con_data);
		*tcp_con_data = 0;
        return MK_CONNECTED;
	}

	default:
		NET_FreeTCPConData(*tcp_con_data);
		*tcp_con_data = 0;
		TRACEMSG(("mktcp.c: bad state during connect"));

		assert(0);  /* should never happen */

	    *error_msg = NET_ExplainErrorDetails(MK_UNABLE_TO_CONNECT, 0);
		return(MK_UNABLE_TO_CONNECT);

	} /* end switch on next_state */

	PR_ASSERT(0);  /* should never get here */

	*error_msg = NET_ExplainErrorDetails(MK_UNABLE_TO_CONNECT, 0);
	return(MK_UNABLE_TO_CONNECT);
}


/* Free any memory used up
 */
MODULE_PRIVATE void
NET_CleanupTCP(void)
{
    if(dns_list)
      {
        DNSEntry * tmp_entry;

        while((tmp_entry = (DNSEntry *)XP_ListRemoveTopObject(dns_list)) != NULL)
          {
             NET_FreeDNSStruct(tmp_entry);
          }

        XP_ListDestroy(dns_list);
		dns_list = 0;
      }

	/* we really should free the socket_buffer but
	 * that is in the other module as a private :(
	 */
    return;
}
