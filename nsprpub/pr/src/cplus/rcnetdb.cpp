/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
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

/*
** Base class implementation for network access functions (ref: prnetdb.h)
*/

#include "rclock.h"
#include "rcnetdb.h"

#include <prmem.h>
#include <prlog.h>
#include <string.h>

RCNetAddr::RCNetAddr(const RCNetAddr& his): RCBase()
    { address = his.address; }

RCNetAddr::RCNetAddr(const RCNetAddr& his, PRUint16 port): RCBase()
{
    address = his.address;
    switch (address.raw.family)
    {
        case PR_AF_INET: address.inet.port = port; break;
        case PR_AF_INET6: address.ipv6.port = port; break;
        default: break;
    }
}  /* RCNetAddr::RCNetAddr */

RCNetAddr::RCNetAddr(RCNetAddr::HostValue host, PRUint16 port): RCBase()
{
    PRNetAddrValue how;
    switch (host)
    {
        case RCNetAddr::any: how = PR_IpAddrAny; break;
        case RCNetAddr::loopback: how = PR_IpAddrLoopback; break;
        default: PR_ASSERT(!"This can't happen -- and did!");
    }
    (void)PR_InitializeNetAddr(how, port, &address);
}  /* RCNetAddr::RCNetAddr */

RCNetAddr::~RCNetAddr() { }

void RCNetAddr::operator=(const RCNetAddr& his) { address = his.address; }

PRStatus RCNetAddr::FromString(const char* string)
    { return PR_StringToNetAddr(string, &address); }

void RCNetAddr::operator=(const PRNetAddr* addr) { address = *addr; }

PRBool RCNetAddr::operator==(const RCNetAddr& his) const
{
    PRBool rv = EqualHost(his);
    if (rv)
    {
        switch (address.raw.family)
        {
            case PR_AF_INET:
                rv = (address.inet.port == his.address.inet.port); break;
            case PR_AF_INET6:
                rv = (address.ipv6.port == his.address.ipv6.port); break;
            case PR_AF_LOCAL:
            default: break;
        }
    }
    return rv;
}  /* RCNetAddr::operator== */

PRBool RCNetAddr::EqualHost(const RCNetAddr& his) const
{
    PRBool rv;
    switch (address.raw.family)
    {
        case PR_AF_INET:
            rv = (address.inet.ip == his.address.inet.ip); break;
        case PR_AF_INET6:
            rv = (0 == memcmp(
                &address.ipv6.ip, &his.address.ipv6.ip,
                sizeof(address.ipv6.ip)));
            break;
#if defined(XP_UNIX)
        case PR_AF_LOCAL:
            rv = (0 == strncmp(
                address.local.path, his.address.local.path,
                sizeof(address.local.path)));
            break;
#endif
        default: break;
    }
    return rv;
}  /* RCNetAddr::operator== */

PRStatus RCNetAddr::ToString(char *string, PRSize size) const
    { return PR_NetAddrToString(&address, string, size); }

/*
** RCHostLookup
*/

RCHostLookup::~RCHostLookup()
{
    if (NULL != address) delete [] address;
}  /* RCHostLookup::~RCHostLookup */

RCHostLookup::RCHostLookup(): RCBase()
{
    address = NULL;
    max_index = 0;
}  /* RCHostLookup::RCHostLookup */

PRStatus RCHostLookup::ByName(const char* name)
{
    PRStatus rv;
    PRNetAddr addr;
    PRHostEnt hostentry;
    PRIntn index = 0, max;
    RCNetAddr* vector = NULL;
    RCNetAddr* old_vector = NULL;
    void* buffer = PR_Malloc(PR_NETDB_BUF_SIZE);
    if (NULL == buffer) return PR_FAILURE;
    rv = PR_GetHostByName(name, (char*)buffer, PR_NETDB_BUF_SIZE, &hostentry);
    if (PR_SUCCESS == rv)
    {
        for (max = 0, index = 0;; ++max)
        {
            index = PR_EnumerateHostEnt(index, &hostentry, 0, &addr);
            if (0 == index) break;
        }
        if (max > 0)
        {
            vector = new RCNetAddr[max];
            while (--max > 0)
            {
                index = PR_EnumerateHostEnt(index, &hostentry, 0, &addr);
                if (0 == index) break;
                vector[index] = &addr;
            }
            {
                RCEnter entry(&ml);
                old_vector = address;
                address = vector;
                max_index = max;
            }
            if (NULL != old_vector) delete [] old_vector;
        }
    }
    if (NULL != buffer) PR_DELETE(buffer);
    return PR_SUCCESS;
}  /* RCHostLookup::ByName */

PRStatus RCHostLookup::ByAddress(const RCNetAddr& host_addr)
{
    PRStatus rv;
    PRNetAddr addr;
    PRHostEnt hostentry;
    PRIntn index = 0, max;
    RCNetAddr* vector = NULL;
    RCNetAddr* old_vector = NULL;
    char *buffer = (char*)PR_Malloc(PR_NETDB_BUF_SIZE);
    if (NULL == buffer) return PR_FAILURE;
    rv = PR_GetHostByAddr(host_addr, buffer, PR_NETDB_BUF_SIZE, &hostentry);
    if (PR_SUCCESS == rv)
    {
        for (max = 0, index = 0;; ++max)
        {
            index = PR_EnumerateHostEnt(index, &hostentry, 0, &addr);
            if (0 == index) break;
        }
        if (max > 0)
        {
            vector = new RCNetAddr[max];
            while (--max > 0)
            {
                index = PR_EnumerateHostEnt(index, &hostentry, 0, &addr);
                if (0 == index) break;
                vector[index] = &addr;
            }
            {
                RCEnter entry(&ml);
                old_vector = address;
                address = vector;
                max_index = max;
            }
            if (NULL != old_vector) delete [] old_vector;
        }
    }
    if (NULL != buffer) PR_DELETE(buffer);
    return PR_SUCCESS;
}  /* RCHostLookup::ByAddress */

const RCNetAddr* RCHostLookup::operator[](PRUintn which)
{
    RCNetAddr* addr = NULL;
    if (which < max_index)
        addr = &address[which];
    return addr;
}  /* RCHostLookup::operator[] */

/* RCNetdb.cpp */
