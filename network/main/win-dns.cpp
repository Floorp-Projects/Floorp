/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#ifdef XP_PC
#include <windows.h>
#ifdef WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#endif
#include "net.h"
#include "nsISupports.h"
#include "merrors.h"
#include "prnetdb.h"
#include "plstr.h"
#include "prmem.h"
#include "prlog.h"

#ifdef DEBUG
static PRLogModuleInfo* gDNSLogModuleInfo;

#define DNS_TRACE_LOOKUPS 0x1
#define DNS_TRACE_SLOW    0x2   // XXX not yet implemented

#define DNS_LOG_TEST(_lm,_bit) (PRIntn((_lm)->level) & (_bit))

#define DNS_TRACE(_bit,_args)                   \
  PR_BEGIN_MACRO                                \
    if (DNS_LOG_TEST(gDNSLogModuleInfo,_bit)) { \
      PR_LogPrint _args;                        \
    }                                           \
  PR_END_MACRO

#else
#define DNS_TRACE(_bit,_args)
#endif

struct SocketWaiter {
  void Init(PRFileDesc* fd) {
    mSocket = fd;
    mNext = NULL;
  }

  PRFileDesc* mSocket;
  SocketWaiter* mNext;
};

struct DNSCacheEntry {
  nsresult Init(void* aContext, const char* aHost, PRFileDesc* fd);

  void Destroy();

  static DNSCacheEntry** Lookup(const char* aHost);

  void* mContext;
  char* mHost;
  SocketWaiter* mSockets;
  PRHostEnt* mHostEnt;
  int mError;
  PRBool mFinished;
  HANDLE mHandle;
  DNSCacheEntry* mNext;
};

static UINT gMSGFoundDNS;
static HWND gDNSWindow;
static DNSCacheEntry* gDNSCache;

void
DNSCacheEntry::Destroy()
{
  PR_Free(mHost);
  PR_Free(mHostEnt);
  SocketWaiter* sw = mSockets;
  while (NULL != sw) {
    SocketWaiter* next = sw->mNext;
    PR_Free(sw);
    sw = next;
  }
}

nsresult
DNSCacheEntry::Init(void* aContext, const char* aHost, PRFileDesc* fd)
{
  mContext = aContext;
  mError = 0;
  mFinished = PR_FALSE;
  mHandle = NULL;
  mNext = NULL;

  mHost = PL_strdup(aHost);
  mHostEnt = (PRHostEnt*) PR_Malloc(sizeof(char) * PR_NETDB_BUF_SIZE);
  mSockets = (SocketWaiter*) PR_Malloc(sizeof(SocketWaiter));
  mSockets->Init(fd);

  if ((NULL == mHost) || (NULL == mHostEnt) || (NULL == mSockets)) {
    return NS_ERROR_FAILURE;
  }
  mHostEnt->h_name = NULL;
  mHostEnt->h_aliases = NULL;
  mHostEnt->h_addr_list = NULL;
  return NS_OK;
}

DNSCacheEntry**
DNSCacheEntry::Lookup(const char* aHost)
{
  DNSCacheEntry** pentry = &gDNSCache;
  DNSCacheEntry* entry = *pentry;
  while (NULL != entry) {
    if (PL_strcasecmp(aHost, entry->mHost) == 0) {
      return pentry;
    }
    pentry = &entry->mNext;
    entry = entry->mNext;
  }
  return pentry;
}

static LRESULT CALLBACK 
#if defined(WIN16)
__loadds
#endif
EventProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  if (gMSGFoundDNS == uMsg) {
    // Get error code now
    int error = WSAGETASYNCERROR(lParam);

    DNS_TRACE(DNS_TRACE_LOOKUPS,
              ("DNS lookup: handle=%p error=%d", (HANDLE)wParam, error));

    // Search cache for matching handle
    LRESULT rv = 1;
    DNSCacheEntry** pentry = &gDNSCache;
    DNSCacheEntry* entry = gDNSCache;
    while (NULL != entry) {
      if (!entry->mFinished && ((HANDLE)wParam == entry->mHandle)) {
        DNS_TRACE(DNS_TRACE_LOOKUPS,
                  ("DNS lookup: finished '%s'", entry->mHost));

        // Found a match
        entry->mFinished = PR_TRUE;
        entry->mError = error;
        if (0 != error) {
          rv = 0;
        }
        SocketWaiter** psw = &entry->mSockets;
        SocketWaiter* sw = entry->mSockets;
        while (NULL != sw) {
          // Poke netlib
          int iWantMore = NET_ProcessNet(sw->mSocket, NET_SOCKET_FD);
          *psw = sw->mNext;
          PR_Free(sw);
          sw = *psw;
        }
#if XXX_DONT_CACHE_DNS_LOOKUPS
        if (NULL == entry->mSockets) {
          // Now that entry is complete, remove it
          *pentry = entry->mNext;
          entry->Destroy();
          PR_Free(entry);
        }
#endif
        return rv;
      }
      pentry = &entry->mNext;
      entry = entry->mNext;
    }
    return rv;
  } 
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void net_InitAsyncDNS()
{
  static char *windowClass = "NETLIB:DNSWindow";

#ifdef DEBUG
  gDNSLogModuleInfo = PR_NewLogModule("netlibdns");
#endif

  // Allocate a message id for the event handler
  gMSGFoundDNS = RegisterWindowMessage("Mozilla:DNSMessage");

  // Register the class for the event receiver window
  WNDCLASS wc;
  wc.style         = 0;
  wc.lpfnWndProc   = EventProc;
  wc.cbClsExtra    = 0;
  wc.cbWndExtra    = 0;
  wc.hInstance     = NULL;
  wc.hIcon         = NULL;
  wc.hCursor       = NULL;
  wc.hbrBackground = (HBRUSH) NULL;
  wc.lpszMenuName  = (LPCSTR) NULL;
  wc.lpszClassName = windowClass;
  RegisterClass(&wc);
        
  // Create the event receiver window
  gDNSWindow = CreateWindow(windowClass,
                            "Mozilla:DNSEventHandler",
                            0, 0, 0, 10, 10,
                            NULL, NULL, NULL,
                            NULL);

  DNS_TRACE(DNS_TRACE_LOOKUPS,
            ("DNS lookup: hidden window=%p msg=%d",
             gDNSWindow, gMSGFoundDNS));
}

extern "C" int
NET_AsyncDNSLookup(void* aContext,
                   const char* aHostPort,
                   PRHostEnt** aHoststructPtrPtr,
                   PRFileDesc* aSocket);

int
NET_AsyncDNSLookup(void* aContext,
                   const char* aHostPort,
                   PRHostEnt** aHoststructPtrPtr,
                   PRFileDesc* aSocket)
{
  /* DNS initialization failed... */
  if (NULL == gDNSWindow) {
    PR_ASSERT(0);
    return -1;
  }

  *aHoststructPtrPtr = NULL;

  // Look in cache first
  DNSCacheEntry** pentry = DNSCacheEntry::Lookup(aHostPort);
  DNSCacheEntry* entry = *pentry;
  if (NULL != entry) {
    // If lookup returned an error then indicate that to the caller...
    if ((0 != entry->mError) && entry->mFinished) {
      WSASetLastError(entry->mError);
      if (NULL == entry->mSockets) {
        // XXX can't get here, right?
        // Delete entry when last socket is done with it...
        *pentry = entry->mNext;
        entry->Destroy();
        PR_Free(entry);
      }
      DNS_TRACE(DNS_TRACE_LOOKUPS,
                ("DNS lookup: '%s' error=%d\n", entry->mHost, entry->mError));
      return -1;
    }

    // If lookup finished then return the answer
    if ((NULL != entry->mHostEnt->h_name) && entry->mFinished) {
      DNS_TRACE(DNS_TRACE_LOOKUPS,
                ("DNS lookup: '%s' finished\n", entry->mHost));
      *aHoststructPtrPtr = entry->mHostEnt;
      return 0;
    }

    // See if we are already waiting for the answer
    SocketWaiter* sw = entry->mSockets;
    while (NULL != sw) {
      if (sw->mSocket == aSocket) {
        return MK_WAITING_FOR_LOOKUP;
      }
      sw = sw->mNext;
    }

    // Add this socket to the wait list
    sw = (SocketWaiter*) PR_Malloc(sizeof(SocketWaiter));
    if (NULL == sw) {
      return -1;
    }
    sw->Init(aSocket);
    sw->mSocket = aSocket;
    sw->mNext = entry->mSockets;
    entry->mSockets = sw;
    DNS_TRACE(DNS_TRACE_LOOKUPS,
              ("DNS lookup: still waiting for host='%s'\n", entry->mHost));
    return MK_WAITING_FOR_LOOKUP;
  }

  // Create a new cache entry for the host name
  entry = (DNSCacheEntry*) PR_Malloc(sizeof(DNSCacheEntry));
  if (NULL == entry) {
    return -1;
  }
  if (NS_OK != entry->Init(aContext, aHostPort, aSocket)) {
    entry->Destroy();
    PR_Free(entry);
    return -1;
  }

  // Start async lookup on that host name
  HANDLE h = WSAAsyncGetHostByName(gDNSWindow,
                                   gMSGFoundDNS,
                                   entry->mHost,
                                   (char *)entry->mHostEnt,
                                   PR_NETDB_BUF_SIZE);
  DNS_TRACE(DNS_TRACE_LOOKUPS,
            ("DNS lookup: begin waiting for host='%s' handle=%p\n",
             entry->mHost, h));
  if (NULL == h) {
    entry->Destroy();
    PR_Free(entry);
    return -1;
  }
  entry->mHandle = h;
  entry->mNext = *pentry;
  *pentry = entry;
  return MK_WAITING_FOR_LOOKUP;
}
#endif
