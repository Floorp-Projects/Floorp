/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
** Base class definitions for network access functions (ref: prnetdb.h)
*/

#if defined(_RCNETDB_H)
#else
#define _RCNETDB_H

#include "rclock.h"
#include "rcbase.h"

#include <prnetdb.h>

class PR_IMPLEMENT(RCNetAddr): public RCBase
{
public:
    typedef enum {
        any = PR_IpAddrAny,             /* assign logical INADDR_ANY */
        loopback = PR_IpAddrLoopback    /* assign logical INADDR_LOOPBACK */
    } HostValue;

    RCNetAddr();                        /* default constructor is unit'd object */
    RCNetAddr(const RCNetAddr&);        /* copy constructor */
    RCNetAddr(HostValue, PRUint16 port);/* init'd w/ 'special' assignments */
    RCNetAddr(const RCNetAddr&, PRUint16 port);
    /* copy w/ port reassigment */

    virtual ~RCNetAddr();

    void operator=(const RCNetAddr&);

    virtual PRBool operator==(const RCNetAddr&) const;
    /* compare of all relavent fields */
    virtual PRBool EqualHost(const RCNetAddr&) const;
    /* compare of just host field */


public:

    void operator=(const PRNetAddr*);   /* construction from more primitive data */
    operator const PRNetAddr*() const;  /* extraction of underlying representation */
    virtual PRStatus FromString(const char* string);
    /* initialization from an ASCII string */
    virtual PRStatus ToString(char *string, PRSize size) const;
    /* convert internal fromat to a string */

private:

    PRNetAddr address;

};  /* RCNetAddr */

/*
** Class: RCHostLookup
**
** Abstractions to look up host names and addresses.
**
** This is a stateful class. One looks up the host by name or by
** address, then enumerates over a possibly empty array of network
** addresses. The storage for the addresses is owned by the class.
*/

class RCHostLookup: public RCBase
{
public:
    virtual ~RCHostLookup();

    RCHostLookup();

    virtual PRStatus ByName(const char* name);
    virtual PRStatus ByAddress(const RCNetAddr&);

    virtual const RCNetAddr* operator[](PRUintn);

private:
    RCLock ml;
    PRIntn max_index;
    RCNetAddr* address;

    RCHostLookup(const RCHostLookup&);
    RCHostLookup& operator=(const RCHostLookup&);
};

inline RCNetAddr::RCNetAddr(): RCBase() { }
inline RCNetAddr::operator const PRNetAddr*() const {
    return &address;
}


#endif /* defined(_RCNETDB_H) */

/* RCNetdb.h */


