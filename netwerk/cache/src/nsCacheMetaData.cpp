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
 * The Original Code is nsCacheMetaData.cpp, released February 22, 2001.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *    Gordon Sheridan, 22-February-2001
 */

#include "nsCacheMetaData.h"
#include "nsICacheEntryDescriptor.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "plstr.h"


nsCacheMetaData::nsCacheMetaData()
    : mData(nsnull)
{
}

void
nsCacheMetaData::Clear()
{
    MetaElement * elem;
    while (mData) {
        elem = mData->mNext;
        delete mData;
        mData = elem;
    }
}

const char *
nsCacheMetaData::GetElement(const char * key)
{
    // We assume the number of meta data elements will be very small, so
    // we keep it real simple.  Singly-linked list, linearly searched.

    nsCOMPtr<nsIAtom> keyAtom = do_GetAtom(key);

    MetaElement * elem = mData;
    while (elem) {
        if (elem->mKey == keyAtom)
            return elem->mValue;
        elem = elem->mNext;
    }
    return nsnull;
}


nsresult
nsCacheMetaData::SetElement(const char * key,
                            const char * value)
{
    nsCOMPtr<nsIAtom> keyAtom = do_GetAtom(key);
    if (!keyAtom)
        return NS_ERROR_OUT_OF_MEMORY;

    // find and remove old meta data element
    MetaElement * elem = mData, * last = nsnull;
    while (elem) {
        if (elem->mKey == keyAtom) {
            // remove elem
            if (last)
                last->mNext = elem->mNext;
            else
                mData = elem->mNext;
            delete elem;
            break;
        }
        last = elem;
        elem = elem->mNext;
    }

    // allocate new meta data element
    if (value) {
        elem = new (value) MetaElement;
        if (!elem)
            return NS_ERROR_OUT_OF_MEMORY;
        elem->mKey = keyAtom;

        // insert after last or as first element...
        if (last) {
            elem->mNext = last->mNext;
            last->mNext = elem;
        }
        else {
            elem->mNext = mData;
            mData = elem;
        }
    }

    return NS_OK;
}

PRUint32
nsCacheMetaData::Size(void)
{
    PRUint32 size = 0;
    const char *key;

    // XXX this should be computed in SetElement

    MetaElement * elem = mData;
    while (elem) {
        elem->mKey->GetUTF8String(&key);

        size += (2 + strlen(key) + strlen(elem->mValue));

        elem = elem->mNext;
    }
    return size;
}

nsresult
nsCacheMetaData::FlattenMetaData(char * buffer, PRUint32 bufSize)
{
    const char *key;
    PRUint32 metaSize = 0;

    MetaElement * elem = mData;
    while (elem) {
        elem->mKey->GetUTF8String(&key);

        PRUint32 keySize = 1 + strlen(key);
        PRUint32 valSize = 1 + strlen(elem->mValue);
        if ((metaSize + keySize + valSize) > bufSize) {
            // not enough space to copy key/value pair
            NS_ERROR("buffer size too small for meta data.");
            return NS_ERROR_OUT_OF_MEMORY;
        }
        
        memcpy(buffer, key, keySize);
        buffer += keySize;
        memcpy(buffer, elem->mValue, valSize);
        buffer += valSize;
        metaSize += keySize + valSize;

        elem = elem->mNext;
    }

    return NS_OK;
}

nsresult
nsCacheMetaData::UnflattenMetaData(char * data, PRUint32 size)
{
    if (size == 0)  return NS_OK;

    nsresult rv = NS_ERROR_UNEXPECTED;
    char* limit = data + size;
    while (data < limit) {
        const char* name = data;
        PRUint32 nameSize = strlen(name);
        data += 1 + nameSize;
        if (data < limit) {
            const char* value = data;
            PRUint32 valueSize = strlen(value);
            data += 1 + valueSize;
            rv = SetElement(name, value);
            if (NS_FAILED(rv)) break;
        }
    }
    return rv;
}

nsresult
nsCacheMetaData::VisitElements(nsICacheMetaDataVisitor * visitor)
{
    const char *key;

    MetaElement * elem = mData;
    while (elem) {
        elem->mKey->GetUTF8String(&key);

        PRBool keepGoing;
        nsresult rv = visitor->VisitMetaDataElement(key, elem->mValue, &keepGoing);

        if (NS_FAILED(rv) || !keepGoing)
            break;

        elem = elem->mNext;
    }

    return NS_OK;
}

void *
nsCacheMetaData::MetaElement::operator new(size_t size, const char *value) CPP_THROW_NEW
{
    int len = strlen(value);
    size += len;

    MetaElement *elem = (MetaElement *) ::operator new(size);
    if (!elem)
        return nsnull;

    memcpy(elem->mValue, value, len);
    elem->mValue[len] = 0;
    return elem;
}
