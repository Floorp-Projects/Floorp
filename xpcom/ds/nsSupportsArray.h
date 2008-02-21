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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef nsSupportsArray_h__
#define nsSupportsArray_h__

//#define DEBUG_SUPPORTSARRAY 1

#include "nsISupportsArray.h"

static const PRUint32 kAutoArraySize = 8;

// Set IMETHOD_VISIBILITY to empty so that the class-level NS_COM declaration
// controls member method visibility.
#undef  IMETHOD_VISIBILITY
#define IMETHOD_VISIBILITY

class NS_COM nsSupportsArray : public nsISupportsArray {
public:
  nsSupportsArray(void);
  ~nsSupportsArray(void); // nonvirtual since we're not subclassed

  static NS_METHOD
  Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

  NS_DECL_ISUPPORTS

  NS_DECL_NSISERIALIZABLE

  // nsICollection methods:
  NS_IMETHOD Count(PRUint32 *result) { *result = mCount; return NS_OK; }
  NS_IMETHOD GetElementAt(PRUint32 aIndex, nsISupports* *result) {
    *result = ElementAt(aIndex);
    return NS_OK;
  }
  NS_IMETHOD QueryElementAt(PRUint32 aIndex, const nsIID & aIID, void * *aResult) {
    if (aIndex < mCount) {
      nsISupports* element = mArray[aIndex];
      if (nsnull != element)
        return element->QueryInterface(aIID, aResult);
    }
    return NS_ERROR_FAILURE;
  }
  NS_IMETHOD SetElementAt(PRUint32 aIndex, nsISupports* value) {
    return ReplaceElementAt(value, aIndex) ? NS_OK : NS_ERROR_FAILURE;
  }
  NS_IMETHOD AppendElement(nsISupports *aElement) {
    return InsertElementAt(aElement, mCount)/* ? NS_OK : NS_ERROR_FAILURE*/;
  }
  // XXX this is badly named - should be RemoveFirstElement
  NS_IMETHOD RemoveElement(nsISupports *aElement) {
    return RemoveElement(aElement, 0)/* ? NS_OK : NS_ERROR_FAILURE*/;
  }
  NS_IMETHOD_(PRBool) MoveElement(PRInt32 aFrom, PRInt32 aTo);
  NS_IMETHOD Enumerate(nsIEnumerator* *result);
  NS_IMETHOD Clear(void);

  // nsISupportsArray methods:
  NS_IMETHOD_(PRBool) Equals(const nsISupportsArray* aOther);

  NS_IMETHOD_(nsISupports*) ElementAt(PRUint32 aIndex);

  NS_IMETHOD_(PRInt32) IndexOf(const nsISupports* aPossibleElement);
  NS_IMETHOD_(PRInt32) IndexOfStartingAt(const nsISupports* aPossibleElement,
                                         PRUint32 aStartIndex = 0);
  NS_IMETHOD_(PRInt32) LastIndexOf(const nsISupports* aPossibleElement);

  NS_IMETHOD GetIndexOf(nsISupports *aPossibleElement, PRInt32 *_retval) {
    *_retval = IndexOf(aPossibleElement);
    return NS_OK;
  }
  
  NS_IMETHOD GetIndexOfStartingAt(nsISupports *aPossibleElement,
                                  PRUint32 aStartIndex, PRInt32 *_retval) {
    *_retval = IndexOfStartingAt(aPossibleElement, aStartIndex);
    return NS_OK;
  }
  
  NS_IMETHOD GetLastIndexOf(nsISupports *aPossibleElement, PRInt32 *_retval) {
    *_retval = LastIndexOf(aPossibleElement);
    return NS_OK;
  }
  
  NS_IMETHOD_(PRBool) InsertElementAt(nsISupports* aElement, PRUint32 aIndex);

  NS_IMETHOD_(PRBool) ReplaceElementAt(nsISupports* aElement, PRUint32 aIndex);

  NS_IMETHOD_(PRBool) RemoveElementAt(PRUint32 aIndex) {
    return RemoveElementsAt(aIndex,1);
  }
  NS_IMETHOD_(PRBool) RemoveElement(const nsISupports* aElement, PRUint32 aStartIndex = 0);
  NS_IMETHOD_(PRBool) RemoveLastElement(const nsISupports* aElement);

  NS_IMETHOD DeleteLastElement(nsISupports *aElement) {
    return (RemoveLastElement(aElement) ? NS_OK : NS_ERROR_FAILURE);
  }
  
  NS_IMETHOD DeleteElementAt(PRUint32 aIndex) {
    return (RemoveElementAt(aIndex) ? NS_OK : NS_ERROR_FAILURE);
  }
  
  NS_IMETHOD_(PRBool) AppendElements(nsISupportsArray* aElements) {
    return InsertElementsAt(aElements,mCount);
  }
  
  NS_IMETHOD Compact(void);

  NS_IMETHOD_(PRBool) EnumerateForwards(nsISupportsArrayEnumFunc aFunc, void* aData);
  NS_IMETHOD_(PRBool) EnumerateBackwards(nsISupportsArrayEnumFunc aFunc, void* aData);

  NS_IMETHOD Clone(nsISupportsArray **_retval);

  NS_IMETHOD_(PRBool) InsertElementsAt(nsISupportsArray *aOther, PRUint32 aIndex);

  NS_IMETHOD_(PRBool) RemoveElementsAt(PRUint32 aIndex, PRUint32 aCount);

  NS_IMETHOD_(PRBool) SizeTo(PRInt32 aSize);
protected:
  void DeleteArray(void);

  NS_IMETHOD_(PRBool) GrowArrayBy(PRInt32 aGrowBy);

  nsISupports** mArray;
  PRUint32 mArraySize;
  PRUint32 mCount;
  nsISupports*  mAutoArray[kAutoArraySize];
#if DEBUG_SUPPORTSARRAY
  PRUint32 mMaxCount;
  PRUint32 mMaxSize;
#endif

private:
  // Copy constructors are not allowed
  nsSupportsArray(const nsISupportsArray& other);
};

#endif // nsSupportsArray_h__
