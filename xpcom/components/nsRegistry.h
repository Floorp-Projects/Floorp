/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *	Edward Kandrot  <kandrot@netscape.com>
 */


#ifndef nsRegistry_h__
#define nsRegistry_h__

#include "nsIRegistry.h"
#include "NSReg.h"

struct nsRegistry : public nsIRegistry {
    // This class implements the nsISupports interface functions.
    NS_DECL_ISUPPORTS

    // This class implements the nsIRegistry interface functions.
    NS_DECL_NSIREGISTRY

    // ctor/dtor
    nsRegistry();
    virtual ~nsRegistry();

    int SetBufferSize( int bufsize );  // changes the file buffer size for this registry

protected:
    HREG   mReg; // Registry handle.
#ifdef EXTRA_THREADSAFE
    PRLock *mregLock;    // libreg isn't threadsafe. Use locks to synchronize.
#endif
    char *mCurRegFile;    // these are to prevent open from opening the registry again
    nsWellKnownRegistry mCurRegID;

    NS_IMETHOD Close();
}; // nsRegistry

#endif
