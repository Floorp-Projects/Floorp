/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
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

#include <stdio.h>
#include "nsUpdateDriver.h"
#include "nsILocalFile.h"
#include "nsISimpleEnumerator.h"
#include "nsIDirectoryEnumerator.h"
#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsString.h"
#include "nsPrintfCString.h"
#include "prproces.h"
#include "prlog.h"

#if defined(XP_WIN)
# include <direct.h>
# include <process.h>
# include <windows.h>
#elif defined(XP_UNIX)
# include <unistd.h>
#endif

#ifdef PR_LOGGING
static PRLogModuleInfo *sUpdateLog = PR_NewLogModule("updatedriver");
#endif
#define LOG(args) PR_LOG(sUpdateLog, PR_LOG_DEBUG, args)

#ifdef XP_WIN
static const char kUpdaterBin[] = "updater.exe";
#else
static const char kUpdaterBin[] = "updater";
#endif
static const char kUpdaterINI[] = "updater.ini";

#ifdef XP_WIN
// On windows, we need to double-quote parameters before passing them to execv.
// Otherwise, they will get parsed as separate parameters by the new process.
static void
DoubleQuoteIfNeeded(nsCString &str)
{
  if (str.FindChar(' ') != kNotFound) {
    str.Insert('\"', 0);
    str.Append('\"');
  }
}
#endif

static void
MakeCommandLine(char **argv, nsCString &result)
{
  result.Truncate();
  while (*argv) {
    result.Append(*argv);
    ++argv;
    if (*argv)
      result.Append(' ');
  }
}

PR_STATIC_CALLBACK(int)
ScanDirComparator(nsIFile *a, nsIFile *b, void *unused)
{
  // lexically compare the leaf names of these two files
  nsCAutoString a_name, b_name;
  a->GetNativeLeafName(a_name);
  b->GetNativeLeafName(b_name);
  return Compare(a_name, b_name);
}

static nsresult
ScanDir(nsIFile *dir, nsCOMArray<nsIFile> *result)
{
  nsresult rv;

  nsCOMPtr<nsISimpleEnumerator> simpEnum;
  rv = dir->GetDirectoryEntries(getter_AddRefs(simpEnum));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIDirectoryEnumerator> dirEnum = do_QueryInterface(simpEnum, &rv);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIFile> file;
  for (;;) {
    rv = dirEnum->GetNextFile(getter_AddRefs(file));
    if (NS_FAILED(rv))
      return rv;

    // enumeration complete when null file is returned
    if (!file)
      break;

    if (!result->AppendObject(file))
      return NS_ERROR_OUT_OF_MEMORY;
  }

  result->Sort(ScanDirComparator, nsnull);
  return NS_OK;
}

static PRBool
GetFile(nsIFile *dir, const nsCSubstring &name, nsCOMPtr<nsILocalFile> &result)
{
  nsresult rv;
  
  nsCOMPtr<nsIFile> statusFile;
  rv = dir->Clone(getter_AddRefs(statusFile));
  if (NS_FAILED(rv))
    return PR_FALSE;

  rv = statusFile->AppendNative(name);
  if (NS_FAILED(rv))
    return PR_FALSE;

  result = do_QueryInterface(statusFile, &rv);
  return NS_SUCCEEDED(rv);
}

static PRBool
GetStatusFile(nsIFile *dir, nsCOMPtr<nsILocalFile> &result)
{
  return GetFile(dir, NS_LITERAL_CSTRING("update.status"), result);
}

static PRBool
IsPending(nsILocalFile *statusFile)
{
  nsresult rv;

  FILE *fp;
  rv = statusFile->OpenANSIFileDesc("r", &fp);
  if (NS_FAILED(rv))
    return PR_FALSE;

  char buf[32];
  char *result = fgets(buf, sizeof(buf), fp);
  fclose(fp);
  if (!result)
    return PR_FALSE;
  
  const char kPending[] = "pending";
  return (strncmp(buf, kPending, sizeof(kPending) - 1) == 0);
}

static PRBool
SetStatus(nsILocalFile *statusFile, const char *status)
{
  FILE *fp;
  nsresult rv = statusFile->OpenANSIFileDesc("w", &fp);
  if (NS_FAILED(rv))
    return PR_FALSE;

  fprintf(fp, "%s\n", status);
  fclose(fp);
  return PR_TRUE;
}

static PRBool
CopyUpdaterIntoUpdateDir(nsIFile *appDir, nsIFile *updateDir,
                         nsCOMPtr<nsIFile> &updater)
{
  // We have to move the updater binary and its resource file.
  const char *filesToMove[] = {
    kUpdaterINI,
    kUpdaterBin,
    nsnull
  };

  nsresult rv;

  for (const char **leafName = filesToMove; *leafName; ++leafName) {
    nsDependentCString leaf(*leafName);
    nsCOMPtr<nsIFile> file;

    // Make sure there is not an existing file in the target location.
    rv = updateDir->Clone(getter_AddRefs(file));
    if (NS_FAILED(rv))
      return PR_FALSE;
    rv = file->AppendNative(leaf);
    if (NS_FAILED(rv))
      return PR_FALSE;
    file->Remove(PR_FALSE);

    // Now, copy into the target location.
    rv = appDir->Clone(getter_AddRefs(file));
    if (NS_FAILED(rv))
      return PR_FALSE;
    rv = file->AppendNative(leaf);
    if (NS_FAILED(rv))
      return PR_FALSE;
    rv = file->CopyToNative(updateDir, EmptyCString());
    if (NS_FAILED(rv))
      return PR_FALSE;
  }
  
  // Finally, return the location of the updater binary.
  rv = updateDir->Clone(getter_AddRefs(updater));
  if (NS_FAILED(rv))
    return PR_FALSE;
  rv = updater->AppendNative(NS_LITERAL_CSTRING(kUpdaterBin));
  return NS_SUCCEEDED(rv); 
}

static void
ApplyUpdate(nsIFile *appDir, nsIFile *updateDir, nsILocalFile *statusFile,
            int appArgc, char **appArgv)
{
  // Steps:
  //  - mark update as 'applying'
  //  - copy updater into update dir
  //  - run updater w/ app dir as the current working dir

  nsCOMPtr<nsIFile> updater;
  if (!CopyUpdaterIntoUpdateDir(appDir, updateDir, updater)) {
    LOG(("failed copying updater\n"));
    return;
  }
  
  nsCAutoString updaterPath;
  nsresult rv = updater->GetNativePath(updaterPath);
  if (NS_FAILED(rv))
    return;
  
  nsCAutoString appDirPath;
  rv = appDir->GetNativePath(appDirPath);
  if (NS_FAILED(rv))
    return;

  nsCAutoString updateDirPath;
  rv = updateDir->GetNativePath(updateDirPath);
  if (NS_FAILED(rv))
    return;

  // Form the command line that will be used to call us back.
  nsCAutoString commandLine;
  MakeCommandLine(appArgv, commandLine);

  if (!SetStatus(statusFile, "applying")) {
    LOG(("failed setting status to 'applying'\n"));
    return;
  }

#if defined(XP_WIN)
  // On windows, we may need to quote these paths.
  DoubleQuoteIfNeeded(updaterPath);
  DoubleQuoteIfNeeded(updateDirPath);
  DoubleQuoteIfNeeded(commandLine);
#endif

  char **argv = new char*[5];
  if (!argv)
    return;
  argv[0] = (char*) updaterPath.get();
  argv[1] = (char*) updateDirPath.get();
  argv[2] = (char*) commandLine.get();
#if defined(XP_WIN)
  nsPrintfCString pid("%u", GetCurrentProcessId());
  argv[3] = (char*) pid.get();
  argv[4] = nsnull;
#else
  argv[3] = nsnull;
#endif

  LOG(("spawning updater process...\n"));

#if defined(XP_WIN) || defined(XP_UNIX)
  chdir(appDirPath.get());
  execv(updaterPath.get(), argv);
#else
  PRStatus status;
  PRProcessAttr *attr;
  
  attr = PR_NewProcessAttr();
  if (!attr)
    goto end;

  status = PR_ProcessAttrSetCurrentDirectory(attr, appDirPath.get());
  if (status != PR_SUCCESS)
    goto end;

  PR_CreateProcessDetached(updaterPath.get(), argv, nsnull, attr);
  exit(1);

end:
  PR_DestroyProcessAttr(attr); 
  delete[] argv;
#endif
}

nsresult
ProcessUpdates(nsIFile *appDir, int argc, char **argv)
{
  nsresult rv;

  nsCOMPtr<nsIFile> updatesDir;
  rv = appDir->Clone(getter_AddRefs(updatesDir));
  if (NS_FAILED(rv))
    return rv;
  rv = updatesDir->AppendNative(NS_LITERAL_CSTRING("updates"));
  if (NS_FAILED(rv))
    return rv;

  PRBool exists;
  rv = updatesDir->Exists(&exists);
  if (NS_FAILED(rv) || !exists)
    return rv;

  nsCOMArray<nsIFile> dirEntries;
  rv = ScanDir(updatesDir, &dirEntries);
  if (NS_FAILED(rv))
    return rv;
  if (dirEntries.Count() == 0)
    return NS_OK;

  // look for the first update subdirectory with a status of pending
  for (int i = 0; i < dirEntries.Count(); ++i) {
    nsCOMPtr<nsILocalFile> statusFile;
    if (GetStatusFile(dirEntries[i], statusFile) && IsPending(statusFile)) {
      ApplyUpdate(appDir, dirEntries[i], statusFile, argc, argv);
      break;
    }
  }

  return NS_OK;
}
