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

#include "nsSetupType.h"

nsSetupType::nsSetupType() :
    mDescShort(NULL),
    mDescLong(NULL),
    mComponents(NULL),
    mNext(NULL)
{
}

nsSetupType::~nsSetupType()
{
    if (mDescShort)
        free (mDescShort);
    if (mDescLong)
        free (mDescLong);
    if (mComponents)   
        delete mComponents;
}

int
nsSetupType::SetDescShort(char *aDescShort)
{
    if (!aDescShort)
        return E_PARAM;
    
    mDescShort = aDescShort;

    return OK;
}

char *
nsSetupType::GetDescShort()
{
    if (mDescShort)
        return mDescShort;

    return NULL;
}

int
nsSetupType::SetDescLong(char *aDescLong)
{
    if (!aDescLong)
        return E_PARAM;
    
    mDescLong = aDescLong;

    return OK;
}

char *
nsSetupType::GetDescLong()
{
    if (mDescLong)
        return mDescLong;

    return NULL;
}

int
nsSetupType::SetComponent(nsComponent *aComponent)
{
    if (!aComponent)
        return E_PARAM;

    if (!mComponents)
        mComponents = new nsComponentList();

    if (!mComponents)
        return E_MEM;

    return mComponents->AddComponent(aComponent);
}

int
nsSetupType::UnsetComponent(nsComponent *aComponent)
{
    if (!aComponent)
        return E_PARAM;

    if (!mComponents)
        return OK;

    return mComponents->RemoveComponent(aComponent);
}

nsComponentList *
nsSetupType::GetComponents()
{
    if (mComponents)
        return mComponents;

    return NULL;
}

int
nsSetupType::SetNext(nsSetupType *aSetupType)
{
    if (!aSetupType)
        return E_PARAM;

    mNext = aSetupType;

    return OK;
}

nsSetupType *
nsSetupType::GetNext()
{
    if (mNext)
        return mNext;

    return NULL;
}
