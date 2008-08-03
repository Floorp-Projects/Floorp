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
inline RCNetAddr::operator const PRNetAddr*() const { return &address; }


#endif /* defined(_RCNETDB_H) */

/* RCNetdb.h */


