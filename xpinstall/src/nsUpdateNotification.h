/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Doug Turner <dougt@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */


#include "nsCOMPtr.h"

#include "nsIRDFDataSource.h"
#include "nsRDFCID.h"
#include "nsIRDFXMLSink.h"
#include "nsIRDFResource.h"
#include "nsIRDFService.h"

#include "VerReg.h"

#include "nsIUpdateNotification.h"

#define NS_XPI_UPDATE_NOTIFIER_DATASOURCE_CID \
{ 0x69fdc800, 0x4050, 0x11d3, { 0xbe, 0x2f, 0x0, 0x10, 0x4b, 0xde, 0x60, 0x48 } }

#define NS_XPI_UPDATE_NOTIFIER_CID \
{ 0x68a24e36, 0x042d, 0x11d4, { 0xac, 0x85, 0x0, 0xc0, 0x4f, 0xa0, 0xd2, 0x6b } }

#define NS_XPI_UPDATE_NOTIFIER_CONTRACTID "@mozilla.org/xpinstall/notifier;1"

#define BASE_DATASOURCE_URL "chrome://communicator/content/xpinstall/SoftwareUpdates.rdf"



class nsXPINotifierImpl : public nsIUpdateNotification, public nsIRDFXMLSinkObserver
{

public:
    static NS_IMETHODIMP New(nsISupports* aOuter, REFNSIID aIID, void** aResult);
    
    NS_DECL_ISUPPORTS
    NS_DECL_NSIRDFXMLSINKOBSERVER
    NS_DECL_NSIUPDATENOTIFICATION    

protected:
    
    nsXPINotifierImpl();
    virtual ~nsXPINotifierImpl();

    nsresult NotificationEnabled(PRBool* aReturn);
    nsresult Init();
    nsresult OpenRemoteDataSource(const char* aURL, PRBool blocking, nsIRDFDataSource** aResult);
    
    PRBool IsNewerOrUninstalled(const char* regKey, const char* versionString);
    PRInt32 CompareVersions(VERSION *oldversion, VERSION *newVersion);
    void   StringToVersionNumbers(const nsString& version, int32 *aMajor, int32 *aMinor, int32 *aRelease, int32 *aBuild);
    
    nsCOMPtr<nsISupports> mNotifications;
    nsIRDFService* mRDF;

    PRUint32 mPendingRefreshes;

    static nsIRDFResource* kXPI_NotifierSources;
    static nsIRDFResource* kXPI_NotifierPackages;
    static nsIRDFResource* kXPI_NotifierPackage_Title;
    static nsIRDFResource* kXPI_NotifierPackage_Version;
    static nsIRDFResource* kXPI_NotifierPackage_Description;
    static nsIRDFResource* kXPI_NotifierPackage_RegKey;

    static nsIRDFResource* kNC_NotificationRoot;
    static nsIRDFResource* kNC_Source;
	static nsIRDFResource* kNC_Name;
	static nsIRDFResource* kNC_URL;
	static nsIRDFResource* kNC_Child;
	
};
