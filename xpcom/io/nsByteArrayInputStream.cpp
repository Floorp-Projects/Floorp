/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 */

#include "nsByteArrayInputStream.h"
#include "nsMemory.h"

NS_IMPL_THREADSAFE_ISUPPORTS2(nsByteArrayInputStream, nsIInputStream, nsIByteArrayInputStream)

nsByteArrayInputStream::nsByteArrayInputStream (char *buffer, PRUint32 bytes)
    : _buffer (buffer), _nbytes (bytes), _pos (0)
{ 
}

nsByteArrayInputStream::~nsByteArrayInputStream ()
{
    if (_buffer != NULL)
        nsMemory::Free (_buffer);
}

NS_IMETHODIMP
nsByteArrayInputStream::Available (PRUint32* aResult)
{
    if (aResult == NULL)
        return NS_ERROR_NULL_POINTER;

    if (_nbytes == 0 || _buffer == NULL)
        *aResult = 0;
    else
        *aResult = _nbytes - _pos;

    return NS_OK;
}

NS_IMETHODIMP
nsByteArrayInputStream::Read (char* aBuffer, PRUint32 aCount, PRUint32 *aNumRead)
{
    if (aBuffer == NULL || aNumRead == NULL)
        return NS_ERROR_NULL_POINTER;

    if (_nbytes == 0)
        return NS_ERROR_FAILURE;

    if (aCount == 0 || _pos == _nbytes)
        *aNumRead = 0;
    else
    {
        NS_ASSERTION (_buffer != NULL, "Stream buffer has been released - there's an ownership problem somewhere!");
        if (_buffer == NULL)
            *aNumRead = 0;
        else
        if (aCount > _nbytes - _pos)
        {        
            memcpy (aBuffer, &_buffer[_pos], *aNumRead = _nbytes - _pos);
            _pos = _nbytes;
        }
        else
        {
            memcpy (aBuffer, &_buffer[_pos], *aNumRead = aCount);
            _pos += aCount;
        }
    }
    return NS_OK;
}

NS_IMETHODIMP 
nsByteArrayInputStream::ReadSegments(nsWriteSegmentFun writer, void * aClosure, PRUint32 aCount, PRUint32 *aNumRead)
{
    nsresult rv = NS_OK;
    if (aNumRead == NULL)
        return NS_ERROR_NULL_POINTER;

    if (_nbytes == 0)
        return NS_ERROR_FAILURE;

    if (aCount == 0 || _pos == _nbytes)
        *aNumRead = 0;
    else {
        NS_ASSERTION (_buffer != NULL, "Stream buffer has been released - there's an ownership problem somewhere!");
        PRUint32 readCount = PR_MIN(aCount, (_nbytes - _pos));
        if (_buffer == NULL)
            *aNumRead = 0;
        else 
            rv = writer (this, aClosure, &_buffer[_pos], 
                         _pos, readCount, aNumRead);
        
        _pos += *aNumRead;
    }

    return rv;
}

NS_IMETHODIMP 
nsByteArrayInputStream::IsNonBlocking(PRBool *aNonBlocking)
{
    *aNonBlocking = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsByteArrayInputStream::Close ()
{
    if (_buffer != NULL)
    {
        nsMemory::Free (_buffer);
        _buffer = NULL;
        _nbytes = 0;
    }
    else
        return NS_ERROR_FAILURE;

    return NS_OK;
}

NS_COM nsresult
NS_NewByteArrayInputStream (nsIByteArrayInputStream* *aResult, char * buffer, unsigned long bytes)
{
    if (aResult == NULL)
        return NS_ERROR_NULL_POINTER;

    nsIByteArrayInputStream * stream = new nsByteArrayInputStream (buffer, bytes);

    if (!stream)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF (stream);
    *aResult = stream;
    return NS_OK;
}
