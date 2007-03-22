/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Samir Gehani <sgehani@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsComponentList.h"
#include "nsComponent.h"

nsComponentList::nsComponentList() :
    mHeadItem(NULL),
    mTailItem(NULL),
    mNextItem(NULL),
    mLength(0)
{
}

nsComponentList::~nsComponentList()
{
    nsComponentItem *currItem = mHeadItem;
    nsComponentItem *nextItem = NULL;

    while (currItem)
    {
        nextItem = currItem->mNext;
        currItem->mComp->Release();
        delete currItem;
        currItem = nextItem;
    }

    mHeadItem = NULL;
    mTailItem = NULL;
    mLength = 0;
}

nsComponent *
nsComponentList::GetHead()
{
    if (mHeadItem)
    {
        mNextItem = mHeadItem->mNext;
        return mHeadItem->mComp;
    }

    return NULL;
}

nsComponent *
nsComponentList::GetNext()
{
    nsComponentItem *currItem = mNextItem;

    if (mNextItem)
    {
        mNextItem = mNextItem->mNext;
        return currItem->mComp;
    }

    return NULL;
}

nsComponent *
nsComponentList::GetTail()
{
    if (mTailItem)
        return mTailItem->mComp;

    return NULL;
}

int
nsComponentList::GetLength()
{
    return mLength;
}

int
nsComponentList::GetLengthVisible()
{
    int numVisible = 0;
    nsComponentItem *currItem;

    currItem = mHeadItem;
    if (!currItem) return 0;

    while (currItem)
    {
        if (!currItem->mComp->IsInvisible())
            numVisible++;
        currItem = currItem->mNext;
    }
        
    return numVisible;
}

int
nsComponentList::GetLengthSelected()
{
    int numSelected = 0;
    nsComponentItem *currItem;

    currItem = mHeadItem;
    if (!currItem) return 0;

    while (currItem)
    {
        if (currItem->mComp->IsSelected())
            numSelected++;
        currItem = currItem->mNext;
    }
        
    return numSelected;
}

int
nsComponentList::AddComponent(nsComponent *aComponent)
{
    if (!aComponent)
        return E_PARAM;

    aComponent->AddRef();
    nsComponentItem *newItem 
       = (nsComponentItem *) malloc(sizeof(nsComponentItem));
    newItem->mComp = aComponent;
    newItem->mNext = NULL;
    mLength++;
    
    if (mHeadItem)
    {
        // non-empty list: the new comp is tacked on then end
        mTailItem->mNext = newItem;
    }
    else
    {
        // empty list: head and tail are the new comp
        mHeadItem = newItem;
    }

    mTailItem = newItem;

    return OK;
}

int
nsComponentList::RemoveComponent(nsComponent *aComponent)
{
    nsComponentItem *currItem = mHeadItem;
    nsComponentItem *last = NULL;

    if (!aComponent)
        return E_PARAM;

    while (currItem)
    {
        if (aComponent == currItem->mComp)
        {
            // remove and link last to next while deleting current
            if (last)
            {
                last->mNext = currItem->mNext;
            }
            else
            {
                mHeadItem = currItem->mNext;
            }

            if (mTailItem == currItem)
                mTailItem = NULL;
            if (mNextItem == currItem)
                mNextItem = mNextItem->mNext;

            aComponent->Release();
            mLength--;

            return OK;
        }
        else
        {
            // move on to next
            last = currItem;
            currItem = currItem->mNext;
        }
    }

    return E_PARAM;
}

nsComponent *
nsComponentList::GetCompByIndex(int aIndex)
{
    nsComponentItem *currItem = mHeadItem;
    int i;

    // param check
    if (!currItem || mLength == 0) return NULL;

    for (i=0; i<mLength; i++)
    { 
        if (aIndex == currItem->mComp->GetIndex())
        {
            return currItem->mComp;
        }

        currItem = currItem->mNext;
    }

    return NULL;
}

nsComponent *
nsComponentList::GetCompByArchive(char *aArchive)
{
    nsComponentItem *currItem = mHeadItem;
    int i;

    // param check
    if (!currItem || mLength == 0 || !aArchive) return NULL;

    for (i=0; i<mLength; i++)
    { 
        if (0==strncmp(aArchive, currItem->mComp->GetArchive(), strlen(aArchive)))
        {
            return currItem->mComp;
        }

        currItem = currItem->mNext;
        if (!currItem) break;
    }

    return NULL;
}

nsComponent *
nsComponentList::GetCompByShortDesc(char *aShortDesc)
{
    nsComponentItem *currItem = mHeadItem;
    int i;

    // param check
    if (!currItem || mLength == 0 || !aShortDesc) return NULL;

    for (i=0; i<mLength; i++)
    { 
        if (0==strncmp(aShortDesc, currItem->mComp->GetDescShort(), 
                       strlen(aShortDesc)))
        {
            return currItem->mComp;
        }

        currItem = currItem->mNext;
        if (!currItem) break;
    }

    return NULL;
}

nsComponent *
nsComponentList::GetFirstVisible()
{
    int i;
    nsComponentItem *currItem = mHeadItem;

    // param check
    if (mLength == 0) return NULL;

    for (i=0; i<mLength; i++)
    { 
        if (!currItem->mComp->IsInvisible())
        {
            return currItem->mComp;
        }

        currItem = currItem->mNext;
        if (!currItem) break;
    }

    return NULL;
}
