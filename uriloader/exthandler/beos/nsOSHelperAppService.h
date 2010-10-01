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
 * The Original Code is nsOSHelperAppService.h. 
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

#ifndef nsOSHelperAppService_h__
#define nsOSHelperAppService_h__

// The OS helper app service is a subclass of nsExternalHelperAppService and is implemented on each
// platform. It contains platform specific code for finding helper applications for a given mime type
// in addition to launching those applications.

#include "nsExternalHelperAppService.h"
#include "nsCExternalHandlerService.h"
#include "nsMIMEInfoImpl.h"
#include "nsCOMPtr.h"

class nsMIMEInfoBeOS;

class nsOSHelperAppService : public nsExternalHelperAppService
{
public:
	nsOSHelperAppService();
	virtual ~nsOSHelperAppService();

	already_AddRefed<nsIMIMEInfo> GetMIMEInfoFromOS(const nsACString& aMIMEType, const nsACString& aFileExt, PRBool *aFound);
	NS_IMETHOD GetProtocolHandlerInfoFromOS(const nsACString &aScheme,
	                                        PRBool *found,
	                                        nsIHandlerInfo **_retval);

	// override nsIExternalProtocolService methods
	nsresult OSProtocolHandlerExists(const char * aProtocolScheme, PRBool * aHandlerExists);

protected:
	nsresult SetMIMEInfoForType(const char *aMIMEType, nsMIMEInfoBeOS **_retval);
	nsresult GetMimeInfoFromExtension(const char *aFileExt, nsMIMEInfoBeOS **_retval);
	nsresult GetMimeInfoFromMIMEType(const char *aMIMEType, nsMIMEInfoBeOS **_retval);
};

#endif // nsOSHelperAppService_h__
