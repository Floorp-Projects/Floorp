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
 *   Vladimir Vukicevic <vladimir@pobox.com>
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Annie Sullivan <annie.sullivan@gmail.com> (modified to work with places)
 *   Robert Sayre <sayrer@gmail.com>
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

// THE ORIGINAL LOCATION OF THIS CODE IS
// /browser/components/bookmarks/src/nsBookmarksFeedHandler.cpp

#include "nsLivemarkService.h"
#include "nsArrayEnumerator.h"
#include "nsArray.h"
#include "nsIDOMWindow.h"
#include "nsIObserverService.h"
#include "nsIRDFContainer.h"
#include "nsIRDFContainerUtils.h"
#include "nsIRDFService.h"
#include "nsIRDFXMLSerializer.h"
#include "nsIRDFXMLSource.h"
#include "nsIRDFXMLParser.h"
#include "nsRDFCID.h"
#include "nsISupportsPrimitives.h"
#include "rdf.h"
#include "nsUnicharUtils.h"
#include "nsInt64.h"

#include "nsIScriptSecurityManager.h"

#include "nsIURL.h"
#include "nsIInputStream.h"
#include "nsNetUtil.h"
#include "nsICachingChannel.h"
#include "nsICacheVisitor.h"

#include "nsIDOMParser.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMCharacterData.h"
#include "nsIDOMNodeList.h"
#include "nsIDOM3Node.h"
#include "nsIDOMDocumentTraversal.h"
#include "nsIDOMTreeWalker.h"
#include "nsIDOMNodeFilter.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIParser.h"
#include "nsParserCIID.h"
#include "nsIFragmentContentSink.h"
#include "nsIContentSink.h"
#include "nsIDocument.h"
#include "nsNavBookmarks.h"

static NS_DEFINE_CID(kCParserCID, NS_PARSER_CID);

// These are defined in nsLivemarkService.cpp
extern nsIRDFResource       *kLMRDF_type;

extern nsIRDFResource       *kLMRSS09_channel;
extern nsIRDFResource       *kLMRSS09_item;
extern nsIRDFResource       *kLMRSS09_title;
extern nsIRDFResource       *kLMRSS09_link;

extern nsIRDFResource       *kLMRSS10_channel;
extern nsIRDFResource       *kLMRSS10_items;
extern nsIRDFResource       *kLMRSS10_title;
extern nsIRDFResource       *kLMRSS10_link;

extern nsIRDFResource       *kLMDC_date;

static NS_DEFINE_CID(kRDFContainerCID,            NS_RDFCONTAINER_CID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID,   NS_RDFINMEMORYDATASOURCE_CID);

////////////////////////////////////////////////////////////////////////
// nsLivemarkLoadListener
//
// An nsIStreamListener implementation that UpdateLivemarkChildren uses
// to aysnchronously fetch and update a livemark's child entries.
//
// This could potentially become a nested class within the livemarks service.
//

class nsLivemarkLoadListener : public nsIStreamListener
{
public:
  nsLivemarkLoadListener(nsLivemarkService *aLivemarkService,
                         nsCOMPtr<nsIAnnotationService> aAnnotationService,
                         nsLivemarkService::LivemarkInfo * pLivemark)
    : mLivemarkService(aLivemarkService), mAnnotationService(aAnnotationService),
      mLivemark(pLivemark), mAborted(PR_FALSE)
  {
    NS_IF_ADDREF(mLivemarkService);
  }

  virtual ~nsLivemarkLoadListener() {
    NS_IF_RELEASE(mLivemarkService);
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIREQUESTOBSERVER

  void Abort () { mAborted = PR_TRUE; }

protected:
  static NS_METHOD StreamReaderCallback(nsIInputStream *in,
                                        void *closure,
                                        const char *fromRawSegment,
                                        PRUint32 toOffset,
                                        PRUint32 count,
                                        PRUint32 *writeCount);

  NS_METHOD TryParseAsRDF();
  NS_METHOD TryParseAsSimpleRSS();
  NS_METHOD SetResourceTTL(PRInt32 ttl);

  // helpers
  NS_METHOD HandleRDFItem (nsIRDFDataSource *aDS, nsIRDFResource *itemResource,
                           nsIRDFResource *aLinkResource, nsIRDFResource *aTitleResource);
  NS_METHOD FindTextInChildNodes (nsIDOMNode *aNode, nsAString &aString);
  NS_METHOD ParseHTMLFragment(nsAString &aFragString, nsIDocument* aTargetDocument, nsIDOMNode **outNode);

  PRBool IsLinkValid(const PRUnichar *aURI);

  nsLivemarkService *mLivemarkService;
  nsCOMPtr<nsIAnnotationService> mAnnotationService;
  nsRefPtr<nsLivemarkService::LivemarkInfo> mLivemark;
  nsCOMPtr<nsIOutputStream> mCacheStream;
  nsCOMPtr<nsIScriptSecurityManager> mSecMan;
  PRBool mAborted;
  nsCString mBody;
};

NS_IMPL_ISUPPORTS2(nsLivemarkLoadListener, nsIStreamListener, nsIRequestObserver)

NS_IMETHODIMP
nsLivemarkLoadListener::OnStartRequest(nsIRequest *aResult, nsISupports *ctxt)
{
  if (mAborted)
    return NS_ERROR_UNEXPECTED;

  mBody.Truncate();
  return NS_OK;
}

NS_METHOD
nsLivemarkLoadListener::StreamReaderCallback(nsIInputStream *aInputStream,
                                             void *aClosure,
                                             const char *aRawSegment,
                                             PRUint32 aToOffset,
                                             PRUint32 aCount,
                                             PRUint32 *aWriteCount)
{
  nsLivemarkLoadListener *rll = (nsLivemarkLoadListener *) aClosure;

  rll->mBody.Append(aRawSegment, aCount);
  *aWriteCount = aCount;

  return NS_OK;
}

NS_IMETHODIMP
nsLivemarkLoadListener::OnDataAvailable(nsIRequest *aRequest,
                                        nsISupports *aContext,
                                        nsIInputStream *aInputStream,
                                        PRUint32 aSourceOffset,
                                        PRUint32 aCount)
{
  PRUint32 totalRead;
  return aInputStream->ReadSegments(StreamReaderCallback, (void *)this, aCount, &totalRead);
}

NS_IMETHODIMP
nsLivemarkLoadListener::OnStopRequest(nsIRequest *aRequest,
                                      nsISupports *aContext,
                                      nsresult aStatus)
{
  nsresult rv;

  if (mAborted) {
    mLivemark->locked = PR_FALSE;
    return NS_OK;
  }

  if (NS_FAILED(aStatus)) {
    // Something went wrong; try to load again in 5 minutes.
    SetResourceTTL (300);
    mLivemark->locked = PR_FALSE;
    return NS_OK;
  }

  // enclose all the changes (deletes and a bunch of adds) in a batch to
  // avoid UI and DB updates
  nsBookmarksUpdateBatcher bookmarksBatch;

  // Clear out any child nodes of the livemark folder, since
  // they're about to be replaced.
  rv = mLivemarkService->DeleteLivemarkChildren(mLivemark->folderId);

  if (NS_FAILED(rv)) {
    mLivemark->locked = PR_FALSE;
    return rv;
  }

  // Grab the security manager
  mSecMan = do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID);

  // We need to parse the returned data here, stored in mBody.  We
  // try parsing as RDF first, then as Atom and the "simple" RSS
  // (the userland 0.91/0.92/2.0 formats)
  //

  // Try parsing as RDF
  rv = TryParseAsRDF();

  // Try parsing as Atom/Simple RSS
  if (!NS_SUCCEEDED(rv)) {
    rv = TryParseAsSimpleRSS();
  }

  // If we weren't able to load with anything, attach a dummy bookmark
  if (!NS_SUCCEEDED(rv)) {
    rv = mLivemarkService->InsertLivemarkFailedItem(mLivemark->folderId);
  }

  // Set an expiration on the livemark, for reloading the data
  PRInt32 ttl;
  if (NS_FAILED(rv)) {
    // if we failed, try again in 1 hour, to avoid trying
    // to load a feed that doesn't parse over and over.
    ttl = 3600;
  } else {
    // TODO -- read the pref for livemark reload time and set to max of 
    // the pref value and 1 minute.
    ttl = 3600;         // 1 hr default

    // ensure that the ttl is at least equal to the cache expiry time, since
    // otherwise a reload won't have much effect
    nsCOMPtr<nsICachingChannel> channel = do_QueryInterface(aRequest);
    if (channel) {
      nsCOMPtr<nsISupports> cacheToken;
      channel->GetCacheToken(getter_AddRefs(cacheToken));
      if (cacheToken) {
        nsCOMPtr<nsICacheEntryInfo> entryInfo = do_QueryInterface(cacheToken);
        if (entryInfo) {
          PRUint32 expiresTime;

          if (NS_SUCCEEDED(entryInfo->GetExpirationTime(&expiresTime))) {
            PRInt64 temp64, nowtime = PR_Now();
            PRUint32 nowsec;
            LL_I2L(temp64, PR_USEC_PER_SEC);
            LL_DIV(temp64, nowtime, temp64);
            LL_L2UI(nowsec, temp64);

            if (nowsec >= expiresTime) {
              expiresTime -= nowsec;
              if (ttl < (PRInt32) expiresTime)
                ttl = (PRInt32) expiresTime;
            }
          }
        }
      }
    }
  }

  rv = SetResourceTTL(ttl);
  if (NS_FAILED(rv)) {
    NS_WARNING ("SetResourceTTL failed on Livemark");
  }
    
  mLivemark->locked = PR_FALSE;

  return NS_OK;
}

/**
 * SetResourceTTL: Set the next time we should attempt to reload this
 * resource's feed
 */

nsresult
nsLivemarkLoadListener::SetResourceTTL (PRInt32 aTTL)
{
  nsresult rv;

  PRTime million, temp64, exptime = PR_Now();
  LL_I2L (million, PR_USEC_PER_SEC);
  LL_I2L (temp64, aTTL);
  LL_MUL (temp64, temp64, million);
  LL_ADD (exptime, exptime, temp64);

    
  rv = mAnnotationService->SetAnnotationInt64(mLivemark->feedURI,
                                              NS_LITERAL_CSTRING(LMANNO_EXPIRATION),
                                              exptime, 0,
                                              nsIAnnotationService::EXPIRE_NEVER);
  if (NS_FAILED(rv)) return rv;

  return NS_OK;
}

/**
 * TryParseAsRDF: attempt to parse the read data in mBody as
 * RSS 0.90/1.0 data.  This is supposed to be RDF, so we use
 * the RDF parser to do our work for us.
 */

nsresult
nsLivemarkLoadListener::TryParseAsRDF ()
{
  nsresult rv;

  nsCOMPtr<nsIRDFXMLParser> rdfparser(do_CreateInstance("@mozilla.org/rdf/xml-parser;1", &rv));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIRDFDataSource> ds(do_CreateInstance(kRDFInMemoryDataSourceCID, &rv));
  if (NS_FAILED(rv)) return rv;

  rv = rdfparser->ParseString (ds, mLivemark->feedURI, mBody);
  if (NS_FAILED(rv)) return rv;

  // Grab the (only) thing that's a channel
  // We try RSS 1.0 first, then RSS 0.9, and set up the remaining
  // resources accordingly

  nsIRDFResource *RSS_items = nsnull;
  nsIRDFResource *RSS_title = nsnull;
  nsIRDFResource *RSS_link = nsnull;

  nsCOMPtr<nsIRDFResource> channelResource = nsnull;

  rv = ds->GetSource(kLMRDF_type, kLMRSS10_channel, PR_TRUE, getter_AddRefs(channelResource));
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
  if (rv == NS_OK) {
    RSS_items = kLMRSS10_items;
    RSS_title = kLMRSS10_title;
    RSS_link = kLMRSS10_link;
  } else {
    // try RSS 0.9
    rv = ds->GetSource(kLMRDF_type, kLMRSS09_channel, PR_TRUE, getter_AddRefs(channelResource));
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
    if (rv == NS_OK) {
      RSS_items = nsnull;
      RSS_title = kLMRSS09_title;
      RSS_link = kLMRSS09_link;
    }
  }

  if (!channelResource) {
    // no channel, either 1.0 or 0.9
    return NS_ERROR_FAILURE;
  }

  // this will get filled in differently.
  nsCOMPtr<nsISimpleEnumerator> itemsEnumerator;

  if (RSS_items) {
    // if there is something that should be rss:items, then it's RSS 1.0
    nsCOMPtr<nsIRDFNode> itemsNode;
    rv = ds->GetTarget(channelResource, RSS_items, PR_TRUE, getter_AddRefs(itemsNode));
    if (NS_FAILED(rv) || rv == NS_RDF_NO_VALUE) return NS_ERROR_FAILURE;

    // items is a seq
    nsCOMPtr<nsIRDFContainer> itemsContainer = do_CreateInstance (kRDFContainerCID, &rv);
    if (NS_FAILED(rv)) return rv;
    rv = itemsContainer->Init(ds, (nsIRDFResource *) itemsNode.get());
    if (NS_FAILED(rv)) return rv;

    rv = itemsContainer->GetElements (getter_AddRefs(itemsEnumerator));
    if (NS_FAILED(rv) || rv == NS_RDF_NO_VALUE) return NS_ERROR_FAILURE;
  } else {
    //
    // if there is no rss:items, but we still were able to parse it as RDF
    // and found a channel, then it's possibly RSS 0.9.  For RSS 0.9,
    // we know that each item will be an <item ...>, so we get everything
    // that has a type of item.
    rv = ds->GetSources(kLMRDF_type, kLMRSS09_item, PR_TRUE, getter_AddRefs(itemsEnumerator));
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
  }

  // Go through each resource and pull out its link/title, if present.
  PRBool more;
  while (NS_SUCCEEDED(rv = itemsEnumerator->HasMoreElements(&more)) && more) {
    nsCOMPtr<nsISupports> iSupports;
    rv = itemsEnumerator->GetNext(getter_AddRefs(iSupports));
    if (NS_FAILED(rv)) break;

    nsCOMPtr<nsIRDFResource> item(do_QueryInterface(iSupports));
    if (!item) {
      rv = NS_ERROR_UNEXPECTED;
      break;
    }

    rv = HandleRDFItem (ds, item, RSS_link, RSS_title);
    // ignore rv
  }
  if (NS_FAILED(rv)) return rv;

  return NS_OK;
}


nsresult
nsLivemarkLoadListener::ParseHTMLFragment(nsAString &aFragString,  
                                      nsIDocument* aTargetDocument,
                                      nsIDOMNode **outNode)
{
  NS_ENSURE_ARG(aTargetDocument);
  
  nsresult rv;
  
  nsCOMPtr<nsIParser> parser;
  parser = do_CreateInstance(kCParserCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  
  // create the html fragment sink
  nsCOMPtr<nsIContentSink> sink;
  sink = do_CreateInstance(NS_HTMLFRAGMENTSINK2_CONTRACTID);
  NS_ENSURE_TRUE(sink, NS_ERROR_FAILURE);
  nsCOMPtr<nsIFragmentContentSink> fragSink(do_QueryInterface(sink));
  NS_ENSURE_TRUE(fragSink, NS_ERROR_FAILURE);
  
  fragSink->SetTargetDocument(aTargetDocument);
  
  // parse the fragment
  parser->SetContentSink(sink);
#ifdef MOZILLA_1_8_BRANCH
  parser->Parse(aFragString, (void*)0, NS_LITERAL_CSTRING("text/html"), PR_FALSE, PR_TRUE, eDTDMode_fragment);
#else
  parser->Parse(aFragString, (void*)0, NS_LITERAL_CSTRING("text/html"), PR_TRUE, eDTDMode_fragment);
#endif
  // get the fragment node
  nsCOMPtr<nsIDOMDocumentFragment> contextfrag;
  rv = fragSink->GetFragment(getter_AddRefs(contextfrag));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = CallQueryInterface (contextfrag, outNode);
  
  return rv;
}
 

// find all of the text and CDATA nodes below aNode and return them as a string
nsresult
nsLivemarkLoadListener::FindTextInChildNodes (nsIDOMNode *aNode, nsAString &aString)
{
  NS_ENSURE_ARG(aNode);
  
  nsresult rv;
  
  nsCOMPtr<nsIDOMDocument> aDoc;
  aNode->GetOwnerDocument(getter_AddRefs(aDoc));
  nsCOMPtr<nsIDOMDocumentTraversal> trav = do_QueryInterface(aDoc, &rv);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIDOMTreeWalker> walker;
  rv = trav->CreateTreeWalker(aNode, 
                              nsIDOMNodeFilter::SHOW_TEXT | nsIDOMNodeFilter::SHOW_CDATA_SECTION,
                              nsnull, PR_TRUE, getter_AddRefs(walker));
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsIDOMNode> currentNode;
  walker->GetCurrentNode(getter_AddRefs(currentNode));
  nsCOMPtr<nsIDOMCharacterData> charTextNode;
  nsAutoString tempString;
  while (currentNode) {
    charTextNode = do_QueryInterface(currentNode);
    if (charTextNode) { 
      charTextNode->GetData(tempString);
      aString.Append(tempString);
    }
    walker->NextNode(getter_AddRefs(currentNode));
  }
 
  if (aString.IsEmpty()) {
    return NS_ERROR_FAILURE;
  } else {
    return NS_OK;
  }
}

nsresult
nsLivemarkLoadListener::HandleRDFItem (nsIRDFDataSource *aDS, nsIRDFResource *aItem,
                                       nsIRDFResource *aLinkResource,
                                       nsIRDFResource *aTitleResource)
{
  nsresult rv;

  // We care about this item's link and title
  nsCOMPtr<nsIRDFNode> linkNode;
  rv = aDS->GetTarget (aItem, aLinkResource, PR_TRUE, getter_AddRefs(linkNode));
  if (NS_FAILED(rv) || rv == NS_RDF_NO_VALUE) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIRDFNode> titleNode;
  rv = aDS->GetTarget (aItem, aTitleResource, PR_TRUE, getter_AddRefs(titleNode));
  if (rv == NS_RDF_NO_VALUE) {
    rv = aDS->GetTarget (aItem, kLMDC_date, PR_TRUE, getter_AddRefs(titleNode));
  }
  if (NS_FAILED(rv) || rv == NS_RDF_NO_VALUE) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIRDFLiteral> linkLiteral(do_QueryInterface(linkNode));
  nsCOMPtr<nsIRDFLiteral> titleLiteral(do_QueryInterface(titleNode));

  // if the link/title points to something other than a literal skip it.
  if (!linkLiteral || !titleLiteral)
    return NS_ERROR_FAILURE;

  const PRUnichar *linkStr, *titleStr;
  rv = linkLiteral->GetValueConst(&linkStr);
  rv |= titleLiteral->GetValueConst(&titleStr);
  if (NS_FAILED(rv)) return rv;

  if (!IsLinkValid(linkStr))
    return NS_OK;

  // Add new bookmark with the link and title.
  nsCOMPtr<nsIURI> linkURI;
  rv = NS_NewURI(getter_AddRefs(linkURI), NS_ConvertUTF16toUTF8(linkStr));
  rv = mLivemarkService->InsertLivemarkChild(mLivemark->folderId,
                                             linkURI,
                                             nsDependentString(titleStr),
                                             nsDependentString(linkStr));
  if (NS_FAILED(rv)) return rv;

  return NS_OK;
}

/**
 * TryParseAsSimpleRSS
 *
 * Tries to parse the content as RSS (Userland) 0.91/0.92/2.0, or Atom
 * These are not RDF formats.
 */

nsresult
nsLivemarkLoadListener::TryParseAsSimpleRSS ()
{
  nsresult rv;

  nsCOMPtr<nsIDOMParser> parser(do_CreateInstance("@mozilla.org/xmlextras/domparser;1", &rv));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIDOMDocument> xmldoc;
  parser->SetBaseURI(mLivemark->feedURI);
  rv = parser->ParseFromBuffer ((const PRUint8*) mBody.get(), mBody.Length(), "text/xml", getter_AddRefs(xmldoc));
  if (NS_FAILED(rv)) return rv;

  // becomes true if we figure out that this is an atom stream.
  PRBool isAtom = PR_FALSE;

  nsCOMPtr<nsIDOMElement> docElement;
  rv = xmldoc->GetDocumentElement(getter_AddRefs(docElement));
  if (!docElement) return NS_ERROR_UNEXPECTED;
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIDOMNode> node;
  rv = xmldoc->GetFirstChild(getter_AddRefs(node));
  if (!node) return NS_ERROR_UNEXPECTED;
  if (NS_FAILED(rv)) return rv;

  PRBool lookingForChannel = PR_FALSE;

  while (node) {
    PRUint16 ntype;
    rv = node->GetNodeType(&ntype);
    if (NS_FAILED(rv)) return rv;

    if (ntype == nsIDOMNode::ELEMENT_NODE) {
      nsAutoString nname;
      rv = node->GetNodeName (nname);

      // slight hack to get node pointing to the thing that
      // has items/entries as its children.  We need to descend
      // into <channel> for RSS, but not for Atom.
      if (!lookingForChannel) {
        if (nname.Equals(NS_LITERAL_STRING("rss"))) {
          lookingForChannel = PR_TRUE;
          nsCOMPtr<nsIDOMNode> temp;
          rv = node->GetFirstChild(getter_AddRefs(temp));
          if (!temp) return NS_ERROR_UNEXPECTED;
          if (NS_FAILED(rv)) return rv;
          node = temp;
          continue;
        }
        if (nname.Equals(NS_LITERAL_STRING("feed"))) {
          // Atom has no <channel>; instead, <item>s are
          // children of <feed>
          isAtom = PR_TRUE;
          break;
        }
      } else {
        if (nname.Equals(NS_LITERAL_STRING("channel"))) {
          break;
        }
      }
    }

    nsCOMPtr<nsIDOMNode> temp;
    rv = node->GetNextSibling(getter_AddRefs(temp));
    if (!temp) return NS_ERROR_UNEXPECTED;
    if (NS_FAILED(rv)) return rv;
    node = temp;
  }

  // we didn't find a rss/feed/channel or whatever
  if (!node)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMElement> chElement = do_QueryInterface(node);
  if (!chElement) return NS_ERROR_UNEXPECTED;

  // Go through the <channel>/<feed> and do what we need
  // with <item> or <entry> nodes

  rv = chElement->GetFirstChild(getter_AddRefs(node));
  if (!node) return NS_ERROR_UNEXPECTED;
  if (NS_FAILED(rv)) return rv;

  while (node) {
    PRUint16 ntype;
    rv = node->GetNodeType(&ntype);
    if (NS_FAILED(rv)) return rv;

    if (ntype == nsIDOMNode::ELEMENT_NODE) {
      nsAutoString nname;
      rv = node->GetNodeName (nname);

      if ((!isAtom && nname.Equals(NS_LITERAL_STRING("item"))) ||
          ( isAtom && nname.Equals(NS_LITERAL_STRING("entry"))))
      {
        // We need to pull out the <title> and <link> children
        nsAutoString titleStr;
        nsAutoString linkStr;
        nsAutoString dateStr;

        nsCOMPtr<nsIDOMNode> childNode;
        rv = node->GetFirstChild(getter_AddRefs(childNode));
        if (NS_FAILED(rv)) return rv;

        while (childNode) {
          PRUint16 childNtype;
          rv = childNode->GetNodeType(&childNtype);
          if (NS_FAILED(rv)) return rv;

          if (childNtype == nsIDOMNode::ELEMENT_NODE) {
            nsAutoString childNname;
            rv = childNode->GetNodeName (childNname);

            if (childNname.Equals(NS_LITERAL_STRING("title"))) {
              if (isAtom) {
                /* Atom titles can contain HTML, so we need to find and remove it */
                nsCOMPtr<nsIDOMElement> titleElem = do_QueryInterface(childNode);
                if (!titleElem) break; // out of while(childNode) loop
                
                nsAutoString titleMode; // Atom 0.3 only
                nsAutoString titleType;
                titleElem->GetAttribute(NS_LITERAL_STRING("type"), titleType);
                titleElem->GetAttribute(NS_LITERAL_STRING("mode"), titleMode);
                
                /*  No one does this in <title> except standards pedants making test feeds, 
                 *  Atom 0.3 is deprecated, and RFC 4287 doesn't allow it
                 */
                if (titleMode.EqualsLiteral("base64")) {
                  break; // out of while(childNode) loop
                }
                
                /* Cover Atom 0.3 and RFC 4287 together */
                if (titleType.EqualsLiteral("text") ||
                    titleType.EqualsLiteral("text/plain") ||
                    titleType.IsEmpty())
                  {
                    rv = FindTextInChildNodes(childNode, titleStr);
                  } else if (titleType.EqualsLiteral("html") || // RFC 4287
                             (titleType.EqualsLiteral("text/html") && // Atom 0.3
                              !titleMode.EqualsLiteral("xml")) ||
                             titleMode.EqualsLiteral("escaped")) // Atom 0.3
                    {
                      nsAutoString escapedHTMLStr;
                      rv = FindTextInChildNodes(childNode, escapedHTMLStr);
                      if (NS_FAILED(rv)) break;  // out of while(childNode) loop
                      
                      nsCOMPtr<nsIDOMNode> newNode;
                      nsCOMPtr<nsIDocument> doc = do_QueryInterface(xmldoc);
                      ParseHTMLFragment(escapedHTMLStr, doc, getter_AddRefs(newNode));
                      rv = FindTextInChildNodes(newNode, titleStr);                   
                      
                    }else if (titleType.EqualsLiteral("xhtml") || // RFC 4287
                              titleType.EqualsLiteral("application/xhtml") || // Atom 0.3
                              titleMode.EqualsLiteral("xml")) // Atom 0.3
                      {
                        rv = FindTextInChildNodes(childNode, titleStr);
                      } else {
                        break; // out of while(childNode) loop
                      }
              } else {
                rv = FindTextInChildNodes(childNode, titleStr);
              }
              if (NS_FAILED(rv)) break;
            } else if (childNname.Equals(NS_LITERAL_STRING("pubDate")) ||
                       childNname.Equals(NS_LITERAL_STRING("updated")))
            {
              rv = FindTextInChildNodes (childNode, dateStr);
              if (NS_FAILED(rv)) break;
            } else if (!isAtom && childNname.Equals(NS_LITERAL_STRING("guid"))) {
              nsCOMPtr<nsIDOMElement> linkElem = do_QueryInterface(childNode);
              if (!linkElem) break; // out of while(childNode) loop

              nsAutoString isPermaLink;
              linkElem->GetAttribute(NS_LITERAL_STRING("isPermaLink"), isPermaLink);
              // Ignore failures. isPermaLink defaults to true.
              if (!isPermaLink.Equals(NS_LITERAL_STRING("false"))) {
                // in node's TEXT
                rv = FindTextInChildNodes (childNode, linkStr);
                if (NS_FAILED(rv)) break;
              }
            } else if (childNname.Equals(NS_LITERAL_STRING("link"))) {
              if (isAtom) { 
                // in HREF attribute
                nsCOMPtr<nsIDOMElement> linkElem = do_QueryInterface(childNode);
                if (!linkElem) break; // out of while(childNode) loop

                nsAutoString rel;
                linkElem->GetAttribute(NS_LITERAL_STRING("rel"), rel);
                if (rel.Equals(NS_LITERAL_STRING("alternate")) ||
                    rel.IsEmpty())
                {
                  rv = linkElem->GetAttribute(NS_LITERAL_STRING("href"), linkStr);
                  if (NS_FAILED(rv)) break; // out of while(childNode) loop

                  nsCOMPtr<nsIDOM3Node> linkElem3 = do_QueryInterface(childNode);
                  if (linkElem3) {
                    // get the BaseURI (as string)
                    nsAutoString base;
                    rv = linkElem3->GetBaseURI(base);
                    if (NS_SUCCEEDED(rv) && !base.IsEmpty()) {
                      // convert a baseURI (string) to a nsIURI
                      nsCOMPtr<nsIURI> baseURI;
                      rv = NS_NewURI(getter_AddRefs(baseURI), base);
                      if (baseURI) {
                        nsString absLinkStr;
                        if (NS_SUCCEEDED(NS_MakeAbsoluteURI(absLinkStr, linkStr, baseURI)))
                          linkStr = absLinkStr;
                      }
                    }
                  }
                }
              } else if (linkStr.IsEmpty()) {
                // in node's TEXT
                rv = FindTextInChildNodes (childNode, linkStr);
                if (NS_FAILED(rv)) break;
              }
            }
          }

          if (!titleStr.IsEmpty() && !linkStr.IsEmpty())
            break;

          nsCOMPtr<nsIDOMNode> temp;
          rv = childNode->GetNextSibling(getter_AddRefs(temp));
          childNode = temp;
          if (!childNode || NS_FAILED(rv)) break;
        }

        // Clean up whitespace
        titleStr.CompressWhitespace();
        linkStr.Trim("\b\t\r\n ");
        dateStr.CompressWhitespace();

        if (titleStr.IsEmpty() && !dateStr.IsEmpty())
          titleStr.Assign(dateStr);

        if (!titleStr.IsEmpty() && !linkStr.IsEmpty() && IsLinkValid(linkStr.get())) {
          nsCOMPtr<nsIURI> linkURI;
          rv = NS_NewURI(getter_AddRefs(linkURI),
                         NS_ConvertUTF16toUTF8(linkStr));
          if (NS_FAILED(rv)) return rv;
          rv = mLivemarkService->InsertLivemarkChild(mLivemark->folderId,
                                                     linkURI,
                                                     nsDependentString(titleStr),
                                                     nsDependentString(linkStr));
          if (NS_FAILED(rv)) return rv;
        }
      }
    }

    nsCOMPtr<nsIDOMNode> temp;
    rv = node->GetNextSibling(getter_AddRefs(temp));
    if (NS_FAILED(rv)) return rv;
    node = temp;
  }

  return NS_OK;
}


// return true if this link is valid and a livemark should be created;
// otherwise, false.
PRBool
nsLivemarkLoadListener::IsLinkValid(const PRUnichar *aURI)
{
  nsCOMPtr<nsIURI> linkuri;
  nsresult rv = NS_NewURI(getter_AddRefs(linkuri), nsDependentString(aURI));
  if (NS_FAILED(rv))
    return PR_FALSE;

  // Er, where'd our security manager go?
  if (!mSecMan)
    return PR_FALSE;

  rv = mSecMan->CheckLoadURI(mLivemark->feedURI, linkuri,
                             nsIScriptSecurityManager::DISALLOW_FROM_MAIL |
                             nsIScriptSecurityManager::DISALLOW_SCRIPT_OR_DATA);
  if (NS_FAILED(rv))
    return PR_FALSE;

  return PR_TRUE;
}

///////////////////////////////////////////////////////////////////////////
////  Main entry point for nsLivemarkService to deal with Livemark
////  load listener
////

/*
 * Update the child elements of a livemark; take care of cache checking,
 * channel setup and firing off the async load and parse.
 */
nsresult
nsLivemarkService::UpdateLivemarkChildren(PRInt32 aLivemarkIndex)
{
  nsresult rv;

  if (mLivemarks[aLivemarkIndex]->locked)
  {
    // We're already loading the livemark
    return NS_OK;
  }

  // Lock the livemark
  mLivemarks[aLivemarkIndex]->locked = PR_TRUE;

  // Check the TTL/expiration on this.  If there isn't one,
  // then we assume it's never been loaded.
  PRTime exprTime;
  rv = mAnnotationService->GetAnnotationInt64(mLivemarks[aLivemarkIndex]->feedURI,
                                              NS_LITERAL_CSTRING(LMANNO_EXPIRATION),
                                              &exprTime);
  if (rv == NS_OK) {
    PRTime nowTime = PR_Now();
    if (LL_CMP(exprTime, >, nowTime)) {
      // No need to refresh yet.
      mLivemarks[aLivemarkIndex]->locked = PR_FALSE;
      return rv;
    }
  } else {
    // This livemark has never been loaded, since it has no expire time.
    rv = InsertLivemarkLoadingItem(mLivemarks[aLivemarkIndex]->folderId);
  }

  // Create a load group for the request.  This will allow us to automatically
  // keep track of redirects, so we can always cancel the channel.

  nsCOMPtr<nsILoadGroup> loadGroup;
  rv = NS_NewLoadGroup(getter_AddRefs(loadGroup), nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIChannel> uriChannel;
  rv = NS_NewChannel(getter_AddRefs(uriChannel),
                     mLivemarks[aLivemarkIndex]->feedURI,
                     nsnull, loadGroup, nsnull,
                     nsIRequest::LOAD_BACKGROUND);
  if (NS_FAILED(rv)) {
    mLivemarks[aLivemarkIndex]->locked = PR_FALSE;
    return rv;
  }

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(uriChannel);
  if (httpChannel) {
    // XXXvladimir - handle POST livemarks
    rv = httpChannel->SetRequestMethod(NS_LITERAL_CSTRING("GET"));
  }
    
  nsCOMPtr<nsLivemarkLoadListener> listener = new nsLivemarkLoadListener(this,
                                                                         mAnnotationService,
                                                                         mLivemarks[aLivemarkIndex]);
  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel), 
                     mLivemarks[aLivemarkIndex]->feedURI, nsnull, nsnull, nsnull,
                     nsIRequest::LOAD_BACKGROUND | nsIRequest::VALIDATE_ALWAYS);
  if (NS_FAILED(rv)) {
    mLivemarks[aLivemarkIndex]->locked = PR_FALSE;
    return rv;
  }

  rv = channel->AsyncOpen(listener, nsnull);
  if (NS_FAILED(rv)) {
    mLivemarks[aLivemarkIndex]->locked = PR_FALSE;
    return rv;
  }

  mLivemarks[aLivemarkIndex]->loadGroup = loadGroup;
  return NS_OK;
}
