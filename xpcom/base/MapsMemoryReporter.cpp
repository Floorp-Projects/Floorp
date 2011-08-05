/* -*- Mode: C++; tab-width: 50; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 ci et: */
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
 * The Original Code is Mozilla.org code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 *
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Justin Lebar <justin.lebar@gmail.com>
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

#include "mozilla/MapsMemoryReporter.h"
#include "nsIMemoryReporter.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsHashSets.h"
#include <stdio.h>

namespace mozilla {
namespace MapsMemoryReporter {

#if !defined(XP_LINUX)
#error "This doesn't have a prayer of working if we're not on Linux."
#endif

// mozillaLibraries is a list of all the shared libraries we build.  This list
// is used for determining whether a library is a "Mozilla library" or a
// "third-party library".  But even if this list is missing items, about:memory
// will identify a library in the same directory as libxul.so as a "Mozilla
// library".
const nsCString mozillaLibraries[] =
{
  NS_LITERAL_CSTRING("libfreebl3.so"),
  NS_LITERAL_CSTRING("libmozalloc.so"),
  NS_LITERAL_CSTRING("libmozsqlite3.so"),
  NS_LITERAL_CSTRING("libnspr4.so"),
  NS_LITERAL_CSTRING("libnss3.so"),
  NS_LITERAL_CSTRING("libnssckbi.so"),
  NS_LITERAL_CSTRING("libnssdbm3.so"),
  NS_LITERAL_CSTRING("libnssutil3.so"),
  NS_LITERAL_CSTRING("libplc4.so"),
  NS_LITERAL_CSTRING("libplds4.so"),
  NS_LITERAL_CSTRING("libsmime3.so"),
  NS_LITERAL_CSTRING("libsoftokn3.so"),
  NS_LITERAL_CSTRING("libssl3.so"),
  NS_LITERAL_CSTRING("libxpcom.so"),
  NS_LITERAL_CSTRING("libxul.so")
};

namespace {

bool EndsWithLiteral(const nsCString &aHaystack, const char *aNeedle)
{
  PRInt32 idx = aHaystack.RFind(aNeedle);
  if (idx == -1) {
    return false;
  }

  return idx + strlen(aNeedle) == aHaystack.Length();
}

void GetDirname(const nsCString &aPath, nsACString &aOut)
{
  PRInt32 idx = aPath.RFind("/");
  if (idx == -1) {
    aOut.Truncate();
  }
  else {
    aOut.Assign(Substring(aPath, 0, idx));
  }
}

void GetBasename(const nsCString &aPath, nsACString &aOut)
{
  PRInt32 idx = aPath.RFind("/");
  if (idx == -1) {
    aOut.Assign(aPath);
  }
  else {
    aOut.Assign(Substring(aPath, idx + 1));
  }
}

} // anonymous namespace

class MapsReporter : public nsIMemoryMultiReporter
{
public:
  MapsReporter();

  NS_DECL_ISUPPORTS

  NS_IMETHOD
  CollectReports(nsIMemoryMultiReporterCallback *aCallback,
                 nsISupports *aClosure);

private:
  // Search through /proc/self/maps for libxul.so, and set mLibxulDir to the
  // the directory containing libxul.
  nsresult FindLibxul();

  nsresult
  ParseMapping(FILE *aFile,
               nsIMemoryMultiReporterCallback *aCallback,
               nsISupports *aClosure);

  void
  GetReporterNameAndDescription(const char *aPath,
                                const char *aPermissions,
                                nsACString &aName,
                                nsACString &aDesc);

  nsresult
  ParseMapBody(FILE *aFile,
               const nsACString &aName,
               const nsACString &aDescription,
               nsIMemoryMultiReporterCallback *aCallback,
               nsISupports *aClosure);

  nsCString mLibxulDir;
  nsCStringHashSet mMozillaLibraries;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(MapsReporter, nsIMemoryMultiReporter)

MapsReporter::MapsReporter()
{
  const PRUint32 len = NS_ARRAY_LENGTH(mozillaLibraries);
  mMozillaLibraries.Init(len);
  for (PRUint32 i = 0; i < len; i++) {
    mMozillaLibraries.Put(mozillaLibraries[i]);
  }
}

NS_IMETHODIMP
MapsReporter::CollectReports(nsIMemoryMultiReporterCallback *aCallback,
                             nsISupports *aClosure)
{
  FILE *f = fopen("/proc/self/smaps", "r");
  if (!f)
    return NS_ERROR_FAILURE;

  while (true) {
    nsresult rv = ParseMapping(f, aCallback, aClosure);
    if (NS_FAILED(rv))
      break;
  }

  fclose(f);
  return NS_OK;
}

nsresult
MapsReporter::FindLibxul()
{
  mLibxulDir.Truncate();

  // Note that we're scanning /proc/self/*maps*, not smaps, here.
  FILE *f = fopen("/proc/self/maps", "r");
  if (!f)
    return NS_ERROR_FAILURE;

  while (true) {
    // Skip any number of non-slash characters, then capture starting with the
    // slash to the newline.  This is the path part of /proc/self/maps.
    char path[1025];
    int numRead = fscanf(f, "%*[^/]%1024[^\n]", path);
    if (numRead != 1) {
      break;
    }

    nsCAutoString pathStr;
    pathStr.Append(path);

    nsCAutoString basename;
    GetBasename(pathStr, basename);

    if (basename.EqualsLiteral("libxul.so")) {
      GetDirname(pathStr, mLibxulDir);
      break;
    }
  }

  fclose(f);
  return mLibxulDir.IsEmpty() ? NS_ERROR_FAILURE : NS_OK;
}

nsresult
MapsReporter::ParseMapping(
  FILE *aFile,
  nsIMemoryMultiReporterCallback *aCallback,
  nsISupports *aClosure)
{
  // We need to use native types in order to get good warnings from fscanf, so
  // let's make sure that the native types have the sizes we expect.
  PR_STATIC_ASSERT(sizeof(long long) == sizeof(PRInt64));
  PR_STATIC_ASSERT(sizeof(int) == sizeof(PRInt32));

  if (mLibxulDir.IsEmpty()) {
    NS_ENSURE_SUCCESS(FindLibxul(), NS_ERROR_FAILURE);
  }

  // The first line of an entry in /proc/self/smaps looks just like an entry
  // in /proc/maps:
  //
  //   address           perms offset  dev   inode  pathname
  //   02366000-025d8000 rw-p 00000000 00:00 0      [heap]

  const int argCount = 8;

  unsigned long long addrStart, addrEnd;
  char perms[5];
  unsigned long long offset;
  unsigned int devMajor, devMinor, inode;
  char path[1025];

  // A path might not be present on this line; set it to the empty string.
  path[0] = '\0';

  // This is a bit tricky.  Whitespace in a scanf pattern matches *any*
  // whitespace, including newlines.  We want this pattern to match a line
  // with or without a path, but we don't want to look to a new line for the
  // path.  Thus we have %u%1024[^\n] at the end of the pattern.  This will
  // capture into the path some leading whitespace, which we'll later trim off.
  int numRead = fscanf(aFile, "%llx-%llx %4s %llx %u:%u %u%1024[^\n]",
                       &addrStart, &addrEnd, perms, &offset, &devMajor,
                       &devMinor, &inode, path);

  // Eat up any whitespace at the end of this line, including the newline.
  fscanf(aFile, " ");

  // We might or might not have a path, but the rest of the arguments should be
  // there.
  if (numRead != argCount && numRead != argCount - 1)
    return NS_ERROR_FAILURE;

  nsCAutoString name, description;
  GetReporterNameAndDescription(path, perms, name, description);

  while (true) {
    nsresult rv = ParseMapBody(aFile, name, description, aCallback, aClosure);
    if (NS_FAILED(rv))
      break;
  }

  return NS_OK;
}

void
MapsReporter::GetReporterNameAndDescription(
  const char *aPath,
  const char *aPerms,
  nsACString &aName,
  nsACString &aDesc)
{
  aName.Truncate();
  aDesc.Truncate();

  // If aPath points to a file, we have its absolute path, plus some
  // whitespace.  Truncate this to its basename, and put the absolute path in
  // the description.
  nsCAutoString absPath;
  absPath.Append(aPath);
  absPath.StripChars(" ");

  nsCAutoString basename;
  GetBasename(absPath, basename);

  if (basename.EqualsLiteral("[heap]")) {
    aName.Append("anonymous/anonymous, within brk()");
    aDesc.Append("Memory in anonymous mappings within the boundaries "
                 "defined by brk() / sbrk().  This is likely to be just "
                 "a portion of the application's heap; the remainder "
                 "lives in other anonymous mappings. This node corresponds to "
                 "'[heap]' in /proc/self/smaps.");
  }
  else if (basename.EqualsLiteral("[stack]")) {
    aName.Append("main thread's stack");
    aDesc.Append("The stack size of the process's main thread.  This node "
                 "corresponds to '[stack]' in /proc/self/smaps.");
  }
  else if (basename.EqualsLiteral("[vdso]")) {
    aName.Append("vdso");
    aDesc.Append("The virtual dynamically-linked shared object, also known as "
                 "the 'vsyscall page'. This is a memory region mapped by the "
                 "operating system for the purpose of allowing processes to "
                 "perform some privileged actions without the overhead of a "
                 "syscall.");
  }
  else if (!basename.IsEmpty()) {
    NS_ASSERTION(!mLibxulDir.IsEmpty(), "mLibxulDir should not be empty.");

    nsCAutoString dirname;
    GetDirname(absPath, dirname);

    // Hack: A file is a shared library if the basename contains ".so" and its
    // dirname contains "/lib", or if the basename ends with ".so".
    if (EndsWithLiteral(basename, ".so") ||
        (basename.Find(".so") != -1 && dirname.Find("/lib") != -1)) {
      aName.Append("shared-libraries/");
      if (dirname.Equals(mLibxulDir) || mMozillaLibraries.Contains(basename)) {
        aName.Append("shared-libraries-mozilla/");
      }
      else {
        aName.Append("shared-libraries-other/");
      }
    }
    else {
      aName.Append("other-files/");
      if (EndsWithLiteral(basename, ".xpi")) {
        aName.Append("extensions/");
      }
      else if (dirname.Find("/fontconfig") != -1) {
        aName.Append("fontconfig/");
      }
    }

    aName.Append(basename);
    aDesc.Append(absPath);
  }
  else {
    aName.Append("anonymous/anonymous, outside brk()");
    aDesc.Append("Memory in anonymous mappings outside the boundaries defined "
                 "by brk() / sbrk().");
  }

  aName.Append(" [");
  aName.Append(aPerms);
  aName.Append("]");

  // Modify the description to include an explanation of the permissions.
  aDesc.Append(" (");
  if (strstr(aPerms, "rw")) {
    aDesc.Append("read/write, ");
  }
  else if (strchr(aPerms, 'r')) {
    aDesc.Append("read-only, ");
  }
  else if (strchr(aPerms, 'w')) {
    aDesc.Append("write-only, ");
  }
  else {
    aDesc.Append("not readable, not writable, ");
  }

  if (strchr(aPerms, 'x')) {
    aDesc.Append("executable, ");
  }
  else {
    aDesc.Append("not executable, ");
  }

  if (strchr(aPerms, 's')) {
    aDesc.Append("shared");
  }
  else if (strchr(aPerms, 'p')) {
    aDesc.Append("private");
  }
  else {
    aDesc.Append("not shared or private??");
  }
  aDesc.Append(")");
}

nsresult
MapsReporter::ParseMapBody(
  FILE *aFile,
  const nsACString &aName,
  const nsACString &aDescription,
  nsIMemoryMultiReporterCallback *aCallback,
  nsISupports *aClosure)
{
  PR_STATIC_ASSERT(sizeof(long long) == sizeof(PRInt64));

  const int argCount = 2;

  char desc[1025];
  unsigned long long size;
  if (fscanf(aFile, "%1024[a-zA-Z_]: %llu kB\n",
             desc, &size) != argCount) {
    return NS_ERROR_FAILURE;
  }

  // Don't report nodes with size 0.
  if (size == 0)
    return NS_OK;

  const char* category;
  if (strcmp(desc, "Size") == 0) {
    category = "vsize";
  }
  else if (strcmp(desc, "Rss") == 0) {
    category = "resident";
  }
  else if (strcmp(desc, "Swap") == 0) {
    category = "swap";
  }
  else {
    // Don't report this category.
    return NS_OK;
  }

  nsCAutoString path;
  path.Append("map/");
  path.Append(category);
  path.Append("/");
  path.Append(aName);

  aCallback->Callback(NS_LITERAL_CSTRING(""),
                      path,
                      nsIMemoryReporter::KIND_NONHEAP,
                      nsIMemoryReporter::UNITS_BYTES,
                      PRInt64(size) * 1024, // convert from kB to bytes
                      aDescription, aClosure);

  return NS_OK;
}

void Init()
{
  nsCOMPtr<nsIMemoryMultiReporter> reporter = new MapsReporter();
  NS_RegisterMemoryMultiReporter(reporter);
}

} // namespace MapsMemoryReporter
} // namespace mozilla
