/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2
-*- 
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

/**
 * This class manages a string in one of the following forms:
 *
 *    protocol://host.domain[:port]/user[:extra]
 *    mailto:user@host.domain
 *
 * It provides quick access to its component parts.
 * Expected protocols are:  CAPI, IRIP, MAILTO
 *
 * This code probably exists somewhere else.
 *
 * sman
 *
 * Modification:  Code supports converting from "file:" and "resource:"
 * to platform specific file handle.
 *
 */

#include "nscore.h"

class nsUrlParser
{
private:
  nsString m_sURL;

public:
  nsUrlParser(char* psCurl);
  nsUrlParser(nsString& sCurl);

  virtual ~nsUrlParser();

  NS_METHOD_(char *)  ToLocalFile();
  NS_METHOD_(PRBool)  IsLocalFile();
  NS_IMETHOD_(char *) LocalFileToURL();

private:
  NS_METHOD_(char *) ResourceToFile();
};




