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
        delete curr;
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

        return OK;
    }

    // non-empty list: the new comp is tacked on and tail is updated
    mTail->SetNext(aComponent);
    mTail = aComponent;
    mTail->InitNext();

    return OK;
}

int
nsComponentList::RemoveComponent(nsComponent *aComponent)
{
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

            delete aComponent;
            
            return OK;
        }
        else
        {
            // move on to next
            last = curr;
            curr = GetNext();
        }
    }
}
