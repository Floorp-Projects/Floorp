/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsRadioGroup.h"
#include "nsToolkit.h"
#include "nsStringUtil.h"


class nsStringHashKey : public nsHashKey {
public:
  nsStringHashKey(const nsString & aKey);
  ~nsStringHashKey();

  PRBool      operator==(const nsStringHashKey& aOther) const;
  PRBool      Equals(const nsHashKey* aOther) const;
  PRUint32    HashValue(void) const;
  nsHashKey*  Clone(void) const;

private:
  nsStringHashKey();
  nsStringHashKey(const nsStringHashKey& aCopy);
  nsStringHashKey& operator=(const nsStringHashKey& aCopy);

protected:
  nsString mKey;
  PRUint32 mHashValid;
  PRUint32 mHashValue;

};

nsStringHashKey::nsStringHashKey(const nsString & aKey)
{
  mKey.SetLength(0);
  mKey.Append(aKey);
  mHashValid = 0;
}

nsStringHashKey::nsStringHashKey(const nsStringHashKey& aCopy)
{
  mKey.SetLength(0);
  mKey.Append(aCopy.mKey);
  mHashValid = aCopy.mHashValid;
  mHashValue = aCopy.mHashValue;
}

nsStringHashKey::~nsStringHashKey()
{
}

PRBool nsStringHashKey::operator==(const nsStringHashKey& aOther) const
{
  return Equals(&aOther);
}

PRBool nsStringHashKey::Equals(const nsHashKey* aOther) const
{
  PRBool  result = PR_TRUE;

  const nsStringHashKey* other = (nsStringHashKey*)aOther;

  if (nsnull != other && other != this) {
    result = mKey.Equals(other->mKey);
  }
  return result;
}

nsHashKey* nsStringHashKey::Clone(void) const
{
  return new nsStringHashKey(*this);
}

PRUint32 nsStringHashKey::HashValue(void) const
{
  if (0 == mHashValid) {
    ((nsStringHashKey*)this)->mHashValue = 0;

    NS_ALLOC_STR_BUF(val, mKey, 256)
	  PRUint32 hash = 0;
	  PRUint32 off  = 0;
	  PRUint32 len  = mKey.Length();

	  if (len < 16) {
 	    for (PRUint32 i = len ; i > 0; i--) {
 		    hash = (hash * 37) + val[off++];
 	    }
 	  } else { // only sample some characters
 	    PRUint32 skip = len / 8;
 	    for (PRUint32 i = len ; i > 0; i -= skip, off += skip) {
 		    hash = (hash * 39) + val[off];
 	    }
 	  }
    NS_FREE_STR_BUF(val)
    ((nsStringHashKey*)this)->mHashValue = hash;//^= (hash & 0x7FFFFFFF);
    ((nsStringHashKey*)this)->mHashValid = 1;
  }
  return mHashValue;
}



nsHashtable * nsRadioGroup::mRadioGroupHashtable = new nsHashtable(8);

PRBool HashtableEnum(nsHashKey *aKey, void *aData) {
  printf("HashtableEnum %s\n", aKey);
  return PR_TRUE;
}

//-------------------------------------------------------------------------
//
// nsRadioGroup static methods
//
//-------------------------------------------------------------------------
nsRadioGroup * nsRadioGroup::getNamedRadioGroup(const nsString & aName) {
  nsStringHashKey key(aName);
  nsRadioGroup * group = (nsRadioGroup *)mRadioGroupHashtable->Get((nsHashKey *)&key);
  return group;
}

NS_EXPORT nsIRadioGroup * NS_GetRadioGroup(const nsString &aName) {
  return nsRadioGroup::getNamedRadioGroup(aName);
}


//-------------------------------------------------------------------------
//
// nsRadioGroup constructor
//
//-------------------------------------------------------------------------
nsRadioGroup::nsRadioGroup(nsISupports *aOuter) : nsObject(aOuter)
{
  mChildren = NULL;
}


//-------------------------------------------------------------------------
//
// nsRadioGroup destructor
//
//-------------------------------------------------------------------------
nsRadioGroup::~nsRadioGroup()
{
    if (mChildren) {
        mChildren = NULL;
    }
}


//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsresult nsRadioGroup::QueryObject(const nsIID& aIID, void** aInstancePtr)
{
    nsresult result = nsObject::QueryObject(aIID, aInstancePtr);

    static NS_DEFINE_IID(kInsRadioGroupIID, NS_IRADIOGROUP_IID);
    if (result == NS_NOINTERFACE && aIID.Equals(kInsRadioGroupIID)) {
        *aInstancePtr = (void*) ((nsIRadioGroup*)this);
        AddRef();
        result = NS_OK;
    }

    return result;

}


//-------------------------------------------------------------------------
//
// Gives a name to this group of radio buttons
//
//-------------------------------------------------------------------------
void nsRadioGroup::SetName(const nsString &aName)
{
  mName.SetLength(0);
  mName.Append(aName);
  mRadioGroupHashtable->Put(new nsStringHashKey(aName), this);
}


//-------------------------------------------------------------------------
//
// Adds a nsRadioButton to this group
//
//-------------------------------------------------------------------------
void nsRadioGroup::Clicked(nsIRadioButton * aChild) 
{
	if (mChildren) {
		nsIEnumerator  * enumerator = GetChildren();
		nsIRadioButton * child      = (nsIRadioButton*)enumerator->Next();
		while (child) {
		  child->SetStateNoNotify((PRBool)(child == aChild));
      NS_RELEASE(child);
			child = (nsIRadioButton*)enumerator->Next();
		}
    NS_RELEASE(enumerator);
	}
}

//-------------------------------------------------------------------------
//
// Adds a nsRadioButton to this group
//
//-------------------------------------------------------------------------
void nsRadioGroup::Add(nsIRadioButton * aChild) 
{
    if (!mChildren) {
      mChildren = new Enumerator();
    }

    mChildren->Append(aChild);

}

//-------------------------------------------------------------------------
//
// Removes a nsRadioButton from this group
//
//-------------------------------------------------------------------------
void nsRadioGroup::Remove(nsIRadioButton * aChild) 
{
	if (mChildren) {
        mChildren->Remove(aChild);
    }

}

//-------------------------------------------------------------------------
//
// Get this window's list of children
//
//-------------------------------------------------------------------------
nsIEnumerator* nsRadioGroup::GetChildren()
{
    if (mChildren) {
        mChildren->Reset();
        mChildren->AddRef();
        return mChildren;
    }

    return NULL;
}


//-------------------------------------------------------------------------
//
// Constructor
//
//-------------------------------------------------------------------------
#define INITIAL_SIZE        2

nsRadioGroup::Enumerator::Enumerator()
{
    mRefCnt = 1;
    mArraySize = INITIAL_SIZE;
    mChildrens = (nsIRadioButton**)new DWORD[mArraySize];
    memset(mChildrens, 0, sizeof(DWORD) * mArraySize);
    mCurrentPosition = 0;
}


//-------------------------------------------------------------------------
//
// Destructor
//
//-------------------------------------------------------------------------
nsRadioGroup::Enumerator::~Enumerator()
{
    if (mChildrens) {
        for (int i = 0; mChildrens[i] && i < mArraySize; i++) {
            NS_RELEASE(mChildrens[i]);
        }

        delete[] mChildrens;
    }
}

//
// The evil triad
//
NS_IMPL_ISUPPORTS(nsRadioGroup::Enumerator, NS_IENUMERATOR_IID);

//-------------------------------------------------------------------------
//
// Get enumeration next element. Return null at the end
//
//-------------------------------------------------------------------------
nsISupports* nsRadioGroup::Enumerator::Next()
{
    if (mCurrentPosition < mArraySize && mChildrens[mCurrentPosition]) {
        mChildrens[mCurrentPosition]->AddRef();
        return mChildrens[mCurrentPosition++];
    }

    return NULL;
}


//-------------------------------------------------------------------------
//
// Reset enumerator internal pointer to the beginning
//
//-------------------------------------------------------------------------
void nsRadioGroup::Enumerator::Reset()
{
    mCurrentPosition = 0;
}


//-------------------------------------------------------------------------
//
// Append an element 
//
//-------------------------------------------------------------------------
void nsRadioGroup::Enumerator::Append(nsIRadioButton* aRadioButton)
{
    NS_PRECONDITION(aRadioButton, "Adding a null radio button to a radio group");
    if (aRadioButton) {
        int pos;
        for (pos = 0; pos < mArraySize && mChildrens[pos]; pos++);
        if (pos == mArraySize) {
            GrowArray();
        }
        mChildrens[pos] = aRadioButton;
        aRadioButton->AddRef();
    }
}


//-------------------------------------------------------------------------
//
// Remove an element 
//
//-------------------------------------------------------------------------
void nsRadioGroup::Enumerator::Remove(nsIRadioButton* aRadioButton)
{
    int pos;
    for(pos = 0; mChildrens[pos] && (mChildrens[pos] != aRadioButton); pos++);
    if (mChildrens[pos] == aRadioButton) {
        NS_RELEASE(aRadioButton);
        memcpy(mChildrens + pos, mChildrens + pos + 1, mArraySize - pos - 1);
    }

}


//-------------------------------------------------------------------------
//
// Grow the size of the children array
//
//-------------------------------------------------------------------------
void nsRadioGroup::Enumerator::GrowArray()
{
    mArraySize <<= 1;
    nsIRadioButton **newArray = (nsIRadioButton**)new DWORD[mArraySize];
    memset(newArray, 0, sizeof(DWORD) * mArraySize);
    memcpy(newArray, mChildrens, (mArraySize>>1) * sizeof(DWORD));
    mChildrens = newArray;
}


