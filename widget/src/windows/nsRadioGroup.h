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

/**
 * Native radio button manager
 */

class nsRadioGroup : public nsObject,
                     public nsIRadioGroup
{

public:
                            nsRadioGroup();
    virtual                 ~nsRadioGroup();

    NS_DECL_ISUPPORTS

    
    // nsIRadioGroup Interface
    NS_IMETHOD Add(nsIRadioButton * aRadioBtn);
    NS_IMETHOD Remove(nsIRadioButton * aRadioBtn);
    NS_IMETHOD SetName(const nsString &aName);
    NS_IMETHOD Clicked(nsIRadioButton * aChild);
    virtual nsIEnumerator* GetChildren();

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
        virtual ~Enumerator();
        
        virtual nsresult First();
        virtual nsresult Last();
        virtual nsresult Next();
        virtual nsresult Prev();
        virtual nsresult CurrentItem(nsISupports **aItem);
        virtual nsresult IsDone();

        void Append(nsIRadioButton* aWidget);
        void Remove(nsIRadioButton* aWidget);

    private:
        void GrowArray();

    } *mChildren;

};

#endif // nsRadioGroup_h__
