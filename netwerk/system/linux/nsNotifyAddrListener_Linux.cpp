/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set et sw=4 ts=4: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdarg.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <ifaddrs.h>
#include <net/if.h>

#include "nsThreadUtils.h"
#include "nsIObserverService.h"
#include "nsServiceManagerUtils.h"
#include "nsNotifyAddrListener_Linux.h"
#include "nsString.h"
#include "mozilla/Logging.h"

#include "mozilla/Base64.h"
#include "mozilla/FileUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/SHA1.h"
#include "mozilla/Sprintf.h"
#include "mozilla/Telemetry.h"

/* a shorter name that better explains what it does */
#define EINTR_RETRY(x) MOZ_TEMP_FAILURE_RETRY(x)

// period during which to absorb subsequent network change events, in
// milliseconds
static const unsigned int kNetworkChangeCoalescingPeriod  = 1000;

using namespace mozilla;

static LazyLogModule gNotifyAddrLog("nsNotifyAddr");
#define LOG(args) MOZ_LOG(gNotifyAddrLog, mozilla::LogLevel::Debug, args)

#define NETWORK_NOTIFY_CHANGED_PREF "network.notify.changed"

NS_IMPL_ISUPPORTS(nsNotifyAddrListener,
                  nsINetworkLinkService,
                  nsIRunnable,
                  nsIObserver)

nsNotifyAddrListener::nsNotifyAddrListener()
    : mLinkUp(true)  // assume true by default
    , mStatusKnown(false)
    , mAllowChangedEvent(true)
    , mCoalescingActive(false)
{
    mShutdownPipe[0] = -1;
    mShutdownPipe[1] = -1;
}

nsNotifyAddrListener::~nsNotifyAddrListener()
{
    MOZ_ASSERT(!mThread, "nsNotifyAddrListener thread shutdown failed");

    if (mShutdownPipe[0] != -1) {
        EINTR_RETRY(close(mShutdownPipe[0]));
    }
    if (mShutdownPipe[1] != -1) {
        EINTR_RETRY(close(mShutdownPipe[1]));
    }
}

NS_IMETHODIMP
nsNotifyAddrListener::GetIsLinkUp(bool *aIsUp)
{
    // XXX This function has not yet been implemented for this platform
    *aIsUp = mLinkUp;
    return NS_OK;
}

NS_IMETHODIMP
nsNotifyAddrListener::GetLinkStatusKnown(bool *aIsUp)
{
    // XXX This function has not yet been implemented for this platform
    *aIsUp = mStatusKnown;
    return NS_OK;
}

NS_IMETHODIMP
nsNotifyAddrListener::GetLinkType(uint32_t *aLinkType)
{
  NS_ENSURE_ARG_POINTER(aLinkType);

  // XXX This function has not yet been implemented for this platform
  *aLinkType = nsINetworkLinkService::LINK_TYPE_UNKNOWN;
  return NS_OK;
}

//
// Figure out the current "network identification" string.
//
// It detects the IP of the default gateway in the routing table, then the MAC
// address of that IP in the ARP table before it hashes that string (to avoid
// information leakage).
//
void nsNotifyAddrListener::calculateNetworkId(void)
{
    const char *kProcRoute = "/proc/net/route"; /* IPv4 routes */
    const char *kProcArp = "/proc/net/arp";
    bool found = false;

    FILE *froute = fopen(kProcRoute, "r");
    if (froute) {
        char buffer[512];
        uint32_t gw = 0;
        char *l = fgets(buffer, sizeof(buffer), froute);
        if (l) {
            /* skip the title line  */
            while (l) {
                char interf[32];
                uint32_t dest;
                uint32_t gateway;
                l = fgets(buffer, sizeof(buffer), froute);
                if (l) {
                    buffer[511]=0; /* as a precaution */
                    int val = sscanf(buffer, "%31s %x %x",
                                     interf, &dest, &gateway);
                    if ((3 == val) && !dest) {
                        gw = gateway;
                        break;
                    }
                }
            }
        }
        fclose(froute);

        if (gw) {
            /* create a string to search for in the arp table */
            char searchfor[16];
            SprintfLiteral(searchfor, "%d.%d.%d.%d",
                           gw & 0xff,
                           (gw >> 8) & 0xff,
                           (gw >> 16) & 0xff,
                           gw >> 24);

            FILE *farp = fopen(kProcArp, "r");
            if (farp) {
                l = fgets(buffer, sizeof(buffer), farp);
                while (l) {
                    /* skip the title line  */
                    l = fgets(buffer, sizeof(buffer), farp);
                    if (l) {
                        buffer[511]=0; /* as a precaution */
                        int p[4];
                        char type[16];
                        char flags[16];
                        char hw[32];
                        if (7 == sscanf(buffer, "%u.%u.%u.%u %15s %15s %31s",
                                        &p[0], &p[1], &p[2], &p[3],
                                        type, flags, hw)) {
                            uint32_t searchip = p[0] | (p[1] << 8) |
                                (p[2] << 16) | (p[3] << 24);
                            if (gw == searchip) {
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
                                if (mNetworkId != output) {
                                    // new id
                                    Telemetry::Accumulate(Telemetry::NETWORK_ID, 1);
                                    mNetworkId = output;
                                }
                                else {
                                    // same id
                                    Telemetry::Accumulate(Telemetry::NETWORK_ID, 2);
                                }
                                found = true;
                                break;
                            }
                        }
                    }
                }
                fclose(farp);
            } /* if (farp) */
        } /* if (gw) */
    } /* if (froute) */
    if (!found) {
        // no id
        Telemetry::Accumulate(Telemetry::NETWORK_ID, 0);
    }
}

//
// Check if there's a network interface available to do networking on.
//
void nsNotifyAddrListener::checkLink(void)
{
    struct ifaddrs *list;
    struct ifaddrs *ifa;
    bool link = false;
    bool prevLinkUp = mLinkUp;

    if (getifaddrs(&list))
        return;

    // Walk through the linked list, maintaining head pointer so we can free
    // list later

    for (ifa = list; ifa != nullptr; ifa = ifa->ifa_next) {
        int family;
        if (ifa->ifa_addr == nullptr)
            continue;

        family = ifa->ifa_addr->sa_family;

        if ((family == AF_INET || family == AF_INET6) &&
            (ifa->ifa_flags & IFF_RUNNING) &&
            !(ifa->ifa_flags & IFF_LOOPBACK)) {
            // An interface that is UP and not loopback
            link = true;
            break;
        }
    }
    mLinkUp = link;
    freeifaddrs(list);

    if (prevLinkUp != mLinkUp) {
        // UP/DOWN status changed, send appropriate UP/DOWN event
        SendEvent(mLinkUp ?
                  NS_NETWORK_LINK_DATA_UP : NS_NETWORK_LINK_DATA_DOWN);
    }
}

void nsNotifyAddrListener::OnNetlinkMessage(int aNetlinkSocket)
{
    struct  nlmsghdr *nlh;

    // The buffer size below, (4095) was chosen partly based on testing and
    // partly on existing sample source code using this size. It needs to be
    // large enough to hold the netlink messages from the kernel.
    char buffer[4095];
    struct rtattr *attr;
    int attr_len;
    const struct ifaddrmsg* newifam;


    ssize_t rc = EINTR_RETRY(recv(aNetlinkSocket, buffer, sizeof(buffer), 0));
    if (rc < 0) {
        return;
    }
    size_t netlink_bytes = rc;

    nlh = reinterpret_cast<struct nlmsghdr *>(buffer);

    bool networkChange = false;

    for (; NLMSG_OK(nlh, netlink_bytes);
         nlh = NLMSG_NEXT(nlh, netlink_bytes)) {
        char prefixaddr[INET6_ADDRSTRLEN];
        char localaddr[INET6_ADDRSTRLEN];
        char* addr = nullptr;
        prefixaddr[0] = localaddr[0] = '\0';

        if (NLMSG_DONE == nlh->nlmsg_type) {
            break;
        }

        LOG(("nsNotifyAddrListener::OnNetlinkMessage: new/deleted address\n"));
        newifam = reinterpret_cast<struct ifaddrmsg*>(NLMSG_DATA(nlh));

        if ((newifam->ifa_family != AF_INET) &&
            (newifam->ifa_family != AF_INET6)) {
            continue;
        }

        attr = IFA_RTA (newifam);
        attr_len = IFA_PAYLOAD (nlh);
        for (;attr_len && RTA_OK (attr, attr_len);
             attr = RTA_NEXT (attr, attr_len)) {
            if (attr->rta_type == IFA_ADDRESS) {
                if (newifam->ifa_family == AF_INET) {
                    struct in_addr* in = (struct in_addr*)RTA_DATA(attr);
                    inet_ntop(AF_INET, in, prefixaddr, INET_ADDRSTRLEN);
                } else {
                    struct in6_addr* in = (struct in6_addr*)RTA_DATA(attr);
                    inet_ntop(AF_INET6, in, prefixaddr, INET6_ADDRSTRLEN);
                }
            } else if (attr->rta_type == IFA_LOCAL) {
                if (newifam->ifa_family == AF_INET) {
                    struct in_addr* in = (struct in_addr*)RTA_DATA(attr);
                    inet_ntop(AF_INET, in, localaddr, INET_ADDRSTRLEN);
                } else {
                    struct in6_addr* in = (struct in6_addr*)RTA_DATA(attr);
                    inet_ntop(AF_INET6, in, localaddr, INET6_ADDRSTRLEN);
                }
            }
        }
        if (localaddr[0]) {
            addr = localaddr;
        } else if (prefixaddr[0]) {
            addr = prefixaddr;
        } else {
            continue;
        }
        if (nlh->nlmsg_type == RTM_NEWADDR) {
            LOG(("nsNotifyAddrListener::OnNetlinkMessage: a new address "
                 "- %s.", addr));
            struct ifaddrmsg* ifam;
            nsCString addrStr;
            addrStr.Assign(addr);
            if (auto entry = mAddressInfo.LookupForAdd(addrStr)) {
                ifam = entry.Data();
                LOG(("nsNotifyAddrListener::OnNetlinkMessage: the address "
                     "already known."));
                if (memcmp(ifam, newifam, sizeof(struct ifaddrmsg))) {
                    LOG(("nsNotifyAddrListener::OnNetlinkMessage: but "
                         "the address info has been changed."));
                    networkChange = true;
                    memcpy(ifam, newifam, sizeof(struct ifaddrmsg));
                }
            } else {
                networkChange = true;
                ifam = (struct ifaddrmsg*)malloc(sizeof(struct ifaddrmsg));
                memcpy(ifam, newifam, sizeof(struct ifaddrmsg));
                entry.OrInsert([ifam] () { return ifam; });
            }
        } else {
            LOG(("nsNotifyAddrListener::OnNetlinkMessage: an address "
                 "has been deleted - %s.", addr));
            networkChange = true;
            nsCString addrStr;
            addrStr.Assign(addr);
            mAddressInfo.Remove(addrStr);
        }
    }

    if (networkChange && mAllowChangedEvent) {
        NetworkChanged();
    }

    if (networkChange) {
        checkLink();
    }
}

NS_IMETHODIMP
nsNotifyAddrListener::Run()
{
    int netlinkSocket = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (netlinkSocket < 0) {
        return NS_ERROR_FAILURE;
    }

    struct sockaddr_nl addr;
    memset(&addr, 0, sizeof(addr));   // clear addr

    addr.nl_family = AF_NETLINK;
    addr.nl_groups = RTMGRP_IPV4_IFADDR | RTMGRP_IPV6_IFADDR;

    if (bind(netlinkSocket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        // failure!
        EINTR_RETRY(close(netlinkSocket));
        return NS_ERROR_FAILURE;
    }

    // switch the socket into non-blocking
    int flags = fcntl(netlinkSocket, F_GETFL, 0);
    (void)fcntl(netlinkSocket, F_SETFL, flags | O_NONBLOCK);

    struct pollfd fds[2];
    fds[0].fd = mShutdownPipe[0];
    fds[0].events = POLLIN;
    fds[0].revents = 0;

    fds[1].fd = netlinkSocket;
    fds[1].events = POLLIN;
    fds[1].revents = 0;

    calculateNetworkId();

    nsresult rv = NS_OK;
    bool shutdown = false;
    int pollWait = -1;
    while (!shutdown) {
        int rc = EINTR_RETRY(poll(fds, 2, pollWait));

        if (rc > 0) {
            if (fds[0].revents & POLLIN) {
                // shutdown, abort the loop!
                LOG(("thread shutdown received, dying...\n"));
                shutdown = true;
            } else if (fds[1].revents & POLLIN) {
                LOG(("netlink message received, handling it...\n"));
                OnNetlinkMessage(netlinkSocket);
            }
        } else if (rc < 0) {
            rv = NS_ERROR_FAILURE;
            break;
        }
        if (mCoalescingActive) {
            // check if coalescing period should continue
            double period = (TimeStamp::Now() - mChangeTime).ToMilliseconds();
            if (period >= kNetworkChangeCoalescingPeriod) {
                SendEvent(NS_NETWORK_LINK_DATA_CHANGED);
                calculateNetworkId();
                mCoalescingActive = false;
                pollWait = -1; // restore to default
            } else {
                // wait no longer than to the end of the period
                pollWait = static_cast<int>
                    (kNetworkChangeCoalescingPeriod - period);
            }
        }
    }

    EINTR_RETRY(close(netlinkSocket));

    return rv;
}

NS_IMETHODIMP
nsNotifyAddrListener::Observe(nsISupports *subject,
                              const char *topic,
                              const char16_t *data)
{
    if (!strcmp("xpcom-shutdown-threads", topic)) {
        Shutdown();
    }

    return NS_OK;
}

nsresult
nsNotifyAddrListener::Init(void)
{
    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    if (!observerService)
        return NS_ERROR_FAILURE;

    nsresult rv = observerService->AddObserver(this, "xpcom-shutdown-threads",
                                               false);
    NS_ENSURE_SUCCESS(rv, rv);

    Preferences::AddBoolVarCache(&mAllowChangedEvent,
                                 NETWORK_NOTIFY_CHANGED_PREF, true);

    if (-1 == pipe(mShutdownPipe)) {
        return NS_ERROR_FAILURE;
    }

    rv = NS_NewNamedThread("Link Monitor", getter_AddRefs(mThread), this);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}

nsresult
nsNotifyAddrListener::Shutdown(void)
{
    // remove xpcom shutdown observer
    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    if (observerService)
        observerService->RemoveObserver(this, "xpcom-shutdown-threads");

    LOG(("write() to signal thread shutdown\n"));

    // awake the thread to make it terminate
    ssize_t rc = EINTR_RETRY(write(mShutdownPipe[1], "1", 1));
    LOG(("write() returned %d, errno == %d\n", (int)rc, errno));

    nsresult rv = mThread->Shutdown();

    // Have to break the cycle here, otherwise nsNotifyAddrListener holds
    // onto the thread and the thread holds onto the nsNotifyAddrListener
    // via its mRunnable
    mThread = nullptr;

    return rv;
}


/*
 * A network event has been registered. Delay the actual sending of the event
 * for a while and absorb subsequent events in the mean time in an effort to
 * squash potentially many triggers into a single event.
 * Only ever called from the same thread.
 */
nsresult
nsNotifyAddrListener::NetworkChanged()
{
    if (mCoalescingActive) {
        LOG(("NetworkChanged: absorbed an event (coalescing active)\n"));
    } else {
        // A fresh trigger!
        mChangeTime = TimeStamp::Now();
        mCoalescingActive = true;
        LOG(("NetworkChanged: coalescing period started\n"));
    }
    return NS_OK;
}

/* Sends the given event.  Assumes aEventID never goes out of scope (static
 * strings are ideal).
 */
nsresult
nsNotifyAddrListener::SendEvent(const char *aEventID)
{
    if (!aEventID)
        return NS_ERROR_NULL_POINTER;

    LOG(("SendEvent: %s\n", aEventID));
    nsresult rv = NS_OK;
    nsCOMPtr<nsIRunnable> event = new ChangeEvent(this, aEventID);
    if (NS_FAILED(rv = NS_DispatchToMainThread(event)))
        NS_WARNING("Failed to dispatch ChangeEvent");
    return rv;
}

NS_IMETHODIMP
nsNotifyAddrListener::ChangeEvent::Run()
{
    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    if (observerService)
        observerService->NotifyObservers(
                mService, NS_NETWORK_LINK_TOPIC,
                NS_ConvertASCIItoUTF16(mEventID).get());
    return NS_OK;
}
