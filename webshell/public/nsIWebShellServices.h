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
#ifndef nsIWebShellServices_h___
#define nsIWebShellServices_h___

#include "nsISupports.h"
#include "nsIParser.h"

// Interface ID for nsIWebShellServices

/* 8b26a346-031e-11d3-aeea-00108300ff91 */
#define NS_IWEB_SHELL_SERVICES_IID \
{ 0x8b26a346, 0x031e, 0x11d3, {0xae, 0xea, 0x00, 0x10, 0x83, 0x00, 0xff, 0x91} }


//----------------------------------------------------------------------

class nsIWebShellServices : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IWEB_SHELL_SERVICES_IID; return iid; }

  NS_IMETHOD LoadDocument(const char* aURL, 
                          const char* aCharset= nsnull , 
                          nsCharsetSource aSource = kCharsetUninitialized) = 0;
  NS_IMETHOD ReloadDocument(const char* aCharset = nsnull , 
                            nsCharsetSource aSource = kCharsetUninitialized) = 0;
  NS_IMETHOD StopDocumentLoad(void) = 0;
  NS_IMETHOD SetRendering(PRBool aRender) = 0;

};

#endif /* nsIWebShellServices_h___ */
