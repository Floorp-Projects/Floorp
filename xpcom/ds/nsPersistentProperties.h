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
 * The Original Code is mozilla.org code.
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

#ifndef nsPersistentProperties_h___
#define nsPersistentProperties_h___

#include "nsIPersistentProperties2.h"

class nsIUnicharInputStream;

class nsPersistentProperties : public nsIPersistentProperties
{
public:
  nsPersistentProperties();
  virtual ~nsPersistentProperties();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROPERTIES

  // nsIPersistentProperties methods:
  NS_IMETHOD Load(nsIInputStream* aIn);
  NS_IMETHOD Save(nsIOutputStream* aOut, const nsString& aHeader);
  NS_IMETHOD Subclass(nsIPersistentProperties* aSubclass);
  NS_IMETHOD EnumerateProperties(nsIBidirectionalEnumerator* *aResult);
  NS_IMETHOD SimpleEnumerateProperties(nsISimpleEnumerator** aResult);

  // XXX these 2 methods will be subsumed by the ones from 
  // nsIProperties once we figure this all out
  NS_IMETHOD GetStringProperty(const nsAString& aKey, nsAString& aValue);
  NS_IMETHOD SetStringProperty(const nsAString& aKey, nsAString& aNewValue,
                               nsAString& aOldValue);

  // nsPersistentProperties methods:
  PRInt32 Read();
  PRInt32 SkipLine(PRInt32 c);
  PRInt32 SkipWhiteSpace(PRInt32 c);

  static NS_METHOD
  Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

protected:
  nsIUnicharInputStream* mIn;
  nsIPersistentProperties* mSubclass;
  struct PLHashTable*    mTable;
};

class nsPropertyElement : public nsIPropertyElement 
{
public:
  nsPropertyElement();
  virtual ~nsPropertyElement();

  NS_DECL_ISUPPORTS

  static NS_METHOD
  Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

  // nsIPropertyElement methods:
  NS_IMETHOD GetKey(PRUnichar **aReturnKey);
  NS_IMETHOD GetValue(PRUnichar **aReturnValue);
  NS_IMETHOD SetKey(nsString* aKey);
  NS_IMETHOD SetValue(nsString* aValue);

protected:
  nsString* mKey;
  nsString* mValue;
};

#endif /* nsPersistentProperties_h___ */
