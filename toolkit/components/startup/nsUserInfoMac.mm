/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "nsUserInfoMac.h"
#include "nsObjCExceptions.h"
#include "nsString.h"

#import <Cocoa/Cocoa.h>
#import <AddressBook/AddressBook.h>

NS_IMPL_ISUPPORTS(nsUserInfo, nsIUserInfo)

nsUserInfo::nsUserInfo() {}

NS_IMETHODIMP
nsUserInfo::GetFullname(char16_t **aFullname)
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
  
  nsAutoCString username([NSUserName() UTF8String]);
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
  nsAutoCString email;
  if (NS_SUCCEEDED(GetPrimaryEmailAddress(email))) 
    *aEmailAddress = ToNewCString(email);
  return NS_OK;
}

NS_IMETHODIMP 
nsUserInfo::GetDomain(char **aDomain)
{
  nsAutoCString email;
  if (NS_SUCCEEDED(GetPrimaryEmailAddress(email))) {
    int32_t index = email.FindChar('@');
    if (index != -1) {
      // chop off everything before, and including the '@'
      *aDomain = ToNewCString(Substring(email, index + 1));
    }
  }
  return NS_OK;
}
