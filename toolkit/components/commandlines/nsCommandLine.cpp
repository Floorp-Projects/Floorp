/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsICommandLineRunner.h"

#include "nsICategoryManager.h"
#include "nsICommandLineHandler.h"
#include "nsICommandLineValidator.h"
#include "nsIConsoleService.h"
#include "nsIClassInfoImpl.h"
#include "nsIDOMWindow.h"
#include "nsIFile.h"
#include "nsISimpleEnumerator.h"
#include "nsIStringEnumerator.h"

#include "nsCOMPtr.h"
#include "mozilla/ModuleUtils.h"
#include "nsISupportsImpl.h"
#include "nsNativeCharsetUtils.h"
#include "nsNetUtil.h"
#include "nsUnicharUtils.h"
#include "nsTArray.h"
#include "nsTextFormatter.h"
#include "nsXPCOMCID.h"
#include "plstr.h"

#ifdef MOZ_WIDGET_COCOA
#include <CoreFoundation/CoreFoundation.h>
#include "nsILocalFileMac.h"
#elif defined(XP_WIN)
#include <windows.h>
#include <shlobj.h>
#elif defined(XP_UNIX)
#include <unistd.h>
#elif defined(XP_OS2)
#include <os2.h>
#endif

#ifdef DEBUG_bsmedberg
#define DEBUG_COMMANDLINE
#endif

#define NS_COMMANDLINE_CID \
  { 0x23bcc750, 0xdc20, 0x460b, { 0xb2, 0xd4, 0x74, 0xd8, 0xf5, 0x8d, 0x36, 0x15 } }

class nsCommandLine : public nsICommandLineRunner
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICOMMANDLINE
  NS_DECL_NSICOMMANDLINERUNNER

  nsCommandLine();

protected:
  ~nsCommandLine() { }

  typedef nsresult (*EnumerateHandlersCallback)(nsICommandLineHandler* aHandler,
					nsICommandLine* aThis,
					void *aClosure);
  typedef nsresult (*EnumerateValidatorsCallback)(nsICommandLineValidator* aValidator,
					nsICommandLine* aThis,
					void *aClosure);

  void appendArg(const char* arg);
  void resolveShortcutURL(nsIFile* aFile, nsACString& outURL);
  nsresult EnumerateHandlers(EnumerateHandlersCallback aCallback, void *aClosure);
  nsresult EnumerateValidators(EnumerateValidatorsCallback aCallback, void *aClosure);

  nsTArray<nsString>      mArgs;
  PRUint32                mState;
  nsCOMPtr<nsIFile>       mWorkingDir;
  nsCOMPtr<nsIDOMWindow>  mWindowContext;
  bool                    mPreventDefault;
};

nsCommandLine::nsCommandLine() :
  mState(STATE_INITIAL_LAUNCH),
  mPreventDefault(false)
{

}


NS_IMPL_CLASSINFO(nsCommandLine, NULL, 0, NS_COMMANDLINE_CID)
NS_IMPL_ISUPPORTS2_CI(nsCommandLine,
                      nsICommandLine,
                      nsICommandLineRunner)

NS_IMETHODIMP
nsCommandLine::GetLength(PRInt32 *aResult)
{
  *aResult = PRInt32(mArgs.Length());
  return NS_OK;
}

NS_IMETHODIMP
nsCommandLine::GetArgument(PRInt32 aIndex, nsAString& aResult)
{
  NS_ENSURE_ARG_MIN(aIndex, 0);
  NS_ENSURE_ARG_MAX(aIndex, PRInt32(mArgs.Length() - 1));

  aResult = mArgs[aIndex];
  return NS_OK;
}

NS_IMETHODIMP
nsCommandLine::FindFlag(const nsAString& aFlag, bool aCaseSensitive, PRInt32 *aResult)
{
  NS_ENSURE_ARG(!aFlag.IsEmpty());

  nsDefaultStringComparator caseCmp;
  nsCaseInsensitiveStringComparator caseICmp;
  nsStringComparator& c = aCaseSensitive ?
    static_cast<nsStringComparator&>(caseCmp) :
    static_cast<nsStringComparator&>(caseICmp);

  for (PRUint32 f = 0; f < mArgs.Length(); f++) {
    const nsString &arg = mArgs[f];

    if (arg.Length() >= 2 && arg.First() == PRUnichar('-')) {
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
nsCommandLine::RemoveArguments(PRInt32 aStart, PRInt32 aEnd)
{
  NS_ENSURE_ARG_MIN(aStart, 0);
  NS_ENSURE_ARG_MAX(PRUint32(aEnd) + 1, mArgs.Length());

  for (PRInt32 i = aEnd; i >= aStart; --i) {
    mArgs.RemoveElementAt(i);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCommandLine::HandleFlag(const nsAString& aFlag, bool aCaseSensitive,
                          bool *aResult)
{
  nsresult rv;

  PRInt32 found;
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
                                   nsAString& aResult)
{
  nsresult rv;

  PRInt32 found;
  rv = FindFlag(aFlag, aCaseSensitive, &found);
  NS_ENSURE_SUCCESS(rv, rv);

  if (found == -1) {
    aResult.SetIsVoid(true);
    return NS_OK;
  }

  if (found == PRInt32(mArgs.Length()) - 1) {
    return NS_ERROR_INVALID_ARG;
  }

  ++found;

  if (mArgs[found].First() == '-') {
    return NS_ERROR_INVALID_ARG;
  }

  aResult = mArgs[found];
  RemoveArguments(found - 1, found);

  return NS_OK;
}

NS_IMETHODIMP
nsCommandLine::GetState(PRUint32 *aResult)
{
  *aResult = mState;
  return NS_OK;
}

NS_IMETHODIMP
nsCommandLine::GetPreventDefault(bool *aResult)
{
  *aResult = mPreventDefault;
  return NS_OK;
}

NS_IMETHODIMP
nsCommandLine::SetPreventDefault(bool aValue)
{
  mPreventDefault = aValue;
  return NS_OK;
}

NS_IMETHODIMP
nsCommandLine::GetWorkingDirectory(nsIFile* *aResult)
{
  NS_ENSURE_TRUE(mWorkingDir, NS_ERROR_NOT_INITIALIZED);

  NS_ADDREF(*aResult = mWorkingDir);
  return NS_OK;
}

NS_IMETHODIMP
nsCommandLine::GetWindowContext(nsIDOMWindow* *aResult)
{
  NS_IF_ADDREF(*aResult = mWindowContext);
  return NS_OK;
}

NS_IMETHODIMP
nsCommandLine::SetWindowContext(nsIDOMWindow* aValue)
{
  mWindowContext = aValue;
  return NS_OK;
}

NS_IMETHODIMP
nsCommandLine::ResolveFile(const nsAString& aArgument, nsIFile* *aResult)
{
  NS_ENSURE_TRUE(mWorkingDir, NS_ERROR_NOT_INITIALIZED);

  // This is some seriously screwed-up code. nsIFile.appendRelativeNativePath
  // explicitly does not accept .. or . path parts, but that is exactly what we
  // need here. So we hack around it.

  nsresult rv;

#if defined(MOZ_WIDGET_COCOA)
  nsCOMPtr<nsILocalFileMac> lfm (do_QueryInterface(mWorkingDir));
  NS_ENSURE_TRUE(lfm, NS_ERROR_NO_INTERFACE);

  nsCOMPtr<nsILocalFileMac> newfile (do_CreateInstance(NS_LOCAL_FILE_CONTRACTID));
  NS_ENSURE_TRUE(newfile, NS_ERROR_OUT_OF_MEMORY);

  CFURLRef baseurl;
  rv = lfm->GetCFURL(&baseurl);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString path;
  NS_CopyUnicodeToNative(aArgument, path);

  CFURLRef newurl =
    CFURLCreateFromFileSystemRepresentationRelativeToBase(NULL, (const UInt8*) path.get(),
                                                          path.Length(),
                                                          true, baseurl);

  CFRelease(baseurl);

  rv = newfile->InitWithCFURL(newurl);
  CFRelease(newurl);
  if (NS_FAILED(rv)) return rv;

  NS_ADDREF(*aResult = newfile);
  return NS_OK;

#elif defined(XP_UNIX)
  nsCOMPtr<nsIFile> lf (do_CreateInstance(NS_LOCAL_FILE_CONTRACTID));
  NS_ENSURE_TRUE(lf, NS_ERROR_OUT_OF_MEMORY);

  if (aArgument.First() == '/') {
    // absolute path
    rv = lf->InitWithPath(aArgument);
    if (NS_FAILED(rv)) return rv;

    NS_ADDREF(*aResult = lf);
    return NS_OK;
  }

  nsCAutoString nativeArg;
  NS_CopyUnicodeToNative(aArgument, nativeArg);

  nsCAutoString newpath;
  mWorkingDir->GetNativePath(newpath);

  newpath.Append('/');
  newpath.Append(nativeArg);

  rv = lf->InitWithNativePath(newpath);
  if (NS_FAILED(rv)) return rv;

  rv = lf->Normalize();
  if (NS_FAILED(rv)) return rv;

  NS_ADDREF(*aResult = lf);
  return NS_OK;

#elif defined(XP_WIN32)
  nsCOMPtr<nsIFile> lf (do_CreateInstance(NS_LOCAL_FILE_CONTRACTID));
  NS_ENSURE_TRUE(lf, NS_ERROR_OUT_OF_MEMORY);

  rv = lf->InitWithPath(aArgument);
  if (NS_FAILED(rv)) {
    // If it's a relative path, the Init is *going* to fail. We use string magic and
    // win32 _fullpath. Note that paths of the form "\Relative\To\CurDrive" are
    // going to fail, and I haven't figured out a way to work around this without
    // the PathCombine() function, which is not available in plain win95/nt4

    nsAutoString fullPath;
    mWorkingDir->GetPath(fullPath);

    fullPath.Append('\\');
    fullPath.Append(aArgument);

    WCHAR pathBuf[MAX_PATH];
    if (!_wfullpath(pathBuf, fullPath.get(), MAX_PATH))
      return NS_ERROR_FAILURE;

    rv = lf->InitWithPath(nsDependentString(pathBuf));
    if (NS_FAILED(rv)) return rv;
  }
  NS_ADDREF(*aResult = lf);
  return NS_OK;

#elif defined(XP_OS2)
  nsCOMPtr<nsIFile> lf (do_CreateInstance(NS_LOCAL_FILE_CONTRACTID));
  NS_ENSURE_TRUE(lf, NS_ERROR_OUT_OF_MEMORY);

  rv = lf->InitWithPath(aArgument);
  if (NS_FAILED(rv)) {

    nsCAutoString fullPath;
    mWorkingDir->GetNativePath(fullPath);

    nsCAutoString carg;
    NS_CopyUnicodeToNative(aArgument, carg);

    fullPath.Append('\\');
    fullPath.Append(carg);

    char pathBuf[CCHMAXPATH];
    if (DosQueryPathInfo(fullPath.get(), FIL_QUERYFULLNAME, pathBuf, sizeof(pathBuf)))
      return NS_ERROR_FAILURE;

    rv = lf->InitWithNativePath(nsDependentCString(pathBuf));
    if (NS_FAILED(rv)) return rv;
  }
  NS_ADDREF(*aResult = lf);
  return NS_OK;

#else
#error Need platform-specific logic here.
#endif
}

NS_IMETHODIMP
nsCommandLine::ResolveURI(const nsAString& aArgument, nsIURI* *aResult)
{
  nsresult rv;

  // First, we try to init the argument as an absolute file path. If this doesn't
  // work, it is an absolute or relative URI.

  nsCOMPtr<nsIIOService> io = do_GetIOService();
  NS_ENSURE_TRUE(io, NS_ERROR_OUT_OF_MEMORY);

  nsCOMPtr<nsIURI> workingDirURI;
  if (mWorkingDir) {
    io->NewFileURI(mWorkingDir, getter_AddRefs(workingDirURI));
  }

  nsCOMPtr<nsIFile> lf (do_CreateInstance(NS_LOCAL_FILE_CONTRACTID));
  rv = lf->InitWithPath(aArgument);
  if (NS_SUCCEEDED(rv)) {
    lf->Normalize();
    nsCAutoString url;
    // Try to resolve the url for .url files.
    resolveShortcutURL(lf, url);
    if (!url.IsEmpty()) {
      return io->NewURI(url,
                        nsnull,
                        workingDirURI,
                        aResult);
    }

    return io->NewFileURI(lf, aResult);
  }

  return io->NewURI(NS_ConvertUTF16toUTF8(aArgument),
                    nsnull,
                    workingDirURI,
                    aResult);
}

void
nsCommandLine::appendArg(const char* arg)
{
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

void
nsCommandLine::resolveShortcutURL(nsIFile* aFile, nsACString& outURL)
{
  nsCOMPtr<nsIFileProtocolHandler> fph;
  nsresult rv = NS_GetFileProtocolHandler(getter_AddRefs(fph));
  if (NS_FAILED(rv))
    return;

  nsCOMPtr<nsIURI> uri;
  rv = fph->ReadURLFile(aFile, getter_AddRefs(uri));
  if (NS_FAILED(rv))
    return;

  uri->GetSpec(outURL);
}

NS_IMETHODIMP
nsCommandLine::Init(PRInt32 argc, char** argv, nsIFile* aWorkingDir,
                    PRUint32 aState)
{
  NS_ENSURE_ARG_MIN(aState, 0);
  NS_ENSURE_ARG_MAX(aState, 2);

  PRInt32 i;

  mWorkingDir = aWorkingDir;

  // skip argv[0], we don't want it
  for (i = 1; i < argc; ++i) {
    const char* curarg = argv[i];

#ifdef DEBUG_COMMANDLINE
    printf("Testing native arg %i: '%s'\n", i, curarg);
#endif
#if defined(XP_WIN) || defined(XP_OS2)
    if (*curarg == '/') {
      char* dup = PL_strdup(curarg);
      if (!dup) return NS_ERROR_OUT_OF_MEMORY;

      *dup = '-';
      char* colon = PL_strchr(dup, ':');
      if (colon) {
        *colon = '\0';
        appendArg(dup);
        appendArg(colon+1);
      } else {
        appendArg(dup);
      }
      PL_strfree(dup);
      continue;
    }
#endif
#ifdef XP_UNIX
    if (*curarg == '-' &&
        *(curarg+1) == '-') {
      ++curarg;

      char* dup = PL_strdup(curarg);
      if (!dup) return NS_ERROR_OUT_OF_MEMORY;

      char* eq = PL_strchr(dup, '=');
      if (eq) {
        *eq = '\0';
        appendArg(dup);
        appendArg(eq + 1);
      } else {
        appendArg(dup);
      }
      PL_strfree(dup);
      continue;
    }
#endif

    appendArg(curarg);
  }

  mState = aState;

  return NS_OK;
}

static void
LogConsoleMessage(const PRUnichar* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  PRUnichar* msg = nsTextFormatter::vsmprintf(fmt, args);
  va_end(args);

  nsCOMPtr<nsIConsoleService> cs = do_GetService("@mozilla.org/consoleservice;1");
  if (cs)
    cs->LogStringMessage(msg);

  NS_Free(msg);
}

nsresult
nsCommandLine::EnumerateHandlers(EnumerateHandlersCallback aCallback, void *aClosure)
{
  nsresult rv;

  nsCOMPtr<nsICategoryManager> catman
    (do_GetService(NS_CATEGORYMANAGER_CONTRACTID));
  NS_ENSURE_TRUE(catman, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsISimpleEnumerator> entenum;
  rv = catman->EnumerateCategory("command-line-handler",
                                 getter_AddRefs(entenum));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIUTF8StringEnumerator> strenum (do_QueryInterface(entenum));
  NS_ENSURE_TRUE(strenum, NS_ERROR_UNEXPECTED);

  nsCAutoString entry;
  bool hasMore;
  while (NS_SUCCEEDED(strenum->HasMore(&hasMore)) && hasMore) {
    strenum->GetNext(entry);

    nsCString contractID;
    rv = catman->GetCategoryEntry("command-line-handler",
				  entry.get(),
				  getter_Copies(contractID));
    if (NS_FAILED(rv))
      continue;

    nsCOMPtr<nsICommandLineHandler> clh(do_GetService(contractID.get()));
    if (!clh) {
      LogConsoleMessage(NS_LITERAL_STRING("Contract ID '%s' was registered as a command line handler for entry '%s', but could not be created.").get(),
                        contractID.get(), entry.get());
      continue;
    }

    rv = (aCallback)(clh, this, aClosure);
    if (rv == NS_ERROR_ABORT)
      break;

    rv = NS_OK;
  }

  return rv;
}

nsresult
nsCommandLine::EnumerateValidators(EnumerateValidatorsCallback aCallback, void *aClosure)
{
  nsresult rv;

  nsCOMPtr<nsICategoryManager> catman
    (do_GetService(NS_CATEGORYMANAGER_CONTRACTID));
  NS_ENSURE_TRUE(catman, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsISimpleEnumerator> entenum;
  rv = catman->EnumerateCategory("command-line-validator",
                                 getter_AddRefs(entenum));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIUTF8StringEnumerator> strenum (do_QueryInterface(entenum));
  NS_ENSURE_TRUE(strenum, NS_ERROR_UNEXPECTED);

  nsCAutoString entry;
  bool hasMore;
  while (NS_SUCCEEDED(strenum->HasMore(&hasMore)) && hasMore) {
    strenum->GetNext(entry);

    nsXPIDLCString contractID;
    rv = catman->GetCategoryEntry("command-line-validator",
				  entry.get(),
				  getter_Copies(contractID));
    if (!contractID)
      continue;

    nsCOMPtr<nsICommandLineValidator> clv(do_GetService(contractID.get()));
    if (!clv)
      continue;

    rv = (aCallback)(clv, this, aClosure);
    if (rv == NS_ERROR_ABORT)
      break;

    rv = NS_OK;
  }

  return rv;
}

static nsresult
EnumValidate(nsICommandLineValidator* aValidator, nsICommandLine* aThis, void*)
{
  return aValidator->Validate(aThis);
}  

static nsresult
EnumRun(nsICommandLineHandler* aHandler, nsICommandLine* aThis, void*)
{
  return aHandler->Handle(aThis);
}  

NS_IMETHODIMP
nsCommandLine::Run()
{
  nsresult rv;

  rv = EnumerateValidators(EnumValidate, nsnull);
  if (rv == NS_ERROR_ABORT)
    return rv;

  rv = EnumerateHandlers(EnumRun, nsnull);
  if (rv == NS_ERROR_ABORT)
    return rv;

  return NS_OK;
}

static nsresult
EnumHelp(nsICommandLineHandler* aHandler, nsICommandLine* aThis, void* aClosure)
{
  nsresult rv;

  nsCString text;
  rv = aHandler->GetHelpInfo(text);
  if (NS_SUCCEEDED(rv)) {
    NS_ASSERTION(text.Length() == 0 || text.Last() == '\n',
                 "Help text from command line handlers should end in a newline.");

    nsACString* totalText = reinterpret_cast<nsACString*>(aClosure);
    totalText->Append(text);
  }

  return NS_OK;
}  

NS_IMETHODIMP
nsCommandLine::GetHelpText(nsACString& aResult)
{
  EnumerateHandlers(EnumHelp, &aResult);

  return NS_OK;
}

NS_GENERIC_FACTORY_CONSTRUCTOR(nsCommandLine)

NS_DEFINE_NAMED_CID(NS_COMMANDLINE_CID);

static const mozilla::Module::CIDEntry kCommandLineCIDs[] = {
  { &kNS_COMMANDLINE_CID, false, NULL, nsCommandLineConstructor },
  { NULL }
};

static const mozilla::Module::ContractIDEntry kCommandLineContracts[] = {
  { "@mozilla.org/toolkit/command-line;1", &kNS_COMMANDLINE_CID },
  { NULL }
};

static const mozilla::Module kCommandLineModule = {
  mozilla::Module::kVersion,
  kCommandLineCIDs,
  kCommandLineContracts
};

NSMODULE_DEFN(CommandLineModule) = &kCommandLineModule;
