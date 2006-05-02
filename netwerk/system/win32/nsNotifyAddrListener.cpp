/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set et sw=4 ts=4: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is written by Juan Lang.
 *
 * The Initial Developer of the Original Code is 
 * Juan Lang.
 * Portions created by the Initial Developer are Copyright (C) 2003,2006
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

#include <stdarg.h>
#include <windef.h>
#include <winbase.h>
#include <winuser.h>
#include <winsock2.h>
#include <iphlpapi.h>
#include "prmem.h"
#include "prthread.h"
#include "plstr.h"
#include "nsEventQueueUtils.h"
#include "nsIObserverService.h"
#include "nsServiceManagerUtils.h"
#include "nsNotifyAddrListener.h"
#include "nsString.h"

typedef DWORD (WINAPI *GetAdaptersAddressesFunc)(ULONG, DWORD, PVOID,
                                                 PIP_ADAPTER_ADDRESSES,
                                                 PULONG);
typedef DWORD (WINAPI *GetAdaptersInfoFunc)(PIP_ADAPTER_INFO, PULONG);
typedef DWORD (WINAPI *GetIfEntryFunc)(PMIB_IFROW);
typedef DWORD (WINAPI *GetIpAddrTableFunc)(PMIB_IPADDRTABLE, PULONG, BOOL);
typedef DWORD (WINAPI *NotifyAddrChangeFunc)(PHANDLE, LPOVERLAPPED);

static HMODULE sIPHelper;
static GetAdaptersAddressesFunc sGetAdaptersAddresses;
static GetAdaptersInfoFunc sGetAdaptersInfo;
static GetIfEntryFunc sGetIfEntry;
static GetIpAddrTableFunc sGetIpAddrTable;
static NotifyAddrChangeFunc sNotifyAddrChange;

static void InitIPHelperLibrary(void)
{
    if (sIPHelper)
        return;

    sIPHelper = LoadLibraryA("iphlpapi.dll");
    if (!sIPHelper)
        return;

    sGetAdaptersAddresses = (GetAdaptersAddressesFunc)
        GetProcAddress(sIPHelper, "GetAdaptersAddresses");
    sGetAdaptersInfo = (GetAdaptersInfoFunc)
        GetProcAddress(sIPHelper, "GetAdaptersInfo");
    sGetIfEntry = (GetIfEntryFunc)
        GetProcAddress(sIPHelper, "GetIfEntry");
    sGetIpAddrTable = (GetIpAddrTableFunc)
        GetProcAddress(sIPHelper, "GetIpAddrTable");
    sNotifyAddrChange = (NotifyAddrChangeFunc)
        GetProcAddress(sIPHelper, "NotifyAddrChange");
}

static void FreeIPHelperLibrary(void)
{
    if (!sIPHelper)
        return;

    sGetAdaptersAddresses = nsnull;
    sGetAdaptersInfo = nsnull;
    sGetIfEntry = nsnull;
    sGetIpAddrTable = nsnull;
    sNotifyAddrChange = nsnull;

    FreeLibrary(sIPHelper);
    sIPHelper = nsnull;
}

NS_IMPL_THREADSAFE_ISUPPORTS3(nsNotifyAddrListener,
                              nsINetworkLinkService,
                              nsIRunnable,
                              nsIObserver)

nsNotifyAddrListener::nsNotifyAddrListener()
    : mLinkUp(PR_FALSE)
    , mStatusKnown(PR_FALSE)
    , mThread(0)
    , mShutdownEvent(nsnull)
{
    mOSVerInfo.dwOSVersionInfoSize = sizeof(mOSVerInfo);
    GetVersionEx(&mOSVerInfo);
    InitIPHelperLibrary();
}

nsNotifyAddrListener::~nsNotifyAddrListener()
{
    NS_ASSERTION(!mThread, "nsNotifyAddrListener thread shutdown failed");
    FreeIPHelperLibrary();
}

NS_IMETHODIMP
nsNotifyAddrListener::GetIsLinkUp(PRBool *aIsUp)
{
    *aIsUp = mLinkUp;
    return NS_OK;
}

NS_IMETHODIMP
nsNotifyAddrListener::GetLinkStatusKnown(PRBool *aIsUp)
{
    *aIsUp = mStatusKnown;
    return NS_OK;
}

NS_IMETHODIMP
nsNotifyAddrListener::Run()
{
    HANDLE ev = CreateEvent(nsnull, FALSE, FALSE, nsnull);
    NS_ENSURE_TRUE(ev, NS_ERROR_OUT_OF_MEMORY);

    HANDLE handles[2] = { ev, mShutdownEvent };
    OVERLAPPED overlapped = { 0 };
    PRBool shuttingDown = PR_FALSE;

    overlapped.hEvent = ev;
    while (!shuttingDown) {
        HANDLE h;
        DWORD ret = sNotifyAddrChange(&h, &overlapped);

        if (ret == ERROR_IO_PENDING) {
            ret = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
            if (ret == WAIT_OBJECT_0) {
                CheckLinkStatus();
            } else {
                shuttingDown = PR_TRUE;
            }
        } else {
            shuttingDown = PR_TRUE;
        }
    }
    CloseHandle(ev);

    return NS_OK;
}

NS_IMETHODIMP
nsNotifyAddrListener::Observe(nsISupports *subject,
                              const char *topic,
                              const PRUnichar *data)
{
    if (!strcmp(NS_XPCOM_SHUTDOWN_OBSERVER_ID, topic))
        Shutdown();

    return NS_OK;
}

nsresult
nsNotifyAddrListener::Init(void)
{
    CheckLinkStatus();

    // only start a thread on Windows 2000 or later
    if (mOSVerInfo.dwPlatformId != VER_PLATFORM_WIN32_NT ||
        mOSVerInfo.dwMajorVersion < 5)
        return NS_OK;

    nsresult rv;
    nsCOMPtr<nsIObserverService> observerService =
        do_GetService("@mozilla.org/observer-service;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID,
                                      PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);

    mShutdownEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    NS_ENSURE_TRUE(mShutdownEvent, NS_ERROR_OUT_OF_MEMORY);

    rv = NS_NewThread(getter_AddRefs(mThread), this, 0,
                      PR_JOINABLE_THREAD);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}

nsresult
nsNotifyAddrListener::Shutdown(void)
{
    // remove xpcom shutdown observer
    nsCOMPtr<nsIObserverService> observerService =
        do_GetService("@mozilla.org/observer-service;1");
    if (observerService)
        observerService->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);

    if (!mShutdownEvent)
        return NS_OK;

    SetEvent(mShutdownEvent);

    nsresult rv = mThread->Join();

    // Have to break the cycle here, otherwise nsNotifyAddrListener holds
    // onto the thread and the thread holds onto the nsNotifyAddrListener
    // via its mRunnable
    mThread = nsnull;

    CloseHandle(mShutdownEvent);
    mShutdownEvent = NULL;

    return rv;
}

/* Sends the given event to the UI thread.  Assumes aEventID never goes out
 * of scope (static strings are ideal).
 */
nsresult
nsNotifyAddrListener::SendEventToUI(const char *aEventID)
{
    nsresult rv;

    if (!aEventID) return NS_ERROR_NULL_POINTER;

    nsCOMPtr<nsIEventQueue> eq;
    rv = NS_GetMainEventQ(getter_AddRefs(eq));
    if (NS_FAILED(rv))
        return rv;

    ChangeEvent *event = new ChangeEvent(aEventID);
    if (!event)
        return NS_ERROR_OUT_OF_MEMORY;
    // AddRef this because it is being placed in the PLEvent; it'll be Released
    // when DestroyInterfaceEvent is called
    NS_ADDREF_THIS();
    PL_InitEvent(event, this, HandleInterfaceEvent, DestroyInterfaceEvent);

    if (NS_FAILED(rv = eq->PostEvent(event))) {
        NS_ERROR("failed to post event to UI EventQueue");
        PL_DestroyEvent(event);
    }
    return rv;
}

/*static*/ void *PR_CALLBACK
nsNotifyAddrListener::HandleInterfaceEvent(PLEvent *aEvent)
{
    ChangeEvent *event = NS_STATIC_CAST(ChangeEvent *, aEvent);

    nsCOMPtr<nsIObserverService> observerService =
        do_GetService("@mozilla.org/observer-service;1");
    if (observerService)
        observerService->NotifyObservers(
            NS_STATIC_CAST(nsINetworkLinkService *, PL_GetEventOwner(aEvent)),
            NS_NETWORK_LINK_TOPIC,
            NS_ConvertASCIItoUTF16(event->mEventID).get());
    return nsnull;
}

/*static*/ void PR_CALLBACK
nsNotifyAddrListener::DestroyInterfaceEvent(PLEvent *aEvent)
{
    nsNotifyAddrListener *self =
        NS_STATIC_CAST(nsNotifyAddrListener *, PL_GetEventOwner(aEvent));
    NS_RELEASE(self);
    delete aEvent;
}

DWORD
nsNotifyAddrListener::GetOperationalStatus(DWORD aAdapterIndex)
{
    DWORD status = MIB_IF_OPER_STATUS_CONNECTED;

    // try to get operational status on WinNT--on Win98, it consistently gives
    // me the wrong status, dagnabbit
    if (mOSVerInfo.dwPlatformId == VER_PLATFORM_WIN32_NT) {
        // If this fails, assume it's connected.  Didn't find a KB, but it
        // failed for me w/Win2K SP2, and succeeded for me w/Win2K SP3.
        if (sGetIfEntry) {
            MIB_IFROW ifRow;

            ifRow.dwIndex = aAdapterIndex;
            if (sGetIfEntry(&ifRow) == ERROR_SUCCESS)
                status = ifRow.dwOperStatus;
        }
    }
    return status;
}

/**
 * Calls GetIpAddrTable to check whether a link is up.  Assumes so if any
 * adapter has a non-zero IP (v4) address.  Sets mLinkUp if GetIpAddrTable
 * succeeds, but doesn't set mStatusKnown.
 * Returns ERROR_SUCCESS on success, and a Win32 error code otherwise.
 */
DWORD
nsNotifyAddrListener::CheckIPAddrTable(void)
{
    if (!sGetIpAddrTable)
        return ERROR_CALL_NOT_IMPLEMENTED;

    ULONG size = 0;
    DWORD ret = sGetIpAddrTable(nsnull, &size, FALSE);
    if (ret == ERROR_INSUFFICIENT_BUFFER && size > 0) {
        PMIB_IPADDRTABLE table = (PMIB_IPADDRTABLE) malloc(size);
        if (!table)
            return ERROR_OUTOFMEMORY;

        ret = sGetIpAddrTable(table, &size, FALSE);
        if (ret == ERROR_SUCCESS) {
            PRBool linkUp = PR_FALSE;

            for (DWORD i = 0; !linkUp && i < table->dwNumEntries; i++) {
                if (GetOperationalStatus(table->table[i].dwIndex) >=
                        MIB_IF_OPER_STATUS_CONNECTED &&
                        table->table[i].dwAddr != 0)
                    linkUp = PR_TRUE;
            }
            mLinkUp = linkUp;
        }
        free(table);
    }
    return ret;
}

/**
 * Checks whether a link is up by calling GetAdaptersInfo.  If any adapter's
 * operational status is at least MIB_IF_OPER_STATUS_CONNECTED, checks:
 * 1. If it's configured for DHCP, the link is considered up if the DHCP
 *    server is initialized.
 * 2. If it's not configured for DHCP, the link is considered up if it has a
 *    nonzero IP address.
 * Sets mLinkUp and mStatusKnown if GetAdaptersInfo succeeds.
 * Returns ERROR_SUCCESS on success, and a Win32 error code otherwise.  If the
 * call is not present on the current platform, returns ERROR_NOT_SUPPORTED.
 */
DWORD
nsNotifyAddrListener::CheckAdaptersInfo(void)
{
    if (!sGetAdaptersInfo)
        return ERROR_NOT_SUPPORTED;

    ULONG adaptersLen = 0;

    DWORD ret = sGetAdaptersInfo(0, &adaptersLen);
    if (ret == ERROR_BUFFER_OVERFLOW && adaptersLen > 0) {
        PIP_ADAPTER_INFO adapters = (PIP_ADAPTER_INFO) malloc(adaptersLen);
        if (sGetAdaptersInfo(adapters, &adaptersLen) == ERROR_SUCCESS) {
            PRBool linkUp = PR_FALSE;
            PIP_ADAPTER_INFO ptr;

            for (ptr = adapters; ptr && !linkUp; ptr = ptr->Next) {
                if (GetOperationalStatus(ptr->Index) >=
                        MIB_IF_OPER_STATUS_CONNECTED) {
                    if (ptr->DhcpEnabled) {
                        if (PL_strcmp(ptr->DhcpServer.IpAddress.String,
                                      "255.255.255.255")) {
                            // it has a DHCP server, therefore it must have
                            // a usable address
                            linkUp = PR_TRUE;
                        }
                    }
                    else {
                        PIP_ADDR_STRING ipAddr;
                        for (ipAddr = &ptr->IpAddressList; ipAddr && !linkUp;
                             ipAddr = ipAddr->Next)
                            if (PL_strcmp(ipAddr->IpAddress.String, "0.0.0.0"))
                                linkUp = PR_TRUE;
                    }
                }
            }
            mLinkUp = linkUp;
            mStatusKnown = PR_TRUE;
            free(adapters);
        }
    }
    return ret;
}

DWORD
nsNotifyAddrListener::CheckAdaptersAddresses(void)
{
    static const DWORD flags =
        GAA_FLAG_SKIP_FRIENDLY_NAME | GAA_FLAG_SKIP_ANYCAST |
        GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;

    if (!sGetAdaptersAddresses)
        return ERROR_NOT_SUPPORTED;

    ULONG len = 0;

    DWORD ret = sGetAdaptersAddresses(AF_UNSPEC, 0, NULL, NULL, &len);
    if (ret == ERROR_BUFFER_OVERFLOW) {
        PIP_ADAPTER_ADDRESSES addresses = (PIP_ADAPTER_ADDRESSES) malloc(len);
        if (addresses) {
            ret = sGetAdaptersAddresses(AF_UNSPEC, 0, NULL, addresses, &len);
            if (ret == ERROR_SUCCESS) {
                PIP_ADAPTER_ADDRESSES ptr;
                BOOL linkUp = FALSE;

                for (ptr = addresses; !linkUp && ptr; ptr = ptr->Next) {
                    if (ptr->OperStatus == IfOperStatusUp &&
                            ptr->IfType != IF_TYPE_SOFTWARE_LOOPBACK)
                        linkUp = TRUE;
                }
                mLinkUp = linkUp;
                mStatusKnown = TRUE;
            }
            free(addresses);
        }
    }
    return ret;
}

/**
 * Checks the status of all network adapters.  If one is up and has a valid IP
 * address, sets mLinkUp to true.  Sets mStatusKnown to true if the link status
 * is definitive.
 */
void
nsNotifyAddrListener::CheckLinkStatus(void)
{
    DWORD ret;
    const char *event;

    ret = CheckAdaptersAddresses();
    if (ret == ERROR_NOT_SUPPORTED)
        ret = CheckAdaptersInfo();
    if (ret == ERROR_NOT_SUPPORTED)
        ret = CheckIPAddrTable();
    if (ret != ERROR_SUCCESS)
        mLinkUp = PR_TRUE; // I can't tell, so assume there's a link

    if (mStatusKnown)
        event = mLinkUp ? NS_NETWORK_LINK_DATA_UP : NS_NETWORK_LINK_DATA_DOWN;
    else
        event = NS_NETWORK_LINK_DATA_UNKNOWN;
    SendEventToUI(event);
}
