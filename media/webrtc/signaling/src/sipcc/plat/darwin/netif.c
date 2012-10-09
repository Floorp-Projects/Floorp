/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Cisco Systems SIP Stack.
 *
 * The Initial Developer of the Original Code is
 * Cisco Systems (CSCO).
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Enda Mannion <emannion@cisco.com>
 *  Suhas Nandakumar <snandaku@cisco.com>
 *  Ethan Hugg <ehugg@cisco.com>
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

#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ifaddrs.h>

#include <string.h>
#include <strings.h>
#include <stdio.h>

#define IN_ADDR_PLEN 16 /* including trailing '\0' */
#define IN6_ADDR_PLEN 128 /* including trailing '\0' */
#define MAC_ADDR_PLEN 18


//static char netif_ip_addr[IN6_ADDR_PLEN];
//static char netif_mac_addr[MAC_ADDR_PLEN];

//static int
//eth_macaddr_ntop(const struct sockaddr_dl *sdl, char *dst, size_t dstsize)
//{
//    const unsigned char *lladdr = LLADDR(sdl);
//    int r;
//
//    r = snprintf(dst, dstsize, "%02x:%02x:%02x:%02x:%02x:%02x",
//                (int) lladdr[0], (int) lladdr[1], (int) lladdr[2],
//                (int) lladdr[3], (int) lladdr[4], (int) lladdr[5]);
//
//    if (r >= dstsize)
//        /* The destination buffer is too small */
//        return -1;
//    else
//        return 0;
//}


/*
 * Find the local IP (v4 or v6) address used by the system to reach a given IP
 * address.
 */
//static int
//findlocaladdr(int af, struct sockaddr *dest, socklen_t destsize,
//                struct sockaddr *local, socklen_t *localsize)
//{
//    int fd;
//
//    if ((fd = socket(af, SOCK_DGRAM, 0)) == -1) {
//        perror("socket");
//        return -1;
//    }
//
//    if (connect(fd, dest, destsize) != 0) {
//        perror("connect");
//        close(fd);
//        return -1;
//    }
//
//    /*
//     * Retrieve the local address associated with the socket.
//     */
//    if (getsockname(fd, (struct sockaddr *) local, localsize) != 0) {
//        perror("getsockname");
//        close(fd);
//        return -1;
//    }
//
//    close(fd);
//
//    /* Check that the retrieved address is of the same family */
//    if (local->sa_family == af)
//        return 0;
//    else
//        return -1;
//}
//
//static int
//inaddrcmp(struct sockaddr *s1, struct sockaddr *s2)
//{
//    if (s1->sa_family == s2->sa_family && s1->sa_family == AF_INET) {
//        return bcmp(&((struct sockaddr_in *) s1)->sin_addr,
//                    &((struct sockaddr_in *) s2)->sin_addr,
//                    sizeof(struct in_addr));
//    } else if (s1->sa_family == s2->sa_family && s1->sa_family == AF_INET6) {
//        return bcmp(&((struct sockaddr_in6 *) s1)->sin6_addr,
//                    &((struct sockaddr_in6 *) s2)->sin6_addr,
//                    sizeof(struct in6_addr));
//    } else {
//        return -1;
//    }
//}

/*
 * Retrieve the IP address (and associated link-layer address) that will be
 * used by the operating system as the source IP address when sending packets
 * to the given destination address.
 */
//static int
//getifaddr(int dstaf, struct sockaddr *dst, socklen_t dstsize,
//          struct sockaddr *ipaddr, size_t ipaddrsize, struct sockaddr_dl *lladdr)
//{
//    struct sockaddr_storage ss;
//    socklen_t sslen;
//    struct ifaddrs *ifap, *ifp;
//    char *ifname = NULL;
//    int found_lladdr = 0;
//
//    /*
//     * First, determine the local IP address that will be used to reach the
//     * given destination IP address.  From that we will retrieve the interface
//     * and then the MAC address.
//     * `dstaf' can only be AF_INET or AF_INET6.
//     */
//    bzero(&ss, sizeof(struct sockaddr_storage));
//    sslen = sizeof(struct sockaddr_storage);
//    if (findlocaladdr(dstaf, dst, dstsize, (struct sockaddr *) &ss, &sslen)
//            != 0) {
//        return -1;
//    }
//
//    /*
//     * Find the name of the network interface matching the address discovered
//     * using findlocaladdr().  Note that this is not garanteed to yield the
//     * correct result (or a result at all) because the network configuration
//     * may change between the call to findlocaladdr() and getifaddrs().
//     * But this should work in most cases.
//     */
//    if (getifaddrs(&ifap) == -1)
//        return -1;
//
//    for (ifp = ifap; ifp->ifa_next != NULL; ifp = ifp->ifa_next) {
//        if (inaddrcmp(ifp->ifa_addr, (struct sockaddr *) &ss) == 0) {
//            ifname = ifp->ifa_name;
//            break;
//        }
//    }
//
//    /*
//     * Get the link-layer address matching the interface name.
//     */
//    if (ifname != NULL) {
//        for (ifp = ifap; ifp->ifa_next != NULL; ifp = ifp->ifa_next) {
//            if (ifp->ifa_addr->sa_family == AF_LINK
//                && strcmp(ifname, ifp->ifa_name) == 0) {
//                bcopy(ifp->ifa_addr, lladdr, sizeof(struct sockaddr_dl));
//                found_lladdr = 1;
//                break;
//            }
//        }
//    }
//
//    freeifaddrs(ifap);
//
//    if (!found_lladdr) {
//        /* The link-layer address was not found! */
//        return -1;
//    } else {
//        /* Copy the IP address to the buffer provided by the caller */
//        bcopy(&ss, ipaddr, ipaddrsize);
//        return 0;
//    }
//}
//
//static int
//getifaddr2(int af, struct sockaddr *ipaddr, size_t ipaddrsize,
//           struct sockaddr_dl *lladdr)
//{
//    struct ifaddrs *ifap, *ifp;
//    char *ifname;
//    int found_lladdr = 0;
//
//    if (getifaddrs(&ifap) == -1)
//        return -1;
//
//    /* Walk the list to find an active interface with an IPv4 or IPv6 address */
//    for (ifp = ifap; ifp->ifa_next != NULL; ifp = ifp->ifa_next) {
//        if (ifp->ifa_flags & (IFF_UP|IFF_RUNNING)
//            && !(ifp->ifa_flags & IFF_LOOPBACK)
//            && ifp->ifa_addr->sa_family == af) {
//            ifname = ifp->ifa_name;
//            bcopy(ifp->ifa_addr, ipaddr, ipaddrsize);
//            break;
//        }
//    }
//
//    /* Get the matching link-layer address */
//    for (ifp = ifap; ifp->ifa_next != NULL; ifp = ifp->ifa_next) {
//        if (ifp->ifa_addr->sa_family == AF_LINK
//            && strcmp(ifname, ifp->ifa_name) == 0) {
//            bcopy(ifp->ifa_addr, lladdr, sizeof(struct sockaddr_dl));
//            found_lladdr = 1;
//        }
//    }
//
//    freeifaddrs(ifap);
//
//    if (found_lladdr)
//        return 0;
//    else
//        return -1;
//}

//char *
//platGetIPAddr(void)
//{
//    struct sockaddr_in inaddr;
//    struct sockaddr_dl lladdr; /* Unused */
//
//    /*
//     * XXX We should use getifaddr() but need to figure out how this can be done
//     * (we would need the IP address of the CUCM at this stage).  Using
//     * getifaddr2() ATM.
//     */
//
//    /*
//     * XXX This will return an IPv4 address.  getifaddr() and getifaddr2() can
//     * handle IPv6 addresses properly though.
//     */
//
//    if (getifaddr2(AF_INET, (struct sockaddr *) &inaddr, sizeof(inaddr),
//            &lladdr) != 0)
//        return NULL;
//
//    inet_ntop(AF_INET, &inaddr.sin_addr, netif_ip_addr, IN_ADDR_PLEN);
//    return netif_ip_addr;
//}

//void
//platGetMacAddr(char *maddr)
//{
//    struct sockaddr_in inaddr; /* Unused */
//    struct sockaddr_dl lladdr;
//
    /*
     * XXX Same comment applies (see platGetIPAddr).  Additionally, it is just
     * not possible to properly implement platGetIpAddr() and platGetMacAddr()
     * so that the caller has a guarantee that both address come from the same
     * network address.
     */
    
//    if (getifaddr2(AF_INET, (struct sockaddr *) &inaddr, sizeof(inaddr),
//            &lladdr) != 0)
//        /* XXX */
//        bzero(maddr, MAC_ADDR_PLEN);
//
//    eth_macaddr_ntop(&lladdr, maddr, MAC_ADDR_PLEN);
//}

