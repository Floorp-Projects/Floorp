/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 */

#include "nsFileSpecStreaming.h"

#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsFileStream.h"
#include "nsFileSpec.h"

#define MAX_PERSISTENT_DATA_SIZE 1000

//----------------------------------------------------------------------------------------
nsOutputStream& operator << (nsOutputStream& s, const nsFileURL& url)
//----------------------------------------------------------------------------------------
{
    return s << url.GetAsString();
}

//----------------------------------------------------------------------------------------
nsresult ReadDescriptor(
    nsIInputStream* aStream,
    nsPersistentFileDescriptor& desc)
//----------------------------------------------------------------------------------------
{
    nsInputStream inputStream(aStream);
    inputStream >> desc;
    return NS_OK;
}

//----------------------------------------------------------------------------------------
nsresult WriteDescriptor(
    nsIOutputStream* aStream,
    const nsPersistentFileDescriptor& desc)
//----------------------------------------------------------------------------------------
{
    nsOutputStream outputStream(aStream);
    outputStream << desc;
    return NS_OK;
}

//----------------------------------------------------------------------------------------
nsInputStream& operator >> (nsInputStream& s, nsPersistentFileDescriptor& d)
// reads the data from a file
//----------------------------------------------------------------------------------------
{
    char bigBuffer[MAX_PERSISTENT_DATA_SIZE + 1];
    // The first 8 bytes of the data should be a hex version of the data size to follow.
    PRInt32 bytesRead = 8;
    bytesRead = s.read(bigBuffer, bytesRead);
    if (bytesRead != 8)
        return s;
    bigBuffer[8] = '\0';
    sscanf(bigBuffer, "%x", (PRUint32*)&bytesRead);
    if (bytesRead > MAX_PERSISTENT_DATA_SIZE)
    {
        // Try to tolerate encoded values with no length header
        bytesRead = 8 + s.read(bigBuffer + 8, MAX_PERSISTENT_DATA_SIZE - 8);
    }
    else
    {
        // Now we know how many bytes to read, do it.
        bytesRead = s.read(bigBuffer, bytesRead);
    }
    // Make sure we are null terminated
    bigBuffer[bytesRead]='\0';
    d.SetData(bigBuffer, bytesRead);
    return s;
}

//----------------------------------------------------------------------------------------
nsOutputStream& operator << (nsOutputStream& s, const nsPersistentFileDescriptor& d)
// writes the data to a file
//----------------------------------------------------------------------------------------
{
    char littleBuf[9];
    PRInt32 dataSize;
    nsSimpleCharString data;
    d.GetData(data, dataSize);
    // First write (in hex) the length of the data to follow.  Exactly 8 bytes
    sprintf(littleBuf, "%.8x", dataSize);
    s << littleBuf;
    // Now write the data itself
    s << (const char*)data;
    return s;
}

