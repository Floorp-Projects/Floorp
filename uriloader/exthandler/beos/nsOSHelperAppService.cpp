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
 * The Original Code is nsOSHelperAppService.cpp. 
 * 
 * The Initial Developer of the Original Code is 
 * Paul Ashford. 
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved. 
 * 
 * Contributor(s): 
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

#include "nsOSHelperAppService.h"
#include "nsISupports.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsIURL.h"
#include "nsILocalFile.h"
#include "nsIProcess.h"
#include "prenv.h"      // for PR_GetEnv()
#include <stdlib.h>		// for system()

#include <Message.h>
#include <Mime.h>
#include <String.h>
#include <Path.h>
#include <Entry.h>
#include <Roster.h>

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG /* Allow logging in the release build */
#endif /* MOZ_LOGGING */
#include "prlog.h"

#ifdef PR_LOGGING
static PRLogModuleInfo *gOSHelperLog = PR_NewLogModule("nsOSHelperAppService");
#endif /* PR_LOGGING */

#define LOG(args) PR_LOG(gOSHelperLog, PR_LOG_DEBUG, args)
#define LOG_ENABLED() PR_LOG_TEST(gOSHelperLog, PR_LOG_DEBUG)

nsOSHelperAppService::nsOSHelperAppService() : nsExternalHelperAppService()
{
}

nsOSHelperAppService::~nsOSHelperAppService()
{}

NS_IMETHODIMP nsOSHelperAppService::LaunchAppWithTempFile(nsIMIMEInfo * aMIMEInfo, nsIFile * aTempFile)
{
	nsresult rv = NS_OK;

	LOG(("nsOSHelperAppService::LaunchAppWithTempFile\n"));

	if (aMIMEInfo)
	{
		nsCOMPtr<nsIFile> application;
		nsCAutoString path;
		aTempFile->GetNativePath(path);

		nsMIMEInfoHandleAction action = nsIMIMEInfo::useSystemDefault;
		aMIMEInfo->GetPreferredAction(&action);

		aMIMEInfo->GetPreferredApplicationHandler(getter_AddRefs(application));
		if (application && action == nsIMIMEInfo::useHelperApp)
		{
			// if we were given an application to use then use it....otherwise
			// make the registry call to launch the app
			const char * strPath = path.get();
			nsCOMPtr<nsIProcess> process = do_CreateInstance(NS_PROCESS_CONTRACTID);
			nsresult rv;
			if (NS_FAILED(rv = process->Init(application)))
				return rv;
			PRUint32 pid;
			if (NS_FAILED(rv = process->Run(PR_FALSE, &strPath, 1, &pid)))
				return rv;
		}
		else // use the system default
		{
			// Launch the temp file, unless it is an executable.
			nsCOMPtr<nsILocalFile> local(do_QueryInterface(aTempFile));
			if (!local)
				return NS_ERROR_FAILURE;

			PRBool executable = PR_TRUE;
			local->IsExecutable(&executable);
			if (executable)
				return NS_ERROR_FAILURE;
			
			rv = local->Launch();
		}
	}

	return rv;
}


NS_IMETHODIMP nsOSHelperAppService::ExternalProtocolHandlerExists(const char * aProtocolScheme, PRBool * aHandlerExists)
{
	LOG(("-- nsOSHelperAppService::ExternalProtocolHandlerExists for '%s'\n",
	     aProtocolScheme));
	// look up the protocol scheme in the MIME database
	*aHandlerExists = PR_FALSE;
	if (aProtocolScheme && *aProtocolScheme)
	{
		BString protoStr(aProtocolScheme);
		protoStr.Prepend("application/x-vnd.Be.URL.");
		BMimeType protocol;
		if (protocol.SetTo(protoStr.String()) == B_OK) {
			*aHandlerExists = PR_TRUE;
		}
	}

	return NS_OK;
}

NS_IMETHODIMP nsOSHelperAppService::LoadUrl(nsIURI * aURL)
{
	LOG(("-- nsOSHelperAppService::LoadUrl\n"));
	nsresult rv = NS_OK;

	if (aURL) {
		// Get the Protocol
		nsCAutoString scheme;
		aURL->GetScheme(scheme);
		BString protoStr(scheme.get());
		protoStr.Prepend("application/x-vnd.Be.URL.");
		// Get the Spec
		nsCAutoString spec;
		aURL->GetSpec(spec);
		// Set up the BRoster message
		BMessage args(B_ARGV_RECEIVED);
		args.AddInt32("argc",1);
		args.AddString("argv",spec.get());

		be_roster->Launch(protoStr.String(), &args);
	}
	return rv;
}


nsresult nsOSHelperAppService::SetMIMEInfoForType(const char *aMIMEType, nsIMIMEInfo **_retval) {

	LOG(("-- nsOSHelperAppService::SetMIMEInfoForType: %s\n",aMIMEType));

	nsresult rv = NS_ERROR_FAILURE;

	nsCOMPtr<nsIMIMEInfo> mimeInfo (do_CreateInstance(NS_MIMEINFO_CONTRACTID));
	if (mimeInfo) {
		BMimeType mimeType(aMIMEType);
		BMessage data;
		mimeInfo->SetMIMEType(aMIMEType);
		int32 index = 0;
		BString strData;
		LOG(("   Adding extensions:\n"));
		if (mimeType.GetFileExtensions(&data) == B_OK) {
			while (data.FindString("extensions",index,&strData) == B_OK) {
				// if the file extension includes the '.' then we don't want to include that when we append
				// it to the mime info object.
				if (strData.ByteAt(0) == '.')
					strData.RemoveFirst(".");
				mimeInfo->AppendExtension(strData.String());
				LOG(("      %s\n",strData.String()));
				index++;
			}
		}

		char desc[B_MIME_TYPE_LENGTH + 1];
		if (mimeType.GetShortDescription(desc) == B_OK) {
			mimeInfo->SetDescription(NS_ConvertUTF8toUCS2(desc).get());
		} else {
			if (mimeType.GetLongDescription(desc) == B_OK) {
				mimeInfo->SetDescription(NS_ConvertUTF8toUCS2(desc).get());
			} else {
				mimeInfo->SetDescription(NS_ConvertUTF8toUCS2(aMIMEType).get());
			}
		}
		
		LOG(("    Description: %s\n",desc));

		//set preferred app and app description
		char appSig[B_MIME_TYPE_LENGTH + 1];
		bool doSave = true;
		if (mimeType.GetPreferredApp(appSig) == B_OK) {
			LOG(("    Got preferred ap\n"));
			BMimeType app(appSig);
			entry_ref ref;
			BEntry entry;
			BPath path;
			if ((app.GetAppHint(&ref) == B_OK) &&
			        (entry.SetTo(&ref, false) == B_OK) &&
			        (entry.GetPath(&path) == B_OK)) {

				LOG(("    Got our path!\n"));
				nsCOMPtr<nsIFile> handlerFile;
				rv = GetFileTokenForPath(NS_ConvertUTF8toUCS2(path.Path()).get(), getter_AddRefs(handlerFile));

				if (NS_SUCCEEDED(rv)) {
					mimeInfo->SetPreferredApplicationHandler(handlerFile);
					mimeInfo->SetPreferredAction(nsIMIMEInfo::useHelperApp);
					mimeInfo->SetApplicationDescription(NS_ConvertUTF8toUCS2(path.Leaf()).get());
					LOG(("    Preferred App: %s\n",path.Leaf()));
					doSave = false;
				}
			}
		}
		if (doSave) {
			mimeInfo->SetPreferredAction(nsIMIMEInfo::saveToDisk);
			LOG(("    No Preferred App\n"));
		}

		*_retval = mimeInfo;
		NS_ADDREF(*_retval);
		rv = NS_OK;
	}
	else
		rv = NS_ERROR_FAILURE;

	return rv;
}

NS_IMETHODIMP nsOSHelperAppService::GetFromExtension(const char *aFileExt,
        nsIMIMEInfo ** _retval) {
	// if the extension is null, return immediately
	if (!aFileExt || !*aFileExt)
		return NS_ERROR_INVALID_ARG;

	// first, see if the base class already has an entry....
	nsresult rv = nsExternalHelperAppService::GetFromExtension(aFileExt, _retval);
	if (NS_SUCCEEDED(rv) && *_retval) return NS_OK; // okay we got an entry so we are done.

	LOG(("Here we do an extension lookup for '%s'\n", aFileExt));

	BString fileExtToUse(aFileExt);
	if (fileExtToUse.ByteAt(0) != '.')
		fileExtToUse.Prepend(".");


	BMessage mimeData;
	BMessage extData;
	BMimeType mimeType;
	int32 mimeIndex = 0;
	int32 extIndex = 0;
	bool found = false;
	BString mimeStr;
	BString extStr;
	// Get a list of all registered MIME types
	if (BMimeType::GetInstalledTypes(&mimeData) == B_OK) {
		// check to see if the given MIME type is registerred
		while (!found && mimeData.FindString("types",mimeIndex,&mimeStr) == B_OK) {
			if ((mimeType.SetTo(mimeStr.String()) == B_OK) &&
			        (mimeType.GetFileExtensions(&extData) == B_OK)) {
				extIndex = 0;
				while (!found && extData.FindString("extensions",extIndex,&extStr) == B_OK) {
					if (extStr.ByteAt(0) != '.')
						extStr.Prepend(".");
					if (fileExtToUse.ICompare(extStr) == 0)
						found = true;
					else
						extIndex++;
				}
			}
			mimeIndex++;
		}
		if (found) {
			rv = SetMIMEInfoForType(mimeStr.String(), _retval);
		}
	}

	return rv;
}

NS_IMETHODIMP nsOSHelperAppService::GetFromMIMEType(const char *aMIMEType,
        nsIMIMEInfo ** _retval) {
	// if the mime type is null, return immediately
	if (!aMIMEType)
		return NS_ERROR_INVALID_ARG;

	// first, see if the base class already has an entry....
	nsresult rv = nsExternalHelperAppService::GetFromMIMEType(aMIMEType, _retval);
	if (NS_SUCCEEDED(rv) && *_retval) return NS_OK; // okay we got an entry so we are done.

	LOG(("Here we do a mimetype lookup for '%s'\n", aMIMEType));

	BMessage data;
	int32 index = 0;
	bool found = false;
	BString strData;
	// Get a list of all registerred MIME types
	if (BMimeType::GetInstalledTypes(&data) == B_OK) {
		// check to see if the given MIME type is registerred
		while (!found && data.FindString("types",index,&strData) == B_OK) {
			if (strData == aMIMEType)
				found = true;
			else
				index++;
		}
		if (found) {
			rv = SetMIMEInfoForType(aMIMEType, _retval);
		}
	}

	return rv;
}

nsresult nsOSHelperAppService::GetFileTokenForPath(const PRUnichar* platformAppPath, nsIFile ** aFile)
{
	LOG(("-- nsOSHelperAppService::GetFileTokenForPath: '%s'\n", NS_ConvertUCS2toUTF8(platformAppPath)));
	nsCOMPtr<nsILocalFile> localFile (do_CreateInstance(NS_LOCAL_FILE_CONTRACTID));
	nsresult rv = NS_OK;

	if (!localFile) return NS_ERROR_NOT_INITIALIZED;

	// A BPath will convert relative paths automagically
	BPath path;
	PRBool exists = PR_FALSE;
	if (path.SetTo(NS_ConvertUCS2toUTF8(platformAppPath).get()) == B_OK) {
		localFile->InitWithPath(NS_ConvertUTF8toUCS2(path.Path()));
		localFile->Exists(&exists);
	}

	if (exists) {
		rv = NS_OK;
	} else {
		rv = NS_ERROR_NOT_AVAILABLE;
	}

	*aFile = localFile;
	NS_IF_ADDREF(*aFile);

	return rv;
}
