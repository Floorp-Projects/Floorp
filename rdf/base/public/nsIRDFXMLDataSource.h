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

  This interface encapsulates information about an RDF/XML file,
  including the root resource, CSS style sheets, and named data
  sources.

  This file also includes an observer interface for nsIRDFXMLDataSource
  objects.

 */

#ifndef nsIRDFXMLDataSource_h__
#define nsIRDFXMLDataSource_h__

#include "nsIRDFDataSource.h"
class nsIAtom;
class nsIOutputStream;
class nsIURL;
class nsString;

// {EB1A5D30-AB33-11d2-8EC6-00805F29F370}
#define NS_IRDFXMLDOCUMENTOBSERVER_IID \
{ 0xeb1a5d30, 0xab33, 0x11d2, { 0x8e, 0xc6, 0x0, 0x80, 0x5f, 0x29, 0xf3, 0x70 } }

class nsIRDFXMLDataSource;

class nsIRDFXMLDataSourceObserver : public nsISupports
{
public:
    /**
     * Called when the RDF/XML document begins to load.
     */
    NS_IMETHOD OnBeginLoad(nsIRDFXMLDataSource* aStream) = 0;

    /**
     * Called when the RDF/XML document load is interrupted for some reason.
     */
    NS_IMETHOD OnInterrupt(nsIRDFXMLDataSource* aStream) = 0;

    /**
     * Called when an interrupted RDF/XML document load is resumed.
     */
    NS_IMETHOD OnResume(nsIRDFXMLDataSource* aStream) = 0;

    /**
     * Called whtn the RDF/XML document load is complete.
     */
    NS_IMETHOD OnEndLoad(nsIRDFXMLDataSource* aStream) = 0;

    /**
     * Called when the root resource of the RDF/XML document is found
     */
    NS_IMETHOD OnRootResourceFound(nsIRDFXMLDataSource* aStream, nsIRDFResource* aResource) = 0;

    /**
     * Called when a CSS style sheet is included (via XML processing
     * instruction) to the document.
     */
    NS_IMETHOD OnCSSStyleSheetAdded(nsIRDFXMLDataSource* aStream, nsIURL* aCSSStyleSheetURL) = 0;

    /**
     * Called when a named data source is included (via XML processing
     * instruction) to the document.
     */
    NS_IMETHOD OnNamedDataSourceAdded(nsIRDFXMLDataSource* aStream, const char* aNamedDataSourceURI) = 0;
};


// {EB1A5D31-AB33-11d2-8EC6-00805F29F370}
#define NS_IRDFXMLDATASOURCE_IID \
{ 0xeb1a5d31, 0xab33, 0x11d2, { 0x8e, 0xc6, 0x0, 0x80, 0x5f, 0x29, 0xf3, 0x70 } }

class nsIRDFXMLDataSource : public nsIRDFDataSource
{
public:
    /**
     * Sets the RDF/XML stream to load either synchronously or
     * asynchronously when nsIRDFDataSource::Init() is called.
     * By default, the stream will load <em>asynchronously</em>.
     */
    NS_IMETHOD SetSynchronous(PRBool aIsSynchronous) = 0;

    /**
     * Sets the RDF/XML stream's read-only status. By default,
     * the stream will be read/write if the URL on which
     * nsIRDFDataSource::Init() is called is writable.
     */
    NS_IMETHOD SetReadOnly(PRBool aIsReadOnly) = 0;

    /**
     * Notify the document that the load is beginning.
     */
    NS_IMETHOD BeginLoad(void) = 0;

    /**
     * Notify the document that the load is being interrupted.
     */
    NS_IMETHOD Interrupt(void) = 0;

    /**
     * Notify the document that an interrupted load is being resumed.
     */
    NS_IMETHOD Resume(void) = 0;

    /**
     * Notify the document that the load is ending.
     */
    NS_IMETHOD EndLoad(void) = 0;

    /**
     * Set the root resource for the document.
     */
    NS_IMETHOD SetRootResource(nsIRDFResource* aResource) = 0;

    /**
     * Retrieve the root resource for the document.
     */
    NS_IMETHOD GetRootResource(nsIRDFResource** aResource) = 0;

    /**
     * Add a CSS style sheet to the document.
     * @param aStyleSheetURL An nsIURL object that is the URL of the style
     * sheet to add to the document.
     */
    NS_IMETHOD AddCSSStyleSheetURL(nsIURL* aStyleSheetURL) = 0;

    /**
     * Get the set of style sheets that have been included in the
     * document.
     * @param aStyleSheetURLs (out) A pointer to an array of pointers to nsIURL objects.
     * @param aCount (out) The number of nsIURL objects returned.
     */
    NS_IMETHOD GetCSSStyleSheetURLs(nsIURL*** aStyleSheetURLs, PRInt32* aCount) = 0;

    /**
     * Add a named data source to the document.
     * @param aNamedDataSoruceURI A URI identifying the data source.
     */
    NS_IMETHOD AddNamedDataSourceURI(const char* aNamedDataSourceURI) = 0;

    /**
     * Get the set of named data sources that have been included in
     * the document
     * @param aNamedDataSourceURIs (out) A pointer to an array of C-style character
     * strings.
     * @param aCount (out) The number of named data sources in the array.
     */
    NS_IMETHOD GetNamedDataSourceURIs(const char* const** aNamedDataSourceURIs, PRInt32* aCount) = 0;

    /**
     * Add a new namespace declaration to the RDF/XML document.
     */
    NS_IMETHOD AddNameSpace(nsIAtom* aPrefix, const nsString& aURI) = 0;

    /**
     * Add an observer to the document. The observer will be notified of
     * RDF/XML events via the nsIRDFXMLDataSourceObserver interface. Note that
     * the observer is <em>not</em> reference counted.
     */
    NS_IMETHOD AddXMLStreamObserver(nsIRDFXMLDataSourceObserver* aObserver) = 0;

    /**
     * Remove an observer from the document.
     */
    NS_IMETHOD RemoveXMLStreamObserver(nsIRDFXMLDataSourceObserver* aObserver) = 0;
};


extern nsresult
NS_NewRDFXMLDataSource(nsIRDFXMLDataSource** result);

#endif // nsIRDFXMLDataSource_h__

