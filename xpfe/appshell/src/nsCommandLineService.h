/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef __nsCommandLineService_h
#define __nsCommandLineService_h

#include "nsISupports.h"
#include "nsVoidArray.h"

class nsCmdLineService : public nsICmdLineService
{
public:
  nsCmdLineService(void);

  NS_DECL_ISUPPORTS
  NS_DECL_NSICMDLINESERVICE

protected:
  virtual ~nsCmdLineService();

  nsVoidArray    mArgList;      // The arguments
  nsVoidArray    mArgValueList; // The argument values
  PRInt32        mArgCount; // This is not argc. This is # of argument pairs 
                            // in the arglist and argvaluelist arrays. This
                            // normally is argc/2.
  PRInt32        mArgc;     // This is argc;
  char **        mArgv;     // This is argv;

  PRBool ArgsMatch(const char *lookingFor, const char *userGave);
};

#endif
