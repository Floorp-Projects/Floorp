/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCommandLine_h
#define nsCommandLine_h

#include "nsICommandLineRunner.h"
#include "nsCOMPtr.h"
#include "nsISupportsImpl.h"
#include "nsString.h"
#include "nsTArray.h"

class nsICommandLineHandler;
class nsICommandLineValidator;
class nsIDOMWindow;
class nsIFile;

class nsCommandLine final : public nsICommandLineRunner {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICOMMANDLINE
  NS_DECL_NSICOMMANDLINERUNNER

  nsCommandLine();

 protected:
  ~nsCommandLine() = default;

  typedef nsresult (*EnumerateHandlersCallback)(nsICommandLineHandler* aHandler,
                                                nsICommandLine* aThis,
                                                void* aClosure);
  typedef nsresult (*EnumerateValidatorsCallback)(
      nsICommandLineValidator* aValidator, nsICommandLine* aThis,
      void* aClosure);

  void appendArg(const char* arg);
  MOZ_MUST_USE nsresult resolveShortcutURL(nsIFile* aFile, nsACString& outURL);
  nsresult EnumerateHandlers(EnumerateHandlersCallback aCallback,
                             void* aClosure);
  nsresult EnumerateValidators(EnumerateValidatorsCallback aCallback,
                               void* aClosure);

  nsTArray<nsString> mArgs;
  uint32_t mState;
  nsCOMPtr<nsIFile> mWorkingDir;
  bool mPreventDefault;
};

#endif
