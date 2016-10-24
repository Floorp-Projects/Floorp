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
#else   /* !WIN32 */

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <errno.h>

#ifndef ANDROID
/* This works on linux and BSD, but not android */
#include <sys/types.h> /* getifaddrs */
#include <ifaddrs.h> /* getifaddrs */
#else
#include "ifaddrs-android.h"
#define getifaddrs android_getifaddrs
#define freeifaddrs android_freeifaddrs
#endif

#ifdef LINUX

#ifdef ANDROID
/* Work around an Android NDK < r8c bug */
#undef __unused
#else
#include <linux/if.h> /* struct ifreq, IFF_POINTTOPOINT */
#include <linux/wireless.h> /* struct iwreq */
#include <linux/ethtool.h> /* struct ethtool_cmd */
#include <linux/sockios.h> /* SIOCETHTOOL */
#endif /* ANDROID */

#endif /* LINUX */

#endif  /* !WIN32 */

#include "stun.h"
#include "addrs.h"
#include "nr_crypto.h"
#include "util.h"

#if defined(WIN32)

#define WIN32_MAX_NUM_INTERFACES  20

#define NR_MD5_HASH_LENGTH 16

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
    strlcpy(adapter_GUID_tchar, adapter_GUID, _NR_MAX_NAME_LENGTH);
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
stun_get_win32_addrs(nr_local_addr addrs[], int maxaddrs, int *count)
{
    int r, _status;
    PIP_ADAPTER_ADDRESSES AdapterAddresses = NULL, tmpAddress = NULL;
    // recomended per https://msdn.microsoft.com/en-us/library/windows/desktop/aa365915(v=vs.85).aspx
    static const ULONG initialBufLen = 15000;
    ULONG buflen = initialBufLen;
    char bin_hashed_ifname[NR_MD5_HASH_LENGTH];
    char hex_hashed_ifname[MAXIFNAME];
    int n = 0;

    *count = 0;

    if (maxaddrs <= 0)
      ABORT(R_BAD_ARGS);

    /* According to MSDN (see above) we have try GetAdapterAddresses() multiple times */
    for (n = 0; n < 5; n++) {
      AdapterAddresses = (PIP_ADAPTER_ADDRESSES) RMALLOC(buflen);
      if (AdapterAddresses == NULL) {
        r_log(NR_LOG_STUN, LOG_ERR, "Error allocating buf for GetAdaptersAddresses()");
        ABORT(R_NO_MEMORY);
      }

      r = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, NULL, AdapterAddresses, &buflen);
      if (r == NO_ERROR) {
        break;
      }
      r_log(NR_LOG_STUN, LOG_ERR, "GetAdaptersAddresses() returned error (%d)", r);
      RFREE(AdapterAddresses);
    }

    if (n >= 5) {
      r_log(NR_LOG_STUN, LOG_ERR, "5 failures calling GetAdaptersAddresses()");
      ABORT(R_INTERNAL);
    }

    n = 0;

    /* Loop through the adapters */

    for (tmpAddress = AdapterAddresses; tmpAddress != NULL; tmpAddress = tmpAddress->Next) {

      if (tmpAddress->OperStatus != IfOperStatusUp)
        continue;

      if ((tmpAddress->IfIndex != 0) || (tmpAddress->Ipv6IfIndex != 0)) {
        IP_ADAPTER_UNICAST_ADDRESS *u = 0;

        if(r=nr_crypto_md5((UCHAR *)tmpAddress->FriendlyName,
                           wcslen(tmpAddress->FriendlyName) * sizeof(wchar_t),
                           bin_hashed_ifname))
          ABORT(r);
        if(r=nr_bin2hex(bin_hashed_ifname, sizeof(bin_hashed_ifname),
          hex_hashed_ifname))
          ABORT(r);

        for (u = tmpAddress->FirstUnicastAddress; u != 0; u = u->Next) {
          SOCKET_ADDRESS *sa_addr = &u->Address;

          if ((sa_addr->lpSockaddr->sa_family == AF_INET) ||
              (sa_addr->lpSockaddr->sa_family == AF_INET6)) {
            if ((r=nr_sockaddr_to_transport_addr((struct sockaddr*)sa_addr->lpSockaddr, IPPROTO_UDP, 0, &(addrs[n].addr))))
                ABORT(r);
          }
          else {
            r_log(NR_LOG_STUN, LOG_DEBUG, "Unrecognized sa_family for address on adapter %lu", tmpAddress->IfIndex);
            continue;
          }

          strlcpy(addrs[n].addr.ifname, hex_hashed_ifname, sizeof(addrs[n].addr.ifname));
          if (tmpAddress->IfType == IF_TYPE_ETHERNET_CSMACD) {
            addrs[n].interface.type = NR_INTERFACE_TYPE_WIRED;
          } else if (tmpAddress->IfType == IF_TYPE_IEEE80211) {
            /* Note: this only works for >= Win Vista */
            addrs[n].interface.type = NR_INTERFACE_TYPE_WIFI;
          } else {
            addrs[n].interface.type = NR_INTERFACE_TYPE_UNKNOWN;
          }
#if (_WIN32_WINNT >= 0x0600)
          /* Note: only >= Vista provide link speed information */
          addrs[n].interface.estimated_speed = tmpAddress->TransmitLinkSpeed / 1000;
#else
          addrs[n].interface.estimated_speed = 0;
#endif
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

#else /* WIN32 */

static int
nr_stun_is_duplicate_addr(nr_local_addr addrs[], int count, nr_local_addr *addr);

static int
stun_getifaddrs(nr_local_addr addrs[], int maxaddrs, int *count)
{
  int r,_status;
  struct ifaddrs* if_addrs_head=NULL;
  struct ifaddrs* if_addr;

  *count = 0;

  if (maxaddrs <= 0)
    ABORT(R_BAD_ARGS);

  if (getifaddrs(&if_addrs_head) == -1) {
    r_log(NR_LOG_STUN, LOG_ERR, "getifaddrs error e = %d", errno);
    ABORT(R_INTERNAL);
  }

  if_addr = if_addrs_head;

  while (if_addr && *count < maxaddrs) {
    /* This can be null */
    if (if_addr->ifa_addr) {
      switch (if_addr->ifa_addr->sa_family) {
        case AF_INET:
        case AF_INET6:
          if (r=nr_sockaddr_to_transport_addr(if_addr->ifa_addr, IPPROTO_UDP, 0, &(addrs[*count].addr))) {
            r_log(NR_LOG_STUN, LOG_ERR, "nr_sockaddr_to_transport_addr error r = %d", r);
          } else {
#if defined(LINUX) && !defined(ANDROID)
            struct ethtool_cmd ecmd;
            struct ifreq ifr;
            struct iwreq wrq;
            int e;
            int s = socket(AF_INET, SOCK_DGRAM, 0);

            strncpy(ifr.ifr_name, if_addr->ifa_name, sizeof(ifr.ifr_name));
            /* TODO (Bug 896851): interface property for Android */
            /* Getting ethtool for ethernet information. */
            ecmd.cmd = ETHTOOL_GSET;
            /* In/out param */
            ifr.ifr_data = (void*)&ecmd;

            e = ioctl(s, SIOCETHTOOL, &ifr);
            if (e == 0)
            {
               /* For wireless network, we won't get ethtool, it's a wired
                * connection */
               addrs[*count].interface.type = NR_INTERFACE_TYPE_WIRED;
#ifdef DONT_HAVE_ETHTOOL_SPEED_HI
               addrs[*count].interface.estimated_speed = ecmd.speed;
#else
               addrs[*count].interface.estimated_speed = ((ecmd.speed_hi << 16) | ecmd.speed) * 1000;
#endif
            }

            strncpy(wrq.ifr_name, if_addr->ifa_name, sizeof(wrq.ifr_name));
            e = ioctl(s, SIOCGIWRATE, &wrq);
            if (e == 0)
            {
               addrs[*count].interface.type = NR_INTERFACE_TYPE_WIFI;
               addrs[*count].interface.estimated_speed = wrq.u.bitrate.value / 1000;
            }

            close(s);

            if (if_addr->ifa_flags & IFF_POINTOPOINT)
            {
               addrs[*count].interface.type = NR_INTERFACE_TYPE_UNKNOWN | NR_INTERFACE_TYPE_VPN;
               /* TODO (Bug 896913): find backend network type of this VPN */
            }
#else
            addrs[*count].interface.type = NR_INTERFACE_TYPE_UNKNOWN;
            addrs[*count].interface.estimated_speed = 0;
#endif
            strlcpy(addrs[*count].addr.ifname, if_addr->ifa_name, sizeof(addrs[*count].addr.ifname));
            ++(*count);
          }
          break;
        default:
          ;
      }
    }

    if_addr = if_addr->ifa_next;
  }

  _status=0;
abort:
  if (if_addrs_head) {
    freeifaddrs(if_addrs_head);
  }
  return(_status);
}

#endif

static int
nr_stun_is_duplicate_addr(nr_local_addr addrs[], int count, nr_local_addr *addr)
{
    int i;
    int different;

    for (i = 0; i < count; ++i) {
        different = nr_transport_addr_cmp(&addrs[i].addr, &(addr->addr),
          NR_TRANSPORT_ADDR_CMP_MODE_ALL);
        if (!different)
            return 1;  /* duplicate */
    }

    return 0;
}

int
nr_stun_remove_duplicate_addrs(nr_local_addr addrs[], int remove_loopback, int remove_link_local, int *count)
{
    int r, _status;
    nr_local_addr *tmp = 0;
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
        else if (remove_loopback && nr_transport_addr_is_loopback(&addrs[i].addr)) {
            /* skip addrs[i], it's a loopback */
        }
        else if (remove_link_local &&
                 addrs[i].addr.ip_version == NR_IPV6 &&
                 nr_transport_addr_is_link_local(&addrs[i].addr)) {
            /* skip addrs[i], it's a link-local address */
        }
        else {
            /* otherwise, copy it to the temporary array */
            if ((r=nr_local_addr_copy(&tmp[n], &addrs[i])))
                ABORT(r);
            ++n;
        }
    }

    *count = n;

    memset(addrs, 0, *count * sizeof(*addrs));
    /* copy temporary array into passed in/out array */
    for (i = 0; i < *count; ++i) {
        if ((r=nr_local_addr_copy(&addrs[i], &tmp[i])))
            ABORT(r);
    }

    _status = 0;
  abort:
    RFREE(tmp);
    return _status;
}

#ifndef USE_PLATFORM_NR_STUN_GET_ADDRS

int
nr_stun_get_addrs(nr_local_addr addrs[], int maxaddrs, int drop_loopback, int drop_link_local, int *count)
{
    int r,_status=0;
    int i;
    char typestr[100];

#ifdef WIN32
    _status = stun_get_win32_addrs(addrs, maxaddrs, count);
#else
    _status = stun_getifaddrs(addrs, maxaddrs, count);
#endif

    if ((r=nr_stun_remove_duplicate_addrs(addrs, drop_loopback, drop_link_local, count)))
      ABORT(r);

    for (i = 0; i < *count; ++i) {
      nr_local_addr_fmt_info_string(addrs+i,typestr,sizeof(typestr));
      r_log(NR_LOG_STUN, LOG_DEBUG, "Address %d: %s on %s, type: %s\n",
            i,addrs[i].addr.as_string,addrs[i].addr.ifname,typestr);
    }

abort:
    return _status;
}

#endif
