/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
#ifndef NS_TAGHANDLER___
#define NS_TAGHANDLER___


#include "nsDTDUtils.h"
#include "nsString.h"

/**
 * MODULE NOTES:
 * @update  DC 11/5/98
 **/


/**
 *  This class defines the object that can do special tag handling
 *  
 *  @update  DC 11/5/98
 */
class nsTagHandler : public nsITagHandler {

// MEMBERS
public:
  nsAutoString  mTheTagName;


// METHODS
public:

  /**
   * Constructor
   * @update dc 11/5/98
   */
  nsTagHandler() {}

  ~nsTagHandler() {}

  /**
   * SetString
   */
  void SetString(nsAutoString *aTheString) {}

  nsAutoString* GetString() {return 0;}

  /**
   *  Handle this token prior to the DTD
   *  @update  dc 11/5/98
   */
  virtual PRBool  HandleToken(CToken* aToken,nsIDTD* aDTD) {return PR_FALSE;};

  /**
   *  Handle this token prior to the DTD
   *  @update  dc 11/5/98
   *  @return	 ptr to previously set contentsink (usually null)  
   */
  virtual PRBool  HandleCapturedTokens(CToken* aToken,nsIDTD* aDTD) {return PR_FALSE;};

};

#endif 
