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

#include "nsComponent.h"

nsComponent::nsComponent() :
    mDescShort(NULL),
    mDescLong(NULL),
    mArchive(NULL),
    mSize(0),
    mDependencies(NULL),
    mAttributes(NO_ATTR)
{
}

nsComponent::~nsComponent()
{
    if (mDescShort)
        free (mDescShort);
    if (mDescLong)
        free (mDescLong);
    if (mArchive)
        free (mArchive);
    if (mDependencies)
        delete mDependencies;
}

int
nsComponent::SetDescShort(char *aDescShort)
{
    if (!aDescShort)
        return E_PARAM;

    mDescShort = aDescShort;

    return OK;
}

char *
nsComponent::GetDescShort()
{
    if (mDescShort)
        return mDescShort;

    return NULL;
}

int
nsComponent::SetDescLong(char *aDescLong)
{
    if (!aDescLong)
        return E_PARAM;

    mDescLong = aDescLong;

    return OK;
}

char *
nsComponent::GetDescLong()
{
    if (mDescLong)
        return mDescLong;

    return NULL;
}

int
nsComponent::SetArchive(char *aArchive)
{
    if (!aArchive)
        return E_PARAM;

    mArchive = aArchive;

    return OK;
}

char *
nsComponent::GetArchive()
{
    if (mArchive)
        return mArchive;

    return NULL;
}

int
nsComponent::SetSize(int aSize)
{
    mSize = aSize;

    return OK;
}

int
nsComponent::GetSize()
{
    if (mSize >= 0)
        return mSize;

    return 0;
}

int 
nsComponent::AddDependency(nsComponent *aDependent)
{
    if (!aDependent)
        return E_PARAM;

    if (!mDependencies)
        mDependencies = new nsComponentList();

    if (!mDependencies)
        return E_MEM;
 
    return mDependencies->AddComponent(aDependent);
}

int
nsComponent::RemoveDependency(nsComponent *aIndependent)
{
    if (!aIndependent)
        return E_PARAM;

    if (!mDependencies)
        return E_NO_MEMBER;

    return mDependencies->RemoveComponent(aIndependent);
}

nsComponentList *
nsComponent::GetDependencies()
{
    if (mDependencies)
        return mDependencies;

    return NULL;
}

int
nsComponent::SetSelected()
{
    mAttributes |= nsComponent::SELECTED;

    return OK;
}

int
nsComponent::SetUnselected()
{
    if (IsSelected())
        mAttributes &= ~nsComponent::SELECTED;

    return OK;
}

int
nsComponent::IsSelected()
{
    if (mAttributes & nsComponent::SELECTED)
        return TRUE;

    return FALSE;
}

int
nsComponent::SetInvisible()
{
    mAttributes |= nsComponent::INVISIBLE;

    return OK;
}

int
nsComponent::SetVisible()
{
    if (IsInvisible())
        mAttributes &= ~nsComponent::INVISIBLE;

    return OK;
}

int
nsComponent::IsInvisible()
{
    if (mAttributes & nsComponent::INVISIBLE)
        return TRUE;

    return FALSE;
}

int
nsComponent::SetNext(nsComponent *aComponent)
{
    if (!aComponent)
        return E_PARAM;

    mNext = aComponent;
    
    return OK;
}

int
nsComponent::InitNext()
{
    mNext = NULL;

    return OK;
}

nsComponent *
nsComponent::GetNext()
{
    if (mNext)
        return mNext;

    return NULL;
}
