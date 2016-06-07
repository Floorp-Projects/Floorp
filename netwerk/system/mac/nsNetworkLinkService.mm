/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <sys/socket.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <arpa/inet.h>
#include "nsCOMPtr.h"
#include "nsIObserverService.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsCRT.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "mozilla/SHA1.h"
#include "mozilla/Base64.h"
#include "nsNetworkLinkService.h"

#import <Cocoa/Cocoa.h>
#import <netinet/in.h>

#define NETWORK_NOTIFY_CHANGED_PREF "network.notify.changed"

using namespace mozilla;

static LazyLogModule gNotifyAddrLog("nsNotifyAddr");
#define LOG(args) MOZ_LOG(gNotifyAddrLog, mozilla::LogLevel::Debug, args)

// If non-successful, extract the error code and return it.  This
// error code dance is inspired by
// http://developer.apple.com/technotes/tn/tn1145.html
static OSStatus getErrorCodeBool(Boolean success)
{
    OSStatus err = noErr;
    if (!success) {
        int scErr = ::SCError();
        if (scErr == kSCStatusOK) {
            scErr = kSCStatusFailed;
        }
        err = scErr;
    }
    return err;
}

// If given a NULL pointer, return the error code.
static OSStatus getErrorCodePtr(const void *value)
{
    return getErrorCodeBool(value != NULL);
}

// Convenience function to allow NULL input.
static void CFReleaseSafe(CFTypeRef cf)
{
    if (cf) {
        // "If cf is NULL, this will cause a runtime error and your
        // application will crash." / Apple docs
        ::CFRelease(cf);
    }
}

NS_IMPL_ISUPPORTS(nsNetworkLinkService,
                  nsINetworkLinkService,
                  nsIObserver)

nsNetworkLinkService::nsNetworkLinkService()
    : mLinkUp(true)
    , mStatusKnown(false)
    , mAllowChangedEvent(true)
    , mReachability(nullptr)
    , mCFRunLoop(nullptr)
    , mRunLoopSource(nullptr)
    , mStoreRef(nullptr)
{
}

nsNetworkLinkService::~nsNetworkLinkService()
{
}

NS_IMETHODIMP
nsNetworkLinkService::GetIsLinkUp(bool *aIsUp)
{
    *aIsUp = mLinkUp;
    return NS_OK;
}

NS_IMETHODIMP
nsNetworkLinkService::GetLinkStatusKnown(bool *aIsUp)
{
    *aIsUp = mStatusKnown;
    return NS_OK;
}

NS_IMETHODIMP
nsNetworkLinkService::GetLinkType(uint32_t *aLinkType)
{
    NS_ENSURE_ARG_POINTER(aLinkType);

    // XXX This function has not yet been implemented for this platform
    *aLinkType = nsINetworkLinkService::LINK_TYPE_UNKNOWN;
    return NS_OK;
}

#ifndef SA_SIZE
#define SA_SIZE(sa)                                                     \
  (  (!(sa) || ((struct sockaddr *)(sa))->sa_len == 0) ?                \
     sizeof(uint32_t)            :                                      \
     1 + ( (((struct sockaddr *)(sa))->sa_len - 1) | (sizeof(uint32_t) - 1) ) )
#endif

static char *getMac(struct sockaddr_dl *sdl, char *buf, size_t bufsize)
{
    char *cp;
    int n, p = 0;

    buf[0] = 0;
    cp = (char *)LLADDR(sdl);
    n = sdl->sdl_alen;
    if (n > 0) {
        while (--n >= 0) {
            p += snprintf(&buf[p], bufsize - p, "%02x%s",
                          *cp++ & 0xff, n > 0 ? ":" : "");
        }
    }
    return buf;
}

/* If the IP matches, get the MAC and return true */
static bool matchIp(struct sockaddr_dl *sdl, struct sockaddr_inarp *addr,
                    char *ip, char *buf, size_t bufsize)
{
    if (sdl->sdl_alen) {
        if (!strcmp(inet_ntoa(addr->sin_addr), ip)) {
            getMac(sdl, buf, bufsize);
            return true; /* done! */
        }
    }
    return false; /* continue */
}

/*
 * Scan for the 'IP' address in the ARP table and store the corresponding MAC
 * address in 'mac'. The output buffer is 'maclen' bytes big.
 *
 * Returns 'true' if it found the IP and returns a MAC.
 */
static bool scanArp(char *ip, char *mac, size_t maclen)
{
    int mib[6];
    char *lim, *next;
    int st;

    mib[0] = CTL_NET;
    mib[1] = PF_ROUTE;
    mib[2] = 0;
    mib[3] = AF_INET;
    mib[4] = NET_RT_FLAGS;
    mib[5] = RTF_LLINFO;

    size_t needed;
    if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0) {
        return false;
    }
    if (needed == 0) {
        // empty table
        return false;
    }

    UniquePtr <char[]>buf(new char[needed]);

    for (;;) {
        st = sysctl(mib, 6, &buf[0], &needed, NULL, 0);
        if (st == 0 || errno != ENOMEM) {
            break;
        }
        needed += needed / 8;

        auto tmp = MakeUnique<char[]>(needed);
        memcpy(&tmp[0], &buf[0], needed);
        buf = Move(tmp);
    }
    if (st == -1) {
        return false;
    }
    lim = &buf[needed];

    struct rt_msghdr *rtm;
    for (next = &buf[0]; next < lim; next += rtm->rtm_msglen) {
        rtm = reinterpret_cast<struct rt_msghdr *>(next);
        struct sockaddr_inarp *sin2 =
            reinterpret_cast<struct sockaddr_inarp *>(rtm + 1);
        struct sockaddr_dl *sdl =
            reinterpret_cast<struct sockaddr_dl *>
            ((char *)sin2 + SA_SIZE(sin2));
        if (matchIp(sdl, sin2, ip, mac, maclen)) {
            return true;
        }
    }

    return false;
}

static int routingTable(char *gw)
{
    size_t needed;
    int mib[6];
    struct rt_msghdr *rtm;
    struct sockaddr *sa;
    struct sockaddr_in *sockin;

    mib[0] = CTL_NET;
    mib[1] = PF_ROUTE;
    mib[2] = 0;
    mib[3] = 0;
    mib[4] = NET_RT_DUMP;
    mib[5] = 0;
    if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0) {
        return 1;
    }

    UniquePtr <char[]>buf(new char[needed]);

    if (sysctl(mib, 6, &buf[0], &needed, NULL, 0) < 0) {
        return 3;
    }

    rtm = reinterpret_cast<struct rt_msghdr *>(&buf[0]);
    sa = reinterpret_cast<struct sockaddr *>(rtm + 1);
    sa = reinterpret_cast<struct sockaddr *>(SA_SIZE(sa) + (char *)sa);
    sockin = reinterpret_cast<struct sockaddr_in *>(sa);
    inet_ntop(AF_INET, &sockin->sin_addr.s_addr, gw, MAXHOSTNAMELEN-1);

    return 0;
}

//
// Figure out the current "network identification" string.
//
// It detects the IP of the default gateway in the routing table, then the MAC
// address of that IP in the ARP table before it hashes that string (to avoid
// information leakage).
//
void nsNetworkLinkService::calculateNetworkId(void)
{
    char hw[MAXHOSTNAMELEN];
    routingTable(hw);

    char mac[256]; // big enough for a printable MAC address
    if (scanArp(hw, mac, sizeof(mac))) {
        LOG(("networkid: MAC %s\n", hw));
        nsAutoCString mac(hw);
        // This 'addition' could potentially be a
        // fixed number from the profile or something.
        nsAutoCString addition("local-rubbish");
        nsAutoCString output;
        SHA1Sum sha1;
        nsCString combined(mac + addition);
        sha1.update(combined.get(), combined.Length());
        uint8_t digest[SHA1Sum::kHashSize];
        sha1.finish(digest);
        nsCString newString(reinterpret_cast<char*>(digest),
                            SHA1Sum::kHashSize);
        nsresult rv = Base64Encode(newString, output);
        MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
        LOG(("networkid: id %s\n", output.get()));
        mNetworkId = output;
    }

}


NS_IMETHODIMP
nsNetworkLinkService::Observe(nsISupports *subject,
                              const char *topic,
                              const char16_t *data)
{
    if (!strcmp(topic, "xpcom-shutdown")) {
        Shutdown();
    }

    return NS_OK;
}

/* static */
void
nsNetworkLinkService::IPConfigChanged(SCDynamicStoreRef aStoreREf,
                                      CFArrayRef aChangedKeys,
                                      void *aInfo)
{
    nsNetworkLinkService *service =
        static_cast<nsNetworkLinkService*>(aInfo);
    service->SendEvent(true);
}

nsresult
nsNetworkLinkService::Init(void)
{
    nsresult rv;

    nsCOMPtr<nsIObserverService> observerService =
        do_GetService("@mozilla.org/observer-service;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = observerService->AddObserver(this, "xpcom-shutdown", false);
    NS_ENSURE_SUCCESS(rv, rv);

    Preferences::AddBoolVarCache(&mAllowChangedEvent,
                                 NETWORK_NOTIFY_CHANGED_PREF, true);

    // If the network reachability API can reach 0.0.0.0 without
    // requiring a connection, there is a network interface available.
    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_len = sizeof(addr);
    addr.sin_family = AF_INET;
    mReachability =
        ::SCNetworkReachabilityCreateWithAddress(NULL,
                                                 (struct sockaddr *)&addr);
    if (!mReachability) {
        return NS_ERROR_NOT_AVAILABLE;
    }

    SCNetworkReachabilityContext context = {0, this, NULL, NULL, NULL};
    if (!::SCNetworkReachabilitySetCallback(mReachability,
                                            ReachabilityChanged,
                                            &context)) {
        NS_WARNING("SCNetworkReachabilitySetCallback failed.");
        ::CFRelease(mReachability);
        mReachability = NULL;
        return NS_ERROR_NOT_AVAILABLE;
    }

    SCDynamicStoreContext storeContext = {0, this, NULL, NULL, NULL};
    mStoreRef =
        ::SCDynamicStoreCreate(NULL,
                               CFSTR("AddIPAddressListChangeCallbackSCF"),
                               IPConfigChanged, &storeContext);

    CFStringRef patterns[2] = {NULL, NULL};
    OSStatus err = getErrorCodePtr(mStoreRef);
    if (err == noErr) {
        // This pattern is "State:/Network/Service/[^/]+/IPv4".
        patterns[0] =
            ::SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
                                                          kSCDynamicStoreDomainState,
                                                          kSCCompAnyRegex,
                                                          kSCEntNetIPv4);
        err = getErrorCodePtr(patterns[0]);
        if (err == noErr) {
            // This pattern is "State:/Network/Service/[^/]+/IPv6".
            patterns[1] =
                ::SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
                                                              kSCDynamicStoreDomainState,
                                                              kSCCompAnyRegex,
                                                              kSCEntNetIPv6);
            err = getErrorCodePtr(patterns[1]);
        }
    }

    CFArrayRef patternList = NULL;
    // Create a pattern list containing just one pattern,
    // then tell SCF that we want to watch changes in keys
    // that match that pattern list, then create our run loop
    // source.
    if (err == noErr) {
        patternList = ::CFArrayCreate(NULL, (const void **) patterns,
                                      2, &kCFTypeArrayCallBacks);
        if (!patternList) {
            err = -1;
        }
    }
    if (err == noErr) {
        err =
            getErrorCodeBool(::SCDynamicStoreSetNotificationKeys(mStoreRef,
                                                                 NULL,
                                                                 patternList));
    }

    if (err == noErr) {
        mRunLoopSource =
            ::SCDynamicStoreCreateRunLoopSource(NULL, mStoreRef, 0);
        err = getErrorCodePtr(mRunLoopSource);
    }

    CFReleaseSafe(patterns[0]);
    CFReleaseSafe(patterns[1]);
    CFReleaseSafe(patternList);

    if (err != noErr) {
        CFReleaseSafe(mStoreRef);
        return NS_ERROR_NOT_AVAILABLE;
    }

    // Get the current run loop.  This service is initialized at startup,
    // so we shouldn't run in to any problems with modal dialog run loops.
    mCFRunLoop = [[NSRunLoop currentRunLoop] getCFRunLoop];
    if (!mCFRunLoop) {
        NS_WARNING("Could not get current run loop.");
        ::CFRelease(mReachability);
        mReachability = NULL;
        return NS_ERROR_NOT_AVAILABLE;
    }
    ::CFRetain(mCFRunLoop);

    ::CFRunLoopAddSource(mCFRunLoop, mRunLoopSource, kCFRunLoopDefaultMode);

    if (!::SCNetworkReachabilityScheduleWithRunLoop(mReachability, mCFRunLoop,
                                                    kCFRunLoopDefaultMode)) {
        NS_WARNING("SCNetworkReachabilityScheduleWIthRunLoop failed.");
        ::CFRelease(mReachability);
        mReachability = NULL;
        ::CFRelease(mCFRunLoop);
        mCFRunLoop = NULL;
        return NS_ERROR_NOT_AVAILABLE;
    }

    UpdateReachability();

    return NS_OK;
}

nsresult
nsNetworkLinkService::Shutdown()
{
    if (!::SCNetworkReachabilityUnscheduleFromRunLoop(mReachability,
                                                      mCFRunLoop,
                                                      kCFRunLoopDefaultMode)) {
        NS_WARNING("SCNetworkReachabilityUnscheduleFromRunLoop failed.");
    }

    CFRunLoopRemoveSource(mCFRunLoop, mRunLoopSource, kCFRunLoopDefaultMode);

    ::CFRelease(mReachability);
    mReachability = nullptr;

    ::CFRelease(mCFRunLoop);
    mCFRunLoop = nullptr;

    ::CFRelease(mStoreRef);
    mStoreRef = nullptr;

    ::CFRelease(mRunLoopSource);
    mRunLoopSource = nullptr;

    return NS_OK;
}

void
nsNetworkLinkService::UpdateReachability()
{
    if (!mReachability) {
        return;
    }

    SCNetworkConnectionFlags flags;
    if (!::SCNetworkReachabilityGetFlags(mReachability, &flags)) {
        mStatusKnown = false;
        return;
    }

    bool reachable = (flags & kSCNetworkFlagsReachable) != 0;
    bool needsConnection = (flags & kSCNetworkFlagsConnectionRequired) != 0;

    mLinkUp = (reachable && !needsConnection);
    mStatusKnown = true;
}

void
nsNetworkLinkService::SendEvent(bool aNetworkChanged)
{
    nsCOMPtr<nsIObserverService> observerService =
        do_GetService("@mozilla.org/observer-service;1");
    if (!observerService) {
        return;
    }

    const char *event;
    if (aNetworkChanged) {
        if (!mAllowChangedEvent) {
            return;
        }
        event = NS_NETWORK_LINK_DATA_CHANGED;
    } else if (!mStatusKnown) {
        event = NS_NETWORK_LINK_DATA_UNKNOWN;
    } else {
        event = mLinkUp ? NS_NETWORK_LINK_DATA_UP
            : NS_NETWORK_LINK_DATA_DOWN;
    }
    LOG(("SendEvent: network is '%s'\n", event));

    observerService->NotifyObservers(static_cast<nsINetworkLinkService*>(this),
                                     NS_NETWORK_LINK_TOPIC,
                                     NS_ConvertASCIItoUTF16(event).get());
}

/* static */
void
nsNetworkLinkService::ReachabilityChanged(SCNetworkReachabilityRef target,
                                          SCNetworkConnectionFlags flags,
                                          void *info)
{
    nsNetworkLinkService *service =
        static_cast<nsNetworkLinkService*>(info);

    service->UpdateReachability();
    service->SendEvent(false);
    service->calculateNetworkId();
}
