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
    mHead(NULL),
    mTail(NULL),
    mNext(NULL),
    mLength(0)
{
}

nsComponentList::~nsComponentList()
{
    nsComponent *curr = mHead;
    nsComponent *next = NULL;

    while (curr)
    {
        next = NULL;
        next = curr->GetNext();
        curr->Release();
        curr = next;
    }

    mHead = NULL;
    mTail = NULL;
    mLength = 0;
}

nsComponent *
nsComponentList::GetHead()
{
    if (mHead)
    {
        mNext = mHead->GetNext();
        return mHead;
    }

    return NULL;
}

nsComponent *
nsComponentList::GetNext()
{
    nsComponent *curr = mNext;

    if (mNext)
    {
        mNext = mNext->GetNext();
        return curr;        
    }

    return NULL;
}

nsComponent *
nsComponentList::GetTail()
{
    if (mTail)
        return mTail;

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
    nsComponent *curr;

    curr = mHead;
    if (!curr) return 0;

    while (curr)
    {
        if (!curr->IsInvisible())
            numVisible++;
        curr = curr->GetNext();
    }
        
    return numVisible;
}

int
nsComponentList::GetLengthSelected()
{
    int numSelected = 0;
    nsComponent *curr;

    /* NOTE:
     * ----
     * If copies of components are help by this list rather than pointers 
     * then this method will return an inaccurate number.  Due to 
     * architecture be very careful when using this method.
     */

    curr = mHead;
    if (!curr) return 0;

    while (curr)
    {
        if (!curr->IsSelected())
            numSelected++;
        curr = curr->GetNext();
    }
        
    return numSelected;
}

int
nsComponentList::AddComponent(nsComponent *aComponent)
{
    if (!aComponent)
        return E_PARAM;

    // empty list: head and tail are the same -- the new comp
    if (!mHead)
    {
        mHead = aComponent;
        mHead->InitNext();
        mTail = mHead;
        aComponent->AddRef();
        mLength = 1;
        mHead->SetIndex(0);

        return OK;
    }

    // non-empty list: the new comp is tacked on and tail is updated
    mTail->SetNext(aComponent);
    mTail = aComponent;
    mTail->InitNext();
    aComponent->AddRef();
    mLength++;
    mTail->SetIndex(mLength - 1);

    return OK;
}

int
nsComponentList::RemoveComponent(nsComponent *aComponent)
{
    int err = OK;
    nsComponent *curr = GetHead();
    nsComponent *last = NULL;

    if (!aComponent)
        return E_PARAM;

    while (curr)
    {
        if (aComponent == curr)
        {
            // remove and link last to next while deleting current
            if (last)
            {
                last->SetNext(curr->GetNext());
            }
            else
            {
                mHead = curr->GetNext();
                if (mTail == curr)
                    mTail = NULL;
            }

            aComponent->Release();
            mLength--;
            
            return OK;
        }
        else
        {
            // move on to next
            last = curr;
            curr = GetNext();
        }
    }

    return err;
}

nsComponent *
nsComponentList::GetCompByIndex(int aIndex)
{
    nsComponent *comp = GetHead();
    int i;

    // param check
    if (!comp || mLength == 0) return NULL;

    for (i=0; i<mLength; i++)
    { 
        if (aIndex == comp->GetIndex())
            return comp;

        comp = GetNext();
        if (!comp) break;
    }

    return NULL;
}

nsComponent *
nsComponentList::GetCompByArchive(char *aArchive)
{
    nsComponent *comp = GetHead();
    int i;

    // param check
    if (!comp || mLength == 0 || !aArchive) return NULL;

    for (i=0; i<mLength; i++)
    { 
        if (0==strncmp(aArchive, comp->GetArchive(), strlen(aArchive)))
            return comp;

        comp = GetNext();
        if (!comp) break;
    }

    return NULL;
}

nsComponent *
nsComponentList::GetCompByShortDesc(char *aShortDesc)
{
    nsComponent *comp = GetHead();
    int i;

    // param check
    if (!comp || mLength == 0 || !aShortDesc) return NULL;

    for (i=0; i<mLength; i++)
    { 
        if (0==strncmp(aShortDesc, comp->GetDescShort(), 
                       strlen(aShortDesc)))
            return comp;

        comp = GetNext();
        if (!comp) break;
    }

    return NULL;
}

nsComponent *
nsComponentList::GetFirstVisible()
{
    int i;
    nsComponent *comp = GetHead();

    // param check
    if (mLength == 0) return NULL;

    for (i=0; i<mLength; i++)
    { 
        if (!comp->IsInvisible())
            return comp;

        comp = GetNext();
        if (!comp) break;
    }

    return NULL;
}
