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
#ifndef NS_TAGHANDLER___
#define NS_TAGHANDLER___


#include "nsDTDUtils.h"
#include "nsString.h"

/**
 * MODULE NOTES:
 * @update  dc 11/20/98
 **/


/**
 *  This class defines an object that can do special tag handling
 *  
 *  @update  DC 11/20/98
 */
class nsTagHandler : public nsITagHandler {

public:
  nsAutoString  mTheTagName;  //ok as autostring; VERY few if any get used.

public:

  /**
   * Constructor
   * @update dc 11/05/98
   */
  nsTagHandler() {}


  /**
   * Destructor
   * @update dc 11/05/98
   */
  virtual ~nsTagHandler() {}

  /**
   * Sets the string (tag) for this nsTagHandler to handle
   * @update dc 11/19/98
   * @param aTheString -- The string (tag) associated with this handler
   * @return VOID
   */
  void SetString(const nsString &aTheString) {mTheTagName.Assign(aTheString);}

  /**
   * Returns the string (tag) handled by this nsTagHandler
   * @update dc 11/19/98
   * @return The tagname associated with this class
   */
  nsString* GetString() {return &mTheTagName;}

  /**
   *  Handle this tag prior to the DTD
   *  @update  dc 11/5/98
   *  @return	 A boolean indicating if this token was handled here
   */
  virtual PRBool  HandleToken(CToken* aToken,nsIDTD* aDTD) {return PR_FALSE;};

  /**
   *  Handle this tag prior to the DTD
   *  @update  dc 11/5/98
   *  @return	 A boolean indicating if this token was handled here
   */
  virtual PRBool  HandleCapturedTokens(CToken* aToken,nsIDTD* aDTD) {return PR_FALSE;};

};


/**
 *  This class defines an object that describes the baseline group that a given
 *  element belongs to.
 *  
 *  @update  rgess 1/15/00
 */
class nsElementGroup {
public:
  nsElementGroup(PRInt32 aGroupBits) {
    mGroupBits=aGroupBits;
  }

  nsElementGroup(const nsElementGroup& aGroup) {
    mGroupBits=aGroup.mGroupBits;
  }

  nsElementGroup& operator=(const nsElementGroup& aGroup) {
    mGroupBits=aGroup.mGroupBits;
    return *this;
  }

  ~nsElementGroup() {
    mGroupBits=0;
  }

private:
  PRInt32 mGroupBits;
};


/**
 *  This class defines an object that describes the each element (tag).
 *  
 *  @update  rgess 1/15/00
 */
class nsElement {
public:
  nsElement() {
    mGroup=0;
  }
  
  nsElement(const nsElement& aGroup){
    mGroup=aGroup.mGroup;
  }
  
  nsElement& operator=(const nsElement& aGroup) {
    mGroup=aGroup.mGroup;
    return *this;
  }

  ~nsElement() {
    mGroup=0;
  }

private:
  nsElementGroup* mGroup;
};


#endif 
