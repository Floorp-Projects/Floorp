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

#ifndef nsStack_h___
#define nsStack_h___

#include "nsIStack.h"

#include "nsVoidArray.h"

class nsStack : public nsIStack
{

public:
  nsStack();

  NS_DECL_ISUPPORTS

  NS_IMETHOD                  Init();

  NS_IMETHOD_(PRBool)         Empty();
  NS_IMETHOD                  Push(nsComponent aComponent);
  NS_IMETHOD_(nsComponent)    Pop();
  NS_IMETHOD_(nsComponent)    Top();

protected:
  ~nsStack();

private:
  nsVoidArray * mVoidArray ;

};

#endif /* nsStack_h___ */
