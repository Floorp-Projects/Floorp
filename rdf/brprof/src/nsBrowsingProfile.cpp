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

#include "nsCOMPtr.h"
#include "nsIBrowsingProfile.h"
#include "nsIRDFObserver.h"
#include "nsCRT.h"
#include "rdf.h"
#include "nsIServiceManager.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsIRDFResource.h"
#include "nsIRDFDataSource.h"
#include "nsHashtable.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "prclist.h"
#include "prprf.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIRDFResourceIID, NS_IRDFRESOURCE_IID);
static NS_DEFINE_IID(kIRDFServiceIID, NS_IRDFSERVICE_IID);
static NS_DEFINE_IID(kIRDFObserverIID, NS_IRDFOBSERVER_IID);
static NS_DEFINE_IID(kIRDFIntIID, NS_IRDFINT_IID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Page);

#define OPENDIR_NAMESPACE_URI "http://directory.mozilla.org/rdf#"
DEFINE_RDF_VOCAB(OPENDIR_NAMESPACE_URI, OPENDIR, Topic);
DEFINE_RDF_VOCAB(OPENDIR_NAMESPACE_URI, OPENDIR, narrow);
DEFINE_RDF_VOCAB(OPENDIR_NAMESPACE_URI, OPENDIR, catid);

////////////////////////////////////////////////////////////////////////////////

struct nsCategory {
    PRCList     mHeader;        // for ordering
    const char* mURL;
    PRInt32     mVectorIndex;
    nsBrowsingProfileCategoryDescriptor mDescriptor;
};

////////////////////////////////////////////////////////////////////////////////

class nsBrowsingProfile : public nsIBrowsingProfile, public nsIRDFObserver {
public:
    NS_DECL_ISUPPORTS

    // nsIBrowsingProfile methods:
    NS_IMETHOD Init(const char* userProfileName);
    NS_IMETHOD GetVector(nsBrowsingProfileVector& result);
    NS_IMETHOD SetVector(nsBrowsingProfileVector& value);
    NS_IMETHOD GetCookieString(char buf[kBrowsingProfileCookieSize]);
    NS_IMETHOD SetCookieString(char buf[kBrowsingProfileCookieSize]);
    NS_IMETHOD GetDescription(char* *htmlResult);
    NS_IMETHOD CountPageVisit(const char* url);

    // nsIRDFObserver methods:
    NS_IMETHOD OnAssert(nsIRDFResource* subject,
                        nsIRDFResource* predicate,
                        nsIRDFNode* object);
    NS_IMETHOD OnUnassert(nsIRDFResource* subject,
                          nsIRDFResource* predicate,
                          nsIRDFNode* object);

    // nsBrowsingProfile methods:
    nsBrowsingProfile();
    virtual ~nsBrowsingProfile();

    nsresult RecordHit(const char* categoryURL, PRUint16 id);
    nsresult GetCategoryID(nsIRDFResource* category, PRUint16 *result);
    void UpdateVector(nsCategory* cat) {
        if (cat->mVectorIndex < nsBrowsingProfile_CategoryCount) {
            mVector.mCategory[cat->mVectorIndex] = cat->mDescriptor;
        }
    }

    static PRUint32 gRefCnt;
    static nsIRDFService* gRDFService;
    static nsIRDFDataSource* gCategoryDB; 
    static nsIRDFDataSource* gHistory;
    static nsIRDFResource* kNC_Page;
    static nsIRDFResource* kOPENDIR_Topic;
    static nsIRDFResource* kOPENDIR_narrow;
    static nsIRDFResource* kOPENDIR_catid;

    // To deal with endian-ness
    static void Uint8ToHex(PRUint8 aNum, char aBuf[2]);
    static void Uint16ToHex(PRUint16 aNum, char aBuf[4]);
    static void Uint32ToHex(PRUint32 aNum, char aBuf[8]);

    static void HexToUint8(const char aBuf[2], PRUint8* aNum);
    static void HexToUint16(const char aBuf[4], PRUint16* aNum);
    static void HexToUint32(const char aBuf[8], PRUint32* aNum);

protected:
    const char* mUserProfileName;
    nsBrowsingProfileVector mVector;
    nsHashtable mCategories;    // for fast indexing into mCategoryChain
    PRCList mCategoryChain;

    static char kHexMap[];
};

PRUint32 nsBrowsingProfile::gRefCnt = 0;
nsIRDFService* nsBrowsingProfile::gRDFService = nsnull;
nsIRDFDataSource* nsBrowsingProfile::gCategoryDB = nsnull; 
nsIRDFDataSource* nsBrowsingProfile::gHistory    = nsnull;

nsIRDFResource* nsBrowsingProfile::kNC_Page       = nsnull;
nsIRDFResource* nsBrowsingProfile::kOPENDIR_Topic = nsnull;
nsIRDFResource* nsBrowsingProfile::kOPENDIR_narrow  = nsnull;
nsIRDFResource* nsBrowsingProfile::kOPENDIR_catid = nsnull;

char nsBrowsingProfile::kHexMap[] =  "0123456789ABCDEF";


////////////////////////////////////////////////////////////////////////////////

nsBrowsingProfile::nsBrowsingProfile()
{
	NS_INIT_REFCNT();
    nsCRT::zero(&mVector, sizeof(nsBrowsingProfileVector));
    mVector.mHeader.mInfo.mCheck = nsBrowsingProfile_Check;
    mVector.mHeader.mInfo.mMajorVersion = nsBrowsingProfile_CurrentMajorVersion;
    mVector.mHeader.mInfo.mMinorVersion = nsBrowsingProfile_CurrentMinorVersion;
    PR_INIT_CLIST(&mCategoryChain);
    gRefCnt++;
}

NS_IMETHODIMP
nsBrowsingProfile::Init(const char* userProfileName)
{
    nsresult rv = NS_OK;

    mUserProfileName = userProfileName;

    if (gRefCnt == 1) {
        NS_ASSERTION(kNC_Page == nsnull, "out of sync");

        rv = nsServiceManager::GetService(kRDFServiceCID,
                                          kIRDFServiceIID,
                                          (nsISupports**)&gRDFService);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF service");
        if (NS_FAILED(rv)) return rv;

        rv = gRDFService->GetDataSource("resource:/res/samples/directory.rdf", &gCategoryDB);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get directory data source");
        if (NS_FAILED(rv)) return rv;

        rv = gRDFService->GetDataSource("rdf:history", &gHistory);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get history data source");
        if (NS_FAILED(rv)) return rv;

        // get all the properties we'll need:
        rv = gRDFService->GetResource(kURINC_Page, &kNC_Page);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get resource");
        if (NS_FAILED(rv)) return rv;
        rv = gRDFService->GetResource(kURIOPENDIR_Topic, &kOPENDIR_Topic);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get resource");
        if (NS_FAILED(rv)) return rv;
        rv = gRDFService->GetResource(kURIOPENDIR_narrow, &kOPENDIR_narrow);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get resource");
        if (NS_FAILED(rv)) return rv;
        rv = gRDFService->GetResource(kURIOPENDIR_catid, &kOPENDIR_catid);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get resource");
        if (NS_FAILED(rv)) return rv;
    }

    // XXX: TODO Grovel through the history data source to initialize
    // the profile. This gets done automagically so long as the
    // history data source creates the profile, which is kind of
    // wrong.

    // add ourself as an observer so that we can keep the profile
    // in-sync as the user browses.
    rv = gHistory->AddObserver(this);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to add self as history observer");
    return rv;
}

nsBrowsingProfile::~nsBrowsingProfile()
{
    // Stop observing the history data source
    if (gHistory)
        gHistory->RemoveObserver(this);

    PRCList* chain = &mCategoryChain;
    while (!PR_CLIST_IS_EMPTY(chain)) {
        PRCList* element = chain;
        chain = PR_NEXT_LINK(chain);
        PR_REMOVE_LINK(element);
        delete element;
    }

    --gRefCnt;
    if (gRefCnt == 0) {
        // release all the properties:
        NS_IF_RELEASE(kOPENDIR_Topic);
        NS_IF_RELEASE(kOPENDIR_catid);
        NS_IF_RELEASE(kOPENDIR_narrow);
        NS_IF_RELEASE(kNC_Page);

        if (gCategoryDB) {
            NS_RELEASE(gCategoryDB);
            gCategoryDB = nsnull;
        }

        if (gHistory) {
            NS_RELEASE(gHistory);
            gHistory = nsnull;
        }

        if (gRDFService) {
            nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
            gRDFService = nsnull;
        }
    }
}

NS_IMPL_ADDREF(nsBrowsingProfile)
NS_IMPL_RELEASE(nsBrowsingProfile)

NS_IMETHODIMP
nsBrowsingProfile::QueryInterface(REFNSIID aIID, void** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (aIID.Equals(nsIBrowsingProfile::GetIID()) ||
        aIID.Equals(kISupportsIID)) {
        *aResult = NS_STATIC_CAST(nsIBrowsingProfile*, this);
        NS_ADDREF(this);
        return NS_OK;
    }
    if (aIID.Equals(kIRDFObserverIID)) {
        *aResult = NS_STATIC_CAST(nsIRDFObserver*, this);
        NS_ADDREF(this);
        return NS_OK;
    }
    return NS_NOINTERFACE;
}

////////////////////////////////////////////////////////////////////////////////
// nsIBrowsingProfile methods:

NS_IMETHODIMP
nsBrowsingProfile::GetVector(nsBrowsingProfileVector& result)
{
    result = mVector;   // copy
    return NS_OK;
}

NS_IMETHODIMP
nsBrowsingProfile::SetVector(nsBrowsingProfileVector& value)
{
    mVector = value;   // copy
    return NS_OK;
}

NS_IMETHODIMP
nsBrowsingProfile::GetCookieString(char buf[kBrowsingProfileCookieSize])
{
    // translate mVector to hex

    char* p = buf;

    Uint32ToHex(mVector.mHeader.mInfo.mCheck, p);
    p += 8;

    Uint16ToHex(mVector.mHeader.mInfo.mMajorVersion, p);
    p += 4;

    Uint16ToHex(mVector.mHeader.mInfo.mMinorVersion, p);
    p += 4;

    while (p < buf + sizeof(mVector.mHeader))
        *p++ = '0'; // pad with zeroes

    for (PRInt32 i = 0; i < nsBrowsingProfile_CategoryCount; ++i) {
        Uint16ToHex(mVector.mCategory[i].mID, p);
        p += 4;

        Uint8ToHex(mVector.mCategory[i].mVisitCount, p);
        p += 2;

        Uint8ToHex(mVector.mCategory[i].mFlags, p);
        p += 2;
    }

    *p = '\0';
    return NS_OK;
}

NS_IMETHODIMP
nsBrowsingProfile::SetCookieString(char buf[kBrowsingProfileCookieSize])
{
    // translate mVector from hex

    char* p = buf;
    HexToUint32(p, &mVector.mHeader.mInfo.mCheck);
    p += 8;

    HexToUint16(p, &mVector.mHeader.mInfo.mMajorVersion);
    p += 4;

    HexToUint16(p, &mVector.mHeader.mInfo.mMinorVersion);
    //p += 4;

    p = buf + sizeof(mVector.mHeader);
    for (PRInt32 i = 0; i < nsBrowsingProfile_CategoryCount; ++i) {
        HexToUint16(p, &mVector.mCategory[i].mID);
        p += 4;

        HexToUint8(p, &mVector.mCategory[i].mVisitCount);
        p += 2;

        HexToUint8(p, &mVector.mCategory[i].mFlags);
        p += 2;
    }
    
    return NS_OK;
}

NS_IMETHODIMP
nsBrowsingProfile::GetDescription(char* *htmlResult)
{
    // generate some nice html
    // XXX really wish I had an nsStringStream here to use

    nsresult rv;
    char* buf = PR_smprintf("<h1>Browsing Profile</h1>format version %d.%d",
                            mVector.mHeader.mInfo.mMajorVersion, 
                            mVector.mHeader.mInfo.mMinorVersion);
    if (buf == nsnull)
        return NS_ERROR_OUT_OF_MEMORY; 

    for (PRUint32 i = 0; i < nsBrowsingProfile_CategoryCount; i++) {
        nsBrowsingProfileCategoryDescriptor* desc = &mVector.mCategory[i];
        nsIRDFInt* intLit;
        rv = gRDFService->GetIntLiteral(desc->mID, &intLit);
        nsIRDFResource* category;
        rv = gCategoryDB->GetSource(kOPENDIR_catid, category, PR_TRUE, &category);
        nsXPIDLCString uri;
        rv = category->GetValue( getter_Copies(uri) );
        char* buf2 = PR_smprintf("%s%s: %d<b>", buf, (const char*) uri, desc->mVisitCount);
        PR_smprintf_free(buf);
        if (buf2 == nsnull)
            return NS_ERROR_OUT_OF_MEMORY; 
        buf = buf2;
    }
    *htmlResult = buf;
    return NS_OK;
}

NS_IMETHODIMP
nsBrowsingProfile::CountPageVisit(const char* initialURL)
{
    // Here's where the real work is:
    // Find the url in the directory, and get the category ID.
    // Then increment the count (and set the flags) for that category ID
    // in the vector.
    
    nsresult rv = NS_OK;
    PRInt32 pos;

    nsAutoString urlStr(initialURL);
    // first chop off any query part of the initialURL
    pos = urlStr.RFind("?");
    if (pos >= 0) {
        urlStr.Cut(pos, urlStr.Length());
    }
    pos = urlStr.RFind("#");
    if (pos >= 0) {
        urlStr.Cut(pos, urlStr.Length());
    }

    PRBool done = PR_FALSE;
    do {
        char* url = urlStr.ToNewCString();
        if (url == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
        
        nsIRDFResource* urlRes;
        rv = gRDFService->GetResource(url, &urlRes);
        if (NS_SUCCEEDED(rv)) {
            nsISimpleEnumerator* sources;
            rv = gCategoryDB->GetSources(kOPENDIR_narrow, urlRes, PR_TRUE, &sources);
            if (NS_SUCCEEDED(rv)) {
                while (1) {
                    PRBool hasMore;
                    rv = sources->HasMoreElements(&hasMore);
                    if (NS_FAILED(rv)) {
                        done = PR_TRUE;
                        break;
                    }

                    if (! hasMore)
                        break;

                    nsISupports* isupports;
                    rv = sources->GetNext(&isupports);
                    if (NS_SUCCEEDED(rv)) {
                        nsCOMPtr<nsIRDFResource> category = do_QueryInterface(isupports);
                        if (category) {
                            // found this page in a category -- count it
                            PRUint16 id;
                            rv = GetCategoryID(category, &id);
                            if (NS_SUCCEEDED(rv)) {
                                nsXPIDLCString catURI;
                                rv = category->GetValue( getter_Copies(catURI) );
                                if (NS_SUCCEEDED(rv)) {
                                    rv = RecordHit(catURI, id);
                                }
                            }
                        }
                        NS_RELEASE(isupports);
                        done = PR_TRUE;
                    }
                }
                NS_RELEASE(sources);
            }
            NS_RELEASE(urlRes);
        }
        delete[] url;

        // we didn't find this page exactly, but see if some parent directory 
        // url is there
        if (!done) {
            // if it already ends with a one or more slashes, rip them off.
            while (urlStr.Length() > 0 && urlStr.Last() == PRUnichar('/')) {
                urlStr.Truncate(urlStr.Length() - 1);
            }

            // _Now_ find the right most forward-slash
            pos = urlStr.RFind("/");

            if (pos >= 0) {
                // leave the last '/', as this is the way most opendir
                // entries are specified; for example,
                //    http://www.amazon.com/
                urlStr.Cut(pos + 1, urlStr.Length());
            }
            else {
                done = PR_TRUE;
            }
        }
    } while (!done);

    return rv;
}

nsresult
nsBrowsingProfile::GetCategoryID(nsIRDFResource* category, PRUint16 *result)
{
    nsresult rv;
    nsCOMPtr<nsIRDFNode> catID;
    rv = gCategoryDB->GetTarget(category, kOPENDIR_catid, PR_TRUE, getter_AddRefs(catID));
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get category ID");
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIRDFLiteral> catIDLiteral(do_QueryInterface(catID));
    NS_ASSERTION(catID != nsnull, "not a literal");
    if (! catID) return NS_ERROR_NO_INTERFACE;

    nsXPIDLString idStr;
    rv = catIDLiteral->GetValue( getter_Copies(idStr) );
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get literal value");

    PRInt32 err;
    *result = nsAutoString(idStr).ToInteger(&err);
    NS_ASSERTION(err == 0, "error converting to integer");

    return (err == 0) ? NS_OK : NS_ERROR_FAILURE;
}

nsresult
nsBrowsingProfile::RecordHit(const char* categoryURL, PRUint16 id)
{
    nsCStringKey key(categoryURL);
    nsCategory* cat = NS_STATIC_CAST(nsCategory*, mCategories.Get(&key));
    if (cat == nsnull) {
        nsCategory* cat = new nsCategory;
        if (cat == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
        cat->mURL = categoryURL;
        cat->mVectorIndex = 0;
        cat->mDescriptor.mID = id;
        cat->mDescriptor.mVisitCount = 1;
        cat->mDescriptor.mFlags = 0;

        // find the right place to insert this
        PRCList* end   = &mCategoryChain;
        PRCList* chain = PR_NEXT_LINK(&mCategoryChain);
        nsCategory* other;
        PRInt32 count = 0;

        while (chain != end) {
            other = (nsCategory*)chain;
            if (cat->mDescriptor.mVisitCount >= other->mDescriptor.mVisitCount)
                break;

            chain = PR_NEXT_LINK(chain);
            ++count;
        }

        // do the deed
        PR_INSERT_BEFORE(&cat->mHeader, chain);
        cat->mVectorIndex = (chain != end) ? other->mVectorIndex : count;
        UpdateVector(cat);

        // slide everybody else down
        for (; chain != end; chain = PR_NEXT_LINK(chain)) {
            other = (nsCategory*)chain;
            other->mVectorIndex++;
            UpdateVector(other);
        }

        // and insert this in the lookup table
        mCategories.Put(&key, cat);
    }
    else {
        cat->mDescriptor.mVisitCount++;
        if (PR_PREV_LINK(&cat->mHeader) != &mCategoryChain) {
            // it's not the first element already
            nsCategory* prev = (nsCategory*)PR_PREV_LINK(&cat->mHeader);
            if (cat->mDescriptor.mVisitCount >= prev->mDescriptor.mVisitCount) {
                // if we got more hits on this category then it's predecessor 
                // then reorder the chain
                PR_REMOVE_LINK(&cat->mHeader);
                PR_INSERT_BEFORE(&cat->mHeader, &prev->mHeader);
                cat->mVectorIndex = prev->mVectorIndex++;
                UpdateVector(prev);
            }
        }
        UpdateVector(cat);
    }
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIRDFObserver methods:

NS_IMETHODIMP
nsBrowsingProfile::OnAssert(nsIRDFResource* subject,
                            nsIRDFResource* predicate,
                            nsIRDFNode* object)
{
    nsresult rv = NS_OK;
    if (predicate == kNC_Page) {
        nsIRDFResource* objRes;
        rv = object->QueryInterface(kIRDFResourceIID, (void**)&objRes);
        if (NS_FAILED(rv)) return rv;
        nsXPIDLCString url;
        rv = objRes->GetValue( getter_Copies(url) );
        if (NS_SUCCEEDED(rv)) {
            rv = CountPageVisit(url);
        }
        NS_RELEASE(objRes);
    }
    return rv;
}

NS_IMETHODIMP
nsBrowsingProfile::OnUnassert(nsIRDFResource* subject,
                              nsIRDFResource* predicate,
                              nsIRDFNode* object)
{
    // we don't care about history entries going away
    return NS_OK;
}


void
nsBrowsingProfile::Uint8ToHex(PRUint8 aNum, char aBuf[2])
{
    char* p = aBuf + 2;
    while (--p >= aBuf) {
        *p = kHexMap[aNum & 0x0f];
        aNum = aNum >> 4;
    }
}

void
nsBrowsingProfile::Uint16ToHex(PRUint16 aNum, char aBuf[4])
{
    char* p = aBuf + 4;
    while (--p >= aBuf) {
        *p = kHexMap[aNum & 0x0f];
        aNum = aNum >> 4;
    }
}

void
nsBrowsingProfile::Uint32ToHex(PRUint32 aNum, char aBuf[8])
{
    char* p = aBuf + 8;
    while (--p >= aBuf) {
        *p = kHexMap[aNum & 0x0f];
        aNum = aNum >> 4;
    }
}


void
nsBrowsingProfile::HexToUint8(const char aBuf[2], PRUint8* aNum)
{
    PRUint32 num = 0;
    for (PRInt32 count = 2; count > 0; --count) {
        const char* hex = PL_strchr(kHexMap, *aBuf);
        NS_ASSERTION(hex != nsnull, "invalid character");
        if (! hex)
            break;

        num = num << 4;
        num += (hex - kHexMap);
        ++aBuf;
    }
    *aNum = num;
}

void
nsBrowsingProfile::HexToUint16(const char aBuf[4], PRUint16* aNum)
{
    PRUint32 num = 0;
    for (PRInt32 count = 4; count > 0; --count) {
        const char* hex = PL_strchr(kHexMap, *aBuf);
        NS_ASSERTION(hex != nsnull, "invalid character");
        if (! hex)
            break;

        num = num << 4;
        num += (hex - kHexMap);
        ++aBuf;
    }
    *aNum = num;
}

void
nsBrowsingProfile::HexToUint32(const char aBuf[8], PRUint32* aNum)
{
    PRUint32 num = 0;
    for (PRInt32 count = 8; count > 0; --count) {
        const char* hex = PL_strchr(kHexMap, *aBuf);
        NS_ASSERTION(hex != nsnull, "invalid character");
        if (! hex)
            break;

        num = num << 4;
        num += (hex - kHexMap);
        ++aBuf;
    }
    *aNum = num;
}



////////////////////////////////////////////////////////////////////////////////

nsresult
NS_NewBrowsingProfile(nsIBrowsingProfile* *aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    nsBrowsingProfile* profile = new nsBrowsingProfile();
    if (profile == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(profile);
    *aResult = profile;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
