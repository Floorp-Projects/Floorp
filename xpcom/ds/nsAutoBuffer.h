/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is auto buffer template.
 *
 * The Initial Developer of the Original Code is
 * Conrad Carlen <ccarlen@mac.com>.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jungshik Shin <jshin@mailaps.org>
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

#ifndef nsAutoBuffer_h__
#define nsAutoBuffer_h__

#ifndef nsMemory_h__
#include "nsMemory.h"
#endif

/**
 * A buffer which will use stack space if the requested size will
 * fit in the stack buffer and allocate from the heap if not.
 * 
 * Below is a usage example : 
 * 
 * typedef nsAutoBuffer<PRUnichar, 256> nsAutoUnicharBuffer;
 *
 * nsAutoUnicharBuffer buffer;
 *
 * if (!buffer.EnsureElemCapacity(initialLength))
 *    return NS_ERROR_OUT_OF_MEMORY;
 *
 * PRUnichar *unicharPtr = buffer.get();
 *
 * // add PRUnichar's to the buffer pointed to by |unicharPtr| as long as
 * // the number of PRUnichar's is less than |intialLength|
 *    
 * // increase the capacity
 * if (!buffer.AddElemCapacity(extraLength))
 *     return NS_ERROR_OUT_OF_MEMORY
 *
 * unicharPtr = buffer.get() + initialLength;
 *
 * //continue to add PRUnichar's....
 */

template <class T, PRInt32 sz>
class nsAutoBuffer
{
public:
  nsAutoBuffer() :
    mBufferPtr(mStackBuffer),
    mCurElemCapacity(sz)
  {
  }

  ~nsAutoBuffer()
  {
    if (mBufferPtr != mStackBuffer)
      nsMemory::Free(mBufferPtr);
  }

  PRBool EnsureElemCapacity(PRInt32 inElemCapacity)
  {
    if (inElemCapacity <= mCurElemCapacity)
      return PR_TRUE;
    
    T* newBuffer;

    if (mBufferPtr != mStackBuffer)
      newBuffer = (T*)nsMemory::Realloc((void *)mBufferPtr, inElemCapacity * sizeof(T));
    else 
      newBuffer = (T*)nsMemory::Alloc(inElemCapacity * sizeof(T));

    if (!newBuffer)
      return PR_FALSE;

    if (mBufferPtr != mStackBuffer)
      nsMemory::Free(mBufferPtr);

    mBufferPtr = newBuffer; 
    mCurElemCapacity = inElemCapacity;
    return PR_TRUE;
  }

  PRBool AddElemCapacity(PRInt32 inElemCapacity)
  {
    return EnsureElemCapacity(mCurElemCapacity + inElemCapacity);
  }

  T*          get()             const  { return mBufferPtr; }
  PRInt32     GetElemCapacity() const  { return mCurElemCapacity;  }

protected:

  T             *mBufferPtr;
  T             mStackBuffer[sz];
  PRInt32       mCurElemCapacity;
};

#endif // nsAutoBuffer_h__
