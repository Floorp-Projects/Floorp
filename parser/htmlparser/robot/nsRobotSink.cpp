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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
#include "nsIRobotSink.h"
#include "nsIRobotSinkObserver.h"
#include "nsIParserNode.h"
#include "nsIParser.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsIURL.h"
#include "nsIURL.h"
#include "nsIServiceManager.h"
#include "nsIIOService.h"
#include "nsNetCID.h"
#include "nsCRT.h"
#include "nsVoidArray.h"
class nsIDocument;

// TODO
// - add in base tag support
// - get links from other sources:
//      - LINK tag
//      - STYLE SRC
//      - IMG SRC
//      - LAYER SRC

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIHTMLContentSinkIID, NS_IHTML_CONTENT_SINK_IID);
static NS_DEFINE_IID(kIRobotSinkIID, NS_IROBOTSINK_IID);

class RobotSink : public nsIRobotSink {
public:
  RobotSink();
  virtual ~RobotSink();

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIHTMLContentSink
  NS_IMETHOD SetTitle(const nsString& aValue);
  NS_IMETHOD OpenHTML(const nsIParserNode& aNode);
  NS_IMETHOD CloseHTML();
  NS_IMETHOD OpenHead(const nsIParserNode& aNode);
  NS_IMETHOD CloseHead();
  NS_IMETHOD OpenBody(const nsIParserNode& aNode);
  NS_IMETHOD CloseBody();
  NS_IMETHOD OpenForm(const nsIParserNode& aNode);
  NS_IMETHOD CloseForm();
  NS_IMETHOD OpenMap(const nsIParserNode& aNode);
  NS_IMETHOD CloseMap();
  NS_IMETHOD OpenFrameset(const nsIParserNode& aNode);
  NS_IMETHOD CloseFrameset();
  NS_IMETHOD IsEnabled(PRInt32 aTag, PRBool* aReturn) { return NS_OK; }
  NS_IMETHOD_(PRBool) IsFormOnStack() { return PR_FALSE; }

  NS_IMETHOD OpenContainer(const nsIParserNode& aNode);
  NS_IMETHOD CloseContainer(const nsHTMLTag aTag);
  NS_IMETHOD CloseTopmostContainer();
  NS_IMETHOD AddHeadContent(const nsIParserNode& aNode);
  NS_IMETHOD AddLeaf(const nsIParserNode& aNode);
  NS_IMETHOD AddComment(const nsIParserNode& aNode);
  NS_IMETHOD AddProcessingInstruction(const nsIParserNode& aNode);
  NS_IMETHOD AddDocTypeDecl(const nsIParserNode& aNode);
  NS_IMETHOD WillBuildModel(void) { return NS_OK; }
  NS_IMETHOD DidBuildModel(void) { return NS_OK; }
  NS_IMETHOD WillInterrupt(void) { return NS_OK; }
  NS_IMETHOD WillResume(void) { return NS_OK; }
  NS_IMETHOD SetParser(nsIParser* aParser) { return NS_OK; }
  virtual void FlushContent(PRBool aNotify) { }
  NS_IMETHOD SetDocumentCharset(nsACString& aCharset) { return NS_OK; }
  virtual nsISupports *GetTarget() { return nsnull; }
  NS_IMETHOD WillProcessTokens(void) { return NS_OK; }
  NS_IMETHOD DidProcessTokens(void) { return NS_OK; }
  NS_IMETHOD WillProcessAToken(void) { return NS_OK; }
  NS_IMETHOD DidProcessAToken(void) { return NS_OK; }
  NS_IMETHOD NotifyTagObservers(nsIParserNode* aNode) { return NS_OK; }

  NS_IMETHOD BeginContext(PRInt32 aPosition){ return NS_OK; }
  NS_IMETHOD EndContext(PRInt32 aPosition){ return NS_OK; }

  // nsIRobotSink
  NS_IMETHOD Init(nsIURI* aDocumentURL);
  NS_IMETHOD AddObserver(nsIRobotSinkObserver* aObserver);
  NS_IMETHOD RemoveObserver(nsIRobotSinkObserver* aObserver);

  void ProcessLink(const nsString& aLink);

protected:
  nsIURI* mDocumentURL;
  nsVoidArray mObservers;
};

nsresult NS_NewRobotSink(nsIRobotSink** aInstancePtrResult)
{
  RobotSink* it = new RobotSink();
  if(it)
    return it->QueryInterface(kIRobotSinkIID, (void**) aInstancePtrResult);
  return NS_OK;
}

RobotSink::RobotSink()
{
}

RobotSink::~RobotSink()
{
  NS_IF_RELEASE(mDocumentURL);
  PRInt32 i, n = mObservers.Count();
  for (i = 0; i < n; ++i) {
    nsIRobotSinkObserver* cop = (nsIRobotSinkObserver*)mObservers.ElementAt(i);
    NS_RELEASE(cop);
  }
}  

NS_IMPL_ADDREF(RobotSink)

NS_IMPL_RELEASE(RobotSink)

NS_IMETHODIMP RobotSink::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIRobotSinkIID)) {
    *aInstancePtr = (void*) this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIHTMLContentSinkIID)) {
    *aInstancePtr = (void*) this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*) ((nsISupports*)this);
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMETHODIMP RobotSink::SetTitle(const nsString& aValue)
{
  return NS_OK;
}

NS_IMETHODIMP RobotSink::OpenHTML(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP RobotSink::CloseHTML()
{
  return NS_OK;
}

NS_IMETHODIMP RobotSink::OpenHead(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP RobotSink::CloseHead()
{
  return NS_OK;
}

NS_IMETHODIMP RobotSink::OpenBody(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP RobotSink::CloseBody()
{
  return NS_OK;
}

NS_IMETHODIMP RobotSink::OpenForm(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP RobotSink::CloseForm()
{
  return NS_OK;
}

NS_IMETHODIMP RobotSink::OpenMap(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP RobotSink::CloseMap()
{
  return NS_OK;
}

NS_IMETHODIMP RobotSink::OpenFrameset(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP RobotSink::CloseFrameset()
{
  return NS_OK;
}

NS_IMETHODIMP RobotSink::OpenContainer(const nsIParserNode& aNode)
{
  nsAutoString tmp; tmp.Assign(aNode.GetText());
  ToLowerCase(tmp);
  if (tmp.EqualsLiteral("a")) {
    nsAutoString k, v;
    PRInt32 ac = aNode.GetAttributeCount();
    for (PRInt32 i = 0; i < ac; ++i) {
      // Get upper-cased key
      const nsAString& key = aNode.GetKeyAt(i);
      k.Assign(key);
      ToLowerCase(k);
      if (k.EqualsLiteral("href")) {
        // Get value and remove mandatory quotes
        v.Truncate();
        v.Append(aNode.GetValueAt(i));
        PRUnichar first = v.First();
        if ((first == '"') || (first == '\'')) {
          if (v.Last() == first) {
            v.Cut(0, 1);
            PRInt32 pos = v.Length() - 1;
            if (pos >= 0) {
              v.Cut(pos, 1);
            }
          } else {
            // Mismatched quotes - leave them in
          }
        }
        ProcessLink(v);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP RobotSink::CloseContainer(const nsHTMLTag aTag)
{
  return NS_OK;
}

NS_IMETHODIMP RobotSink::CloseTopmostContainer()
{
  return NS_OK;
}

NS_IMETHODIMP RobotSink::AddHeadContent(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP RobotSink::AddLeaf(const nsIParserNode& aNode)
{
  return NS_OK;
}

/**
 * This gets called by the parsing system when we find a comment
 * @update	gess11/9/98
 * @param   aNode contains a comment token
 * @return  error code
 */
NS_IMETHODIMP RobotSink::AddComment(const nsIParserNode& aNode) {
  nsresult result= NS_OK;
  return result;
}

/**
 * This gets called by the parsing system when we find a PI
 * @update	gess11/9/98
 * @param   aNode contains a comment token
 * @return  error code
 */
NS_IMETHODIMP RobotSink::AddProcessingInstruction(const nsIParserNode& aNode) {
  nsresult result= NS_OK;
  return result;
}

/**
 *  This gets called by the parser when it encounters
 *  a DOCTYPE declaration in the HTML document.
 */

NS_IMETHODIMP
RobotSink::AddDocTypeDecl(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP RobotSink::Init(nsIURI* aDocumentURL)
{
  NS_IF_RELEASE(mDocumentURL);
  mDocumentURL = aDocumentURL;
  NS_IF_ADDREF(aDocumentURL);
  return NS_OK;
}

NS_IMETHODIMP RobotSink::AddObserver(nsIRobotSinkObserver* aObserver)
{
  if (mObservers.AppendElement(aObserver)) {
    NS_ADDREF(aObserver);
    return NS_OK;
  }
  return NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP RobotSink::RemoveObserver(nsIRobotSinkObserver* aObserver)
{
  if (mObservers.RemoveElement(aObserver)) {
    NS_RELEASE(aObserver);
    return NS_OK;
  }
  //XXX return NS_ERROR_NOT_FOUND;
  return NS_OK;
}

void RobotSink::ProcessLink(const nsString& aLink)
{
  nsAutoString absURLSpec; absURLSpec.Assign(aLink);

  // Make link absolute
  // XXX base tag handling
  nsIURI* docURL = mDocumentURL;
  if (nsnull != docURL) {
    nsIURI* absurl;
    nsresult rv;
    nsCOMPtr<nsIIOService> service(do_GetService(kIOServiceCID, &rv));
    if (NS_FAILED(rv)) return;

    nsIURI *uri = nsnull, *baseUri = nsnull;

    rv = mDocumentURL->QueryInterface(NS_GET_IID(nsIURI), (void**)&baseUri);
    if (NS_FAILED(rv)) return;

    NS_ConvertUCS2toUTF8 uriStr(aLink);
    rv = service->NewURI(uriStr, nsnull, baseUri, &uri);
    NS_RELEASE(baseUri);
    if (NS_FAILED(rv)) return;

    rv = uri->QueryInterface(NS_GET_IID(nsIURI), (void**)&absurl);
    NS_RELEASE(uri);

    if (NS_OK == rv) {
      absURLSpec.Truncate();
      nsCAutoString str;
      absurl->GetSpec(str);
      absURLSpec = NS_ConvertUTF8toUCS2(str);
    }
  }

  // Now give link to robot observers
  PRInt32 i, n = mObservers.Count();
  for (i = 0; i < n; ++i) {
    nsIRobotSinkObserver* cop = (nsIRobotSinkObserver*)mObservers.ElementAt(i);
    cop->ProcessLink(absURLSpec);
  }
}


