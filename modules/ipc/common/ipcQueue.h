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
 * The Original Code is Mozilla IPC.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef ipcQueue_h__
#define ipcQueue_h__

#include "prtypes.h"

//-----------------------------------------------------------------------------
// simple queue of objects
//-----------------------------------------------------------------------------

template<class T>
class ipcQueue
{
public:
    ipcQueue()
        : mHead(NULL)
        , mTail(NULL)
        { }
   ~ipcQueue() { DeleteAll(); }

    //
    // appends msg to the end of the queue.  caller loses ownership of |msg|.
    //
    void Append(T *obj)
    {
        obj->mNext = NULL;
        if (mTail) {
            mTail->mNext = obj;
            mTail = obj;
        }
        else
            mTail = mHead = obj;
    }

    // 
    // removes first element w/o deleting it
    //
    void RemoveFirst()
    {
        if (mHead)
            AdvanceHead();
    }

    //
    // deletes first element
    //
    void DeleteFirst()
    {
        T *first = mHead;
        if (first) {
            AdvanceHead();
            delete first;
        }
    }

    //
    // deletes all elements
    //
    void DeleteAll()
    {
        while (mHead)
            DeleteFirst();
    }

    T      *First()   { return mHead; }
    PRBool  IsEmpty() { return mHead == NULL; }

private:
    void AdvanceHead()
    {
        mHead = mHead->mNext;
        if (!mHead)
            mTail = NULL;
    }

    T *mHead;
    T *mTail;
};

#endif // !ipcQueue_h__
