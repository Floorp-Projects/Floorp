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
    mAttributes(NO_ATTR),
    mNext(NULL),
    mIndex(-1),
    mRefCount(0)
{
    int i;

    for (i = 0; i < MAX_URLS; i++)
        mURL[i] = NULL;
}

nsComponent::~nsComponent()
{
    int i;

    XI_IF_FREE(mDescShort);
    XI_IF_FREE(mDescLong);
    XI_IF_FREE(mArchive);
    XI_IF_DELETE(mDependencies)
    for (i = 0; i < MAX_URLS; i++)
        XI_IF_FREE(mURL[i]);
}

nsComponent *
nsComponent::Duplicate()
{
    nsComponent *zdup = new nsComponent();
    *zdup = *this;
    zdup->InitRefCount();
    zdup->InitNext();

    return zdup;
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
nsComponent::SetURL(char *aURL, int aIndex)
{
    if (!aURL)
        return E_PARAM;
    if (mURL[aIndex])
        return E_URL_ALREADY;

    mURL[aIndex] = aURL;
    
    return OK;
}

char *
nsComponent::GetURL(int aIndex)
{
    if (aIndex < 0 || aIndex >= MAX_URLS)
        return NULL;

    return mURL[aIndex];
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
nsComponent::SetLaunchApp()
{
    mAttributes |= nsComponent::LAUNCHAPP;

    return OK;
}

int
nsComponent::SetDontLaunchApp()
{
    if (IsLaunchApp())
        mAttributes &= ~nsComponent::LAUNCHAPP;

    return OK;
}

int
nsComponent::IsLaunchApp()
{
    if (mAttributes & nsComponent::LAUNCHAPP)
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

int
nsComponent::SetIndex(int aIndex)
{
    if (aIndex < 0 || aIndex > MAX_COMPONENTS)
        return E_OUT_OF_BOUNDS;

    mIndex = aIndex;
        
    return OK;
}

int
nsComponent::GetIndex()
{
    if (mIndex < 0 || mIndex > MAX_COMPONENTS)
        return E_OUT_OF_BOUNDS;
    
    return mIndex;
}

int
nsComponent::AddRef()
{
    mRefCount++;
    
    return OK;
}

int
nsComponent::Release()
{
    mRefCount--;

    if (mRefCount < 0)
        return E_REF_COUNT;

    if (mRefCount == 0)
        delete this;

    return OK;
}

int
nsComponent::InitRefCount()
{
    mRefCount = 1;

    return OK;
}
