/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_ILABEL_IID)
 
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
