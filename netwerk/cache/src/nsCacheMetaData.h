/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is nsCacheMetaData.h, released February 22, 2001.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 *
 * Contributor(s):
 *    Gordon Sheridan <gordon@netscape.com>
 */

#ifndef _nsCacheMetaData_h_
#define _nsCacheMetaData_h_

#include "nspr.h"
#include "pldhash.h"
#include "nscore.h"
#include "nsIAtom.h"

class nsICacheMetaDataVisitor;

class nsCacheMetaData {
public:
    nsCacheMetaData();
    ~nsCacheMetaData()  { Clear(); }

    void                  Clear();
    PRBool                IsEmpty() { return (mData == nsnull); }

    const char *          GetElement(const char * key);

    nsresult              SetElement(const char * key,
                                     const char * value);

    PRUint32              Size(void) { return mMetaSize; }

    nsresult              FlattenMetaData(char * buffer, PRUint32 bufSize);

    nsresult              UnflattenMetaData(char * buffer, PRUint32 bufSize);

    nsresult              VisitElements(nsICacheMetaDataVisitor * visitor);

private:

    struct MetaElement
    {
        struct MetaElement * mNext;
        nsCOMPtr<nsIAtom>    mKey;
        char                 mValue[1]; // actually, bigger than 1

        // MetaElement and mValue are allocated together via:
        void *operator new(size_t size,
                           const char *value,
                           PRUint32 valueSize) CPP_THROW_NEW;
    };

    MetaElement * mData;
    PRUint32      mMetaSize;
};

#endif // _nsCacheMetaData_h
