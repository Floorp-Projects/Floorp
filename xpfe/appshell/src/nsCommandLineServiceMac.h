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

// Special stuff for the Macintosh implementation of command-line service.

#ifndef nsCommandLineServiceMac_h_
#define nsCommandLineServiceMac_h_

#include <Files.h>

#include "nscore.h"
#include "nsError.h"
#include "nsString.h"

#include "nsAEDefs.h"

#ifdef __cplusplus

class nsMacCommandLine
{
public:


  enum
  {
    kMaxTokens      = 20  
  };

                  nsMacCommandLine();
                  ~nsMacCommandLine();

  nsresult        Initialize(int& argc, char**& argv);
  
  nsresult        AddToCommandLine(const char* inArgText);
  nsresult        AddToCommandLine(const char* inOptionString, const FSSpec& inFileSpec);
  nsresult        AddToEnvironmentVars(const char* inArgText);

  OSErr           HandleOpenOneDoc(const FSSpec& inFileSpec, OSType inFileType);
  OSErr           HandlePrintOneDoc(const FSSpec& inFileSpec, OSType fileType);

	OSErr						DispatchURLToNewBrowser(const char* url);
	  
  OSErr						Quit(TAskSave askSave);
  
protected:

  nsresult        OpenWindow(const char *chrome, const PRUnichar *url);
  
  nsCString       mTempArgsString;    // temp storage for args as we accumulate them
  char*           mArgsBuffer;        // final, immutable container for args
  
  char**          mArgs;              // array of pointers into argBuffer

  PRBool          mStartedUp;

public:

  static nsMacCommandLine& GetMacCommandLine() { return sMacCommandLine; }
  
private:

  static nsMacCommandLine sMacCommandLine;
  
};

#endif    //__cplusplus


#ifdef __cplusplus
extern "C" {
#endif

nsresult InitializeMacCommandLine(int& argc, char**& argv);

#ifdef __cplusplus
}
#endif


#endif // nsCommandLineServiceMac_h_
