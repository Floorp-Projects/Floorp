/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is nsCacheMetaData.cpp, released
 * February 22, 2001.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Gordon Sheridan <gordon@netscape.com>
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

#include "nsCacheMetaData.h"
#include "nsICacheEntryDescriptor.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "plstr.h"

nsCacheMetaData::nsCacheMetaData()
    : mData(nsnull), mMetaSize(0)
{
}

void
nsCacheMetaData::Clear()
{
    mMetaSize = 0;
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

    PRUint32 keySize = strlen(key);
    PRUint32 valueSize = value ? strlen(value) : 0;

    // find and remove or update old meta data element
    MetaElement * elem = mData, * last = nsnull;
    while (elem) {
        if (elem->mKey == keyAtom) {
            // Get length of old value
            PRUint32 oldValueLen = strlen(elem->mValue);
            if (valueSize == oldValueLen) {
                // Just replace value
                memcpy(elem->mValue, value, valueSize);
                return NS_OK;
            }
            // remove elem
            if (last)
                last->mNext = elem->mNext;
            else
                mData = elem->mNext;
            // 2 for the zero bytes of both strings
            mMetaSize -= 2 + keySize + oldValueLen;
            delete elem;
            break;
        }
        last = elem;
        elem = elem->mNext;
    }

    // allocate new meta data element
    if (value) {
        elem = new (value, valueSize) MetaElement;
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

        // Adjust CacheMetaData size, 2 for the zero bytes of both strings
        mMetaSize += 2 + keySize + valueSize;
    }

    return NS_OK;
}

nsresult
nsCacheMetaData::FlattenMetaData(char * buffer, PRUint32 bufSize)
{
    const char *key;

    if (mMetaSize > bufSize) {
        NS_ERROR("buffer size too small for meta data.");
        return NS_ERROR_OUT_OF_MEMORY;
    }

    MetaElement * elem = mData;
    while (elem) {
        elem->mKey->GetUTF8String(&key);

        PRUint32 keySize = 1 + strlen(key);
        memcpy(buffer, key, keySize);
        buffer += keySize;

        PRUint32 valSize = 1 + strlen(elem->mValue);
        memcpy(buffer, elem->mValue, valSize);
        buffer += valSize;

        elem = elem->mNext;
    }
    return NS_OK;
}

nsresult
nsCacheMetaData::UnflattenMetaData(const char * data, PRUint32 size)
{
    if (size == 0) return NS_OK;

    const char* limit = data + size;
    MetaElement * last = nsnull;

    while (data < limit) {
        const char* key = data;
        PRUint32 keySize = strlen(key);
        data += 1 + keySize;
        if (data < limit) {
            nsCOMPtr<nsIAtom> keyAtom = do_GetAtom(key);
            if (!keyAtom)
                return NS_ERROR_OUT_OF_MEMORY;

            PRUint32 valueSize = strlen(data);
            MetaElement *elem = new (data, valueSize) MetaElement;
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

            last = elem;
            data += 1 + valueSize;

            // Adjust CacheMetaData size, 2 for the zero bytes of both strings
            mMetaSize += 2 + keySize + valueSize;
        }
    }
    return NS_OK;
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
nsCacheMetaData::MetaElement::operator new(size_t size,
                                           const char *value,
                                           PRUint32 valueSize) CPP_THROW_NEW
{
    size += valueSize;

    MetaElement *elem = (MetaElement *) ::operator new(size);
    if (!elem)
        return nsnull;

    memcpy(elem->mValue, value, valueSize);
    elem->mValue[valueSize] = 0;

    return elem;
}
