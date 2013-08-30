/*
Copyright (c) 2007, Adobe Systems, Incorporated
Copyright (c) 2013, Mozilla

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

* Neither the name of Adobe Systems, Network Resonance, Mozilla nor
  the names of its contributors may be used to endorse or promote
  products derived from this software without specific prior written
  permission.

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

#include <nr_api.h>
#include <string.h>
#include "local_addr.h"

int nr_local_addr_copy(nr_local_addr *to, nr_local_addr *from)
  {
    nr_transport_addr_copy(&(to->addr), &(from->addr));
    to->interface = from->interface;
    return(0);
  }

int nr_local_addr_fmt_info_string(nr_local_addr *addr, char *buf, int len)
  {
    int addr_type = addr->interface.type;
    const char *vpn = (addr_type & NR_INTERFACE_TYPE_VPN) ? "VPN on " : "";

    const char *type = (addr_type & NR_INTERFACE_TYPE_WIRED) ? "wired" :
                       (addr_type & NR_INTERFACE_TYPE_WIFI) ? "wifi" :
                       (addr_type & NR_INTERFACE_TYPE_MOBILE) ? "mobile" :
                       "unknown";

    snprintf(buf, len, "%s%s, estimated speed: %d kbps",
             vpn, type, addr->interface.estimated_speed);
    return (0);
  }
