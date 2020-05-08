/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OSReauthenticator.h"

#include "OSKeyStore.h"
#include "nsNetCID.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/Logging.h"
#include "nsIBaseWindow.h"
#include "nsIDocShell.h"
#include "nsISupportsUtils.h"
#include "nsIWidget.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "ipc/IPCMessageUtils.h"

NS_IMPL_ISUPPORTS(OSReauthenticator, nsIOSReauthenticator)

extern mozilla::LazyLogModule gCredentialManagerSecretLog;

using mozilla::LogLevel;
using mozilla::Preferences;
using mozilla::WindowsHandle;
using mozilla::dom::Promise;

#define PREF_BLANK_PASSWORD "security.osreauthenticator.blank_password"
#define PREF_PASSWORD_LAST_CHANGED_LO \
  "security.osreauthenticator.password_last_changed_lo"
#define PREF_PASSWORD_LAST_CHANGED_HI \
  "security.osreauthenticator.password_last_changed_hi"

#if defined(XP_WIN)
#  include <combaseapi.h>
#  include <ntsecapi.h>
#  include <wincred.h>
#  include <windows.h>
#  define SECURITY_WIN32
#  include <security.h>
#  include <shlwapi.h>
#  if !defined(__MINGW32__)
#    include <Lm.h>
#  endif  // !defined(__MINGW32__)
struct HandleCloser {
  typedef HANDLE pointer;
  void operator()(HANDLE h) {
    if (h != INVALID_HANDLE_VALUE) {
      CloseHandle(h);
    }
  }
};
struct BufferFreer {
  typedef LPVOID pointer;
  ULONG mSize;
  explicit BufferFreer(ULONG size) : mSize(size) {}
  void operator()(LPVOID b) {
    SecureZeroMemory(b, mSize);
    CoTaskMemFree(b);
  }
};
typedef std::unique_ptr<HANDLE, HandleCloser> ScopedHANDLE;
typedef std::unique_ptr<LPVOID, BufferFreer> ScopedBuffer;

constexpr int64_t Int32Modulo = 2147483648;

// Get the token info holding the sid.
std::unique_ptr<char[]> GetTokenInfo(ScopedHANDLE& token) {
  DWORD length = 0;
  // https://docs.microsoft.com/en-us/windows/desktop/api/securitybaseapi/nf-securitybaseapi-gettokeninformation
  mozilla::Unused << GetTokenInformation(token.get(), TokenUser, nullptr, 0,
                                         &length);
  if (!length || GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
    MOZ_LOG(gCredentialManagerSecretLog, LogLevel::Debug,
            ("Unable to obtain current token info."));
    return nullptr;
  }
  std::unique_ptr<char[]> token_info(new char[length]);
  if (!GetTokenInformation(token.get(), TokenUser, token_info.get(), length,
                           &length)) {
    MOZ_LOG(gCredentialManagerSecretLog, LogLevel::Debug,
            ("Unable to obtain current token info (second call, possible "
             "system error."));
    return nullptr;
  }
  return token_info;
}

std::unique_ptr<char[]> GetUserTokenInfo() {
  // Get current user sid to make sure the same user got logged in.
  HANDLE token;
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
    // Couldn't get a process token. This will fail any unlock attempts later.
    MOZ_LOG(gCredentialManagerSecretLog, LogLevel::Debug,
            ("Unable to obtain process token."));
    return nullptr;
  }
  ScopedHANDLE scopedToken(token);
  return GetTokenInfo(scopedToken);
}

Maybe<int64_t> GetPasswordLastChanged(const WCHAR* username) {
#  if defined(__MINGW32__)
  // NetUserGetInfo requires Lm.h which is not provided in MinGW builds
  return mozilla::Nothing();
#  else
  LPUSER_INFO_1 user_info = NULL;
  DWORD passwordAgeInSeconds = 0;

  NET_API_STATUS ret =
      NetUserGetInfo(NULL, username, 1, reinterpret_cast<LPBYTE*>(&user_info));

  if (ret == NERR_Success) {
    // Returns seconds since last password change.
    passwordAgeInSeconds = user_info->usri1_password_age;
    NetApiBufferFree(user_info);
  } else {
    return mozilla::Nothing();
  }

  // Return the time that the password was changed so we can use this
  // for future comparisons.
  return mozilla::Some(PR_Now() - passwordAgeInSeconds * PR_USEC_PER_SEC);
#  endif
}

// Use the Windows credential prompt to ask the user to authenticate the
// currently used account.
static nsresult ReauthenticateUserWindows(
    const nsAString& aMessageText, const nsAString& aCaptionText,
    const WindowsHandle& hwndParent,
    /* out */ bool& reauthenticated,
    /* inout */ bool& isBlankPassword,
    /* inout */ int64_t& prefLastChanged) {
  reauthenticated = false;

  // Check if the user has a blank password before proceeding
  DWORD usernameLength = CREDUI_MAX_USERNAME_LENGTH + 1;
  WCHAR username[CREDUI_MAX_USERNAME_LENGTH + 1] = {0};

  if (!GetUserNameEx(NameSamCompatible, username, &usernameLength)) {
    MOZ_LOG(gCredentialManagerSecretLog, LogLevel::Debug,
            ("Error getting username"));
    return NS_ERROR_FAILURE;
  }

#  ifdef OS_DOMAINMEMBER
  bool isDomainMember = IsOS(OS_DOMAINMEMBER);
#  else
  // Bug 1633097
  bool isDomainMember = false;
#  endif
  if (!isDomainMember) {
    const WCHAR* usernameNoDomain = username;
    // Don't include the domain portion of the username when calling LogonUser.
    LPCWSTR backslash = wcschr(username, L'\\');
    if (backslash) {
      usernameNoDomain = backslash + 1;
    }

    Maybe<int64_t> lastChanged = GetPasswordLastChanged(usernameNoDomain);
    if (lastChanged.isSome()) {
      bool shouldCheckAgain = lastChanged.value() > prefLastChanged;
      // Update the value stored in preferences
      prefLastChanged = lastChanged.value();

      if (shouldCheckAgain) {
        HANDLE logonUserHandle = INVALID_HANDLE_VALUE;
        bool result =
            LogonUser(usernameNoDomain, L".", L"", LOGON32_LOGON_INTERACTIVE,
                      LOGON32_PROVIDER_DEFAULT, &logonUserHandle);
        if (result) {
          CloseHandle(logonUserHandle);
        }
        // ERROR_ACCOUNT_RESTRICTION: Indicates a referenced user name and
        // authentication information are valid, but some user account
        // restriction has prevented successful authentication (such as
        // time-of-day restrictions).
        reauthenticated = isBlankPassword =
            (result || GetLastError() == ERROR_ACCOUNT_RESTRICTION);
      } else if (isBlankPassword) {
        reauthenticated = true;
      }

      if (reauthenticated) {
        return NS_OK;
      }
    } else {
      isBlankPassword = false;
    }
  } else {
    // Update any preferences, assuming domain members do not have blank
    // passwords
    isBlankPassword = false;
  }

  // Is used in next iteration if the previous login failed.
  DWORD err = 0;
  std::unique_ptr<char[]> userTokenInfo = GetUserTokenInfo();

  // CredUI prompt.
  CREDUI_INFOW credui = {};
  credui.cbSize = sizeof(credui);
  credui.hwndParent = reinterpret_cast<HWND>(hwndParent);
  const nsString& messageText = PromiseFlatString(aMessageText);
  credui.pszMessageText = messageText.get();
  const nsString& captionText = PromiseFlatString(aCaptionText);
  credui.pszCaptionText = captionText.get();
  credui.hbmBanner = nullptr;  // ignored

  while (!reauthenticated) {
    HANDLE lsa;
    // Get authentication handle for future user authentications.
    // https://docs.microsoft.com/en-us/windows/desktop/api/ntsecapi/nf-ntsecapi-lsaconnectuntrusted
    if (LsaConnectUntrusted(&lsa) != ERROR_SUCCESS) {
      MOZ_LOG(gCredentialManagerSecretLog, LogLevel::Debug,
              ("Error acquiring lsa. Authentication attempts will fail."));
      return NS_ERROR_FAILURE;
    }
    ScopedHANDLE scopedLsa(lsa);

    if (!userTokenInfo || lsa == INVALID_HANDLE_VALUE) {
      MOZ_LOG(gCredentialManagerSecretLog, LogLevel::Debug,
              ("Error setting up login and user token."));
      return NS_ERROR_FAILURE;
    }

    ULONG authPackage = 0;
    ULONG outCredSize = 0;
    LPVOID outCredBuffer = nullptr;

    // Get user's Windows credentials.
    // https://docs.microsoft.com/en-us/windows/desktop/api/wincred/nf-wincred-creduipromptforwindowscredentialsw
    err = CredUIPromptForWindowsCredentialsW(
        &credui, err, &authPackage, nullptr, 0, &outCredBuffer, &outCredSize,
        nullptr, CREDUIWIN_ENUMERATE_CURRENT_USER);
    ScopedBuffer scopedOutCredBuffer(outCredBuffer, BufferFreer(outCredSize));
    if (err == ERROR_CANCELLED) {
      MOZ_LOG(gCredentialManagerSecretLog, LogLevel::Debug,
              ("Error getting authPackage for user login, user cancel."));
      return NS_OK;
    }
    if (err != ERROR_SUCCESS) {
      MOZ_LOG(gCredentialManagerSecretLog, LogLevel::Debug,
              ("Error getting authPackage for user login."));
      return NS_ERROR_FAILURE;
    }

    // Verify the credentials.
    TOKEN_SOURCE source;
    PCHAR contextName = const_cast<PCHAR>("Mozilla");
    size_t nameLength =
        std::min(TOKEN_SOURCE_LENGTH, static_cast<int>(strlen(contextName)));
    // Note that the string must not be longer than TOKEN_SOURCE_LENGTH.
    memcpy(source.SourceName, contextName, nameLength);
    // https://docs.microsoft.com/en-us/windows/desktop/api/securitybaseapi/nf-securitybaseapi-allocatelocallyuniqueid
    if (!AllocateLocallyUniqueId(&source.SourceIdentifier)) {
      MOZ_LOG(gCredentialManagerSecretLog, LogLevel::Debug,
              ("Error allocating ID for logon process."));
      return NS_ERROR_FAILURE;
    }

    NTSTATUS substs;
    void* profileBuffer = nullptr;
    ULONG profileBufferLength = 0;
    QUOTA_LIMITS limits = {0};
    LUID luid;
    HANDLE token;
    LSA_STRING name;
    name.Buffer = contextName;
    name.Length = strlen(name.Buffer);
    name.MaximumLength = name.Length;
    // https://docs.microsoft.com/en-us/windows/desktop/api/ntsecapi/nf-ntsecapi-lsalogonuser
    NTSTATUS sts = LsaLogonUser(
        scopedLsa.get(), &name, (SECURITY_LOGON_TYPE)Interactive, authPackage,
        scopedOutCredBuffer.get(), outCredSize, nullptr, &source,
        &profileBuffer, &profileBufferLength, &luid, &token, &limits, &substs);
    ScopedHANDLE scopedToken(token);
    LsaFreeReturnBuffer(profileBuffer);
    LsaDeregisterLogonProcess(scopedLsa.get());
    if (sts == ERROR_SUCCESS) {
      MOZ_LOG(gCredentialManagerSecretLog, LogLevel::Debug,
              ("User logged in successfully."));
    } else {
      err = LsaNtStatusToWinError(sts);
      MOZ_LOG(gCredentialManagerSecretLog, LogLevel::Debug,
              ("Login failed with %lx (%lx).", sts, err));
      continue;
    }

    // The user can select any user to log-in on the authentication prompt.
    // Make sure that the logged in user is the current user.
    std::unique_ptr<char[]> logonTokenInfo = GetTokenInfo(scopedToken);
    if (!logonTokenInfo) {
      MOZ_LOG(gCredentialManagerSecretLog, LogLevel::Debug,
              ("Error getting logon token info."));
      return NS_ERROR_FAILURE;
    }
    PSID logonSID =
        reinterpret_cast<TOKEN_USER*>(logonTokenInfo.get())->User.Sid;
    PSID userSID = reinterpret_cast<TOKEN_USER*>(userTokenInfo.get())->User.Sid;
    if (EqualSid(userSID, logonSID)) {
      MOZ_LOG(gCredentialManagerSecretLog, LogLevel::Debug,
              ("Login successfully (correct user)."));
      reauthenticated = true;
      break;
    } else {
      err = ERROR_LOGON_FAILURE;
    }
  }
  return NS_OK;
}
#endif  // XP_WIN

static nsresult ReauthenticateUser(const nsAString& prompt,
                                   const nsAString& caption,
                                   const WindowsHandle& hwndParent,
                                   /* out */ bool& reauthenticated,
                                   /* inout */ bool& isBlankPassword,
                                   /* inout */ int64_t& prefLastChanged) {
  reauthenticated = false;
#if defined(XP_WIN)
  return ReauthenticateUserWindows(prompt, caption, hwndParent, reauthenticated,
                                   isBlankPassword, prefLastChanged);
#elif defined(XP_MACOSX)
  return ReauthenticateUserMacOS(prompt, reauthenticated, isBlankPassword);
#endif  // Reauthentication is not implemented for this platform.
  return NS_OK;
}

static void BackgroundReauthenticateUser(RefPtr<Promise>& aPromise,
                                         const nsAString& aMessageText,
                                         const nsAString& aCaptionText,
                                         const WindowsHandle& hwndParent,
                                         bool isBlankPassword,
                                         int64_t prefLastChanged) {
  nsAutoCString recovery;
  bool reauthenticated;
  nsresult rv =
      ReauthenticateUser(aMessageText, aCaptionText, hwndParent,
                         reauthenticated, isBlankPassword, prefLastChanged);

  nsTArray<int32_t> prefLastChangedUpdates;
#if defined(XP_WIN)
  // Increase the lastChanged time to account for clock skew.
  prefLastChanged += PR_USEC_PER_SEC;
  // Need to split the 64bit integer to its hi and lo bits before sending it
  // back to JS.
  int32_t prefLastChangedHi = prefLastChanged / Int32Modulo;
  int32_t prefLastChangedLo = prefLastChanged % Int32Modulo;
  prefLastChangedUpdates.AppendElement(prefLastChangedHi);
  prefLastChangedUpdates.AppendElement(prefLastChangedLo);
#endif

  nsTArray<int32_t> results(2);
  results.AppendElement(reauthenticated);
  results.AppendElement(isBlankPassword);
  nsCOMPtr<nsIRunnable> runnable(NS_NewRunnableFunction(
      "BackgroundReauthenticateUserResolve",
      [rv, results = std::move(results),
       prefLastChangedUpdates = std::move(prefLastChangedUpdates),
       aPromise = std::move(aPromise)]() {
        if (NS_FAILED(rv)) {
          aPromise->MaybeReject(rv);
        } else {
          aPromise->MaybeResolve(results);
        }

        nsresult rv = Preferences::SetBool(PREF_BLANK_PASSWORD, results[1]);
        if (NS_FAILED(rv)) {
          return;
        }
        if (prefLastChangedUpdates.Length() > 1) {
          rv = Preferences::SetInt(PREF_PASSWORD_LAST_CHANGED_HI,
                                   prefLastChangedUpdates[0]);
          if (NS_FAILED(rv)) {
            return;
          }
          Preferences::SetInt(PREF_PASSWORD_LAST_CHANGED_LO,
                              prefLastChangedUpdates[1]);
        }
      }));
  NS_DispatchToMainThread(runnable.forget());
}

NS_IMETHODIMP
OSReauthenticator::AsyncReauthenticateUser(const nsAString& aMessageText,
                                           const nsAString& aCaptionText,
                                           mozIDOMWindow* aParentWindow,
                                           JSContext* aCx,
                                           Promise** promiseOut) {
  NS_ENSURE_ARG_POINTER(aCx);

  RefPtr<Promise> promiseHandle;
  nsresult rv = GetPromise(aCx, promiseHandle);
  if (NS_FAILED(rv)) {
    return rv;
  }

  WindowsHandle hwndParent = 0;
  if (aParentWindow) {
    nsPIDOMWindowInner* win = nsPIDOMWindowInner::From(aParentWindow);
    nsIDocShell* docShell = win->GetDocShell();
    if (docShell) {
      nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(docShell);
      if (baseWindow) {
        nsCOMPtr<nsIWidget> widget;
        baseWindow->GetMainWidget(getter_AddRefs(widget));
        if (widget) {
          hwndParent = reinterpret_cast<WindowsHandle>(
              widget->GetNativeData(NS_NATIVE_WINDOW));
        }
      }
    }
  }

  int64_t prefLastChanged = 0;
  bool isBlankPassword = false;
#if defined(XP_WIN)
  // These preferences are only supported on Windows.
  // Preferences are read/write main-thread only.
  int32_t prefLastChangedLo;
  int32_t prefLastChangedHi;
  rv = Preferences::GetBool(PREF_BLANK_PASSWORD, &isBlankPassword);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Preferences::GetInt(PREF_PASSWORD_LAST_CHANGED_LO, &prefLastChangedLo);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Preferences::GetInt(PREF_PASSWORD_LAST_CHANGED_HI, &prefLastChangedHi);
  if (NS_FAILED(rv)) {
    return rv;
  }
  prefLastChanged = prefLastChangedHi * Int32Modulo + prefLastChangedLo;
#endif

  nsCOMPtr<nsIRunnable> runnable(NS_NewRunnableFunction(
      "BackgroundReauthenticateUser",
      [promiseHandle, aMessageText = nsAutoString(aMessageText),
       aCaptionText = nsAutoString(aCaptionText), hwndParent, isBlankPassword,
       prefLastChanged]() mutable {
        BackgroundReauthenticateUser(promiseHandle, aMessageText, aCaptionText,
                                     hwndParent, isBlankPassword,
                                     prefLastChanged);
      }));

  nsCOMPtr<nsIEventTarget> target(
      do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID));
  if (!target) {
    return NS_ERROR_FAILURE;
  }
  rv = target->Dispatch(runnable, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  promiseHandle.forget(promiseOut);
  return NS_OK;
}
