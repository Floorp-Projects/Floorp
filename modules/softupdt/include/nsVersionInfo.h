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

#ifndef nsVersionInfo_h__
#define nsVersionInfo_h__

#include "prtypes.h"

#include "nsSoftUpdateEnums.h"
#include "nsVersionInfo.h"

PR_BEGIN_EXTERN_C

struct nsVersionInfo {

public:

  /* Public Methods */

  nsVersionInfo(PRInt32 maj, PRInt32 min, PRInt32 rel, PRInt32 bld);
  nsVersionInfo(char* version);
  ~nsVersionInfo();

  /* Text representation of the version info */
  char* toString();

  /*
   * compareTo() -- Compares version info.
   * Returns -n, 0, n, where n = {1-4}
   */
  nsVersionEnum compareTo(nsVersionInfo* vi);

  nsVersionEnum compareTo(char* version);

  nsVersionEnum compareTo(PRInt32 maj, PRInt32 min, PRInt32 rel, PRInt32 bld);


private:

  /* Private Fields */

  /* Equivalent to Version Registry structure fields */
  PRInt32 major;
  PRInt32 minor;
  PRInt32 release;
  PRInt32 build;

  /* Private Methods */

};

PR_END_EXTERN_C

#endif /* nsVersionInfo_h__ */
