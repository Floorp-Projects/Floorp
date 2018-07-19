/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/MemoryMapping.h"

#include "mozilla/BinarySearch.h"
#include "mozilla/FileUtils.h"

#include <fstream>
#include <string>

namespace mozilla {

namespace {
struct VMFlagString
{
  const char* mName;
  const char* mPrettyName;
  VMFlag mFlag;
};

static const VMFlagString sVMFlagStrings[] = {
  {"ac", "Accountable",   VMFlag::Accountable},
  {"ar", "ArchSpecific",  VMFlag::ArchSpecific},
  {"dc", "NoFork",        VMFlag::NoFork},
  {"dd", "NoCore",        VMFlag::NoCore},
  {"de", "NoExpand",      VMFlag::NoExpand},
  {"dw", "DisabledWrite", VMFlag::DisabledWrite},
  {"ex", "Executable",    VMFlag::Executable},
  {"gd", "GrowsDown",     VMFlag::GrowsDown},
  {"hg", "HugePage",      VMFlag::HugePage},
  {"ht", "HugeTLB",       VMFlag::HugeTLB},
  {"io", "IO",            VMFlag::IO},
  {"lo", "Locked",        VMFlag::Locked},
  {"me", "MayExecute",    VMFlag::MayExecute},
  {"mg", "Mergeable",     VMFlag::Mergeable},
  {"mm", "MixedMap",      VMFlag::MixedMap},
  {"mr", "MayRead",       VMFlag::MayRead},
  {"ms", "MayShare",      VMFlag::MayShare},
  {"mw", "MayWrite",      VMFlag::MayWrite},
  {"nh", "NoHugePage",    VMFlag::NoHugePage},
  {"nl", "NonLinear",     VMFlag::NonLinear},
  {"nr", "NotReserved",   VMFlag::NotReserved},
  {"pf", "PurePFN",       VMFlag::PurePFN},
  {"rd", "Readable",      VMFlag::Readable},
  {"rr", "Random",        VMFlag::Random},
  {"sd", "SoftDirty",     VMFlag::SoftDirty},
  {"sh", "Shared",        VMFlag::Shared},
  {"sr", "Sequential",    VMFlag::Sequential},
  {"wr", "Writable",      VMFlag::Writable},
};
} // anonymous namespace

constexpr size_t kVMFlags = size_t(-1);

// An array of known field names which may be present in an smaps file, and the
// offsets of the corresponding fields in a MemoryMapping class.
const MemoryMapping::Field MemoryMapping::sFields[] = {
  {"AnonHugePages",   offsetof(MemoryMapping, mAnonHugePages)},
  {"Anonymous",       offsetof(MemoryMapping, mAnonymous)},
  {"KernelPageSize",  offsetof(MemoryMapping, mKernelPageSize)},
  {"LazyFree",        offsetof(MemoryMapping, mLazyFree)},
  {"Locked",          offsetof(MemoryMapping, mLocked)},
  {"MMUPageSize",     offsetof(MemoryMapping, mMMUPageSize)},
  {"Private_Clean",   offsetof(MemoryMapping, mPrivate_Clean)},
  {"Private_Dirty",   offsetof(MemoryMapping, mPrivate_Dirty)},
  {"Private_Hugetlb", offsetof(MemoryMapping, mPrivate_Hugetlb)},
  {"Pss",             offsetof(MemoryMapping, mPss)},
  {"Referenced",      offsetof(MemoryMapping, mReferenced)},
  {"Rss",             offsetof(MemoryMapping, mRss)},
  {"Shared_Clean",    offsetof(MemoryMapping, mShared_Clean)},
  {"Shared_Dirty",    offsetof(MemoryMapping, mShared_Dirty)},
  {"Shared_Hugetlb",  offsetof(MemoryMapping, mShared_Hugetlb)},
  {"ShmemPmdMapped",  offsetof(MemoryMapping, mShmemPmdMapped)},
  {"Size",            offsetof(MemoryMapping, mSize)},
  {"Swap",            offsetof(MemoryMapping, mSwap)},
  {"SwapPss",         offsetof(MemoryMapping, mSwapPss)},
  // VmFlags is a special case. It contains an array of flag strings, which
  // describe attributes of the mapping, rather than a mapping size. We include
  // it in this array to aid in parsing, but give it a separate sentinel value,
  // and treat it specially.
  {"VmFlags",         kVMFlags},
};

template <typename T, int n>
const T*
FindEntry(const char* aName, const T (&aEntries)[n])
{
  size_t index;
  if (BinarySearchIf(aEntries, 0, n,
                     [&] (const T& aEntry) {
                       return strcmp(aName, aEntry.mName);
                     },
                     &index)) {
    return &aEntries[index];
  }
  return nullptr;
}

using Perm = MemoryMapping::Perm;
using PermSet = MemoryMapping::PermSet;

nsresult
GetMemoryMappings(nsTArray<MemoryMapping>& aMappings)
{
  std::ifstream stream("/proc/self/smaps");
  if (stream.fail()) {
    return NS_ERROR_FAILURE;
  }

  MemoryMapping* current = nullptr;
  std::string buffer;
  while (std::getline(stream, buffer)) {
    size_t start, end, offset;
    char flags[4] = "---";
    char name[512];

    name[0] = 0;

    // Match the start of an entry. A typical line looks something like:
    //
    // 1487118a7000-148711a5a000 r-xp 00000000 103:03 54004561                  /usr/lib/libc-2.27.so
    if (sscanf(buffer.c_str(), "%zx-%zx %4c %zx %*u:%*u %*u %511s\n",
               &start, &end, flags, &offset, name) >= 4) {
      PermSet perms;
      if (flags[0] == 'r') {
        perms += Perm::Read;
      }
      if (flags[1] == 'w') {
        perms += Perm::Write;
      }
      if (flags[2] == 'x') {
        perms += Perm::Execute;
      }
      if (flags[3] == 'p') {
        perms += Perm::Private;
      } else if (flags[3] == 's') {
        perms += Perm::Shared;
      }

      current = aMappings.AppendElement(MemoryMapping{start, end, perms, offset, name});
      continue;
    }
    if (!current) {
      continue;
    }

    nsAutoCStringN<128> line(buffer.c_str());
    char* savePtr;
    char* fieldName = strtok_r(line.BeginWriting(), ":", &savePtr);
    if (!fieldName) {
      continue;
    }
    auto* field = FindEntry(fieldName, MemoryMapping::sFields);
    if (!field) {
      continue;
    }

    if (field->mOffset == kVMFlags) {
      while (char* flagName = strtok_r(nullptr, " \n", &savePtr)) {
        if (auto* flag = FindEntry(flagName, sVMFlagStrings)) {
          current->mFlags += flag->mFlag;
        }
      }
      continue;
    }

    const char* rest = strtok_r(nullptr, "\n", &savePtr);
    size_t value;
    if (sscanf(rest, "%zd kB", &value) > 0) {
      current->ValueForField(*field) = value * 1024;
    }
  }

  return NS_OK;
}

void
MemoryMapping::Dump(nsACString& aOut) const
{
  aOut.AppendPrintf("%zx-%zx Size: %zu Offset: %zx %s\n",
                    mStart, mEnd,
                    mEnd - mStart,
                    mOffset, mName.get());

  for (auto& field : MemoryMapping::sFields) {
    if (field.mOffset < sizeof(*this)) {
      aOut.AppendPrintf("  %s: %zd\n", field.mName, ValueForField(field));
    }
  }

  aOut.AppendPrintf("  Flags: %x\n", mFlags.serialize());
  for (auto& flag : sVMFlagStrings) {
    if (mFlags.contains(flag.mFlag)) {
      aOut.AppendPrintf("       : %s %s\n", flag.mName, flag.mPrettyName);
    }
  }
}

} // namespace mozilla
