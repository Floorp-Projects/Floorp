/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chak Nanga <chak@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsViewSourceHandler_h___
#define nsViewSourceHandler_h___

#include "nsIProtocolHandler.h"
#include "nsCOMPtr.h"
#include "nsSimpleURI.h"
#include "nsINestedURI.h"

class nsViewSourceHandler : public nsIProtocolHandler
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPROTOCOLHANDLER
};

#define NS_VIEWSOURCEURI_CID                             \
{ /* 2545766f-3a27-4fd1-8e88-b0886d346242 */             \
     0x2545766f,                                         \
     0x3a27,                                             \
     0x4fd1,                                             \
     { 0x8e, 0x88, 0xb0, 0x88, 0x6d, 0x34, 0x62, 0x42 }  \
}

class nsViewSourceURI : public nsSimpleURI,
                        public nsINestedURI
{
public:
  nsViewSourceURI(nsIURI* innerURI)
    : nsSimpleURI(nsnull),
      mInnerURI(innerURI)
  {
    NS_ASSERTION(innerURI, "Must have inner URI");
  }

  // To be used by deserialization only
  nsViewSourceURI()
    : nsSimpleURI(nsnull)
  {
  }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSINESTEDURI

  // Overrides for various methods nsSimpleURI implements follow.
  
  // nsIURI overrides
  NS_IMETHOD Equals(nsIURI* other, PRBool *result);
  virtual nsSimpleURI* StartClone();

  // nsISerializable overrides -- we can use the same Write(), but we
  // need a different Read().
  NS_IMETHOD Read(nsIObjectInputStream* aStream);

  // Override the nsIClassInfo method GetClassIDNoAlloc to make sure our
  // nsISerializable impl works right.
  NS_IMETHOD GetClassIDNoAlloc(nsCID *aClassIDNoAlloc);  

protected:
  nsCOMPtr<nsIURI> mInnerURI;  
};                        

#endif /* !defined( nsViewSourceHandler_h___ ) */
