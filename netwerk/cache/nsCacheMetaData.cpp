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
#include "prmem.h"

const char *
nsCacheMetaData::GetElement(const char * key)
{
    const char * data = mBuffer;
    const char * limit = mBuffer + mMetaSize;

    while (data < limit) {
        // Point to the value part
        const char * value = data + strlen(data) + 1;
        NS_ABORT_IF_FALSE(value < limit, "Cache Metadata corrupted");
        if (strcmp(data, key) == 0)
            return value;

        // Skip value part
        data = value + strlen(value) + 1;
    }
    NS_ABORT_IF_FALSE(data == limit, "Metadata corrupted");
    return nsnull;
}


nsresult
nsCacheMetaData::SetElement(const char * key,
                            const char * value)
{
    const PRUint32 keySize = strlen(key) + 1;
    char * pos = (char *)GetElement(key);

    if (!value) {
        // No value means remove the key/value pair completely, if existing
        if (pos) {
            PRUint32 oldValueSize = strlen(pos) + 1;
            PRUint32 offset = pos - mBuffer;
            PRUint32 remainder = mMetaSize - (offset + oldValueSize);

            memmove(pos - keySize, pos + oldValueSize, remainder);
            mMetaSize -= keySize + oldValueSize;
        }
        return NS_OK;
    }

    const PRUint32 valueSize = strlen(value) + 1;
    PRUint32 newSize = mMetaSize + valueSize;
    if (pos) {
        const PRUint32 oldValueSize = strlen(pos) + 1;
        const PRUint32 offset = pos - mBuffer;
        const PRUint32 remainder = mMetaSize - (offset + oldValueSize);

        // Update the value in place
        newSize -= oldValueSize;
        nsresult rv = EnsureBuffer(newSize);
        NS_ENSURE_SUCCESS(rv, rv);

        // Move the remainder to the right place
        pos = mBuffer + offset;
        memmove(pos + valueSize, pos + oldValueSize, remainder);
    } else {
        // allocate new meta data element
        newSize += keySize;
        nsresult rv = EnsureBuffer(newSize);
        NS_ENSURE_SUCCESS(rv, rv);

        // Add after last element
        pos = mBuffer + mMetaSize;
        memcpy(pos, key, keySize);
        pos += keySize;
    }

    // Update value
    memcpy(pos, value, valueSize);
    mMetaSize = newSize;

    return NS_OK;
}

nsresult
nsCacheMetaData::FlattenMetaData(char * buffer, PRUint32 bufSize)
{
    if (mMetaSize > bufSize) {
        NS_ERROR("buffer size too small for meta data.");
        return NS_ERROR_OUT_OF_MEMORY;
    }

    memcpy(buffer, mBuffer, mMetaSize);
    return NS_OK;
}

nsresult
nsCacheMetaData::UnflattenMetaData(const char * data, PRUint32 size)
{
    if (data && size) {
        // Check if the metadata ends with a zero byte.
        if (data[size-1] != '\0') {
            NS_ERROR("Cache MetaData is not null terminated");
            return NS_ERROR_ILLEGAL_VALUE;
        }
        // Check that there are an even number of zero bytes
        // to match the pattern { key \0 value \0 }
        PRBool odd = PR_FALSE;
        for (int i = 0; i < size; i++) {
            if (data[i] == '\0') 
                odd = !odd;
        }
        if (odd) {
            NS_ERROR("Cache MetaData is malformed");
            return NS_ERROR_ILLEGAL_VALUE;
        }

        nsresult rv = EnsureBuffer(size);
        NS_ENSURE_SUCCESS(rv, rv);

        memcpy(mBuffer, data, size);
        mMetaSize = size;
    }
    return NS_OK;
}

nsresult
nsCacheMetaData::VisitElements(nsICacheMetaDataVisitor * visitor)
{
    const char * data = mBuffer;
    const char * limit = mBuffer + mMetaSize;

    while (data < limit) {
        const char * key = data;
        // Skip key part
        data += strlen(data) + 1;
        NS_ABORT_IF_FALSE(data < limit, "Metadata corrupted");
        PRBool keepGoing;
        nsresult rv = visitor->VisitMetaDataElement(key, data, &keepGoing);
        if (NS_FAILED(rv) || !keepGoing)
            break;

        // Skip value part
        data += strlen(data) + 1;
    }
    NS_ABORT_IF_FALSE(data == limit, "Metadata corrupted");
    return NS_OK;
}

nsresult
nsCacheMetaData::EnsureBuffer(PRUint32 bufSize)
{
    if (mBufferSize < bufSize) {
        char * buf = (char *)PR_REALLOC(mBuffer, bufSize);
        if (!buf) {
            return NS_ERROR_OUT_OF_MEMORY;
        }
        mBuffer = buf;
        mBufferSize = bufSize;
    }
    return NS_OK;
}        
