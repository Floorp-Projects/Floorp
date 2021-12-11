/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMIMEInfoWin_h_
#define nsMIMEInfoWin_h_

#include "nsMIMEInfoImpl.h"
#include "nsIPropertyBag.h"
#include "nsIMutableArray.h"
#include "nsTArray.h"

class nsMIMEInfoWin : public nsMIMEInfoBase, public nsIPropertyBag {
  virtual ~nsMIMEInfoWin();

 public:
  explicit nsMIMEInfoWin(const char* aType = "") : nsMIMEInfoBase(aType) {}
  explicit nsMIMEInfoWin(const nsACString& aMIMEType)
      : nsMIMEInfoBase(aMIMEType) {}
  nsMIMEInfoWin(const nsACString& aType, HandlerClass aClass)
      : nsMIMEInfoBase(aType, aClass) {}

  NS_IMETHOD LaunchWithFile(nsIFile* aFile) override;
  NS_IMETHOD GetHasDefaultHandler(bool* _retval) override;
  NS_IMETHOD GetPossibleLocalHandlers(nsIArray** _retval) override;
  NS_IMETHOD IsCurrentAppOSDefault(bool* _retval) override;

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIPROPERTYBAG

  void SetDefaultApplicationHandler(nsIFile* aDefaultApplication) {
    mDefaultApplication = aDefaultApplication;
  }

 protected:
  virtual nsresult LoadUriInternal(nsIURI* aURI);
  virtual nsresult LaunchDefaultWithFile(nsIFile* aFile);

 private:
  nsCOMPtr<nsIFile> mDefaultApplication;

  // Given a path to a local handler, return its
  // nsILocalHandlerApp instance.
  bool GetLocalHandlerApp(const nsAString& aCommandHandler,
                          nsCOMPtr<nsILocalHandlerApp>& aApp);

  // Return the cleaned up file path associated
  // with a command verb located in root/Applications.
  bool GetAppsVerbCommandHandler(const nsAString& appExeName,
                                 nsAString& applicationPath, bool bEdit);

  // Return the cleaned up file path associated
  // with a progid command verb located in root.
  bool GetProgIDVerbCommandHandler(const nsAString& appProgIDName,
                                   nsAString& applicationPath, bool bEdit);

  // Lookup a rundll command handler and return
  // a populated command template for use with rundll32.exe.
  bool GetDllLaunchInfo(nsIFile* aDll, nsIFile* aFile, nsAString& args,
                        bool bEdit);

  // Helper routine used in tracking app lists
  void ProcessPath(nsCOMPtr<nsIMutableArray>& appList,
                   nsTArray<nsString>& trackList,
                   const nsAString& appFilesystemCommand);

  // Helper routine to call mozilla::ShellExecuteByExplorer
  nsresult ShellExecuteWithIFile(nsIFile* aExecutable, int aArgc,
                                 const wchar_t** aArgv);
};

#endif
