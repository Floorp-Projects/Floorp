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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsPersistentProperties_h___
#define nsPersistentProperties_h___

#include "nsIPersistentProperties2.h"
#include "pldhash.h"
#include "plarena.h"
#include "nsString.h"

class nsIUnicharInputStream;


class nsPersistentProperties : public nsIPersistentProperties
{
public:
  nsPersistentProperties();
  nsresult Init();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROPERTIES
  NS_DECL_NSIPERSISTENTPROPERTIES


  // nsPersistentProperties methods:
  PRInt32 Read();
  PRInt32 SkipLine(PRInt32 c);
  PRInt32 SkipWhiteSpace(PRInt32 c);

  static NS_METHOD
  Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

private:
  ~nsPersistentProperties();

protected:
  nsIUnicharInputStream* mIn;
  PRUint32 mBufferPos;
  PRUint32 mBufferLength;
  nsIPersistentProperties* mSubclass;
  struct PLDHashTable mTable;
  PLArenaPool mArena;
};

class nsPropertyElement : public nsIPropertyElement 
{
public:
  nsPropertyElement()
  {
  }

  nsPropertyElement(const nsACString& aKey, const nsAString& aValue)
    : mKey(aKey), mValue(aValue)
  {
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROPERTYELEMENT

  static NS_METHOD
  Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

private:
  ~nsPropertyElement() {}

protected:
  nsCString mKey;
  nsString mValue;
};

#endif /* nsPersistentProperties_h___ */
