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


/*

  Class declarations for mail data source resource types.

  XXX Should these be factored out into their own implementation files, etc.?

*/

#ifndef nsMailDataSource_h__
#define nsMailDataSource_h__

////////////////////////////////////////////////////////////////////////
// NS_DECL_IRDFRESOURCE, NS_IMPL_IRDFRESOURCE
//
//   Convenience macros for implementing the RDF resource interface.
//
//   XXX It might make sense to move these macros to nsIRDFResource.h?

#define NS_DECL_IRDFRESOURCE \
    NS_IMETHOD EqualsNode(nsIRDFNode* node, PRBool* result) const;\
    NS_IMETHOD GetValue(const char* *uri) const;\
    NS_IMETHOD EqualsResource(const nsIRDFResource* resource, PRBool* result) const;\
    NS_IMETHOD EqualsString(const char* uri, PRBool* result) const;


#define NS_IMPL_IRDFRESOURCE(__class) \
NS_IMETHODIMP \
__class::EqualsNode(nsIRDFNode* node, PRBool* result) const {\
    nsresult rv;\
    nsIRDFResource* resource;\
    if (NS_SUCCEEDED(node->QueryInterface(kIRDFResourceIID, (void**) &resource))) {\
        rv = EqualsResource(resource, result);\
        NS_RELEASE(resource);\
    }\
    else {\
        *result = PR_FALSE;\
        rv = NS_OK;\
    }\
    return rv;\
}\
NS_IMETHODIMP \
__class::GetValue(const char* *uri) const{\
    if (!uri)\
        return NS_ERROR_NULL_POINTER;\
    *uri = mURI;\
    return NS_OK;\
}\
NS_IMETHODIMP \
__class::EqualsResource(const nsIRDFResource* resource, PRBool* result) const {\
    if (!resource || !result)  return NS_ERROR_NULL_POINTER;\
    *result = (resource == (nsIRDFResource*) this);\
    return NS_OK;\
}\
NS_IMETHODIMP \
__class::EqualsString(const char* uri, PRBool* result) const {\
    if (!uri || !result)  return NS_ERROR_NULL_POINTER;\
    *result = (PL_strcmp(uri, mURI) == 0);\
    return NS_OK;\
}

////////////////////////////////////////////////////////////////////////

/**
 * The mail data source.
 */
class MailDataSource : public nsIRDFMailDataSource 
{
private:
    char*          mURI;
    nsVoidArray*   mObservers;

    // internal methods
    nsresult InitAccountList (void);
    nsresult AddColumns (void);

public:
  
    NS_DECL_ISUPPORTS

    MailDataSource(void);
    virtual ~MailDataSource (void);

    // nsIRDFMailDataSource  methods
    NS_IMETHOD AddAccount (nsIRDFMailAccount* folder);
    NS_IMETHOD RemoveAccount (nsIRDFMailAccount* folder);

    // nsIRDFDataSource methods
    NS_IMETHOD Init(const char* uri);

    NS_IMETHOD GetURI(const char* *uri) const;

    NS_IMETHOD GetSource(nsIRDFResource* property,
                         nsIRDFNode* target,
                         PRBool tv,
                         nsIRDFResource** source /* out */);

    NS_IMETHOD GetTarget(nsIRDFResource* source,
                         nsIRDFResource* property,
                         PRBool tv,
                         nsIRDFNode** target);

    NS_IMETHOD GetSources(nsIRDFResource* property,
                          nsIRDFNode* target,
                          PRBool tv,
                          nsIRDFAssertionCursor** sources);

    NS_IMETHOD GetTargets(nsIRDFResource* source,
                          nsIRDFResource* property,    
                          PRBool tv,
                          nsIRDFAssertionCursor** targets);

    NS_IMETHOD Assert(nsIRDFResource* source,
                      nsIRDFResource* property, 
                      nsIRDFNode* target,
                      PRBool tv);

    NS_IMETHOD Unassert(nsIRDFResource* source,
                        nsIRDFResource* property,
                        nsIRDFNode* target);

    NS_IMETHOD HasAssertion(nsIRDFResource* source,
                            nsIRDFResource* property,
                            nsIRDFNode* target,
                            PRBool tv,
                            PRBool* hasAssertion);

    NS_IMETHOD AddObserver(nsIRDFObserver* n);

    NS_IMETHOD RemoveObserver(nsIRDFObserver* n);

    NS_IMETHOD ArcLabelsIn(nsIRDFNode* node,
                           nsIRDFArcsInCursor** labels);

    NS_IMETHOD ArcLabelsOut(nsIRDFResource* source,
                            nsIRDFArcsOutCursor** labels); 

    NS_IMETHOD Flush();

    // caching frequently used resources
    nsIRDFResource* mResourceChild;
    nsIRDFResource* mResourceFolder;
    nsIRDFResource* mResourceFrom;
    nsIRDFResource* mResourceSubject;
    nsIRDFResource* mResourceDate;
    nsIRDFResource* mResourceUser;
    nsIRDFResource* mResourceHost;
    nsIRDFResource* mResourceAccount;
    nsIRDFResource* mResourceName;
    nsIRDFResource* mMailRoot;
    nsIRDFResource* mResourceColumns;

    nsIRDFDataSource* mMiscMailData;
};

////////////////////////////////////////////////////////////////////////

class MailAccount : public nsIRDFMailAccount 
{
private:
    char*                mURI;        
    nsresult InitMailAccount (const char* uri);

public:
    MailAccount (const char* uri);
    MailAccount (char* uri,  nsIRDFLiteral* user,  nsIRDFLiteral* host);
    virtual ~MailAccount (void);
    
    NS_DECL_ISUPPORTS
    NS_DECL_IRDFRESOURCE

    NS_IMETHOD GetUser(nsIRDFLiteral**  result) const;
    NS_IMETHOD GetName(nsIRDFLiteral**  result) const;
    NS_IMETHOD GetHost(nsIRDFLiteral**  result) const;
    NS_IMETHOD AddFolder (nsIRDFMailFolder* folder);
    NS_IMETHOD RemoveFolder (nsIRDFMailFolder* folder);
};

////////////////////////////////////////////////////////////////////////

class MailFolder : public nsIRDFMailFolder 
{
private:
    enum MailFolderStatus {
        eMailFolder_Uninitialized,
        eMailFolder_InitInProgress,
        eMailFolder_WriteInProgress,
        eMailFolder_OK
    };

    nsVoidArray      mMessages;
    FILE*            mSummaryFile;
    MailFolderStatus mStatus;
    char*            mURI;

public:
    MailFolder (const char* uri);
    virtual ~MailFolder (void);

    NS_DECL_ISUPPORTS
    NS_DECL_IRDFRESOURCE

    NS_IMETHOD GetAccount(nsIRDFMailAccount** account);
    NS_IMETHOD GetName(nsIRDFLiteral**  result) const;
    NS_IMETHOD GetMessageList (nsVoidArray** result);
    NS_IMETHOD AddMessage (nsIRDFMailMessage* msg);
    NS_IMETHOD RemoveMessage (nsIRDFMailMessage* msg);

    nsresult  AddMessage(PRUnichar* uri,
                         MailFolder* folder,
                         nsIRDFResource* from,
                         nsIRDFLiteral* subject,
                         nsIRDFLiteral* date,
                         int summaryFileOffset,
                         int mailFileOffset,
                         char* flags, 
                         nsIRDFLiteral* messageID);

    nsresult AddMessage(PRUnichar* uri,
                        MailFolder* folder,
                        nsIRDFResource* from,
                        nsIRDFLiteral* subject,
                        nsIRDFLiteral* date,
                        int mailFileOffset,
                        char* flags,
                        nsIRDFLiteral* messageID);    

    nsresult ReadSummaryFile(char* uri);
};

////////////////////////////////////////////////////////////////////////

class MailMessage : public nsIRDFMailMessage  
{
private:
    MailFolder*     mFolder;
    nsIRDFResource* mFrom;
    nsIRDFLiteral*  mSubject;
    int             mSummaryFileOffset;
    int             mMailFileOffset;
    nsIRDFLiteral*  mDate;
    nsIRDFLiteral*  mMessageID;
    char            mFlags[4];
    char*           mURI;
    
public:
    MailMessage (const char* uri);
    virtual ~MailMessage (void);

    NS_DECL_ISUPPORTS
    NS_DECL_IRDFRESOURCE
    
    NS_IMETHOD GetFolder(nsIRDFMailFolder**  result);
    NS_IMETHOD GetSubject(nsIRDFLiteral**  result);
    NS_IMETHOD GetSender(nsIRDFResource**  result);
    NS_IMETHOD GetDate(nsIRDFLiteral**  result);
    NS_IMETHOD GetContent(const char* result);
    NS_IMETHOD GetMessageID(nsIRDFLiteral** id);
    NS_IMETHOD GetFlags(char** result);
    NS_IMETHOD SetFlags(const char* result);

    nsresult SetupMessage (MailFolder* folder,
                           nsIRDFResource* from,
                           nsIRDFLiteral* subject,
                           nsIRDFLiteral* date,
                           int summaryFileOffset,
                           int mailFileOffset,
                           char* flags, 
                           nsIRDFLiteral* messageID);
};

////////////////////////////////////////////////////////////////////////

class SingletonMailCursor : public nsIRDFAssertionCursor 
{
private:
    nsIRDFNode*     mValue;
    nsIRDFResource* mSource;
    nsIRDFResource* mProperty;
    nsIRDFNode*     mTarget;
    PRBool          mValueReturnedp;
    PRBool          mInversep;

public:
    SingletonMailCursor(nsIRDFNode* u,
                        nsIRDFResource* s,
                        PRBool inversep);
    virtual ~SingletonMailCursor(void);

    // nsISupports interface
    NS_DECL_ISUPPORTS
   
    // nsIRDFCursor interface
    NS_IMETHOD Advance(void);
    NS_IMETHOD GetValue(nsIRDFNode** aValue);

    // nsIRDFAssertionCursor interface
    NS_IMETHOD GetDataSource(nsIRDFDataSource** aDataSource);
    NS_IMETHOD GetSubject(nsIRDFResource** aResource);
    NS_IMETHOD GetPredicate(nsIRDFResource** aPredicate);
    NS_IMETHOD GetObject(nsIRDFNode** aObject);
    NS_IMETHOD GetTruthValue(PRBool* aTruthValue);
};

////////////////////////////////////////////////////////////////////////

class ArrayMailCursor : public nsIRDFAssertionCursor , 
                        public nsIRDFArcsOutCursor
{
private:
    nsIRDFNode*     mValue;
    nsIRDFResource* mSource;
    nsIRDFResource* mProperty;
    nsIRDFNode*     mTarget;
    int             mCount;
    nsVoidArray*    mArray;

public:
    ArrayMailCursor(nsIRDFResource* u, nsIRDFResource* s, nsVoidArray* array);
    virtual ~ArrayMailCursor(void);
    // nsISupports interface
    NS_DECL_ISUPPORTS
   
    // nsIRDFCursor interface
    NS_IMETHOD Advance(void);
    NS_IMETHOD GetValue(nsIRDFNode** aValue);
    // nsIRDFAssertionCursor interface
    NS_IMETHOD GetDataSource(nsIRDFDataSource** aDataSource);
    NS_IMETHOD GetSubject(nsIRDFResource** aResource);
    NS_IMETHOD GetPredicate(nsIRDFResource** aPredicate);
    NS_IMETHOD GetObject(nsIRDFNode** aObject);
    NS_IMETHOD GetTruthValue(PRBool* aTruthValue);
};


#endif // nsMailDataSource_h__
