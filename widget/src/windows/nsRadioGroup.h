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

#ifndef nsRadioGroup_h__
#define nsRadioGroup_h__

#include "nsdefs.h"
#include "nsWindow.h"
#include "nsSwitchToUIThread.h"

#include "nsIRadioGroup.h"
#include "nsRadioButton.h"
#include "nsHashtable.h"

class nsRadioGroup : public nsObject,
                     public nsIRadioGroup
{

public:
                            nsRadioGroup(nsISupports *aOuter);
    virtual                 ~nsRadioGroup();

    // nsISupports. Forward to the nsObject base class
    BASE_SUPPORT

    virtual nsresult        QueryObject(const nsIID& aIID, void** aInstancePtr);

    // nsIRadioGroup part
    virtual void            Add(nsIRadioButton * aChild);
    virtual void            Remove(nsIRadioButton * aChild);

    virtual void            SetName(const nsString &aName);

    virtual void            Clicked(nsIRadioButton * aChild);
    virtual nsIEnumerator*  GetChildren();

    static nsRadioGroup *   getNamedRadioGroup(const nsString & aName);

protected:
    nsString mName;

    static nsHashtable * mRadioGroupHashtable;
    //static PRBool HashtableEnum(nsHashKey *aKey, void *aData);


    // keep the list of children
    class Enumerator : public nsIEnumerator {
        nsIRadioButton   **mChildrens;
        int  mCurrentPosition;
        int  mArraySize;

    public:
        NS_DECL_ISUPPORTS

        Enumerator();
        ~Enumerator();

        NS_IMETHOD_(nsISupports*) Next();
        NS_IMETHOD_(void) Reset();

        void Append(nsIRadioButton* aWidget);
        void Remove(nsIRadioButton* aWidget);

    private:
        void GrowArray();

    } *mChildren;

};

#endif // nsRadioGroup_h__
