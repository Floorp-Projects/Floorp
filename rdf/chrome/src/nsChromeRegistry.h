/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

class nsIRDFService;
class nsIRDFDataSource;
class nsIRDFResource;
class nsICSSLoader;
class nsISimpleEnumerator;
class nsSupportsHashtable;
class nsIRDFContainer;
class nsIDOMWindow;
class nsIDocument;

class nsChromeRegistry : public nsIChromeRegistry
{
public:
    NS_DECL_ISUPPORTS

    // nsIChromeRegistry methods:
    NS_DECL_NSICHROMEREGISTRY

    // nsChromeRegistry methods:
    nsChromeRegistry();
    virtual ~nsChromeRegistry();

    static PRUint32 gRefCnt;
    static nsIRDFService* gRDFService;
    static nsIRDFResource* kCHROME_chrome;
    static nsIRDFResource* kCHROME_skin;
    static nsIRDFResource* kCHROME_content;
    static nsIRDFResource* kCHROME_locale;
    static nsIRDFResource* kCHROME_base;
    static nsIRDFResource* kCHROME_main;
    
    static nsIRDFResource* kCHROME_name;
    static nsIRDFResource* kCHROME_archive;
    static nsIRDFResource* kCHROME_text;
    static nsIRDFResource* kCHROME_version;
    static nsIRDFResource* kCHROME_author;
    static nsIRDFResource* kCHROME_siteURL;
    static nsIRDFResource* kCHROME_previewImageURL;
    
    static nsSupportsHashtable *mDataSourceTable;

public:
    static nsresult GetChromeResource(nsIRDFDataSource *aDataSource,
                               nsString& aResult, nsIRDFResource* aChromeResource,
                               nsIRDFResource* aProperty);

protected:
    NS_IMETHOD SelectProviderForPackage(const PRUnichar *aThemeFileName,
                                        const PRUnichar *aPackageName, 
                                        const PRUnichar *aProviderName);

    NS_IMETHOD GetOverlayDataSource(nsIURI *aChromeURL, nsIRDFDataSource **aResult);
    NS_IMETHOD InitializeDataSource(nsString &aPackage,
                                    nsString &aProvider,
                                    nsIRDFDataSource **aResult,
                                    PRBool aUseProfileDirOnly = PR_FALSE);

    nsresult GetPackageTypeResource(const nsString& aChromeType, nsIRDFResource** aResult);
    
    NS_IMETHOD RemoveOverlay(nsIRDFDataSource *aDataSource, nsIRDFResource *aResource);
    NS_IMETHOD RemoveOverlays(nsAutoString aPackage,
                              nsAutoString aProvider,
                              nsIRDFContainer *aContainer,
                              nsIRDFDataSource *aDataSource);

    NS_IMETHOD GetEnumeratorForType(const nsCAutoString& type, nsISimpleEnumerator** aResult);

private:
    NS_IMETHOD ReallyRemoveOverlayFromDataSource(const PRUnichar *aDocURI, char *aOverlayURI);
    NS_IMETHOD LoadDataSource(const nsCAutoString &aFileName, nsIRDFDataSource **aResult,
                              PRBool aUseProfileDirOnly = PR_FALSE);

    NS_IMETHOD CheckForProfileFile(const nsCAutoString& aFileName, nsCAutoString& aFileURL);
    NS_IMETHOD GetProfileRoot(nsCAutoString& aFileURL);

    NS_IMETHOD RefreshWindow(nsIDOMWindow* aWindow);

		NS_IMETHOD ProcessStyleSheet(nsIURL* aURL, nsICSSLoader* aLoader, nsIDocument* aDocument);

    NS_IMETHOD GetArcs(nsIRDFDataSource* aDataSource,
                          const nsCAutoString& aType,
                          nsISimpleEnumerator** aResult);

};
