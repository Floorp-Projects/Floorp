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

  Implementation for a mail RDF data store.

  It supports the following kinds of objects.
  - mail accounts
  - mail folders
  - mail message

  There is a single Mail Data Source Object. When the data source is
  initialized, it reads in the list of accounts and their folders.
  The summary file for a folder is read in on demand (either when the
  folder is asked for its children or when one of the properties of a
  mail message is queried).

  Mail accounts, folders and messages are represented by
  nsIMailAccount, nsIMailFolder and nsIMailMessage, which are
  subclasses of nsIRDFResource.

  The implementations of these interfaces provided here assume certain
  standard fields for each of these kinds of objects.

  The MailDataSource can only store information about and answer
  queries pertaining to the standard mail related properties of mail
  objects. Other properties of mail objects and properties about
  non-mail objects which might be in mail folders are taken care off
  by other data sources (such as the default local store).

  TO DO

  1) Create nsIRDFPerson.

  2) Move more of the data to nsMiscMailData.

  3) Make sure we have refcounting right. Add null pointer checks,
     checks for malloc() failures, etc.

  4) Hook up observers.

  5) Make sure to use NSPR instead of C runtime where appropriate
     (e.g., file I/O).

  6) Factor into individual files for each major class?

 */
#include <ctype.h> // for toupper()
#include <stdio.h>
#include "nscore.h"
#include "nsIRDFCursor.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFNode.h"
#include "nsIRDFObserver.h"
#include "nsIRDFResourceFactory.h"
#include "nsIServiceManager.h"
#include "nsString.h"
#include "nsVoidArray.h"  // XXX introduces dependency on raptorbase
#include "nsRDFCID.h"
#include "rdfutil.h"
#include "nsIRDFMail.h"
#include "nsIRDFService.h"
#include "plhash.h"
#include "plstr.h"
#include "prmem.h"
#include "prprf.h"
#include "prio.h"

#include "nsMailDataSource.h"

////////////////////////////////////////////////////////////////////////
// Interface IDs

static NS_DEFINE_IID(kIRDFArcsInCursorIID,     NS_IRDFARCSINCURSOR_IID);
static NS_DEFINE_IID(kIRDFArcsOutCursorIID,    NS_IRDFARCSOUTCURSOR_IID);
static NS_DEFINE_IID(kIRDFAssertionCursorIID,  NS_IRDFASSERTIONCURSOR_IID);
static NS_DEFINE_IID(kIRDFCursorIID,           NS_IRDFCURSOR_IID);
static NS_DEFINE_IID(kIRDFDataSourceIID,       NS_IRDFDATASOURCE_IID);
static NS_DEFINE_IID(kIRDFLiteralIID,          NS_IRDFLITERAL_IID);
static NS_DEFINE_IID(kIRDFMailAccountIID,      NS_IMAILACCOUNT_IID);
static NS_DEFINE_IID(kIRDFMailDataSourceIID,   NS_IRDFMAILDATAOURCE_IID);
static NS_DEFINE_IID(kIRDFMailFolderIID,       NS_IMAILFOLDER_IID);
static NS_DEFINE_IID(kIRDFMailMessageIID,      NS_IMAILMESSAGE_IID);
static NS_DEFINE_IID(kIRDFNodeIID,             NS_IRDFNODE_IID);
static NS_DEFINE_IID(kIRDFResourceIID,         NS_IRDFRESOURCE_IID);
static NS_DEFINE_IID(kIRDFResourceFactoryIID,  NS_IRDFRESOURCEFACTORY_IID);
static NS_DEFINE_IID(kIRDFServiceIID,          NS_IRDFSERVICE_IID);
static NS_DEFINE_IID(kISupportsIID,            NS_ISUPPORTS_IID);
static NS_DEFINE_CID(kRDFServiceCID,           NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID, NS_RDFINMEMORYDATASOURCE_CID);

////////////////////////////////////////////////////////////////////////
// RDF property & resource declarations

static const char kURINC_MailRoot[]  = "NC:MailRoot";

#define NC_NAMESPACE_URI "http://home.netscape.com/NC-rdf#"
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, child);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, subject);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, from);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, date);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Name);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Folder);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Column);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Columns);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Title);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, user);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, account);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, host);

////////////////////////////////////////////////////////////////////////
// The RDF service manager. Cached in the mail data source's
// constructor

static nsIRDFService* gRDFService = nsnull;

////////////////////////////////////////////////////////////////////////
// THE mail data source.

static MailDataSource* gMailDataSource = nsnull;

////////////////////////////////////////////////////////////////////////
// Utilities

static PRBool
peq(nsIRDFResource* r1, nsIRDFResource* r2)
{
    PRBool result;
    if (NS_SUCCEEDED(r1->EqualsResource(r2, &result)) && result) {
        return PR_TRUE;
    } else {
        return PR_FALSE;
    }
}



/********************************** MailDataSource **************************************
 ************************************************************************************/

nsIRDFResource* MailDataSource::kNC_Child;
nsIRDFResource* MailDataSource::kNC_Folder;
nsIRDFResource* MailDataSource::kNC_From;
nsIRDFResource* MailDataSource::kNC_subject;
nsIRDFResource* MailDataSource::kNC_date;
nsIRDFResource* MailDataSource::kNC_user;
nsIRDFResource* MailDataSource::kNC_host;
nsIRDFResource* MailDataSource::kNC_account;
nsIRDFResource* MailDataSource::kNC_Name;
nsIRDFResource* MailDataSource::kNC_MailRoot;
nsIRDFResource* MailDataSource::kNC_Columns;

MailDataSource::MailDataSource(void)
    : mURI(nsnull), 
      mMiscMailData(nsnull),
      mObservers(nsnull)
{
    NS_INIT_REFCNT();

    nsresult rv = nsServiceManager::GetService(kRDFServiceCID,
                                               kIRDFServiceIID,
                                               (nsISupports**) &gRDFService);

    PR_ASSERT(NS_SUCCEEDED(rv));

    gMailDataSource = this;
}

MailDataSource::~MailDataSource (void)
{
    PL_strfree(mURI);
    if (mObservers) {
        for (PRInt32 i = mObservers->Count(); i >= 0; --i) {
            nsIRDFObserver* obs = (nsIRDFObserver*) mObservers->ElementAt(i);
            NS_RELEASE(obs);
        }
        delete mObservers;
    }
    nsrefcnt refcnt;
    NS_RELEASE2(kNC_Child, refcnt);
    NS_RELEASE2(kNC_Folder, refcnt);
    NS_RELEASE2(kNC_From, refcnt);
    NS_RELEASE2(kNC_subject, refcnt);
    NS_RELEASE2(kNC_date, refcnt);
    NS_RELEASE2(kNC_user, refcnt);
    NS_RELEASE2(kNC_host, refcnt);
    NS_RELEASE2(kNC_account, refcnt);
    NS_RELEASE2(kNC_Name, refcnt);
    NS_RELEASE2(kNC_MailRoot, refcnt);
    NS_RELEASE2(kNC_Columns, refcnt);

    gMailDataSource = nsnull;

    nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
    gRDFService = nsnull;
}

NS_IMPL_ISUPPORTS(MailDataSource, kIRDFMailDataSourceIID);

NS_IMETHODIMP
MailDataSource::AddAccount(nsIRDFMailAccount* folder)
{
    return gMailDataSource->mMiscMailData->Assert(MailDataSource::kNC_MailRoot,
                                                  MailDataSource::kNC_Folder,
                                                  folder, PR_TRUE);
}

NS_IMETHODIMP
MailDataSource::RemoveAccount(nsIRDFMailAccount* folder)
{
    return gMailDataSource->mMiscMailData->Unassert(MailDataSource::kNC_MailRoot,
                                                    MailDataSource::kNC_Folder,
                                                    folder);
}

NS_IMETHODIMP
MailDataSource::Init(const char* uri)
{
    if (mMiscMailData)
        return NS_ERROR_ALREADY_INITIALIZED;

    if ((mURI = PL_strdup(uri)) == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv;

    if (NS_FAILED(rv = nsRepository::CreateInstance(kRDFInMemoryDataSourceCID,
                                                    nsnull,
                                                    kIRDFDataSourceIID,
                                                    (void**) &mMiscMailData)))
        return rv;

    if (! kNC_Child) {
        gRDFService->GetResource(kURINC_child,   &kNC_Child);
        gRDFService->GetResource(kURINC_Folder,  &kNC_Folder);
        gRDFService->GetResource(kURINC_from,    &kNC_From);
        gRDFService->GetResource(kURINC_subject, &kNC_subject);
        gRDFService->GetResource(kURINC_date,    &kNC_date);
        gRDFService->GetResource(kURINC_user,    &kNC_user);
        gRDFService->GetResource(kURINC_host,    &kNC_host);
        gRDFService->GetResource(kURINC_account, &kNC_account);
        gRDFService->GetResource(kURINC_Name,    &kNC_Name);
        gRDFService->GetResource(kURINC_Columns, &kNC_Columns);
        gRDFService->GetResource(kURINC_MailRoot, &kNC_MailRoot);
    }

    if (NS_FAILED(rv = InitAccountList()))
        return rv;

    return NS_OK;
}

NS_IMETHODIMP
MailDataSource::GetURI(const char* *uri) const
{
    *uri = mURI;
    return NS_OK;
}

NS_IMETHODIMP
MailDataSource::GetSource(nsIRDFResource* property,
                          nsIRDFNode* target,
                          PRBool tv,
                          nsIRDFResource** source /* out */)
{
    nsIRDFMailMessage* msg;
    nsIRDFMailFolder*  fl;
    nsresult rv = NS_ERROR_RDF_NO_VALUE;
    if (!tv) return rv;
    if (NS_OK == target->QueryInterface(kIRDFMailMessageIID, (void**)&msg)) {
        if (peq(kNC_Child, property)) {
            rv = msg->GetFolder((nsIRDFMailFolder**)source);
        }
        NS_RELEASE(msg);
        return rv;
    } else  if (NS_OK == target->QueryInterface(kIRDFMailFolderIID, (void**)&fl)) {
        if (peq(kNC_account, property)) {
            rv = fl->GetAccount((nsIRDFMailAccount**)source);
        }
        NS_RELEASE(fl);
        return rv;
    } else return rv;
}


NS_IMETHODIMP
MailDataSource::GetTarget(nsIRDFResource* source,
                          nsIRDFResource* property,
                          PRBool tv,
                          nsIRDFNode** target /* out */)
{
    nsIRDFMailMessage* msg;

    // we only have positive assertions in the mail data source.
    if (! tv)
        return NS_ERROR_RDF_NO_VALUE;

    if (NS_SUCCEEDED(source->QueryInterface(kIRDFMailMessageIID, (void**) &msg))) {
        nsresult rv;

        // XXX maybe something here to make sure that the folder corresponding to the message
        // has been initialized?
        if (peq(kNC_From, property)) {
            rv = msg->GetSender((nsIRDFResource**)target);
        }
        else if (peq(kNC_subject, property)) {
            rv = msg->GetSubject((nsIRDFLiteral**)target);
        }
        else if (peq(kNC_date, property)) {
            rv = msg->GetDate((nsIRDFLiteral**)target);
        }
        else {
            // XXX should this fall through to the misc mail data datasource?
            rv = NS_ERROR_RDF_NO_VALUE;
        }
        NS_RELEASE(msg);
        return rv;
    } else {
        return mMiscMailData->GetTarget(source, property, tv, target);
    }
}


NS_IMETHODIMP
MailDataSource::Assert(nsIRDFResource* source,
                       nsIRDFResource* property,
                       nsIRDFNode* target,
                       PRBool tv)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MailDataSource::Unassert(nsIRDFResource* source,
                         nsIRDFResource* property,
                         nsIRDFNode* target)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MailDataSource::HasAssertion(nsIRDFResource* source,
                             nsIRDFResource* property,
                             nsIRDFNode* target,
                             PRBool tv,
                             PRBool* hasAssertion /* out */)
{
    return mMiscMailData->HasAssertion(source, property, target, tv, hasAssertion);
}

NS_IMETHODIMP
MailDataSource::AddObserver(nsIRDFObserver* n)
{
    if (! mObservers) {
        if ((mObservers = new nsVoidArray()) == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
    }
    mObservers->AppendElement(n);
    return NS_OK;
}

NS_IMETHODIMP
MailDataSource::RemoveObserver(nsIRDFObserver* n)
{
    if (! mObservers)
        return NS_OK;
    mObservers->RemoveElement(n);
    return NS_OK;
}

NS_IMETHODIMP
MailDataSource::ArcLabelsIn(nsIRDFNode* node,
                            nsIRDFArcsInCursor** labels /* out */)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP MailDataSource::Flush()
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
MailDataSource::IsCommandEnabled(const char* aCommand,
                                 nsIRDFResource* aCommandTarget,
                                 PRBool* aResult)
{
    return NS_OK;
}

NS_IMETHODIMP
MailDataSource::DoCommand(const char* aCommand,
                          nsIRDFResource* aCommandTarget)
{
    return NS_OK;
}

NS_IMETHODIMP
MailDataSource::GetSources(nsIRDFResource* property,
                           nsIRDFNode* target, PRBool tv,
                           nsIRDFAssertionCursor** sources /* out */)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
MailDataSource::GetTargets(nsIRDFResource* source,
                           nsIRDFResource* property,
                           PRBool tv,
                           nsIRDFAssertionCursor** targets /* out */)
{
    nsIRDFMailMessage* msg;
    nsIRDFMailFolder*  fl;
    nsVoidArray*       array = nsnull;
    nsresult rv = NS_ERROR_FAILURE;

    if (peq(kNC_Child, property) &&
        (NS_OK == source->QueryInterface(kIRDFMailFolderIID, (void**)&fl))) {
        rv = fl->GetMessageList(&array);
        *targets = new ArrayMailCursor(source, property, array);
        NS_ADDREF(*targets);
        NS_IF_RELEASE(fl);
        return rv;
    } else if ((peq(kNC_date, property) ||
                peq(kNC_From, property) ||
                peq(kNC_subject, property)) &&
               (NS_OK == source->QueryInterface(kIRDFMailMessageIID, (void**)&msg))) {
        *targets = new SingletonMailCursor(source, property, PR_FALSE);
        NS_IF_RELEASE(msg);
        return rv;
    } else {
        return mMiscMailData->GetTargets(source, property, tv, targets);
    }
}

NS_IMETHODIMP
MailDataSource::ArcLabelsOut(nsIRDFResource* source,
                             nsIRDFArcsOutCursor** labels /* out */)
{
    nsIRDFMailMessage* message;
    nsIRDFMailFolder*  folder;
    nsIRDFMailAccount* account;

    if (NS_SUCCEEDED(source->QueryInterface(kIRDFMailMessageIID, (void**) &message))) {
        nsVoidArray* temp = new nsVoidArray();
        if (! temp)
            return NS_ERROR_OUT_OF_MEMORY;

        temp->AppendElement(kNC_Name); // XXX hack to get things to look right
        temp->AppendElement(kNC_subject);
        temp->AppendElement(kNC_From);
        temp->AppendElement(kNC_date);
        *labels = new ArrayMailCursor(source, kNC_Child, temp);
        NS_ADDREF(*labels);

        NS_RELEASE(message);
        return NS_OK;
    }
    else if (NS_SUCCEEDED(source->QueryInterface(kIRDFMailFolderIID, (void**) &folder))) {
        nsVoidArray* temp = new nsVoidArray();
        if (! temp)
            return NS_ERROR_OUT_OF_MEMORY;

        temp->AppendElement(kNC_Name);
        temp->AppendElement(kNC_subject); // XXX hack to get things to look right
        temp->AppendElement(kNC_Child);
        *labels = new ArrayMailCursor(source, kNC_Child, temp);
        NS_ADDREF(*labels);

        NS_RELEASE(folder);
        return NS_OK;
    }
    else if (NS_SUCCEEDED(source->QueryInterface(kIRDFMailAccountIID, (void**) &account))) {
        nsVoidArray* temp = new nsVoidArray();
        if (! temp)
            return NS_ERROR_OUT_OF_MEMORY;

        temp->AppendElement(kNC_Name);
        temp->AppendElement(kNC_subject); // XXX hack to get things to look right
        temp->AppendElement(kNC_Folder);
        *labels = new ArrayMailCursor(source, kNC_Child, temp);
        NS_ADDREF(*labels);

        NS_RELEASE(account);
        return NS_OK;
    }
    else {
        return mMiscMailData->ArcLabelsOut(source, labels);
    }
}

nsresult
MailDataSource::InitAccountList(void)
{
    char fileurl[256];
    PRInt32 n = PR_SKIP_BOTH;
    PRDirEntry	*de;
    PRDir* dir;
    nsIRDFMailAccount* account;
    PR_snprintf(fileurl, sizeof(fileurl), "res\\rdf\\Mail");
    dir =  PR_OpenDir(fileurl);
    if (dir == NULL) {
        //if (CallPRMkDirUsingFileURL(fileurl, 00700) > -1) dir = OpenDir(fileurl);
    }
    while ((dir != NULL) && ((de = PR_ReadDir(dir, (PRDirFlags)(n++))) != NULL)) {
        if (strchr(de->name, '@')) {
            PR_snprintf(fileurl, sizeof(fileurl), "mailaccount:%s", de->name);
            gRDFService->GetResource(fileurl, (nsIRDFResource**)&account);
            rdf_Assert(mMiscMailData, account, kNC_Name, de->name);
            rdf_Assert(mMiscMailData, account, kNC_subject, de->name);
            AddAccount(account);
        }
    }
    if (dir) PR_CloseDir(dir);
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////

/**
 * Return the singleton mail data source, constructing it if necessary.
 */
nsresult
NS_NewRDFMailDataSource(nsIRDFDataSource** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    // Only one mail data source
    if (! gMailDataSource) {
        if ((gMailDataSource = new MailDataSource()) == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
    }

    NS_ADDREF(gMailDataSource);
    *result = gMailDataSource;
    return NS_OK;
}



/********************************** MailAccount **************************************
 ************************************************************************************/

NS_IMPL_IRDFRESOURCE(MailAccount);

NS_IMETHODIMP
MailAccount::GetUser(nsIRDFLiteral**  result) const
{
    return gMailDataSource->mMiscMailData->GetTarget((nsIRDFResource*)this,
                                      MailDataSource::kNC_user, PR_TRUE,
                                      (nsIRDFNode**)result);
}

NS_IMETHODIMP
MailAccount::GetName(nsIRDFLiteral** result) const
{
    // NS_ADDREF(mName);
    return gMailDataSource->mMiscMailData->GetTarget(NS_CONST_CAST(MailAccount*, this),
                                                     MailDataSource::kNC_Name,
                                                     PR_TRUE,
                                                     (nsIRDFNode**)result);
}


NS_IMETHODIMP
MailAccount::GetHost(nsIRDFLiteral** result) const
{
    return gMailDataSource->mMiscMailData->GetTarget(NS_CONST_CAST(MailAccount*, this),
                                                     MailDataSource::kNC_host, 
                                                     PR_TRUE,
                                                     (nsIRDFNode**)result);
}


NS_IMETHODIMP
MailAccount::AddFolder (nsIRDFMailFolder* folder)
{
    return gMailDataSource->mMiscMailData->Assert(this,
                                                  MailDataSource::kNC_Folder,
                                                  folder, PR_TRUE);
}

NS_IMETHODIMP
MailAccount::RemoveFolder (nsIRDFMailFolder* folder)
{
    return gMailDataSource->mMiscMailData->Unassert(this,
                                                    MailDataSource::kNC_Folder,
                                                    folder);
}

MailAccount::MailAccount (const char* uri)
{
    mURI = PL_strdup(uri);
    InitMailAccount(uri);
}


MailAccount::~MailAccount (void)
{
    // Need to uncache before freeing mURI, due to the
    // implementation of the RDF service.
    gRDFService->UnCacheResource(this);
    PL_strfree(mURI);
}


NS_IMPL_ADDREF(MailAccount);
NS_IMPL_RELEASE(MailAccount);


static FILE *
openFileWR (char* path)
{
	FILE* ans = fopen(path, "r+");
	if (!ans) {
		ans = fopen(path, "w");
		if (ans) fclose(ans);
		ans = fopen(path, "r+");
	}
	return ans;
}


static PRBool
startsWith (const char* pattern, const char* uuid)
{
  size_t l1 = PL_strlen(pattern);
  size_t l2 = PL_strlen(uuid);
  if (l2 < l1)
      return PR_FALSE;
  return (strncmp(pattern, uuid, l1) == 0);
}


static PRBool
endsWith (const char* pattern, const char* uuid)
{
  short l1 = PL_strlen(pattern);
  short l2 = PL_strlen(uuid);
  short index;

  if (l2 < l1) return false;

  for (index = 1; index <= l1; index++) {
    if (toupper(pattern[l1-index]) != toupper(uuid[l2-index])) return false;
  }

  return true;
}

static void
convertSlashes (char* str) {
	size_t len = PL_strlen(str);
	size_t n = 0;
	while (n < len) {
		if (str[n] == '/') str[n] = '\\';
        n++;
	}
}


NS_IMETHODIMP
MailAccount::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    *result = nsnull;
    if (iid.Equals(kIRDFResourceIID) ||
        iid.Equals(kIRDFNodeIID) ||
        iid.Equals(kIRDFMailAccountIID) ||
        iid.Equals(kISupportsIID)) {
        *result = NS_STATIC_CAST(nsIRDFMailAccount*, this);
        AddRef();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}


nsresult
MailAccount::InitMailAccount (const char* url)
{
    char fileurl[256];
    PRInt32 n = PR_SKIP_BOTH;
    PRDirEntry	*de;
    PRDir* dir;
    nsIRDFMailFolder* folder;
    PR_snprintf(fileurl, sizeof(fileurl), "res\\rdf\\Mail\\%s",  &url[12]);
    dir =  PR_OpenDir(fileurl);
    if (dir == NULL) {
        //if (CallPRMkDirUsingFileURL(fileurl, 00700) > -1) dir = OpenDir(fileurl);
    }
    while ((dir != NULL) && ((de = PR_ReadDir(dir, (PRDirFlags)(n++))) != NULL)) {
        if ((!endsWith(".ssf", de->name)) && (!endsWith(".dat", de->name)) &&
            (!endsWith(".snm", de->name)) && (!endsWith("~", de->name))) {
            PR_snprintf(fileurl, sizeof(fileurl), "mailbox://%s/%s/", &url[12], de->name);
            gRDFService->GetResource(fileurl, (nsIRDFResource**)&folder);
            rdf_Assert(gMailDataSource->mMiscMailData, folder,
                       MailDataSource::kNC_Name, de->name);
            rdf_Assert(gMailDataSource->mMiscMailData, folder,
                       MailDataSource::kNC_subject, de->name);
            AddFolder(folder);
        }
    }
    if (dir) PR_CloseDir(dir);
    return NS_OK;
}




/********************************** MailFolder **************************************
 ************************************************************************************/

NS_IMPL_IRDFRESOURCE(MailFolder);

NS_IMETHODIMP
MailFolder::GetAccount(nsIRDFMailAccount** account)
{
    return gMailDataSource->mMiscMailData->GetTarget((nsIRDFResource*)this,
                                                    MailDataSource::kNC_account, PR_TRUE,
                                                    (nsIRDFNode**)account);
}

NS_IMETHODIMP
MailFolder::GetName(nsIRDFLiteral**  result) const
{
    return gMailDataSource->mMiscMailData->GetTarget((nsIRDFResource*)this,
                                                     MailDataSource::kNC_Name, PR_TRUE,
                                                     (nsIRDFNode**)result);
}

NS_IMETHODIMP
MailFolder::GetMessageList (nsVoidArray** result)
{
    nsresult rv = NS_OK;
    if (mStatus == eMailFolder_Uninitialized) {
        mStatus = eMailFolder_OK;
        rv = ReadSummaryFile(mURI);
    }
    *result = &mMessages;
    return rv;
}

NS_IMETHODIMP
MailFolder::AddMessage (nsIRDFMailMessage* msg)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MailFolder::RemoveMessage (nsIRDFMailMessage* msg)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

MailFolder::MailFolder (const char* uri)
{
    mURI = PL_strdup(uri);
    mStatus = eMailFolder_Uninitialized;
    NS_INIT_REFCNT();
}

MailFolder::~MailFolder (void)
{
    gRDFService->UnCacheResource(this);
    PL_strfree(mURI);
}


NS_IMPL_ADDREF(MailFolder);
NS_IMPL_RELEASE(MailFolder);

NS_IMETHODIMP
MailFolder::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    *result = nsnull;
    if (iid.Equals(kIRDFResourceIID) ||
        iid.Equals(kIRDFNodeIID) ||
        iid.Equals(kIRDFMailFolderIID) ||
        iid.Equals(kISupportsIID)) {
        *result = NS_STATIC_CAST(nsIRDFMailFolder*, this);
        AddRef();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}


nsresult
MailFolder::AddMessage(PRUnichar* uri,
                       MailFolder* folder,
                       nsIRDFResource* from,
                       nsIRDFLiteral* subject,
                       nsIRDFLiteral* date,
                       int summaryFileOffset,
                       int mailFileOffset,
                       char* flags,
                       nsIRDFLiteral* messageID)
{
    MailMessage* msg;
    gRDFService->GetUnicodeResource(uri, (nsIRDFResource**)&msg);
    msg->SetupMessage(folder, from, subject, date, summaryFileOffset, mailFileOffset, flags,
                      messageID);
    mMessages.AppendElement(msg);
    return NS_OK;
}


void
GetCValue (nsIRDFLiteral *node, char** result)
{
    const PRUnichar* str;
    node->GetValue(&str);
    nsAutoString pstr(str);
    *result = pstr.ToNewCString();

}

nsresult
MailFolder::AddMessage(PRUnichar* uri,
                       MailFolder* folder,
                       nsIRDFResource* from,
                       nsIRDFLiteral* subject,
                       nsIRDFLiteral* date,
                       int mailFileOffset,
                       char* flags,
                       nsIRDFLiteral* messageID)
{
    MailMessage* msg;
	char buff[1000];
    int summaryFileOffset;
    char  *xfrom, *xsubject, *xdate;
    gRDFService->GetUnicodeResource(uri, (nsIRDFResource**)&msg);
    if (!flags) flags = "0000";
    fseek(mSummaryFile, 0L, SEEK_END);
    summaryFileOffset = ftell(mSummaryFile);

    from->GetValue((const char**) &xfrom);

    GetCValue(date, &xdate);
    GetCValue(subject, &xsubject);

    PR_snprintf(buff, sizeof(buff),
                "Status: %s\nSOffset: %d\nFrom: %s\nSubject: %s\nDate: %s\nMOffset: %d\n",
                flags, summaryFileOffset, xfrom, xsubject, xdate, mailFileOffset);

    fprintf(mSummaryFile, buff);
    delete(xsubject);
    delete(xdate);
     fflush(mSummaryFile);
    msg->SetupMessage(folder, from, subject, date, summaryFileOffset, mailFileOffset, flags,
                      messageID);
    mMessages.AppendElement(msg);
    return NS_OK;
}

char*
trimString (char* str)
{
    size_t len = PL_strlen(str);
    size_t n = len-1;
    while (n > 0) {
        if ((str[n] == ' ') || (str[n] == '\n') || (str[n] == '\r')) {
            str[n--] = '\0';
        } else break;
    }
    n = 0;
    while (n < len) {
        if ((str[n] == ' ') || (str[n] == '\n') || (str[n] == '\r')) {
            n++;
        } else break;
    }
    return &str[n];
}


nsresult
MailFolder::ReadSummaryFile (char* url)
{
    nsresult rv = NS_OK;
    static const PRInt32 kBufferSize = 4096;

    if (startsWith("mailbox://", url)) {
        char* folderURL = &url[10];	
        PRInt32 flen = PL_strlen("res\\rdf\\Mail") + PL_strlen(folderURL) + 4 + 1;
        char* fileurl = (char*) PR_MALLOC(flen);
        PRInt32 nurllen = PL_strlen(url) + 20 + 1;
        char* nurl = (char*) PR_MALLOC(nurllen);
        char* buff = (char*) PR_MALLOC(kBufferSize);
        FILE *mf;
        int   summOffset, messageOffset;
        nsIRDFLiteral *rSubject, *rDate;
        nsIRDFResource * rFrom;
        PRBool summaryFileFound = PR_FALSE;
        char* flags = 0;

        PR_snprintf(fileurl, flen, "res\\rdf\\Mail\\%sssf",  folderURL);
        fileurl[PL_strlen(fileurl)-4] = '.'; //XXX how do you spell kludge?
        convertSlashes(fileurl);
        // fileurl = MCDepFileURL(fileurl);
        mSummaryFile = openFileWR(fileurl);
        PR_snprintf(fileurl, flen, "res\\rdf\\Mail\\%s",  folderURL);
        //	fileurl = MCDepFileURL(fileurl);
        mf = openFileWR(fileurl);


        while (mSummaryFile && fgets(buff, kBufferSize, mSummaryFile)) {
            if (startsWith("Status:", buff)) {
                summaryFileFound = 1;
                flags = PL_strdup(&buff[8]);
                fgets(buff, kBufferSize, mSummaryFile);
                sscanf(&buff[9], "%d", &summOffset);
                fgets(buff, kBufferSize, mSummaryFile);
                nsAutoString pfrom(trimString(&buff[6]));
                rv = gRDFService->GetUnicodeResource(pfrom, &rFrom);
                if (rv == NS_OK) {
                    fgets(buff, kBufferSize, mSummaryFile);
                    nsAutoString psubject(trimString(&buff[8]));
                    rv = gRDFService->GetLiteral(psubject, &rSubject);
                    if (rv == NS_OK) {
                        fgets(buff, kBufferSize, mSummaryFile);
                        nsAutoString pdate(trimString(&buff[6]));
                        rv = gRDFService->GetLiteral(pdate, &rDate);
                        if (rv == NS_OK) {
                            fgets(buff, kBufferSize, mSummaryFile);
                            sscanf(&buff[9], "%d", &messageOffset);
                            PR_snprintf(nurl, nurllen, "%s?%d", url, messageOffset); // XXX unsafe
                            nsAutoString purl(nurl);
                            rv = AddMessage(purl, this, rFrom, rSubject, rDate, summOffset,
                                            messageOffset, flags, 0);
                        }
                    }
                }
                PL_strfree(flags);
                if (rv != NS_OK) return rv;
            }
        }

        rFrom = nsnull;
        if (!summaryFileFound) {
            /* either a new mailbox or need to read BMF to recreate */
            while (mf && fgets(buff, kBufferSize, mf)) {
                if (strncmp("From ", buff, 5) ==0)  {
                    if (rFrom) {
                        nsAutoString purl(nurl);
                        rv = AddMessage(purl, this, rFrom, rSubject, rDate, messageOffset, flags, nsnull);
                        if (rv != NS_OK) goto done;
                    }
                    messageOffset = ftell(mf);
                    if (flags) PL_strfree(flags);
                    PR_snprintf(nurl, nurllen, "%s?%i", url, messageOffset); // XXX unsafe
                    flags = nsnull;
                    rFrom = nsnull;
                    rSubject = rDate = nsnull;
                }
                buff[PL_strlen(buff)-1] = '\0';
                if ((!rFrom) && (startsWith("From:", buff))) {
                    nsAutoString pfrom(&buff[6]);
                    gRDFService->GetUnicodeResource(pfrom, &rFrom);

                } else if ((!rDate) && (startsWith("Date:", buff))) {
                    nsAutoString pdate(&buff[6]);
                    gRDFService->GetLiteral(pdate, &rDate);
                } else if ((!rSubject) && (startsWith("Subject:", buff))) {
                    nsAutoString psubject(&buff[8]);
                    gRDFService->GetLiteral(psubject, &rSubject);

                } else if ((!flags) && (startsWith("X-Mozilla-Status:", buff))) {
                    flags = PL_strdup(&buff[17]);
                }
            }
            if (rFrom){
                nsAutoString purl(nurl);
                rv = AddMessage(purl, this, rFrom, rSubject, rDate, messageOffset, flags, nsnull);
                // fall through with rv
            }
            fflush(mSummaryFile);
        }
      done:
        PR_DELETE(buff);
        PR_DELETE(nurl);
    }
    return rv;
}

/********************************** MailMessage **************************************
 ************************************************************************************/

NS_IMPL_IRDFRESOURCE(MailMessage);

NS_IMETHODIMP
MailMessage::GetFolder(nsIRDFMailFolder**  result)
{
    *result = mFolder;
    return NS_OK;
}


NS_IMETHODIMP
MailMessage::GetSubject(nsIRDFLiteral**  result)
{
    NS_ADDREF(mSubject);
    *result = mSubject;
    return NS_OK;
}

NS_IMETHODIMP
MailMessage::GetSender(nsIRDFResource**  result)
{
    NS_ADDREF(mFrom);
    *result = mFrom;
    return NS_OK;
}

NS_IMETHODIMP
MailMessage::GetDate(nsIRDFLiteral**  result)
{
    NS_ADDREF(mDate);
    *result = mDate;
    return NS_OK;
}

NS_IMETHODIMP
MailMessage::GetContent(const char* result)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MailMessage::GetMessageID(nsIRDFLiteral** id)
{
    NS_ADDREF(mMessageID);
    *id = mMessageID;
    return NS_OK;
}

NS_IMETHODIMP
MailMessage::GetFlags(char** result)
{
    *result = mFlags;
    return NS_OK;
}

NS_IMETHODIMP
MailMessage::SetFlags(const char* result)
{
    memcpy(mFlags, result, 4);
    //xxx write this into the summary file
    return NS_OK;
}

nsresult MailMessage::SetupMessage (MailFolder* folder,
                                    nsIRDFResource* from,
                                    nsIRDFLiteral* subject,
                                    nsIRDFLiteral* date,
                                    int summaryFileOffset,
                                    int mailFileOffset,
                                    char* flags,
                                    nsIRDFLiteral* messageID)
{
    NS_INIT_REFCNT();
    mFolder = folder;
    mFrom = from;
    mSubject = subject;
    mDate    = date;
    mSummaryFileOffset = summaryFileOffset;
    mMailFileOffset    = mailFileOffset;
    memcpy(mFlags, flags, 4);
    mMessageID = messageID;

    NS_IF_ADDREF(mFrom);
    NS_IF_ADDREF(mSubject);
    NS_IF_ADDREF(mDate);
    return NS_OK;
}

MailMessage::MailMessage (const char* uri)
{
    mURI = PL_strdup(uri);
    NS_INIT_REFCNT();
}

MailMessage::~MailMessage (void) {
    gRDFService->UnCacheResource(this);
    PL_strfree(mURI);
    NS_IF_RELEASE(mFrom);
    NS_IF_RELEASE(mSubject);
    NS_IF_RELEASE(mDate);
    NS_IF_RELEASE(mMessageID);
}

NS_IMPL_ADDREF(MailMessage);
NS_IMPL_RELEASE(MailMessage);

NS_IMETHODIMP
MailMessage::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    *result = nsnull;
    if (iid.Equals(kIRDFResourceIID) ||
        iid.Equals(kIRDFNodeIID) ||
        iid.Equals(kIRDFMailMessageIID) ||
        iid.Equals(kISupportsIID)) {
        *result = NS_STATIC_CAST(nsIRDFMailMessage*, this);
        AddRef();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}


/********************************** nsIRDFPerson **************************************
 * nsIRDFPerson is not really a person. It corresponds to someone you can recieve
 * mail from.
 ************************************************************************************/

// XXX --- needs to be done.

/********************************** Cursors **************************************
 * There are two kinds of cursors for the mail data source.
 * --- those that cough up a singleton value (sender, subject, etc.)
 * --- those that cough up a sequence of values from an nsVoidArray (children of folders, folders
 *     in an account
 ************************************************************************************/

SingletonMailCursor::SingletonMailCursor(nsIRDFNode* u,
                                         nsIRDFResource* s,
                                         PRBool inversep)
    : mValueReturnedp(PR_FALSE),
      mProperty(s),
      mInversep(inversep),
      mValue(nsnull)
{
    if (!inversep) {
        mSource = (nsIRDFResource*)u;
        mTarget = nsnull;
    } else {
        mSource = nsnull;
        mTarget = u;
    }
    NS_ADDREF(u);
    NS_ADDREF(mProperty);
}


SingletonMailCursor::~SingletonMailCursor(void)
{
    NS_IF_RELEASE(mSource);
    NS_IF_RELEASE(mValue);
    NS_IF_RELEASE(mProperty);
    NS_IF_RELEASE(mTarget);
}



    // nsIRDFCursor interface
NS_IMETHODIMP
SingletonMailCursor::Advance(void)
{
    nsresult rv = NS_ERROR_RDF_CURSOR_EMPTY;
    if (mValueReturnedp) {
        NS_IF_RELEASE(mValue);
        mValue = nsnull;
        return NS_ERROR_RDF_CURSOR_EMPTY;
    }
    mValueReturnedp = PR_TRUE;
    if (mInversep) {
        rv = gMailDataSource->GetSource(mProperty, mTarget, 1, (nsIRDFResource**)&mValue);
        mSource = (nsIRDFResource*)mValue;
        NS_IF_ADDREF(mSource);
    } else {
        rv = gMailDataSource->GetTarget(mSource, mProperty,  1, &mValue);
        mTarget = mValue;
        NS_IF_ADDREF(mTarget);
    }
    if (rv == NS_ERROR_RDF_NO_VALUE)
        return NS_ERROR_RDF_CURSOR_EMPTY;
    return rv;
}

NS_IMETHODIMP
SingletonMailCursor::GetValue(nsIRDFNode** aValue)
{
    NS_ADDREF(mValue);
    *aValue = mValue;
    return NS_OK;
}

    // nsIRDFAssertionCursor interface
NS_IMETHODIMP
SingletonMailCursor::GetDataSource(nsIRDFDataSource** aDataSource)
{
    NS_ADDREF(gMailDataSource);
    *aDataSource = gMailDataSource;
    return NS_OK;
}

NS_IMETHODIMP
SingletonMailCursor::GetSubject(nsIRDFResource** aResource)
{
    NS_ADDREF(mSource);
    *aResource = mSource;
    return NS_OK;
}

NS_IMETHODIMP
SingletonMailCursor::GetPredicate(nsIRDFResource** aPredicate)
{
    NS_ADDREF(mProperty);
    *aPredicate = mProperty;
    return NS_OK;
}

NS_IMETHODIMP
SingletonMailCursor::GetObject(nsIRDFNode** aObject)
{
    NS_ADDREF(mTarget);
    *aObject = mTarget;
    return NS_OK;
}

NS_IMETHODIMP
SingletonMailCursor::GetTruthValue(PRBool* aTruthValue)
{
    *aTruthValue = PR_TRUE;
    return NS_OK;
}


NS_IMPL_ISUPPORTS(SingletonMailCursor, kIRDFAssertionCursorIID);

////////////////////////////////////////////////////////////////////////

ArrayMailCursor::ArrayMailCursor(nsIRDFResource* u, nsIRDFResource* s, nsVoidArray* array)
    : mSource(u),
      mProperty(s),
      mArray(array),
      mCount(0),
      mTarget(nsnull),
      mValue(nsnull)
{
    //  getsources and gettargets will call this with the array
    NS_INIT_REFCNT();
    NS_ADDREF(mProperty);
    NS_ADDREF(u);
}

ArrayMailCursor::~ArrayMailCursor(void)
{
    // note that individual elements in the array are _not_ refcounted.
    NS_IF_RELEASE(mSource);
    NS_IF_RELEASE(mValue);
    NS_IF_RELEASE(mProperty);
    NS_IF_RELEASE(mTarget);
    delete mArray;
}


    // nsIRDFCursor interface
NS_IMETHODIMP
ArrayMailCursor::Advance(void)
{
    if (mArray->Count() <= mCount)
        return NS_ERROR_RDF_CURSOR_EMPTY;

    NS_IF_RELEASE(mValue);
    mTarget = mValue = (nsIRDFNode*) mArray->ElementAt(mCount++);
    NS_ADDREF(mValue);
    return NS_OK;
}

NS_IMETHODIMP
ArrayMailCursor::GetValue(nsIRDFNode** aValue)
{
    NS_ADDREF(mValue);
    *aValue = mValue;
    return NS_OK;
}

    // nsIRDFAssertionCursor interface
NS_IMETHODIMP
ArrayMailCursor::GetDataSource(nsIRDFDataSource** aDataSource)
{
    NS_ADDREF(gMailDataSource);
    *aDataSource = gMailDataSource;
    return NS_OK;
}

NS_IMETHODIMP
ArrayMailCursor::GetSubject(nsIRDFResource** aResource)
{
    NS_ADDREF(mSource);
    *aResource = mSource;
    return NS_OK;
}

NS_IMETHODIMP
ArrayMailCursor::GetPredicate(nsIRDFResource** aPredicate)
{
    NS_ADDREF(mProperty);
    *aPredicate = mProperty;
    return NS_OK;
}

NS_IMETHODIMP
ArrayMailCursor::GetObject(nsIRDFNode** aObject)
{
    NS_ADDREF(mTarget);
    *aObject = mTarget;
    return NS_OK;
}

NS_IMETHODIMP
ArrayMailCursor::GetTruthValue(PRBool* aTruthValue)
{
    *aTruthValue = PR_TRUE;
    return NS_OK;
}

NS_IMPL_ADDREF(ArrayMailCursor);
NS_IMPL_RELEASE(ArrayMailCursor);

NS_IMETHODIMP
ArrayMailCursor::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    *result = nsnull;
    if (iid.Equals(kIRDFAssertionCursorIID) ||
        iid.Equals(kIRDFCursorIID) ||
        iid.Equals(kIRDFArcsOutCursorIID) ||
        iid.Equals(kISupportsIID)) {
        *result = NS_STATIC_CAST(nsIRDFAssertionCursor*, this);
        AddRef();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}


////////////////////////////////////////////////////////////////////////

/**
 * This class creates resources for account URIs. It should be
 * registered for the "mailaccount:" prefix.
 */
class MailAccountResourceFactoryImpl : public nsIRDFResourceFactory
{
public:
    MailAccountResourceFactoryImpl(void);
    virtual ~MailAccountResourceFactoryImpl(void);

    NS_DECL_ISUPPORTS

    NS_IMETHOD CreateResource(const char* aURI, nsIRDFResource** aResult);
};

MailAccountResourceFactoryImpl::MailAccountResourceFactoryImpl(void)
{
    NS_INIT_REFCNT();
}

MailAccountResourceFactoryImpl::~MailAccountResourceFactoryImpl(void)
{
}

NS_IMPL_ISUPPORTS(MailAccountResourceFactoryImpl, kIRDFResourceFactoryIID);

NS_IMETHODIMP
MailAccountResourceFactoryImpl::CreateResource(const char* aURI, nsIRDFResource** aResult)
{
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    MailAccount* account = new MailAccount(aURI);
    if (! account)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(account);
    *aResult = account;
    return NS_OK;
}

nsresult
NS_NewRDFMailAccountResourceFactory(nsIRDFResourceFactory** aResult)
{
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    MailAccountResourceFactoryImpl* factory =
        new MailAccountResourceFactoryImpl();

    if (! factory)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(factory);
    *aResult = factory;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////

/**
 * This class creates resources for folder and message URIs. It should
 * be registered for the "mailbox:" prefix.
 */
class MailResourceFactoryImpl : public nsIRDFResourceFactory
{
public:
    MailResourceFactoryImpl(void);
    virtual ~MailResourceFactoryImpl(void);

    NS_DECL_ISUPPORTS

    NS_IMETHOD CreateResource(const char* aURI, nsIRDFResource** aResult);
};

MailResourceFactoryImpl::MailResourceFactoryImpl(void)
{
    NS_INIT_REFCNT();
}

MailResourceFactoryImpl::~MailResourceFactoryImpl(void)
{
}

NS_IMPL_ISUPPORTS(MailResourceFactoryImpl, kIRDFResourceFactoryIID);

NS_IMETHODIMP
MailResourceFactoryImpl::CreateResource(const char* aURI, nsIRDFResource** aResult)
{
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    nsIRDFResource* resource;

    if (aURI[PL_strlen(aURI) - 1] == '/') {
        resource = new MailFolder(aURI);
    } else {
        resource = new MailMessage(aURI);
    }

    if (! resource)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(resource);
    *aResult = resource;
    return NS_OK;
}

nsresult
NS_NewRDFMailResourceFactory(nsIRDFResourceFactory** aResult)
{
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    MailResourceFactoryImpl* factory =
        new MailResourceFactoryImpl();

    if (! factory)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(factory);
    *aResult = factory;
    return NS_OK;
}

