/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author(s):
 *   Chris Waterson           <waterson@netscape.com>
 *   Robert John Churchill    <rjc@netscape.com>
 *
 * Contributor(s): 
 */

#ifndef nsdirectoryviewer__h____
#define nsdirectoryviewer__h____

#include "nsNetUtil.h"
#include "nsIStreamListener.h"
#include "nsIContentViewer.h"
#include "nsIDocument.h"
#include "nsIHTTPIndex.h"
#include "nsIRDFDataSource.h"
#include "nsIDocumentLoaderFactory.h"
#include "nsITimer.h"
#include "nsISupportsArray.h"
#include "nsIRDFLiteral.h"



class nsDirectoryViewerFactory : public nsIDocumentLoaderFactory
{
public:
  nsDirectoryViewerFactory();
  virtual ~nsDirectoryViewerFactory();

  // nsISupports interface
  NS_DECL_ISUPPORTS

  // nsIDocumentLoaderFactory interface
  NS_IMETHOD CreateInstance(const char *aCommand,
                            nsIChannel* aChannel,
                            nsILoadGroup* aLoadGroup,
                            const char* aContentType, 
                            nsISupports* aContainer,
                            nsISupports* aExtraInfo,
                            nsIStreamListener** aDocListenerResult,
                            nsIContentViewer** aDocViewerResult);

  NS_IMETHOD CreateInstanceForDocument(nsISupports* aContainer,
                                       nsIDocument* aDocument,
                                       const char *aCommand,
                                       nsIContentViewer** aDocViewerResult);
};


class nsHTTPIndex : public nsIHTTPIndex, public nsIRDFDataSource
{
private:

	// note: these are NOT statics due to the native of nsHTTPIndex
	// where it may or may not be treated as a singleton

	nsIRDFResource		*kNC_Child;
	nsIRDFResource		*kNC_loading;
	nsIRDFLiteral		*kTrueLiteral;

protected:
	// We grab a reference to the content viewer container (which
	// indirectly owns us) so that we can insert ourselves as a global
	// in the script context _after_ the XUL doc has been embedded into
	// content viewer. We'll know that this has happened once we receive
	// an OnStartRequest() notification

	nsCOMPtr<nsIRDFDataSource>	mInner;
	nsCOMPtr<nsISupportsArray>	mConnectionList;
	nsCOMPtr<nsITimer>		mTimer;
	nsISupports			*mContainer;	// [WEAK]
	nsCString			mBaseURL;

			    nsHTTPIndex(nsISupports* aContainer);
	nsresult	CommonInit(void);
	nsresult 	Init(nsIURI* aBaseURL);
	PRBool		isWellknownContainerURI(nsIRDFResource *r);
static	void	FireTimer(nsITimer* aTimer, void* aClosure);

public:
			    nsHTTPIndex();
	virtual		~nsHTTPIndex();
	nsresult	Init(void);
	nsCString   mEncoding;

static	nsresult	Create(nsIURI* aBaseURI, nsISupports* aContainer, nsIHTTPIndex** aResult);

	// nsIHTTPIndex interface
	NS_DECL_NSIHTTPINDEX

	// NSIRDFDataSource interface
	NS_DECL_NSIRDFDATASOURCE

	// nsISupports interface
	NS_DECL_ISUPPORTS
};

// {82776710-5690-11d3-BE36-00104BDE6048}
#define NS_DIRECTORYVIEWERFACTORY_CID \
{ 0x82776710, 0x5690, 0x11d3, { 0xbe, 0x36, 0x0, 0x10, 0x4b, 0xde, 0x60, 0x48 } }


#endif // nsdirectoryviewer__h____
