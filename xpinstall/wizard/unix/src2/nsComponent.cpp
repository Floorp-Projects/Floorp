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
#include <sys/stat.h>

nsComponent::nsComponent() :
    mDescShort(NULL),
    mDescLong(NULL),
    mArchive(NULL),
    mInstallSize(0),
    mArchiveSize(0),
    mNextDependeeIdx(0),
    mAttributes(NO_ATTR),
    mNext(NULL),
    mIndex(-1),
    mRefCount(0),
    mDepRefCount(0),
    mResPos(0),
    mDownloaded(FALSE)
{
    int i;

    for (i = 0; i < MAX_URLS; i++)
        mURL[i] = NULL;
    for (i = 0; i < MAX_COMPONENTS; i++)
        mDependees[i] = NULL;
}

nsComponent::~nsComponent()
{
    int i;

    XI_IF_FREE(mDescShort);
    XI_IF_FREE(mDescLong);
    XI_IF_FREE(mArchive);
    for (i = 0; i < MAX_URLS; i++)
        XI_IF_FREE(mURL[i]);
    for (i = 0; i < MAX_COMPONENTS; i++)
        XI_IF_FREE(mDependees[i]);
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
nsComponent::SetInstallSize(int aInstallSize)
{
    mInstallSize = aInstallSize;

    return OK;
}

int
nsComponent::GetInstallSize()
{
    if (mInstallSize >= 0)
        return mInstallSize;

    return 0;
}

int
nsComponent::SetArchiveSize(int aArchiveSize)
{
    mArchiveSize = aArchiveSize;

    return OK;
}

int
nsComponent::GetArchiveSize()
{
    if (mArchiveSize >= 0)
        return mArchiveSize;

    return 0;
}

int
nsComponent::GetCurrentSize()
{
    // assumes cwd is the same as the installer binary location

    char path[MAXPATHLEN];
    struct stat stbuf;

    if (!mArchive)
        return 0;

    sprintf(path, "./xpi/%s", mArchive);
    if (0 != stat(path, &stbuf))
        return 0;

    return (stbuf.st_size/1024); // return size in KB
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
nsComponent::AddDependee(char *aDependee)
{
    if (!aDependee)
        return E_PARAM;

    mDependees[mNextDependeeIdx] = aDependee;
    mDependees[++mNextDependeeIdx] = NULL;

    return OK;
}

int 
nsComponent::ResolveDependees(int aBeingSelected, nsComponentList *aComps)
{
    int i;
    nsComponent *currComp = NULL;
    
    // param check
    if (!aComps)
        return E_PARAM;

    // loop over all dependees
    for (i = 0; i < mNextDependeeIdx; i++)
    {
        if (!mDependees[i])
            break;

        currComp = aComps->GetCompByShortDesc(mDependees[i]);
        if (!currComp)
            continue;
        
        if (aBeingSelected)
            currComp->DepAddRef();
        else    
            currComp->DepRelease();
    }

    return OK;
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
    mDepRefCount = 0;

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
nsComponent::SetDownloadOnly()
{
    mAttributes |= nsComponent::DOWNLOAD_ONLY;

    return OK;
}

int
nsComponent::IsDownloadOnly()
{
    if (mAttributes & nsComponent::DOWNLOAD_ONLY)
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

int 
nsComponent::DepAddRef()
{
    if (mDepRefCount == 0)
        SetSelected();

    mDepRefCount++;
        
    return OK;
}

int
nsComponent::DepRelease()
{
    mDepRefCount--;

    if (mDepRefCount < 0)
        mDepRefCount = 0;

    if (mDepRefCount == 0)
        SetUnselected();
 
    return OK;
}

int
nsComponent::DepGetRefCount()
{
    return mDepRefCount;
}

int
nsComponent::SetResumePos(int aResPos)
{
    mResPos = aResPos;

    return OK;
}

int
nsComponent::GetResumePos()
{
    if (mResPos > 0)
        return mResPos;

    return 0;
}

int
nsComponent::SetDownloaded( int which )
{
    mDownloaded = which;

    return OK;
}

int
nsComponent::IsDownloaded()
{
    return mDownloaded;
}
