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

  Implementation for an mail RDF data store.
  It supports the following kinds of objects.
  - mail message
  - mail folders

  There is a single Mail Data Source Object. When the data source
  is initialized, it reads in the list of accounts and their folders.
  The summary file for a folder is read in on demand (either when the
  folder is asked for its children or when one of the properties of
  a mail message is queried).  

  Each mail account,  mail folder and message is represented as 
   an nsIRDFResource.

  For the sake of compactness, the standard attributes of a mail message,
  account and folder are represented by using a C++ object --- corresponding
  to the message/account/folder. This is in contrast to the default InMemoryDataSource
  which uses a C++ object for each Assertion. The MailDataSource can store information
  about  and answer queries pertaining to the standard mail related properties of
  mail objects. Other properties of mail objects and properties about non-mail
  objects which might be in mail folders are taken care off by other data sources
  (such as the default local store). 

  There is a hash table (mResourceToMOTable) that keeps the mapping from
  nsIRDFResource(s) to MailObject(s). This is kind of kludgy --- it would
  be nice if MailObject and its subclasses could directly implement nsIRDFResource,
  but given the global namespace requirements for Resources, that causes
  some problems.
  
 */

#include "nscore.h"
#include "nsIRDFCursor.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFNode.h"
#include "nsIRDFObserver.h"
#include "nsIServiceManager.h"
#include "nsVoidArray.h"  // XXX introduces dependency on raptorbase
#include "nsRDFCID.h"
#include "rdfutil.h"
#include "plhash.h"
#include "plstr.h"

static NS_DEFINE_IID(kIRDFAssertionCursorIID,  NS_IRDFASSERTIONCURSOR_IID);
static NS_DEFINE_IID(kIRDFArcsInCursorIID,     NS_IRDFARCSINCURSOR_IID);
static NS_DEFINE_IID(kIRDFArcsOutCursorIID,    NS_IRDFARCSOUTCURSOR_IID);
static NS_DEFINE_IID(kIRDFCursorIID,           NS_IRDFCURSOR_IID);
static NS_DEFINE_IID(kIRDFDataSourceIID,       NS_IRDFDATASOURCE_IID);
static NS_DEFINE_IID(kIRDFLiteralIID,          NS_IRDFLITERAL_IID);
static NS_DEFINE_IID(kIRDFNodeIID,             NS_IRDFNODE_IID);
static NS_DEFINE_IID(kIRDFResourceIID,         NS_IRDFRESOURCE_IID);
static NS_DEFINE_IID(kISupportsIID,            NS_ISUPPORTS_IID);

class MailObject;
class MailMessage;
class MailFolder;
class MailAccount;

enum MACType {
  POP,
  IMAP
};

enum MOType {
  FOLDER,
  ACCOUNT,
  MESSAGE
}

class MailObject 
{
public:
  MOTYPE  mType;
  nsIRDFResource* mResource;
}

class MailAccount : MailObject
{
public:
  nsVoidArray* mFolders;
  char*        mUserName;
  char*        mServer;
  MACType      mAccountType;
}

class MailFolder : MailObject
{
public:
  nsVoidArray*  mMessages;
  char*         mPathname;
  MailAccount*  mAccount;
  PRBool        mReadInp;
}

class MailMessage : MailObject 
{
public:
  nsIRDFResource* mFrom;
  nsVoidArray*    mTo;
  nsIRDFLiteral*  mSubject;
  int             mDate;
  int             mSize;
  char            mFlags[8];
  size_t          mSummaryFileOffset;
  size_t          mBerkeleyFileOffset;
  MailFolder*     mFolder;
  char*           mMessageID;
}
  

class MailDataSource : public nsIRDFDataSource
{
protected:
  char*        mURL;
  PLHashTable* mResourceToMOTable;
  nsVoidArray* mObservers;  
  nsVoidArray* mAccounts;


public:
    MailDataSource(void);
    virtual ~MailDataSource(void);

    // nsISupports
    NS_DECL_ISUPPORTS

    // nsIRDFDataSource methods
    NS_IMETHOD Init(const char* uri);

    NS_IMETHOD GetSource(nsIRDFResource* property,
                         nsIRDFNode* target,
                         PRBool tv,
                         nsIRDFResource** source);

    NS_IMETHOD GetSources(nsIRDFResource* property,
                          nsIRDFNode* target,
                          PRBool tv,
                          nsIRDFAssertionCursor** sources);

    NS_IMETHOD GetTarget(nsIRDFResource* source,
                         nsIRDFResource* property,
                         PRBool tv,
                         nsIRDFNode** target);

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

    // Implemenatation methods

  MailObject* getResourceMailObject (nsIRDFResource* node);
  nsresult    setResourceMailObject (nsIRDFResource* node, MailObject* mo);
   
};

NS_IMPL_ISUPPORTS(MailDataSource, kIRDFDataSourceIID);


InMemoryDataSource::InMemoryDataSource(void)
    : mURL(nsnull),
      mResourceToMOTable(nsnull),
      mObservers(nsnull),
      mAccounts(nsnull)
{
    mResourceToMOTable = PL_NewHashTable(500,
                            rdf_HashPointer,
                            PL_CompareValues,
                            PL_CompareValues,
                            nsnull,
                            nsnull);
    getAccounts();
}

InMemoryDataSource::~InMemoryDataSource(void)
{
    if (mResourceToMOTable) {
        PL_HashTableDestroy(mResourceToMOTable);
        mResourceToMOTable = nsnull;
    }
    if (mObservers) {
        for (PRInt32 i = mObservers->Count(); i >= 0; --i) {
            nsIRDFObserver* obs = (nsIRDFObserver*) mObservers->ElementAt(i);
            NS_RELEASE(obs);
        }
        delete mObservers;
    }
}










