/* -*- Mode: C++; tab-width: 50; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 ci et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "mozilla/MapsMemoryReporter.h"
#include "nsIMemoryReporter.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsTHashtable.h"
#include "nsHashKeys.h"
#include "mozilla/Attributes.h"
#include "mozilla/unused.h"
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
const char* mozillaLibraries[] =
{
  "libfreebl3.so",
  "libmozalloc.so",
  "libmozsqlite3.so",
  "libnspr4.so",
  "libnss3.so",
  "libnssckbi.so",
  "libnssdbm3.so",
  "libnssutil3.so",
  "libplc4.so",
  "libplds4.so",
  "libsmime3.so",
  "libsoftokn3.so",
  "libssl3.so",
  "libxpcom.so",
  "libxul.so"
};

namespace {

bool EndsWithLiteral(const nsCString &aHaystack, const char *aNeedle)
{
  int32_t idx = aHaystack.RFind(aNeedle);
  if (idx == -1) {
    return false;
  }

  return idx + strlen(aNeedle) == aHaystack.Length();
}

void GetDirname(const nsCString &aPath, nsACString &aOut)
{
  int32_t idx = aPath.RFind("/");
  if (idx == -1) {
    aOut.Truncate();
  }
  else {
    aOut.Assign(Substring(aPath, 0, idx));
  }
}

void GetBasename(const nsCString &aPath, nsACString &aOut)
{
  nsCString out;
  int32_t idx = aPath.RFind("/");
  if (idx == -1) {
    out.Assign(aPath);
  }
  else {
    out.Assign(Substring(aPath, idx + 1));
  }

  // On Android, some entries in /dev/ashmem end with "(deleted)" (e.g.
  // "/dev/ashmem/libxul.so(deleted)").  We don't care about this modifier, so
  // cut it off when getting the entry's basename.
  if (EndsWithLiteral(out, "(deleted)")) {
    out.Assign(Substring(out, 0, out.RFind("(deleted)")));
  }
  out.StripChars(" ");

  aOut.Assign(out);
}

// MapsReporter::CollectReports uses this stuct to keep track of whether it's
// seen a mapping under 'rss', 'pss', 'size', and 'swap'.
struct CategoriesSeen {
  CategoriesSeen() :
    mSeenRss(false),
    mSeenPss(false),
    mSeenSize(false),
    mSeenSwap(false)
  {
  }

  bool mSeenRss;
  bool mSeenPss;
  bool mSeenSize;
  bool mSeenSwap;
};

} // anonymous namespace

class MapsReporter MOZ_FINAL : public nsIMemoryMultiReporter
{
public:
  MapsReporter();

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetName(nsACString &aName)
  {
      aName.AssignLiteral("smaps");
      return NS_OK;
  }

  NS_IMETHOD
  CollectReports(nsIMemoryMultiReporterCallback *aCb,
                 nsISupports *aClosure);

  NS_IMETHOD
  GetExplicitNonHeap(int64_t *aAmount) {
    // This reporter doesn't do any "explicit" measurements.
    *aAmount = 0;
    return NS_OK;
  }

private:
  // Search through /proc/self/maps for libxul.so, and set mLibxulDir to the
  // the directory containing libxul.
  nsresult FindLibxul();

  nsresult
  ParseMapping(FILE *aFile,
               nsIMemoryMultiReporterCallback *aCb,
               nsISupports *aClosure,
               CategoriesSeen *aCategoriesSeen);

  void
  GetReporterNameAndDescription(const char *aPath,
                                const char *aPermissions,
                                nsACString &aName,
                                nsACString &aDesc);

  nsresult
  ParseMapBody(FILE *aFile,
               const nsACString &aName,
               const nsACString &aDescription,
               nsIMemoryMultiReporterCallback *aCb,
               nsISupports *aClosure,
               CategoriesSeen *aCategoriesSeen);

  bool mSearchedForLibxul;
  nsCString mLibxulDir;
  nsTHashtable<nsCStringHashKey> mMozillaLibraries;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(MapsReporter, nsIMemoryMultiReporter)

MapsReporter::MapsReporter()
  : mSearchedForLibxul(false)
{
  const uint32_t len = ArrayLength(mozillaLibraries);
  mMozillaLibraries.Init(len);
  for (uint32_t i = 0; i < len; i++) {
    nsAutoCString str;
    str.Assign(mozillaLibraries[i]);
    mMozillaLibraries.PutEntry(str);
  }
}

NS_IMETHODIMP
MapsReporter::CollectReports(nsIMemoryMultiReporterCallback *aCb,
                             nsISupports *aClosure)
{
  CategoriesSeen categoriesSeen;

  FILE *f = fopen("/proc/self/smaps", "r");
  if (!f)
    return NS_ERROR_FAILURE;

  while (true) {
    nsresult rv = ParseMapping(f, aCb, aClosure, &categoriesSeen);
    if (NS_FAILED(rv))
      break;
  }

  fclose(f);

  // For sure we should have created some node under 'rss' and
  // 'size'; otherwise we're probably not reading smaps correctly.  If we
  // didn't create a node under 'swap', create one here so about:memory
  // knows to create an empty 'swap' tree;  it needs a 'total' child because
  // about:memory expects at least one report whose path begins with 'swap/'.

  NS_ASSERTION(categoriesSeen.mSeenSize, "Didn't create a size node?");
  NS_ASSERTION(categoriesSeen.mSeenRss, "Didn't create a rss node?");
  NS_ASSERTION(categoriesSeen.mSeenPss, "Didn't create a pss node?");
  if (!categoriesSeen.mSeenSwap) {
    nsresult rv;
    rv = aCb->Callback(NS_LITERAL_CSTRING(""),
                       NS_LITERAL_CSTRING("swap/total"),
                       nsIMemoryReporter::KIND_NONHEAP,
                       nsIMemoryReporter::UNITS_BYTES,
                       0,
                       NS_LITERAL_CSTRING("This process uses no swap space."),
                       aClosure);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
MapsReporter::FindLibxul()
{
  if (mSearchedForLibxul)
    return NS_OK;

  mSearchedForLibxul = true;

  mLibxulDir.Truncate();

  // Note that we're scanning /proc/self/*maps*, not smaps, here.
  FILE *f = fopen("/proc/self/maps", "r");
  if (!f) {
    return NS_ERROR_FAILURE;
  }

  while (true) {
    // Skip any number of non-slash characters, then capture starting with the
    // slash to the newline.  This is the path part of /proc/self/maps.
    char path[1025];
    int numRead = fscanf(f, "%*[^/]%1024[^\n]", path);
    if (numRead != 1) {
      break;
    }

    nsAutoCString pathStr;
    pathStr.Append(path);

    nsAutoCString basename;
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
  nsIMemoryMultiReporterCallback *aCb,
  nsISupports *aClosure,
  CategoriesSeen *aCategoriesSeen)
{
  // We need to use native types in order to get good warnings from fscanf, so
  // let's make sure that the native types have the sizes we expect.
  MOZ_STATIC_ASSERT(sizeof(long long) == sizeof(int64_t),
                    "size of (long long) is expected to match (int64_t)");
  MOZ_STATIC_ASSERT(sizeof(int) == sizeof(int32_t),
                    "size of (int) is expected to match (int32_t)");

  // Don't bail if FindLibxul fails.  We can still gather meaningful stats
  // here.
  FindLibxul();

  // The first line of an entry in /proc/self/smaps looks just like an entry
  // in /proc/maps:
  //
  //   address           perms offset  dev   inode  pathname
  //   02366000-025d8000 rw-p 00000000 00:00 0      [heap]

  const int argCount = 8;

  unsigned long long addrStart, addrEnd;
  char perms[5];
  unsigned long long offset;
  // The 2.6 and 3.0 kernels allocate 12 bits for the major device number and
  // 20 bits for the minor device number.  Future kernels might allocate more.
  // 64 bits ought to be enough for anybody.
  char devMajor[17];
  char devMinor[17];
  unsigned int inode;
  char path[1025];

  // A path might not be present on this line; set it to the empty string.
  path[0] = '\0';

  // This is a bit tricky.  Whitespace in a scanf pattern matches *any*
  // whitespace, including newlines.  We want this pattern to match a line
  // with or without a path, but we don't want to look to a new line for the
  // path.  Thus we have %u%1024[^\n] at the end of the pattern.  This will
  // capture into the path some leading whitespace, which we'll later trim off.
  int numRead = fscanf(aFile,
                       "%llx-%llx %4s %llx "
                       "%16[0-9a-fA-F]:%16[0-9a-fA-F] %u%1024[^\n]",
                       &addrStart, &addrEnd, perms, &offset, devMajor,
                       devMinor, &inode, path);

  // Eat up any whitespace at the end of this line, including the newline.
  unused << fscanf(aFile, " ");

  // We might or might not have a path, but the rest of the arguments should be
  // there.
  if (numRead != argCount && numRead != argCount - 1) {
    return NS_ERROR_FAILURE;
  }

  nsAutoCString name, description;
  GetReporterNameAndDescription(path, perms, name, description);

  while (true) {
    nsresult rv = ParseMapBody(aFile, name, description, aCb,
                               aClosure, aCategoriesSeen);
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
  nsAutoCString absPath;
  absPath.Append(aPath);
  absPath.StripChars(" ");

  nsAutoCString basename;
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
    nsAutoCString dirname;
    GetDirname(absPath, dirname);

    // Hack: A file is a shared library if the basename contains ".so" and its
    // dirname contains "/lib", or if the basename ends with ".so".
    if (EndsWithLiteral(basename, ".so") ||
        (basename.Find(".so") != -1 && dirname.Find("/lib") != -1)) {
      aName.Append("shared-libraries/");
      if ((!mLibxulDir.IsEmpty() && dirname.Equals(mLibxulDir)) ||
          mMozillaLibraries.Contains(basename)) {
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

  aName.Append("/[");
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
  nsIMemoryMultiReporterCallback *aCb,
  nsISupports *aClosure,
  CategoriesSeen *aCategoriesSeen)
{
  MOZ_STATIC_ASSERT(sizeof(long long) == sizeof(int64_t),
                    "size of (long long) is expected to match (int64_t)");

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
    category = "size";
    aCategoriesSeen->mSeenSize = true;
  }
  else if (strcmp(desc, "Rss") == 0) {
    category = "rss";
    aCategoriesSeen->mSeenRss = true;
  }
  else if (strcmp(desc, "Pss") == 0) {
    category = "pss";
    aCategoriesSeen->mSeenPss = true;
  }
  else if (strcmp(desc, "Swap") == 0) {
    category = "swap";
    aCategoriesSeen->mSeenSwap = true;
  }
  else {
    // Don't report this category.
    return NS_OK;
  }

  nsAutoCString path;
  path.Append(category);
  path.Append("/");
  path.Append(aName);

  nsresult rv;
  rv = aCb->Callback(NS_LITERAL_CSTRING(""),
                     path,
                     nsIMemoryReporter::KIND_NONHEAP,
                     nsIMemoryReporter::UNITS_BYTES,
                     int64_t(size) * 1024, // convert from kB to bytes
                     aDescription, aClosure);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

static nsresult GetUSS(int64_t *n)
{
    // You might be tempted to calculate USS by subtracting the "shared" value
    // from the "resident" value in /proc/<pid>/statm.  But at least on Linux,
    // statm's "shared" value actually counts pages backed by files, which has
    // little to do with whether the pages are actually shared.  smaps on the
    // other hand appears to give us the correct information.
    //
    // We could calculate this data during the smaps multi-reporter, but the
    // overhead of the smaps reporter is considerable (we don't even run the
    // smaps reporter in normal about:memory operation).  Hopefully this
    // implementation is fast enough not to matter.

    *n = 0;

    FILE *f = fopen("/proc/self/smaps", "r");
    NS_ENSURE_STATE(f);

    int64_t total = 0;
    char line[256];
    while(fgets(line, sizeof(line), f)) {
        long long val = 0;
        if(sscanf(line, "Private_Dirty: %lld kB", &val) == 1 ||
           sscanf(line, "Private_Clean: %lld kB", &val) == 1) {
            total += val * 1024; // convert from kB to bytes
        }
    }
    *n = total;

    fclose(f);
    return NS_OK;
}

NS_FALLIBLE_MEMORY_REPORTER_IMPLEMENT(USS,
    "resident-unique",
    KIND_OTHER,
    UNITS_BYTES,
    GetUSS,
    "Memory mapped by the process that is present in physical memory and not "
    "shared with any other processes.  This is also known as the process's "
    "unique set size (USS).  This is the amount of RAM we'd expect to be freed "
    "if we closed this process.")

void Init()
{
  nsCOMPtr<nsIMemoryMultiReporter> reporter = new MapsReporter();
  NS_RegisterMemoryMultiReporter(reporter);

  NS_RegisterMemoryReporter(new NS_MEMORY_REPORTER_NAME(USS));
}

} // namespace MapsMemoryReporter
} // namespace mozilla
