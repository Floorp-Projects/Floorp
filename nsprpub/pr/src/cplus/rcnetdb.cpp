/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
** Base class implementation for network access functions (ref: prnetdb.h)
*/

#include "rclock.h"
#include "rcnetdb.h"

#include <prmem.h>
#include <prlog.h>
#include <string.h>

RCNetAddr::RCNetAddr(const RCNetAddr& his): RCBase()
{
    address = his.address;
}

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
        default: PR_NOT_REACHED("This can't happen -- and did!");
    }
    (void)PR_InitializeNetAddr(how, port, &address);
}  /* RCNetAddr::RCNetAddr */

RCNetAddr::~RCNetAddr() { }

void RCNetAddr::operator=(const RCNetAddr& his) {
    address = his.address;
}

PRStatus RCNetAddr::FromString(const char* string)
{
    return PR_StringToNetAddr(string, &address);
}

void RCNetAddr::operator=(const PRNetAddr* addr) {
    address = *addr;
}

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
{
    return PR_NetAddrToString(&address, string, size);
}

/*
** RCHostLookup
*/

RCHostLookup::~RCHostLookup()
{
    if (NULL != address) {
        delete [] address;
    }
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
    if (NULL == buffer) {
        return PR_FAILURE;
    }
    rv = PR_GetHostByName(name, (char*)buffer, PR_NETDB_BUF_SIZE, &hostentry);
    if (PR_SUCCESS == rv)
    {
        for (max = 0, index = 0;; ++max)
        {
            index = PR_EnumerateHostEnt(index, &hostentry, 0, &addr);
            if (0 == index) {
                break;
            }
        }
        if (max > 0)
        {
            vector = new RCNetAddr[max];
            while (--max > 0)
            {
                index = PR_EnumerateHostEnt(index, &hostentry, 0, &addr);
                if (0 == index) {
                    break;
                }
                vector[index] = &addr;
            }
            {
                RCEnter entry(&ml);
                old_vector = address;
                address = vector;
                max_index = max;
            }
            if (NULL != old_vector) {
                delete [] old_vector;
            }
        }
    }
    if (NULL != buffer) {
        PR_DELETE(buffer);
    }
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
    if (NULL == buffer) {
        return PR_FAILURE;
    }
    rv = PR_GetHostByAddr(host_addr, buffer, PR_NETDB_BUF_SIZE, &hostentry);
    if (PR_SUCCESS == rv)
    {
        for (max = 0, index = 0;; ++max)
        {
            index = PR_EnumerateHostEnt(index, &hostentry, 0, &addr);
            if (0 == index) {
                break;
            }
        }
        if (max > 0)
        {
            vector = new RCNetAddr[max];
            while (--max > 0)
            {
                index = PR_EnumerateHostEnt(index, &hostentry, 0, &addr);
                if (0 == index) {
                    break;
                }
                vector[index] = &addr;
            }
            {
                RCEnter entry(&ml);
                old_vector = address;
                address = vector;
                max_index = max;
            }
            if (NULL != old_vector) {
                delete [] old_vector;
            }
        }
    }
    if (NULL != buffer) {
        PR_DELETE(buffer);
    }
    return PR_SUCCESS;
}  /* RCHostLookup::ByAddress */

const RCNetAddr* RCHostLookup::operator[](PRUintn which)
{
    RCNetAddr* addr = NULL;
    if (which < max_index) {
        addr = &address[which];
    }
    return addr;
}  /* RCHostLookup::operator[] */

/* RCNetdb.cpp */
