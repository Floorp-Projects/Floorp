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
#ifndef nsIRadioGroup_h__
#define nsIRadioGroup_h__

#include "nsString.h"

#define NS_IRADIOGROUP_IID    \
{ 0x961085f1, 0xbd28, 0x11d1, \
{ 0x97, 0xef, 0x0, 0x60, 0x97, 0x3, 0xc1, 0x4e } }


class nsIRadioGroup;
class nsIRadioButton;
class nsIEnumerator;

/**
 * Help class for implementing a "group" of radio buttons
 * 
 */

class nsIRadioGroup : public nsISupports 
{

public:

    /**
     * Adds a RadioButton to the group
     * @param aRadioBtn the radio button to be added
     *
     */
    virtual void Add(nsIRadioButton * aRadioBtn) = 0;

    /**
     * Removes a RadioButton from the group
     * @param aRadioBtn the radio button to be removed
     *
     */
    virtual void Remove(nsIRadioButton * aRadioBtn) = 0;

    /**
     * Setd the name of the RadioGroup
     * @param aName The new name of the radio group
     *
     */
    virtual void SetName(const nsString &aName) = 0;

    /**
     * Tells the RadioGroup that a child RadioButton has been clicked and it should set 
     * the approproate state on the other buttons
     * @param aChild The RadioButton that was clicked
     *
     */
    virtual void Clicked(nsIRadioButton * aChild) = 0;

    /**
     * Gets the enumeration of children
     * @param nsIEnumerator* The enumeration of children (radio buttons) in the RadioGroup
     *
     */
    virtual nsIEnumerator* GetChildren() = 0;
};

#endif  // nsIRadioGroup_h__

