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

#ifndef nsIParserService_h__
#define nsIParserService_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsHTMLTags.h"
#include "nsIParserNode.h"
#include "nsIParser.h"
#include "nsVoidArray.h"
#include "nsIElementObserver.h"

#define NS_IPARSERSERVICE_IID                           \
{ 0xa6cf9111, 0x15b3, 0x11d2,                           \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } }

// {78081E70-AD53-11d5-8498-0010A4E0C706}
#define NS_IOBSERVERENTRY_IID \
{ 0x78081e70, 0xad53, 0x11d5, { 0x84, 0x98, 0x0, 0x10, 0xa4, 0xe0, 0xc7, 0x6 } };


class nsIObserverEntry : public nsISupports {
 public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IOBSERVERENTRY_IID)

  NS_IMETHOD Notify(nsIParserNode* aNode,
                    nsIParser* aParser,
                    nsISupports* aWebShell) = 0;

};


class nsIParserService : public nsISupports {
 public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IPARSERSERVICE_IID)

  NS_IMETHOD HTMLAtomTagToId(nsIAtom* aAtom, PRInt32* aId) const=0;

  NS_IMETHOD HTMLStringTagToId(const nsString &aTag, PRInt32* aId) const =0;

  NS_IMETHOD HTMLIdToStringTag(PRInt32 aId, nsString& aTag) const =0;
  
  NS_IMETHOD HTMLConvertEntityToUnicode(const nsString& aEntity, 
                                        PRInt32* aUnicode) const =0;

  NS_IMETHOD HTMLConvertUnicodeToEntity(PRInt32 aUnicode,
                                        nsCString& aEntity) const =0;

  NS_IMETHOD IsContainer(PRInt32 aId, PRBool& aIsContainer) const =0;
  NS_IMETHOD IsBlock(PRInt32 aId, PRBool& aIsBlock) const =0;

  // Observer mechanism
  NS_IMETHOD RegisterObserver(nsIElementObserver* aObserver,
                              const nsAString& aTopic,
                              const eHTMLTags* aTags = nsnull) = 0;

  NS_IMETHOD UnregisterObserver(nsIElementObserver* aObserver,
                                const nsAString& aTopic) = 0;
  NS_IMETHOD GetTopicObservers(const nsAString& aTopic,
                               nsIObserverEntry** aEntry) = 0;

};

#endif // nsIParserService_h__
