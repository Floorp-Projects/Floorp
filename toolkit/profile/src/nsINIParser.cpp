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
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Samir Gehani <sgehani@netscape.com>
 *     Benjamin Smedberg <bsmedberg@covad.net>
 */


#include "nsINIParser.h"
#include "nsError.h"
#include "nsILocalFile.h"

#include <stdlib.h>
#include <stdio.h>

nsINIParser::nsINIParser() :
    mFileBuf(nsnull),
    mFileBufSize(0)
{ }

nsresult
nsINIParser::Init(nsILocalFile* aFile)
{
    NS_ASSERTION(aFile, "Null param.");

    nsresult rv;
    FILE    *fd;
    long    eofpos = 0;
    int     rd = 0;

    /* open the file */
    rv = aFile->OpenANSIFileDesc("r", &fd);
    NS_ENSURE_SUCCESS(rv, rv);
    
    /* get file size */
    if (fseek(fd, 0, SEEK_END) != 0) {
        rv = NS_BASE_STREAM_OSERROR;
        goto bail;
    }

    eofpos = ftell(fd);
    if (eofpos == 0) {
        rv = NS_BASE_STREAM_OSERROR;
        goto bail;
    }

    /* malloc an internal buf the size of the file */
    mFileBuf = (char *) malloc((eofpos+1) * sizeof(char));
    if (!mFileBuf) {
        rv = NS_ERROR_OUT_OF_MEMORY;
        goto bail;
    }

    mFileBufSize = eofpos;

    /* read the file in one swoop */
    if (fseek(fd, 0, SEEK_SET) != 0) {
        rv = NS_BASE_STREAM_OSERROR;
        goto bail;
    }

    rd = fread((void *)mFileBuf, 1, eofpos, fd);
    if (!rd) {
        rv = NS_BASE_STREAM_OSERROR;
        goto bail;
    }
    mFileBuf[mFileBufSize] = '\0';

    /* close file */
    fclose(fd);
    return NS_OK;

bail:
    if (fd)
        fclose(fd);

    if (mFileBuf) {
        free(mFileBuf);
        mFileBuf = nsnull;
    }
    return NS_ERROR_FAILURE;
}

nsINIParser::~nsINIParser()
{
    if (mFileBuf) {
        free(mFileBuf);
        mFileBuf = nsnull;
    }
}

nsresult
nsINIParser::GetString(const char *aSection, const char *aKey, 
                       char *aValBuf, PRUint32 aIOValBufSize)
{
    NS_ASSERTION(aSection && aKey && aValBuf && aIOValBufSize,
                 "Null param!");

    nsresult rv;
    char *secPtr;

    /* find the section if it exists */
    rv = FindSection(aSection, &secPtr);
    if (NS_FAILED(rv)) return rv;

    /* find the key if it exists in the valid section we found */
    rv = FindKey(secPtr, aKey, aValBuf, aIOValBufSize);
    return rv;
}

nsresult
nsINIParser::FindSection(const char *aSection, char **aOutSecPtr)
{
    char *currChar = mFileBuf;
    char *nextSec = nsnull;
    char *secClose = nsnull;
    char *nextNL = nsnull;
    int aSectionLen = strlen(aSection);
    nsresult rv = NS_ERROR_FAILURE;

    while (currChar < (mFileBuf + mFileBufSize))
    {
        // look for first '['
        nextSec = NULL;
        nextSec = strchr(currChar, '[');
        if (!nextSec)
            break;
            
        currChar = nextSec + 1;

        // extract section name till first ']'
        secClose = NULL; nextNL = NULL;
        secClose = strchr(currChar, ']');
        nextNL = strchr(currChar, '\n');
        if ((!nextNL) || (nextNL < secClose))
        {
            currChar = nextNL;
            continue;
        }

        // if section name matches we succeeded
        if (strncmp(aSection, currChar, aSectionLen) == 0
              && secClose-currChar == aSectionLen)
        {
            *aOutSecPtr = secClose + 1;
            rv = NS_OK;
            break;
        }
    }

    return rv;
}

nsresult
nsINIParser::FindKey(const char *aSecPtr, const char *aKey, char *aVal, int aIOValSize)
{
    const char *nextNL = nsnull;
    const char *secEnd = nsnull;
    const char *currLine = aSecPtr;
    const char *nextEq = nsnull;
    int  aKeyLen = strlen(aKey); 

    // determine the section end
    secEnd = aSecPtr;
find_end:
    if (secEnd)
        secEnd = strchr(secEnd, '['); // search for next sec start
    if (!secEnd)
    {
        secEnd = strchr(aSecPtr, '\0'); // else search for file end
        if (!secEnd)
        {
            return NS_ERROR_FILE_CORRUPTED;
        }
    }

    // handle start section token ('[') in values for i18n
    if (*secEnd == '[' && !(secEnd == aSecPtr || *(secEnd-1) == '\n'))
    {
        secEnd++;
        goto find_end;
    }

    while (currLine < secEnd)
    {
        nextNL = NULL;
        nextNL = strchr(currLine, '\n');
        if (!nextNL)
            nextNL = mFileBuf + mFileBufSize;

        // ignore commented lines (starting with ;)
        if (currLine == strchr(currLine, ';'))
        {
            currLine = nextNL + 1;
            continue;
        }

        // extract key before '='
        nextEq = NULL;
        nextEq = strchr(currLine, '=');
        if (!nextEq || nextEq > nextNL) 
        {
            currLine = nextNL + 1;
            continue;
        }

        // if key matches we succeeded
        if (strncmp(currLine, aKey, aKeyLen) == 0
              && nextEq-currLine == aKeyLen)
        {
            // extract the value and return
            if (aIOValSize < nextNL - nextEq)
            {
                *aVal = '\0';
                return NS_ERROR_LOSS_OF_SIGNIFICANT_DATA;
            }
                
            PRUint32 len = nextNL - (nextEq + 1); 

            // allows win32-style \r\n line endings
            if ( *(nextEq + len) == '\r' )
                --len;

            strncpy(aVal, (nextEq + 1), len);
            *(aVal + len) = 0; // null terminate
            return NS_OK;
        }
        else
        {
            currLine = nextNL + 1;
        }
    }

    return NS_ERROR_FAILURE;
}
