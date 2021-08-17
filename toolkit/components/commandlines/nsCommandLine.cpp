/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCommandLine.h"

#include "nsComponentManagerUtils.h"
#include "nsICategoryManager.h"
#include "nsICommandLineHandler.h"
#include "nsICommandLineValidator.h"
#include "nsIConsoleService.h"
#include "nsIClassInfoImpl.h"
#include "nsIFile.h"
#include "nsISimpleEnumerator.h"
#include "mozilla/SimpleEnumerator.h"

#include "nsNativeCharsetUtils.h"
#include "nsNetUtil.h"
#include "nsIFileProtocolHandler.h"
#include "nsIURI.h"
#include "nsUnicharUtils.h"
#include "nsTextFormatter.h"
#include "nsXPCOMCID.h"
#include "plstr.h"
#include "mozilla/Attributes.h"

#ifdef MOZ_WIDGET_COCOA
#  include <CoreFoundation/CoreFoundation.h>
#  include "nsILocalFileMac.h"
#elif defined(XP_WIN)
#  include <windows.h>
#  include <shlobj.h>
#elif defined(XP_UNIX)
#  include <unistd.h>
#endif

#ifdef DEBUG_bsmedberg
#  define DEBUG_COMMANDLINE
#endif

#define NS_COMMANDLINE_CID                           \
  {                                                  \
    0x23bcc750, 0xdc20, 0x460b, {                    \
      0xb2, 0xd4, 0x74, 0xd8, 0xf5, 0x8d, 0x36, 0x15 \
    }                                                \
  }

using mozilla::SimpleEnumerator;

nsCommandLine::nsCommandLine()
    : mState(STATE_INITIAL_LAUNCH), mPreventDefault(false) {}

NS_IMPL_CLASSINFO(nsCommandLine, nullptr, 0, NS_COMMANDLINE_CID)
NS_IMPL_ISUPPORTS_CI(nsCommandLine, nsICommandLine, nsICommandLineRunner)

NS_IMETHODIMP
nsCommandLine::GetLength(int32_t* aResult) {
  *aResult = int32_t(mArgs.Length());
  return NS_OK;
}

NS_IMETHODIMP
nsCommandLine::GetArgument(int32_t aIndex, nsAString& aResult) {
  NS_ENSURE_ARG_MIN(aIndex, 0);
  NS_ENSURE_ARG_MAX(aIndex, int32_t(mArgs.Length() - 1));

  aResult = mArgs[aIndex];
  return NS_OK;
}

NS_IMETHODIMP
nsCommandLine::FindFlag(const nsAString& aFlag, bool aCaseSensitive,
                        int32_t* aResult) {
  NS_ENSURE_ARG(!aFlag.IsEmpty());

  auto c = aCaseSensitive ? nsTDefaultStringComparator<char16_t>
                          : nsCaseInsensitiveStringComparator;

  for (uint32_t f = 0; f < mArgs.Length(); f++) {
    const nsString& arg = mArgs[f];

    if (arg.Length() >= 2 && arg.First() == char16_t('-')) {
      if (aFlag.Equals(Substring(arg, 1), c)) {
        *aResult = f;
        return NS_OK;
      }
    }
  }

  *aResult = -1;
  return NS_OK;
}

NS_IMETHODIMP
nsCommandLine::RemoveArguments(int32_t aStart, int32_t aEnd) {
  NS_ENSURE_ARG_MIN(aStart, 0);
  NS_ENSURE_ARG_MAX(uint32_t(aEnd) + 1, mArgs.Length());

  mArgs.RemoveElementsRange(mArgs.begin() + aStart, mArgs.begin() + aEnd + 1);

  return NS_OK;
}

NS_IMETHODIMP
nsCommandLine::HandleFlag(const nsAString& aFlag, bool aCaseSensitive,
                          bool* aResult) {
  nsresult rv;

  int32_t found;
  rv = FindFlag(aFlag, aCaseSensitive, &found);
  NS_ENSURE_SUCCESS(rv, rv);

  if (found == -1) {
    *aResult = false;
    return NS_OK;
  }

  *aResult = true;
  RemoveArguments(found, found);

  return NS_OK;
}

NS_IMETHODIMP
nsCommandLine::HandleFlagWithParam(const nsAString& aFlag, bool aCaseSensitive,
                                   nsAString& aResult) {
  nsresult rv;

  int32_t found;
  rv = FindFlag(aFlag, aCaseSensitive, &found);
  NS_ENSURE_SUCCESS(rv, rv);

  if (found == -1) {
    aResult.SetIsVoid(true);
    return NS_OK;
  }

  if (found == int32_t(mArgs.Length()) - 1) {
    return NS_ERROR_INVALID_ARG;
  }

  ++found;

  {  // scope for validity of |param|, which RemoveArguments call invalidates
    const nsString& param = mArgs[found];
    if (!param.IsEmpty() && param.First() == '-') {
      return NS_ERROR_INVALID_ARG;
    }

    aResult = param;
  }

  RemoveArguments(found - 1, found);

  return NS_OK;
}

NS_IMETHODIMP
nsCommandLine::GetState(uint32_t* aResult) {
  *aResult = mState;
  return NS_OK;
}

NS_IMETHODIMP
nsCommandLine::GetPreventDefault(bool* aResult) {
  *aResult = mPreventDefault;
  return NS_OK;
}

NS_IMETHODIMP
nsCommandLine::SetPreventDefault(bool aValue) {
  mPreventDefault = aValue;
  return NS_OK;
}

NS_IMETHODIMP
nsCommandLine::GetWorkingDirectory(nsIFile** aResult) {
  NS_ENSURE_TRUE(mWorkingDir, NS_ERROR_NOT_INITIALIZED);

  NS_ADDREF(*aResult = mWorkingDir);
  return NS_OK;
}

NS_IMETHODIMP
nsCommandLine::ResolveFile(const nsAString& aArgument, nsIFile** aResult) {
  // First try to resolve as an absolute path if we can.
  // This will work even if we have no mWorkingDir, which happens if e.g.
  // the dir from which we were started was deleted before we started,
#if defined(XP_UNIX)
  if (aArgument.First() == '/') {
    nsCOMPtr<nsIFile> lf(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID));
    NS_ENSURE_TRUE(lf, NS_ERROR_OUT_OF_MEMORY);
    nsresult rv = lf->InitWithPath(aArgument);
    if (NS_FAILED(rv)) {
      return rv;
    }

    lf.forget(aResult);
    return NS_OK;
  }
#elif defined(XP_WIN)
  nsCOMPtr<nsIFile> lf(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID));
  NS_ENSURE_TRUE(lf, NS_ERROR_OUT_OF_MEMORY);

  // Just try creating the file with the absolute path; if it fails,
  // we'll keep going and try it as a relative path.
  if (NS_SUCCEEDED(lf->InitWithPath(aArgument))) {
    lf.forget(aResult);
    return NS_OK;
  }
#endif
  // If that fails
  return ResolveRelativeFile(aArgument, aResult);
}

nsresult nsCommandLine::ResolveRelativeFile(const nsAString& aArgument,
                                            nsIFile** aResult) {
  if (!mWorkingDir) {
    *aResult = nullptr;
    return NS_OK;
  }

  // This is some seriously screwed-up code. nsIFile.appendRelativeNativePath
  // explicitly does not accept .. or . path parts, but that is exactly what we
  // need here. So we hack around it.

  nsresult rv;

#if defined(MOZ_WIDGET_COCOA)
  nsCOMPtr<nsILocalFileMac> lfm(do_QueryInterface(mWorkingDir));
  NS_ENSURE_TRUE(lfm, NS_ERROR_NO_INTERFACE);

  nsCOMPtr<nsILocalFileMac> newfile(
      do_CreateInstance(NS_LOCAL_FILE_CONTRACTID));
  NS_ENSURE_TRUE(newfile, NS_ERROR_OUT_OF_MEMORY);

  CFURLRef baseurl;
  rv = lfm->GetCFURL(&baseurl);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString path;
  NS_CopyUnicodeToNative(aArgument, path);

  CFURLRef newurl = CFURLCreateFromFileSystemRepresentationRelativeToBase(
      nullptr, (const UInt8*)path.get(), path.Length(), true, baseurl);

  CFRelease(baseurl);

  rv = newfile->InitWithCFURL(newurl);
  CFRelease(newurl);
  if (NS_FAILED(rv)) return rv;

  newfile.forget(aResult);
  return NS_OK;

#elif defined(XP_UNIX)
  nsCOMPtr<nsIFile> lf(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID));
  NS_ENSURE_TRUE(lf, NS_ERROR_OUT_OF_MEMORY);

  nsAutoCString nativeArg;
  NS_CopyUnicodeToNative(aArgument, nativeArg);

  nsAutoCString newpath;
  mWorkingDir->GetNativePath(newpath);

  newpath.Append('/');
  newpath.Append(nativeArg);

  rv = lf->InitWithNativePath(newpath);
  if (NS_FAILED(rv)) return rv;

  rv = lf->Normalize();
  if (NS_FAILED(rv)) return rv;

  lf.forget(aResult);
  return NS_OK;

#elif defined(XP_WIN)
  nsCOMPtr<nsIFile> lf(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID));
  NS_ENSURE_TRUE(lf, NS_ERROR_OUT_OF_MEMORY);

  // This is a relative path. We use string magic
  // and win32 _fullpath. Note that paths of the form "\Relative\To\CurDrive"
  // are going to fail, and I haven't figured out a way to work around this
  // without the PathCombine() function, which is not available before
  // Windows 8; see https://bugzilla.mozilla.org/show_bug.cgi?id=1672814

  nsAutoString fullPath;
  mWorkingDir->GetPath(fullPath);

  fullPath.Append('\\');
  fullPath.Append(aArgument);

  WCHAR pathBuf[MAX_PATH];
  if (!_wfullpath(pathBuf, fullPath.get(), MAX_PATH)) return NS_ERROR_FAILURE;

  rv = lf->InitWithPath(nsDependentString(pathBuf));
  if (NS_FAILED(rv)) return rv;
  lf.forget(aResult);
  return NS_OK;

#else
#  error Need platform-specific logic here.
#endif
}

NS_IMETHODIMP
nsCommandLine::ResolveURI(const nsAString& aArgument, nsIURI** aResult) {
  nsresult rv;

  // First, we try to init the argument as an absolute file path. If this
  // doesn't work, it is an absolute or relative URI.

  nsCOMPtr<nsIIOService> io = do_GetIOService();
  NS_ENSURE_TRUE(io, NS_ERROR_OUT_OF_MEMORY);

  nsCOMPtr<nsIURI> workingDirURI;
  if (mWorkingDir) {
    io->NewFileURI(mWorkingDir, getter_AddRefs(workingDirURI));
  }

  nsCOMPtr<nsIFile> lf(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID));
  rv = lf->InitWithPath(aArgument);
  if (NS_SUCCEEDED(rv)) {
    lf->Normalize();
    nsAutoCString url;
    // Try to resolve the url for .url files.
    rv = resolveShortcutURL(lf, url);
    if (NS_SUCCEEDED(rv) && !url.IsEmpty()) {
      return io->NewURI(url, nullptr, workingDirURI, aResult);
    }

    return io->NewFileURI(lf, aResult);
  }

  return io->NewURI(NS_ConvertUTF16toUTF8(aArgument), nullptr, workingDirURI,
                    aResult);
}

void nsCommandLine::appendArg(const char* arg) {
#ifdef DEBUG_COMMANDLINE
  printf("Adding XP arg: %s\n", arg);
#endif

  nsAutoString warg;
#ifdef XP_WIN
  CopyUTF8toUTF16(nsDependentCString(arg), warg);
#else
  NS_CopyNativeToUnicode(nsDependentCString(arg), warg);
#endif

  mArgs.AppendElement(warg);
}

nsresult nsCommandLine::resolveShortcutURL(nsIFile* aFile, nsACString& outURL) {
  nsCOMPtr<nsIFileProtocolHandler> fph;
  nsresult rv = NS_GetFileProtocolHandler(getter_AddRefs(fph));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIURI> uri;
  rv = fph->ReadURLFile(aFile, getter_AddRefs(uri));
  if (NS_FAILED(rv)) return rv;

  return uri->GetSpec(outURL);
}

NS_IMETHODIMP
nsCommandLine::Init(int32_t argc, const char* const* argv, nsIFile* aWorkingDir,
                    uint32_t aState) {
  NS_ENSURE_ARG_MAX(aState, 2);

  int32_t i;

  mWorkingDir = aWorkingDir;

  // skip argv[0], we don't want it
  for (i = 1; i < argc; ++i) {
    const char* curarg = argv[i];

#ifdef DEBUG_COMMANDLINE
    printf("Testing native arg %i: '%s'\n", i, curarg);
#endif
#if defined(XP_WIN)
    if (*curarg == '/') {
      char* dup = strdup(curarg);
      if (!dup) return NS_ERROR_OUT_OF_MEMORY;

      *dup = '-';
      char* colon = PL_strchr(dup, ':');
      if (colon) {
        *colon = '\0';
        appendArg(dup);
        appendArg(colon + 1);
      } else {
        appendArg(dup);
      }
      free(dup);
      continue;
    }
#endif
    if (*curarg == '-') {
      if (*(curarg + 1) == '-') ++curarg;

      char* dup = strdup(curarg);
      if (!dup) return NS_ERROR_OUT_OF_MEMORY;

      char* eq = PL_strchr(dup, '=');
      if (eq) {
        *eq = '\0';
        appendArg(dup);
        appendArg(eq + 1);
      } else {
        appendArg(dup);
      }
      free(dup);
      continue;
    }

    appendArg(curarg);
  }

  mState = aState;

  return NS_OK;
}

template <typename... T>
static void LogConsoleMessage(const char16_t* fmt, T... args) {
  nsString msg;
  nsTextFormatter::ssprintf(msg, fmt, args...);

  nsCOMPtr<nsIConsoleService> cs =
      do_GetService("@mozilla.org/consoleservice;1");
  if (cs) cs->LogStringMessage(msg.get());
}

nsresult nsCommandLine::EnumerateHandlers(EnumerateHandlersCallback aCallback,
                                          void* aClosure) {
  nsresult rv;

  nsCOMPtr<nsICategoryManager> catman(
      do_GetService(NS_CATEGORYMANAGER_CONTRACTID));
  NS_ENSURE_TRUE(catman, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsISimpleEnumerator> entenum;
  rv = catman->EnumerateCategory("command-line-handler",
                                 getter_AddRefs(entenum));
  NS_ENSURE_SUCCESS(rv, rv);

  for (auto& categoryEntry : SimpleEnumerator<nsICategoryEntry>(entenum)) {
    nsAutoCString contractID;
    categoryEntry->GetValue(contractID);

    nsCOMPtr<nsICommandLineHandler> clh(do_GetService(contractID.get()));
    if (!clh) {
      nsCString entry;
      categoryEntry->GetEntry(entry);

      LogConsoleMessage(
          u"Contract ID '%s' was registered as a command line handler for "
          u"entry '%s', but could not be created.",
          contractID.get(), entry.get());
      continue;
    }

    rv = (aCallback)(clh, this, aClosure);
    if (rv == NS_ERROR_ABORT) break;

    rv = NS_OK;
  }

  return rv;
}

nsresult nsCommandLine::EnumerateValidators(
    EnumerateValidatorsCallback aCallback, void* aClosure) {
  nsresult rv;

  nsCOMPtr<nsICategoryManager> catman(
      do_GetService(NS_CATEGORYMANAGER_CONTRACTID));
  NS_ENSURE_TRUE(catman, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsISimpleEnumerator> entenum;
  rv = catman->EnumerateCategory("command-line-validator",
                                 getter_AddRefs(entenum));
  NS_ENSURE_SUCCESS(rv, rv);

  for (auto& categoryEntry : SimpleEnumerator<nsICategoryEntry>(entenum)) {
    nsAutoCString contractID;
    categoryEntry->GetValue(contractID);

    nsCOMPtr<nsICommandLineValidator> clv(do_GetService(contractID.get()));
    if (!clv) continue;

    rv = (aCallback)(clv, this, aClosure);
    if (rv == NS_ERROR_ABORT) break;

    rv = NS_OK;
  }

  return rv;
}

static nsresult EnumValidate(nsICommandLineValidator* aValidator,
                             nsICommandLine* aThis, void*) {
  return aValidator->Validate(aThis);
}

static nsresult EnumRun(nsICommandLineHandler* aHandler, nsICommandLine* aThis,
                        void*) {
  return aHandler->Handle(aThis);
}

NS_IMETHODIMP
nsCommandLine::Run() {
  nsresult rv;

  rv = EnumerateValidators(EnumValidate, nullptr);
  if (rv == NS_ERROR_ABORT) return rv;

  rv = EnumerateHandlers(EnumRun, nullptr);
  if (rv == NS_ERROR_ABORT) return rv;

  return NS_OK;
}

static nsresult EnumHelp(nsICommandLineHandler* aHandler, nsICommandLine* aThis,
                         void* aClosure) {
  nsresult rv;

  nsCString text;
  rv = aHandler->GetHelpInfo(text);
  if (NS_SUCCEEDED(rv)) {
    NS_ASSERTION(
        text.Length() == 0 || text.Last() == '\n',
        "Help text from command line handlers should end in a newline.");

    nsACString* totalText = reinterpret_cast<nsACString*>(aClosure);
    totalText->Append(text);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCommandLine::GetHelpText(nsACString& aResult) {
  EnumerateHandlers(EnumHelp, &aResult);

  return NS_OK;
}
