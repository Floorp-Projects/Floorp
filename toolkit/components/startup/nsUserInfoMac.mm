/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla toolkit code.
 *
 * The Initial Developer of the Original Code is Håkan Waara <hwaara@gmail.com>.
 * Portions created by the Initial Developer are Copyright (C) 2008
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#include "nsUserInfoMac.h"
#include "nsObjCExceptions.h"
#include "nsString.h"

#import <Cocoa/Cocoa.h>
#import <AddressBook/AddressBook.h>

NS_IMPL_ISUPPORTS1(nsUserInfo, nsIUserInfo)

nsUserInfo::nsUserInfo() {}

NS_IMETHODIMP
nsUserInfo::GetFullname(PRUnichar **aFullname)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT
  
  NS_ConvertUTF8toUTF16 fullName([NSFullUserName() UTF8String]);
  *aFullname = ToNewUnicode(fullName);
  return NS_OK;
  
  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT
}

NS_IMETHODIMP 
nsUserInfo::GetUsername(char **aUsername)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT
  
  nsCAutoString username([NSUserName() UTF8String]);
  *aUsername = ToNewCString(username);
  return NS_OK;
  
  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT
}

nsresult 
nsUserInfo::GetPrimaryEmailAddress(nsCString &aEmailAddress)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT
  
  // Try to get this user's primary email from the system addressbook's "me card" 
  // (if they've filled it)
  ABPerson *me = [[ABAddressBook sharedAddressBook] me];
  ABMultiValue *emailAddresses = [me valueForProperty:kABEmailProperty];
  if ([emailAddresses count] > 0) {
    // get the index of the primary email, in case there are more than one
    int primaryEmailIndex = [emailAddresses indexForIdentifier:[emailAddresses primaryIdentifier]];
    aEmailAddress.Assign([[emailAddresses valueAtIndex:primaryEmailIndex] UTF8String]);
    return NS_OK;
  }
  
  return NS_ERROR_FAILURE;
  
  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT
}

NS_IMETHODIMP 
nsUserInfo::GetEmailAddress(char **aEmailAddress)
{
  nsCAutoString email;
  if (NS_SUCCEEDED(GetPrimaryEmailAddress(email))) 
    *aEmailAddress = ToNewCString(email);
  return NS_OK;
}

NS_IMETHODIMP 
nsUserInfo::GetDomain(char **aDomain)
{
  nsCAutoString email;
  if (NS_SUCCEEDED(GetPrimaryEmailAddress(email))) {
    PRInt32 index = email.FindChar('@');
    if (index != -1) {
      // chop off everything before, and including the '@'
      *aDomain = ToNewCString(Substring(email, index + 1));
    }
  }
  return NS_OK;
}
