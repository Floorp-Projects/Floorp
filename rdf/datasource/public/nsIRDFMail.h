/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsIRDFMail_h__
#define nsIRDFMail_h__

#include "nscore.h"
#include "nsISupports.h"

/*
  Interfaces for the mail data source.

  The UI will interact with the mail data source via the
  nsIRDFDataSource interface. The following methods are mostly for
  netlib to talk to the database. It needs to be able to add/delete an
  account, folder or message.
 */

// {669F3361-9EAF-11d2-80B4-006097B76B8E}
#define NS_IRDFMAILDATAOURCE_IID \
{ 0x669f3361, 0x9eaf, 0x11d2, { 0x80, 0xb4, 0x0, 0x60, 0x97, 0xb7, 0x6b, 0x8e } }

class nsIRDFMailAccount;
class nsIRDFMailFolder;
class nsIRDFMailMessage;

class nsIRDFMailDataSource : public nsIRDFDataSource {
public:
    /**
     * Add the specified mail account to the data source.
     */
    NS_IMETHOD AddAccount (nsIRDFMailAccount* folder) = 0;
  
    /**
     * Remove the specified mail account from the data source.
     */
    NS_IMETHOD RemoveAccount (nsIRDFMailAccount* folder) = 0;
};

// {9D2792E0-9EAE-11d2-80B4-006097B76B8E}
#define NS_IMAILACCOUNT_IID \
{ 0x9d2792e0, 0x9eae, 0x11d2, { 0x80, 0xb4, 0x0, 0x60, 0x97, 0xb7, 0x6b, 0x8e } }

/**
 * A resource specialization for mail accounts.
 */
class nsIRDFMailAccount : public nsIRDFResource {
public:

    NS_IMETHOD GetUser(nsIRDFLiteral**  result) const = 0;

    NS_IMETHOD GetHost(nsIRDFLiteral**  result) const = 0;

    NS_IMETHOD GetName(nsIRDFLiteral**  result) const = 0;

    NS_IMETHOD AddFolder (nsIRDFMailFolder* folder) = 0;

    NS_IMETHOD RemoveFolder (nsIRDFMailFolder* folder) = 0;
};

// {9D2792E1-9EAE-11d2-80B4-006097B76B8E}
#define NS_IMAILFOLDER_IID \
{ 0x9d2792e1, 0x9eae, 0x11d2, { 0x80, 0xb4, 0x0, 0x60, 0x97, 0xb7, 0x6b, 0x8e } }

/**
 * A resource specialization for mail folders.
 */
class nsIRDFMailFolder : public nsIRDFResource {
public:

    NS_IMETHOD GetAccount(nsIRDFMailAccount** account) = 0;

    NS_IMETHOD GetName(nsIRDFLiteral**  result) const = 0;

    NS_IMETHOD GetMessageList (nsVoidArray** result) = 0;

    NS_IMETHOD AddMessage (nsIRDFMailMessage* msg) = 0;

    NS_IMETHOD RemoveMessage (nsIRDFMailMessage* msg) = 0;
};

// {9D2792E2-9EAE-11d2-80B4-006097B76B8E}
#define NS_IMAILMESSAGE_IID \
{ 0x9d2792e2, 0x9eae, 0x11d2, { 0x80, 0xb4, 0x0, 0x60, 0x97, 0xb7, 0x6b, 0x8e } }

/**
 * A resource specialization for mail messages.
 */
class nsIRDFMailMessage : public nsIRDFResource {
public:
   
    NS_IMETHOD GetFolder(nsIRDFMailFolder** account) = 0;

    NS_IMETHOD GetSubject(nsIRDFLiteral**  result) = 0;

    NS_IMETHOD GetSender(nsIRDFResource**  result) = 0;

    NS_IMETHOD GetDate(nsIRDFLiteral**  result) = 0;

    NS_IMETHOD GetContent(const char* result) = 0; 

    NS_IMETHOD GetMessageID(nsIRDFLiteral** id) = 0;

// XXX this really sucks --- the flags, such as replied, forwarded, read, etc.
//     should have separate methods
    NS_IMETHOD GetFlags(char** result) = 0; 

    NS_IMETHOD SetFlags(const char* result) = 0; 
};



#endif nsIRDFMail_h
