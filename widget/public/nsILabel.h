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

#ifndef nsILabel_h__
#define nsILabel_h__

#include "nsIWidget.h"
#include "nsString.h"

/* F3131891-3DC7-11d2-8DB8-00609703C14E */
#define NS_ILABEL_IID \
{ 0xf3131891, 0x3dc7, 0x11d2, \
    { 0x8d, 0xb8, 0x0, 0x60, 0x97, 0x3, 0xc1, 0x4e } }

/**
 * Label Alignments 
 */

enum nsLabelAlignment {   
    eAlign_Right,
    eAlign_Left,
    eAlign_Center
  }; 

struct nsLabelInitData : public nsWidgetInitData {
  nsLabelInitData()
    : mAlignment(eAlign_Left)
  {
  }

  nsLabelAlignment mAlignment;
};

/**
 * Label widget.
 * Automatically shows itself as depressed when clicked on.
 */
class nsILabel : public nsISupports {

  public:
 
    /**
    * Set the label
    *
    * @param  Set the label to aText
    * @result NS_Ok if no errors
    */
  
    NS_IMETHOD SetLabel(const nsString &aText) = 0;
    
   /**
    * Get the button label
    *
    * @param aBuffer contains label upon return
    * @result NS_Ok if no errors
    */
 
    NS_IMETHOD GetLabel(nsString &aBuffer) = 0;


   /**
    * Set the Label Alignemnt for creation
    *
    * @param aAlignment the alignment
    * @result NS_Ok if no errors
    */
    NS_IMETHOD SetAlignment(nsLabelAlignment aAlignment) = 0;

};

#endif
