/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#ifndef nsWinRegValue_h__
#define nsWinRegValue_h__

#include "prtypes.h"


PR_BEGIN_EXTERN_C

struct nsWinRegValue {

public:

  /* Public Fields */
  PRInt32 type;
  void* data;
  PRInt32 data_length;

  /* Public Methods */
  nsWinRegValue(PRInt32 datatype, void* regdata, PRInt32 len) 
	{type = datatype; data = regdata; data_length = len;} 
  
  /* should we copy the regdata? */
private:
  
  /* Private Fields */
  

  /* Private Methods */

};

PR_END_EXTERN_C

#endif /* nsWinRegValue_h__ */
