/*
Copyright (c) 2007, Adobe Systems, Incorporated
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

* Neither the name of Adobe Systems, Network Resonance nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


static char *RCSSTRING __UNUSED__="$Id: addrs.c,v 1.2 2008/04/28 18:21:30 ekr Exp $";


#include <csi_platform.h>
#include <assert.h>
#include <string.h>

#ifdef WIN32
#include <winsock2.h>
#include <iphlpapi.h>
#include <tchar.h>
#else   /* UNIX */
#include <sys/sysctl.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <net/if.h>
#ifndef LINUX
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <sys/sockio.h>
#else
#include <linux/if.h>
#endif
#include <net/route.h>

/* IP */
#include <netinet/in.h>
#ifdef LINUX
#include "sys/ioctl.h"
#else
#include <netinet/in_var.h>
#endif
#include <arpa/inet.h>
#include <netdb.h>
#endif  /* UNIX */

#include "stun.h"
#include "addrs.h"



#ifdef DARWIN
/*
 * Copyright (c) 1983, 1993
 *    The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by the University of
 *    California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <err.h>

static void stun_rt_xaddrs(caddr_t, caddr_t, struct rt_addrinfo *);
static int stun_grab_addrs(char *name, int addrcount,
               struct ifa_msghdr *ifam,
               nr_transport_addr addrs[], int maxaddrs, int *count);
static int
nr_stun_is_duplicate_addr(nr_transport_addr addrs[], int count, nr_transport_addr *addr);


/*
 * Expand the compacted form of addresses as returned via the
 * configuration read via sysctl().
 */
#define ROUNDUP(a) \
    ((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
#define ADVANCE(x, n) (x += ROUNDUP((n)->sa_len))

static void
stun_rt_xaddrs(cp, cplim, rtinfo)
    caddr_t cp, cplim;
    struct rt_addrinfo *rtinfo;
{
    struct sockaddr *sa;
    int i;

    memset(rtinfo->rti_info, 0, sizeof(rtinfo->rti_info));
    for (i = 0; (i < RTAX_MAX) && (cp < cplim); i++) {
        if ((rtinfo->rti_addrs & (1 << i)) == 0)
            continue;
        rtinfo->rti_info[i] = sa = (struct sockaddr *)cp;
        ADVANCE(cp, sa);
    }
}

static int
stun_grab_addrs(char *name, int addrcount, struct ifa_msghdr *ifam, nr_transport_addr addrs[], int maxaddrs, int *count)
{
    int r,_status;
    int s = -1;
    struct ifreq ifr;
    struct rt_addrinfo info;
    struct sockaddr_in *sin;

    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name, name, sizeof ifr.ifr_name);

    if ((s = socket(ifr.ifr_addr.sa_family, SOCK_DGRAM, 0)) < 0) {
      r_log(NR_LOG_STUN, LOG_WARNING, "unable to obtain addresses from socket");
      ABORT(R_FAILED);
    }

    while (addrcount > 0) {
        info.rti_addrs = ifam->ifam_addrs;

        /* Expand the compacted addresses */
        stun_rt_xaddrs((char *)(ifam + 1), ifam->ifam_msglen + (char *)ifam, &info);

        switch (info.rti_info[RTAX_IFA]->sa_family) {
        case AF_INET:
            sin = (struct sockaddr_in *)info.rti_info[RTAX_IFA];

            if ((r=nr_sockaddr_to_transport_addr((struct sockaddr*)sin, sizeof(*sin), IPPROTO_UDP, 0, &(addrs[*count]))))
                ABORT(r);

            strlcpy(addrs[*count].ifname, name, sizeof(addrs[*count].ifname));

            ++*count;
            break;
        case AF_INET6:
            UNIMPLEMENTED;
            break;
        }

        addrcount--;

        if (*count >= maxaddrs) {
            r_log(NR_LOG_STUN, LOG_WARNING, "Address list truncated at %d out of entries", maxaddrs, maxaddrs+addrcount);
            break;
        }

        ifam = (struct ifa_msghdr *)((char *)ifam + ifam->ifam_msglen);
    }

    _status = 0;
  abort:
    if (s != -1) close(s);
    return _status;
}

static int
stun_get_mib_addrs(nr_transport_addr addrs[], int maxaddrs, int *count)
{
    int _status;
    char name[32];
    int flags;
    int addrcount;
    struct if_msghdr *ifm, *nextifm;
    struct ifa_msghdr *ifam;
    struct sockaddr_dl *sdl;
    char *buf = 0;
    char *lim;
    char *next;
    size_t needed;
    int mib[6];

    *count = 0;

    mib[0] = CTL_NET;
    mib[1] = PF_ROUTE;
    mib[2] = 0;
    mib[3] = AF_INET;
    mib[4] = NET_RT_IFLIST;
    mib[5] = 0;

    if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0)
        errx(1, "iflist-sysctl-estimate");
    if ((buf = malloc(needed)) == NULL)
        errx(1, "malloc");
    if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0)
        errx(1, "actual retrieval of interface table");
    lim = buf + needed;

    next = buf;
    while (next < lim) {
        ifm = (struct if_msghdr *)next;

        if (ifm->ifm_type == RTM_IFINFO) {
            sdl = (struct sockaddr_dl *)(ifm + 1);
            flags = ifm->ifm_flags;
        } else {
            r_log(NR_LOG_STUN, LOG_WARNING, "out of sync parsing NET_RT_IFLIST");
            r_log(NR_LOG_STUN, LOG_DEBUG, "expected %d, got %d, msglen = %d, buf:%p, next:%p, lim:%p", RTM_IFINFO, ifm->ifm_type, ifm->ifm_msglen, buf, next, lim);
            ABORT(R_FAILED);
        }

        next += ifm->ifm_msglen;
        ifam = NULL;
        addrcount = 0;
        while (next < lim) {

            nextifm = (struct if_msghdr *)next;

            if (nextifm->ifm_type != RTM_NEWADDR)
                break;

            if (ifam == NULL)
                ifam = (struct ifa_msghdr *)nextifm;

            addrcount++;
            next += nextifm->ifm_msglen;
        }

        strncpy(name, sdl->sdl_data, sdl->sdl_nlen);
        name[sdl->sdl_nlen] = '\0';

        stun_grab_addrs(name, addrcount, ifam, addrs, maxaddrs, count);
    }

    _status = 0;
abort:
    if (buf) free(buf);
    return _status;
}

#elif defined(WIN32)

#define WIN32_MAX_NUM_INTERFACES  20


#define _NR_MAX_KEY_LENGTH 256
#define _NR_MAX_NAME_LENGTH 512

#define _ADAPTERS_BASE_REG "SYSTEM\\CurrentControlSet\\Control\\Network\\{4D36E972-E325-11CE-BFC1-08002BE10318}"

static int nr_win32_get_adapter_friendly_name(char *adapter_GUID, char **friendly_name)
{
    int r,_status;
    HKEY adapter_reg;
    TCHAR adapter_key[_NR_MAX_KEY_LENGTH];
	TCHAR keyval_buf[_NR_MAX_KEY_LENGTH];
    TCHAR adapter_GUID_tchar[_NR_MAX_NAME_LENGTH];
    DWORD keyval_len, key_type;
    size_t converted_chars, newlen;
    char *my_fn = 0;

#ifdef _UNICODE
    mbstowcs_s(&converted_chars, adapter_GUID_tchar, strlen(adapter_GUID)+1,
               adapter_GUID, _TRUNCATE);
#else
    strncpy(adapter_GUID_tchar, _NR_MAX_NAME_LENGTH, adapter_GUID);
#endif

    _tcscpy_s(adapter_key, _NR_MAX_KEY_LENGTH, TEXT(_ADAPTERS_BASE_REG));
    _tcscat_s(adapter_key, _NR_MAX_KEY_LENGTH, TEXT("\\"));
    _tcscat_s(adapter_key, _NR_MAX_KEY_LENGTH, adapter_GUID_tchar);
    _tcscat_s(adapter_key, _NR_MAX_KEY_LENGTH, TEXT("\\Connection"));

	r = RegOpenKeyEx(HKEY_LOCAL_MACHINE, adapter_key, 0, KEY_READ, &adapter_reg);

    if (r != ERROR_SUCCESS) {
      r_log(NR_LOG_STUN, LOG_ERR, "Got error %d opening adapter reg key\n", r);
      ABORT(R_INTERNAL);
    }

    keyval_len = sizeof(keyval_buf);
    r = RegQueryValueEx(adapter_reg, TEXT("Name"), NULL, &key_type,
                        (BYTE *)keyval_buf, &keyval_len);

    RegCloseKey(adapter_reg);

#ifdef UNICODE
    newlen = wcslen(keyval_buf)+1;
    my_fn = (char *) RCALLOC(newlen);
    if (!my_fn) {
      ABORT(R_NO_MEMORY); 
    }
    wcstombs_s(&converted_chars, my_fn, newlen, keyval_buf, _TRUNCATE);
#else
    my_fn = r_strdup(keyval_buf);
#endif

    *friendly_name = my_fn;
    _status=0;

abort:
    if (_status) {
      if (my_fn) free(my_fn);
    }
    return(_status);
}


static int
stun_get_win32_addrs(nr_transport_addr addrs[], int maxaddrs, int *count)
{
    int r,_status;
    PIP_ADAPTER_INFO pAdapterInfo;
    PIP_ADAPTER_INFO pAdapter = NULL;
    PIP_ADDR_STRING pAddrString;
    ULONG out_buf_len;
    char *friendly_name=0;
    char munged_ifname[IFNAMSIZ];
    int n = 0;

    *count = 0;

    pAdapterInfo = (IP_ADAPTER_INFO *) RMALLOC(sizeof(IP_ADAPTER_INFO));
    out_buf_len = sizeof(IP_ADAPTER_INFO);

    /* First call to GetAdaptersInfo is mainly to get length */

    if (GetAdaptersInfo(pAdapterInfo, &out_buf_len) == ERROR_BUFFER_OVERFLOW) {
      RFREE(pAdapterInfo);
      pAdapterInfo = (IP_ADAPTER_INFO *) RMALLOC(out_buf_len);
      if (pAdapterInfo == NULL) {
        r_log(NR_LOG_STUN, LOG_ERR, "Error allocating memory for GetAdaptersInfo output");
        ABORT(R_NO_MEMORY);
      }
    }
    if ((r = GetAdaptersInfo(pAdapterInfo, &out_buf_len)) != NO_ERROR) {
      r_log(NR_LOG_STUN, LOG_ERR, "Got error from GetAdaptersInfo");
      ABORT(R_INTERNAL);
    }
    r_log(NR_LOG_STUN, LOG_DEBUG, "Got AdaptersInfo");

    pAdapter = pAdapterInfo;

    while (pAdapter) {
      char *c;

      r_log(NR_LOG_STUN, LOG_DEBUG, "Adapter Name (GUID) = %s", pAdapter->AdapterName);
      r_log(NR_LOG_STUN, LOG_DEBUG, "Adapter Description = %s", pAdapter->Description);

      if ((r = nr_win32_get_adapter_friendly_name(pAdapter->AdapterName, &friendly_name))) {
        r_log(NR_LOG_STUN, LOG_ERR, "Error %d getting friendly name for adapter with GUID = %s", r, 
              pAdapter->AdapterName);
        ABORT(r);
      }

      r_log(NR_LOG_STUN, LOG_INFO, "Found adapter with friendly name: %s", friendly_name);

      snprintf(munged_ifname, IFNAMSIZ, "%s%c", friendly_name, 0);
      RFREE(friendly_name);
      friendly_name = 0;

      /* replace spaces with underscores */
      c = strchr(munged_ifname, ' ');
      while (c != NULL) {
        *c = '_';
         c = strchr(munged_ifname, ' ');
      }
      c = strchr(munged_ifname, '.'); 
      while (c != NULL) {
        *c = '+';
         c = strchr(munged_ifname, '+'); 
      }

      r_log(NR_LOG_STUN, LOG_INFO, "Converted ifname: %s", munged_ifname);

      for (pAddrString = &(pAdapter->IpAddressList); pAddrString != NULL; pAddrString = pAddrString->Next) {
        unsigned long this_addr = inet_addr(pAddrString->IpAddress.String);

        if (this_addr == 0)
          continue;

        r_log(NR_LOG_STUN, LOG_INFO, "Adapter %s address: %s", munged_ifname, pAddrString->IpAddress.String);

        addrs[n].ip_version=NR_IPV4;
        addrs[n].protocol = IPPROTO_UDP; 

        addrs[n].u.addr4.sin_family=PF_INET;
        addrs[n].u.addr4.sin_port=0;
        addrs[n].u.addr4.sin_addr.s_addr=this_addr;
        addrs[n].addr=(struct sockaddr *)&(addrs[n].u.addr4);
        addrs[n].addr_len=sizeof(struct sockaddr_in);

        strncpy(addrs[n].ifname, munged_ifname, sizeof(addrs[n].ifname));
        snprintf(addrs[n].as_string,40,"IP4:%s:%d",inet_ntoa(addrs[n].u.addr4.sin_addr),
                 ntohs(addrs[n].u.addr4.sin_port));

        if (++n >= maxaddrs)
          goto done;
      }

      pAdapter = pAdapter->Next;
    }

  done:
    *count = n;
    _status = 0;

  abort:
    RFREE(pAdapterInfo);
    RFREE(friendly_name);
    return _status;
}

#ifdef GET_WIN32_ADDRS_NO_WIN2K
   /* Here's a nice way to get adapter addresses and names, but it
    * isn't supported on Win2000.
    */
static int
stun_get_win32_addrs(nr_transport_addr addrs[], int maxaddrs, int *count)
{
    int r,_status;
    PIP_ADAPTER_ADDRESSES AdapterAddresses = NULL, tmpAddress = NULL;
    ULONG buflen;
    char munged_ifname[IFNAMSIZ];
    int n = 0;

    *count = 0;

    if (maxaddrs <= 0)
      ABORT(R_INTERNAL);

    /* Call GetAdaptersAddresses() twice.  First, just to get the buf length */

    buflen = 0;

    r = GetAdaptersAddresses(AF_INET, 0, NULL, AdapterAddresses, &buflen);
    if (r != ERROR_BUFFER_OVERFLOW) {
      r_log(NR_LOG_STUN, LOG_ERR, "Error getting buf len from GetAdaptersAddresses()");
      ABORT(R_INTERNAL);
    }
        
    AdapterAddresses = (PIP_ADAPTER_ADDRESSES) RMALLOC(buflen);
    if (AdapterAddresses == NULL) {
      r_log(NR_LOG_STUN, LOG_ERR, "Error allocating buf for GetAdaptersAddresses()");
      ABORT(R_NO_MEMORY);
    }

    /* for real, this time */

    r = GetAdaptersAddresses(AF_INET, 0, NULL, AdapterAddresses, &buflen);
    if (r != NO_ERROR) {
      r_log(NR_LOG_STUN, LOG_ERR, "Error getting addresses from GetAdaptersAddresses()");
      ABORT(R_INTERNAL);
    }

    /* Loop through the adapters */

    for (tmpAddress = AdapterAddresses; tmpAddress != NULL; tmpAddress = tmpAddress->Next) {
      char *c;
        
      if (tmpAddress->OperStatus != IfOperStatusUp)
        continue;

      snprintf(munged_ifname, IFNAMSIZ, "%S%c", tmpAddress->FriendlyName, 0);
      /* replace spaces with underscores */
      c = strchr(munged_ifname, ' '); 
      while (c != NULL) {
        *c = '_';
         c = strchr(munged_ifname, ' '); 
      }
      c = strchr(munged_ifname, '.'); 
      while (c != NULL) {
        *c = '+';
         c = strchr(munged_ifname, '+'); 
      }

      if ((tmpAddress->IfIndex != 0) || (tmpAddress->Ipv6IfIndex != 0)) {
        IP_ADAPTER_UNICAST_ADDRESS *u = 0;
        
        for (u = tmpAddress->FirstUnicastAddress; u != 0; u = u->Next) {
          SOCKET_ADDRESS *sa_addr = &u->Address;

          if ((sa_addr->lpSockaddr->sa_family == AF_INET) ||
              (sa_addr->lpSockaddr->sa_family == AF_INET6)) {
            if ((r=nr_sockaddr_to_transport_addr((struct sockaddr*)sa_addr->lpSockaddr, sizeof(*sa_addr->lpSockaddr), IPPROTO_UDP, 0, &(addrs[n]))))
                ABORT(r);
          }
          else {
            r_log(NR_LOG_STUN, LOG_DEBUG, "Unrecognized sa_family for adapteraddress %s",munged_ifname);
            continue;
          }

          strncpy(addrs[n].ifname, munged_ifname, sizeof(addrs[n].ifname));
          if (++n >= maxaddrs)
            goto done;
        }
      }
    }

   done:
    *count = n;
    _status = 0;

  abort:
    RFREE(AdapterAddresses);
    return _status;
}
#endif  /* GET_WIN32_ADDRS_NO_WIN2K */

#elif defined(__sparc__)

static int
stun_get_sparc_addrs(nr_transport_addr addrs[], int maxaddrs, int *count)
{
    *count = 0;
    UNIMPLEMENTED; /*TODO !nn! - sparc */
    return 0;
}

#else

static int
stun_get_siocgifconf_addrs(nr_transport_addr addrs[], int maxaddrs, int *count)
{
   struct ifconf ifc;
   int _status;
   int s = socket( AF_INET, SOCK_DGRAM, 0 );
   int len = 100 * sizeof(struct ifreq);
   int r;
   int e;
   char *ptr;
   int tl;
   int n;
   struct ifreq ifr2;

   char buf[ len ];

   ifc.ifc_len = len;
   ifc.ifc_buf = buf;

   e = ioctl(s,SIOCGIFCONF,&ifc);
   ptr = buf;
   tl = ifc.ifc_len;
   n=0;

   while ( (tl > 0) && ( n < maxaddrs) )
   {
      struct ifreq* ifr = (struct ifreq *)ptr;

#ifdef LINUX
      int si = sizeof(struct ifreq);
#else
      int si = sizeof(ifr->ifr_name) + MAX(ifr->ifr_addr.sa_len, sizeof(ifr->ifr_addr));
#endif
      tl -= si;
      ptr += si;

      ifr2 = *ifr;

      e = ioctl(s,SIOCGIFADDR,&ifr2);
      if ( e == -1 )
      {
          continue;
      }

      //r_log(NR_LOG_STUN, LOG_ERR, "ioctl addr e = %d",e);

      if ((r=nr_sockaddr_to_transport_addr(&ifr2.ifr_addr, sizeof(ifr2.ifr_addr), IPPROTO_UDP, 0, &(addrs[n])))) {
          r_log(NR_LOG_STUN, LOG_WARNING, "Problem transforming address");
      }
      else {
          strlcpy(addrs[n].ifname, ifr->ifr_name, sizeof(addrs[n].ifname));
          ++n;
      }
   }

   close(s);

   *count = n;

    _status = 0;
    return _status;
}
#endif

static int
nr_stun_is_duplicate_addr(nr_transport_addr addrs[], int count, nr_transport_addr *addr)
{
    int i;
    int different;

    for (i = 0; i < count; ++i) {
        different = nr_transport_addr_cmp(&addrs[i], addr, NR_TRANSPORT_ADDR_CMP_MODE_ALL);
        if (!different) 
            return 1;  /* duplicate */
    }

    return 0;
}

int
nr_stun_remove_duplicate_addrs(nr_transport_addr addrs[], int remove_loopback, int *count)
{
    int r, _status;
    nr_transport_addr *tmp = 0;
    int i;
    int n;

    tmp = RMALLOC(*count * sizeof(*tmp));
    if (!tmp)
        ABORT(R_NO_MEMORY);

    n = 0;
    for (i = 0; i < *count; ++i) {
        if (nr_stun_is_duplicate_addr(tmp, n, &addrs[i])) {
            /* skip addrs[i], it's a duplicate */
        }
        else if (remove_loopback && nr_transport_addr_is_loopback(&addrs[i])) {
            /* skip addrs[i], it's a loopback */
        }
        else {
            /* otherwise, copy it to the temporary array */
            if ((r=nr_transport_addr_copy(&tmp[n], &addrs[i])))
                ABORT(r);
            ++n;
        }
    }

    *count = n;

    /* copy temporary array into passed in/out array */
    for (i = 0; i < *count; ++i) {
        if ((r=nr_transport_addr_copy(&addrs[i], &tmp[i])))
            ABORT(r);
    }

    _status = 0;
  abort:
    RFREE(tmp);
    return _status;
}

int
nr_stun_get_addrs(nr_transport_addr addrs[], int maxaddrs, int drop_loopback, int *count)
{
    int _status=0;
    int i;

#ifdef DARWIN
    _status = stun_get_mib_addrs(addrs, maxaddrs, count);
#elif defined(WIN32)
    _status = stun_get_win32_addrs(addrs, maxaddrs, count);
#elif defined(__sparc__)
    _status = stun_get_sparc_addrs(addrs, maxaddrs, count);
#else
    _status = stun_get_siocgifconf_addrs(addrs, maxaddrs, count);
#endif

    nr_stun_remove_duplicate_addrs(addrs, drop_loopback, count);

    for (i = 0; i < *count; ++i) {
        r_log(NR_LOG_STUN, LOG_DEBUG, "Address %d: %s on %s", i, addrs[i].as_string, addrs[i].ifname);
    }

    return _status;
}

