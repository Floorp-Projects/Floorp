/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Waterson           <waterson@netscape.com>
 *   Robert John Churchill    <rjc@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsdirectoryviewer__h____
#define nsdirectoryviewer__h____

#include "nsNetUtil.h"
#include "nsIStreamListener.h"
#include "nsIContentViewer.h"
#include "nsIDocument.h"
#include "nsIHTTPIndex.h"
#include "nsIRDFService.h"
#include "nsIRDFDataSource.h"
#include "nsIDocumentLoaderFactory.h"
#include "nsITimer.h"
#include "nsISupportsArray.h"
#include "nsIRDFLiteral.h"
#include "nsXPIDLString.h"
#include "nsIDirIndexListener.h"
#include "nsIFTPChannel.h"

class nsDirectoryViewerFactory : public nsIDocumentLoaderFactory
{
public:
    nsDirectoryViewerFactory();
    virtual ~nsDirectoryViewerFactory();

    // nsISupports interface
    NS_DECL_ISUPPORTS
    NS_DECL_NSIDOCUMENTLOADERFACTORY
};


class nsHTTPIndex : public nsIHTTPIndex,
                    public nsIRDFDataSource,
                    public nsIStreamListener,
                    public nsIDirIndexListener,
                    public nsIInterfaceRequestor,
                    public nsIFTPEventSink
{
private:

	// note: these are NOT statics due to the native of nsHTTPIndex
	// where it may or may not be treated as a singleton

    nsCOMPtr<nsIRDFResource>     kNC_Child;
    nsCOMPtr<nsIRDFResource>     kNC_Comment;
    nsCOMPtr<nsIRDFResource>     kNC_Loading;
    nsCOMPtr<nsIRDFResource>     kNC_URL;
    nsCOMPtr<nsIRDFResource>     kNC_Description;
    nsCOMPtr<nsIRDFResource>     kNC_ContentLength;
    nsCOMPtr<nsIRDFResource>     kNC_LastModified;
    nsCOMPtr<nsIRDFResource>     kNC_ContentType;
    nsCOMPtr<nsIRDFResource>     kNC_FileType;
    nsCOMPtr<nsIRDFResource>     kNC_IsContainer;
    nsCOMPtr<nsIRDFLiteral>      kTrueLiteral;
    nsCOMPtr<nsIRDFLiteral>      kFalseLiteral;

	nsCOMPtr<nsIRDFService>      mDirRDF;

protected:
	// We grab a reference to the content viewer container (which
	// indirectly owns us) so that we can insert ourselves as a global
	// in the script context _after_ the XUL doc has been embedded into
	// content viewer. We'll know that this has happened once we receive
	// an OnStartRequest() notification

	nsCOMPtr<nsIRDFDataSource>	 mInner;
	nsCOMPtr<nsISupportsArray>	 mConnectionList;
	nsCOMPtr<nsISupportsArray>   mNodeList;
	nsCOMPtr<nsITimer>		     mTimer;
    nsCOMPtr<nsIDirIndexParser>  mParser;
	nsCString			         mBaseURL;
	nsCString                    mEncoding;
    PRBool                       mBindToGlobalObject;
    nsIInterfaceRequestor*       mRequestor; // WEAK
    nsCOMPtr<nsIRDFResource>     mDirectory;

    nsHTTPIndex(nsIInterfaceRequestor* aRequestor);
	nsresult	CommonInit(void);
	nsresult 	Init(nsIURI* aBaseURL);
    void        GetDestination(nsIRDFResource* r, nsXPIDLCString& dest);
	PRBool		isWellknownContainerURI(nsIRDFResource *r);
    nsresult    AddElement(nsIRDFResource *parent, nsIRDFResource *prop,
                           nsIRDFNode *child);

    static void FireTimer(nsITimer* aTimer, void* aClosure);

public:
    nsHTTPIndex();
	virtual		~nsHTTPIndex();
	nsresult	Init(void);

    static	nsresult	Create(nsIURI* aBaseURI, nsIInterfaceRequestor* aContainer,
                               nsIHTTPIndex** aResult);

	// nsIHTTPIndex interface
	NS_DECL_NSIHTTPINDEX

	// NSIRDFDataSource interface
	NS_DECL_NSIRDFDATASOURCE

    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER
    
    NS_DECL_NSIDIRINDEXLISTENER
    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_NSIFTPEVENTSINK

	// nsISupports interface
	NS_DECL_ISUPPORTS
};

// {82776710-5690-11d3-BE36-00104BDE6048}
#define NS_DIRECTORYVIEWERFACTORY_CID \
{ 0x82776710, 0x5690, 0x11d3, { 0xbe, 0x36, 0x0, 0x10, 0x4b, 0xde, 0x60, 0x48 } }


#endif // nsdirectoryviewer__h____
