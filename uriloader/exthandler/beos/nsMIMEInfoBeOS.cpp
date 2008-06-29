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
 *    Christian Biesinger <cbiesinger@web.de>
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


#include "nsMIMEInfoBeOS.h"
#include "nsILocalFile.h"

nsMIMEInfoBeOS::~nsMIMEInfoBeOS()
{
}

nsresult
nsMIMEInfoBeOS::LaunchDefaultWithFile(nsIFile* aFile)
{
  // Launch the file, unless it is an executable.
  nsCOMPtr<nsILocalFile> local(do_QueryInterface(aFile));
  if (!local)
    return NS_ERROR_FAILURE;

  PRBool executable = PR_TRUE;
  local->IsExecutable(&executable);
  if (executable)
    return NS_ERROR_FAILURE;

  return local->Launch();
}

nsresult
nsMIMEInfoBeOS::LoadUriInternal(nsIURI * aURL)
{
	nsresult rv = NS_OK;

	if (aURL) {
		// Get the Protocol
		nsCAutoString scheme;
		aURL->GetScheme(scheme);
		BString protoStr(scheme.get());
		protoStr.Prepend("application/x-vnd.Be.URL.");
		// Get the Spec
		nsCAutoString spec;
		aURL->GetAsciiSpec(spec);
		const char* args[] = { spec.get() };
		
		//Launch the app		
		BMimeType protocol;
		bool isInstalled = false;
		if (protocol.SetTo(protoStr.String()) == B_OK)
		{
			if(protocol.IsInstalled())
			{
				isInstalled = true;	
				be_roster->Launch(protoStr.String(), NS_ARRAY_LENGTH(args), (char **)args);
			}
		}
		if ((!isInstalled) && (!strcmp("mailto", scheme.get())))
			be_roster->Launch("text/x-email", NS_ARRAY_LENGTH(args), (char **)args);
	}
	return rv;
}
