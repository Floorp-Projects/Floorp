/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#ifndef nsICmdLineervice_h__
#define nsICmdLineService_h__

#include "nsISupports.h"
#include "nsString.h"

/* Forward declarations... */
class nsIFactory;


// e34783f4-ac08-11d2-8d19-00805fc2500c
#define NS_ICOMMANDLINE_SERVICE_IID \
{  0xe34783f4, 0xac08, 0x11d2, \
  {0x8d, 0x19, 0x00, 0x80, 0x5f, 0xc2, 0x50,0xc} }

// e34783f5-ac08-11d2-8d19-00805fc2500c
#define NS_COMMANDLINE_SERVICE_CID \
{  0xe34783f5, 0xac08, 0x11d2, \
  {0x8d, 0x19, 0x00, 0x80, 0x5f, 0xc2, 0x50,0xc} }


class nsICmdLineService : public nsISupports
{
public:

  NS_IMETHOD Initialize(PRInt32 aArgc, char ** aArgv) = 0;
  NS_IMETHOD GetCmdLineValue(char * aArg, char ** aValue) = 0;
  NS_IMETHOD GetURLToLoad(char ** aResult) = 0;
  NS_IMETHOD GetProgramName(char ** aResult) = 0;
  NS_IMETHOD GetArgc(PRInt32 *  aResult) = 0;
  NS_IMETHOD GetArgv(char ** aResult[]) = 0;

};


extern "C" NS_APPSHELL nsresult
NS_NewCmdLineServiceFactory(nsIFactory** aFactory);

#endif /* nsICmdLineService_h__ */
