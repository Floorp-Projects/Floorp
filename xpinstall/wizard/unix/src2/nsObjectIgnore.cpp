/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 */

#include "nsObjectIgnore.h"
#include "assert.h"

nsObjectIgnore::nsObjectIgnore() :
    mFilename(NULL),
    mNext(NULL)
{
}

nsObjectIgnore::~nsObjectIgnore()
{
    XI_IF_FREE(mFilename);
}

int
nsObjectIgnore::SetFilename(char *aFilename)
{
    if (!aFilename)
        return E_PARAM;
    
    mFilename = aFilename;

    return OK;
}

char *
nsObjectIgnore::GetFilename()
{
    return mFilename;
}

int
nsObjectIgnore::SetNext(nsObjectIgnore *aNext)
{
    if (!aNext)
        return E_PARAM;

    mNext = aNext;

    return OK;
}

nsObjectIgnore *
nsObjectIgnore::GetNext()
{
    return mNext;
}

int
nsObjectIgnore::InitNext()
{
    XI_ASSERT((mNext==NULL), "Leaking nsObjectIgnore::mNext!\n");
    mNext = NULL;

    return OK;
}
