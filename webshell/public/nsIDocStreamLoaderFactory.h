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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
#ifndef nsIDocStreamLoaderFactory_h___
#define nsIDocStreamLoaderFactory_h___

// Wrapping includes can speed up compiles (see "Large Scale C++
// Software Design")
#ifndef nsISupports_h___
#include "nsISupports.h"
#endif

class nsIInputStream;
class nsIContentViewer;

/* 9188bc80-f92e-11d2-81ef-0060083a0bcf */
#define NS_IDOCSTREAMLOADERFACTORY_IID \
 { 0x9188bc80, 0xf92e, 0x11d2,{0x81, 0xef, 0x00, 0x60, 0x08, 0x3a, 0x0b, 0xcf}}

class nsIDocStreamLoaderFactory : public nsISupports
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IDOCSTREAMLOADERFACTORY_IID)

  NS_IMETHOD CreateInstance(nsIInputStream& aInputStream,
			    const char* aContentType,
			    const char* aCommand,
			    nsISupports* aContainer,
			    nsISupports* aExtraInfo,
			    nsIContentViewer** aDocViewer) = 0;
};

#endif /* !defined(nsIDocStreamLoaderFactory_h___) */
