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
 * The Original Code is the Windows MIME Info Implementation.
 *
 * The Initial Developer of the Original Code is
 * Christian Biesinger <cbiesinger@web.de>.
 * Portions created by the Initial Developer are Copyright (C) 2004
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

#ifndef nsMIMEInfoWin_h_
#define nsMIMEInfoWin_h_

#include "nsMIMEInfoImpl.h"
#include "nsIPropertyBag.h"
#include "nsIMutableArray.h"
#include "nsTArray.h"

class nsMIMEInfoWin : public nsMIMEInfoBase, public nsIPropertyBag {
  public:
    nsMIMEInfoWin(const char* aType = "") : nsMIMEInfoBase(aType) {}
    nsMIMEInfoWin(const nsACString& aMIMEType) : nsMIMEInfoBase(aMIMEType) {}
    nsMIMEInfoWin(const nsACString& aType, HandlerClass aClass) :
      nsMIMEInfoBase(aType, aClass) {}
    virtual ~nsMIMEInfoWin();

    NS_IMETHOD LaunchWithFile(nsIFile* aFile);
    NS_IMETHOD GetHasDefaultHandler(bool * _retval);
    NS_IMETHOD GetPossibleLocalHandlers(nsIArray **_retval); 

    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIPROPERTYBAG

    void SetDefaultApplicationHandler(nsIFile* aDefaultApplication) 
    { 
      mDefaultApplication = aDefaultApplication; 
    }

  protected:
    virtual NS_HIDDEN_(nsresult) LoadUriInternal(nsIURI *aURI);
    virtual nsresult LaunchDefaultWithFile(nsIFile* aFile);

  private:
    nsCOMPtr<nsIFile>      mDefaultApplication;
    
    // Given a path to a local handler, return its 
    // nsILocalHandlerApp instance.
    bool GetLocalHandlerApp(const nsAString& aCommandHandler,
                              nsCOMPtr<nsILocalHandlerApp>& aApp);

    // Return the cleaned up file path associated 
    // with a command verb located in root/Applications.
    bool GetAppsVerbCommandHandler(const nsAString& appExeName,
                                     nsAString& applicationPath,
                                     bool bEdit);

    // Return the cleaned up file path associated 
    // with a progid command verb located in root.
    bool GetProgIDVerbCommandHandler(const nsAString& appProgIDName,
                                       nsAString& applicationPath,
                                       bool bEdit);

    // Lookup a rundll command handler and return
    // a populated command template for use with rundll32.exe.
    bool GetDllLaunchInfo(nsIFile * aDll,
                            nsILocalFile * aFile,
                            nsAString& args, bool bEdit);

    // Helper routine used in tracking app lists
    void ProcessPath(nsCOMPtr<nsIMutableArray>& appList,
                     nsTArray<nsString>& trackList,
                     const nsAString& appFilesystemCommand);

};

#endif
