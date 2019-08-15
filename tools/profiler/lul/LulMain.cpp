/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LulMain.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>  // write(), only for testing LUL

#include <algorithm>  // std::sort
#include <string>

#include "mozilla/Assertions.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/MemoryChecking.h"
#include "mozilla/Move.h"
#include "mozilla/Sprintf.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"

#include "LulCommonExt.h"
#include "LulElfExt.h"

#include "LulMainInt.h"

#include "GeckoProfiler.h"  // for profiler_current_thread_id()

// Set this to 1 for verbose logging
#define DEBUG_MAIN 0

namespace lul {

using mozilla::CheckedInt;
using mozilla::DebugOnly;
using mozilla::MallocSizeOf;
using mozilla::Unused;
using std::pair;
using std::string;
using std::vector;

// WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
//
// Some functions in this file are marked RUNS IN NO-MALLOC CONTEXT.
// Any such function -- and, hence, the transitive closure of those
// reachable from it -- must not do any dynamic memory allocation.
// Doing so risks deadlock.  There is exactly one root function for
// the transitive closure: Lul::Unwind.
//
// WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING

////////////////////////////////////////////////////////////////
// RuleSet                                                    //
////////////////////////////////////////////////////////////////

static const char* NameOf_DW_REG(int16_t aReg) {
  switch (aReg) {
    case DW_REG_CFA:
      return "cfa";
#if defined(GP_ARCH_amd64) || defined(GP_ARCH_x86)
    case DW_REG_INTEL_XBP:
      return "xbp";
    case DW_REG_INTEL_XSP:
      return "xsp";
    case DW_REG_INTEL_XIP:
      return "xip";
#elif defined(GP_ARCH_arm)
    case DW_REG_ARM_R7:
      return "r7";
    case DW_REG_ARM_R11:
      return "r11";
    case DW_REG_ARM_R12:
      return "r12";
    case DW_REG_ARM_R13:
      return "r13";
    case DW_REG_ARM_R14:
      return "r14";
    case DW_REG_ARM_R15:
      return "r15";
#elif defined(GP_ARCH_arm64)
    case DW_REG_AARCH64_X29:
      return "x29";
    case DW_REG_AARCH64_X30:
      return "x30";
    case DW_REG_AARCH64_SP:
      return "sp";
#elif defined(GP_ARCH_mips64)
    case DW_REG_MIPS_SP:
      return "sp";
    case DW_REG_MIPS_FP:
      return "fp";
    case DW_REG_MIPS_PC:
      return "pc";
#else
#  error "Unsupported arch"
#endif
    default:
      return "???";
  }
}

string LExpr::ShowRule(const char* aNewReg) const {
  char buf[64];
  string res = string(aNewReg) + "=";
  switch (mHow) {
    case UNKNOWN:
      res += "Unknown";
      break;
    case NODEREF:
      SprintfLiteral(buf, "%s+%d", NameOf_DW_REG(mReg), (int)mOffset);
      res += buf;
      break;
    case DEREF:
      SprintfLiteral(buf, "*(%s+%d)", NameOf_DW_REG(mReg), (int)mOffset);
      res += buf;
      break;
    case PFXEXPR:
      SprintfLiteral(buf, "PfxExpr-at-%d", (int)mOffset);
      res += buf;
      break;
    default:
      res += "???";
      break;
  }
  return res;
}

void RuleSet::Print(void (*aLog)(const char*)) const {
  char buf[96];
  SprintfLiteral(buf, "[%llx .. %llx]: let ", (unsigned long long int)mAddr,
                 (unsigned long long int)(mAddr + mLen - 1));
  string res = string(buf);
  res += mCfaExpr.ShowRule("cfa");
  res += " in";
  // For each reg we care about, print the recovery expression.
#if defined(GP_ARCH_amd64) || defined(GP_ARCH_x86)
  res += mXipExpr.ShowRule(" RA");
  res += mXspExpr.ShowRule(" SP");
  res += mXbpExpr.ShowRule(" BP");
#elif defined(GP_ARCH_arm)
  res += mR15expr.ShowRule(" R15");
  res += mR7expr.ShowRule(" R7");
  res += mR11expr.ShowRule(" R11");
  res += mR12expr.ShowRule(" R12");
  res += mR13expr.ShowRule(" R13");
  res += mR14expr.ShowRule(" R14");
#elif defined(GP_ARCH_arm64)
  res += mX29expr.ShowRule(" X29");
  res += mX30expr.ShowRule(" X30");
  res += mSPexpr.ShowRule(" SP");
#elif defined(GP_ARCH_mips64)
  res += mPCexpr.ShowRule(" PC");
  res += mSPexpr.ShowRule(" SP");
  res += mFPexpr.ShowRule(" FP");
#else
#  error "Unsupported arch"
#endif
  aLog(res.c_str());
}

LExpr* RuleSet::ExprForRegno(DW_REG_NUMBER aRegno) {
  switch (aRegno) {
    case DW_REG_CFA:
      return &mCfaExpr;
#if defined(GP_ARCH_amd64) || defined(GP_ARCH_x86)
    case DW_REG_INTEL_XIP:
      return &mXipExpr;
    case DW_REG_INTEL_XSP:
      return &mXspExpr;
    case DW_REG_INTEL_XBP:
      return &mXbpExpr;
#elif defined(GP_ARCH_arm)
    case DW_REG_ARM_R15:
      return &mR15expr;
    case DW_REG_ARM_R14:
      return &mR14expr;
    case DW_REG_ARM_R13:
      return &mR13expr;
    case DW_REG_ARM_R12:
      return &mR12expr;
    case DW_REG_ARM_R11:
      return &mR11expr;
    case DW_REG_ARM_R7:
      return &mR7expr;
#elif defined(GP_ARCH_arm64)
    case DW_REG_AARCH64_X29:
      return &mX29expr;
    case DW_REG_AARCH64_X30:
      return &mX30expr;
    case DW_REG_AARCH64_SP:
      return &mSPexpr;
#elif defined(GP_ARCH_mips64)
    case DW_REG_MIPS_SP:
      return &mSPexpr;
    case DW_REG_MIPS_FP:
      return &mFPexpr;
    case DW_REG_MIPS_PC:
      return &mPCexpr;
#else
#  error "Unknown arch"
#endif
    default:
      return nullptr;
  }
}

RuleSet::RuleSet() {
  mAddr = 0;
  mLen = 0;
  // The only other fields are of type LExpr and those are initialised
  // by LExpr::LExpr().
}

////////////////////////////////////////////////////////////////
// SecMap                                                     //
////////////////////////////////////////////////////////////////

// See header file LulMainInt.h for comments about invariants.

SecMap::SecMap(void (*aLog)(const char*))
    : mSummaryMinAddr(1), mSummaryMaxAddr(0), mUsable(true), mLog(aLog) {}

SecMap::~SecMap() { mRuleSets.clear(); }

// RUNS IN NO-MALLOC CONTEXT
RuleSet* SecMap::FindRuleSet(uintptr_t ia) {
  // Binary search mRuleSets to find one that brackets |ia|.
  // lo and hi need to be signed, else the loop termination tests
  // don't work properly.  Note that this works correctly even when
  // mRuleSets.size() == 0.

  // Can't do this until the array has been sorted and preened.
  MOZ_ASSERT(mUsable);

  long int lo = 0;
  long int hi = (long int)mRuleSets.size() - 1;
  while (true) {
    // current unsearched space is from lo to hi, inclusive.
    if (lo > hi) {
      // not found
      return nullptr;
    }
    long int mid = lo + ((hi - lo) / 2);
    RuleSet* mid_ruleSet = &mRuleSets[mid];
    uintptr_t mid_minAddr = mid_ruleSet->mAddr;
    uintptr_t mid_maxAddr = mid_minAddr + mid_ruleSet->mLen - 1;
    if (ia < mid_minAddr) {
      hi = mid - 1;
      continue;
    }
    if (ia > mid_maxAddr) {
      lo = mid + 1;
      continue;
    }
    MOZ_ASSERT(mid_minAddr <= ia && ia <= mid_maxAddr);
    return mid_ruleSet;
  }
  // NOTREACHED
}

// Add a RuleSet to the collection.  The rule is copied in.  Calling
// this makes the map non-searchable.
void SecMap::AddRuleSet(const RuleSet* rs) {
  mUsable = false;
  mRuleSets.push_back(*rs);
}

// Add a PfxInstr to the vector of such instrs, and return the index
// in the vector.  Calling this makes the map non-searchable.
uint32_t SecMap::AddPfxInstr(PfxInstr pfxi) {
  mUsable = false;
  mPfxInstrs.push_back(pfxi);
  return mPfxInstrs.size() - 1;
}

static bool CmpRuleSetsByAddrLE(const RuleSet& rs1, const RuleSet& rs2) {
  return rs1.mAddr < rs2.mAddr;
}

// Prepare the map for searching.  Completely remove any which don't
// fall inside the specified range [start, +len).
void SecMap::PrepareRuleSets(uintptr_t aStart, size_t aLen) {
  if (mRuleSets.empty()) {
    return;
  }

  MOZ_ASSERT(aLen > 0);
  if (aLen == 0) {
    // This should never happen.
    mRuleSets.clear();
    return;
  }

  // Sort by start addresses.
  std::sort(mRuleSets.begin(), mRuleSets.end(), CmpRuleSetsByAddrLE);

  // Detect any entry not completely contained within [start, +len).
  // Set its length to zero, so that the next pass will remove it.
  for (size_t i = 0; i < mRuleSets.size(); ++i) {
    RuleSet* rs = &mRuleSets[i];
    if (rs->mLen > 0 &&
        (rs->mAddr < aStart || rs->mAddr + rs->mLen > aStart + aLen)) {
      rs->mLen = 0;
    }
  }

  // Iteratively truncate any overlaps and remove any zero length
  // entries that might result, or that may have been present
  // initially.  Unless the input is seriously screwy, this is
  // expected to iterate only once.
  while (true) {
    size_t i;
    size_t n = mRuleSets.size();
    size_t nZeroLen = 0;

    if (n == 0) {
      break;
    }

    for (i = 1; i < n; ++i) {
      RuleSet* prev = &mRuleSets[i - 1];
      RuleSet* here = &mRuleSets[i];
      MOZ_ASSERT(prev->mAddr <= here->mAddr);
      if (prev->mAddr + prev->mLen > here->mAddr) {
        prev->mLen = here->mAddr - prev->mAddr;
      }
      if (prev->mLen == 0) nZeroLen++;
    }

    if (mRuleSets[n - 1].mLen == 0) {
      nZeroLen++;
    }

    // At this point, the entries are in-order and non-overlapping.
    // If none of them are zero-length, we are done.
    if (nZeroLen == 0) {
      break;
    }

    // Slide back the entries to remove the zero length ones.
    size_t j = 0;  // The write-point.
    for (i = 0; i < n; ++i) {
      if (mRuleSets[i].mLen == 0) {
        continue;
      }
      if (j != i) mRuleSets[j] = mRuleSets[i];
      ++j;
    }
    MOZ_ASSERT(i == n);
    MOZ_ASSERT(nZeroLen <= n);
    MOZ_ASSERT(j == n - nZeroLen);
    while (nZeroLen > 0) {
      mRuleSets.pop_back();
      nZeroLen--;
    }

    MOZ_ASSERT(mRuleSets.size() == j);
  }

  size_t n = mRuleSets.size();

#ifdef DEBUG
  // Do a final check on the rules: their address ranges must be
  // ascending, non overlapping, non zero sized.
  if (n > 0) {
    MOZ_ASSERT(mRuleSets[0].mLen > 0);
    for (size_t i = 1; i < n; ++i) {
      RuleSet* prev = &mRuleSets[i - 1];
      RuleSet* here = &mRuleSets[i];
      MOZ_ASSERT(prev->mAddr < here->mAddr);
      MOZ_ASSERT(here->mLen > 0);
      MOZ_ASSERT(prev->mAddr + prev->mLen <= here->mAddr);
    }
  }
#endif

  // Set the summary min and max address values.
  if (n == 0) {
    // Use the values defined in comments in the class declaration.
    mSummaryMinAddr = 1;
    mSummaryMaxAddr = 0;
  } else {
    mSummaryMinAddr = mRuleSets[0].mAddr;
    mSummaryMaxAddr = mRuleSets[n - 1].mAddr + mRuleSets[n - 1].mLen - 1;
  }
  char buf[150];
  SprintfLiteral(buf, "PrepareRuleSets: %d entries, smin/smax 0x%llx, 0x%llx\n",
                 (int)n, (unsigned long long int)mSummaryMinAddr,
                 (unsigned long long int)mSummaryMaxAddr);
  buf[sizeof(buf) - 1] = 0;
  mLog(buf);

  // Is now usable for binary search.
  mUsable = true;

#if 0
  mLog("\nRulesets after preening\n");
  for (size_t i = 0; i < mRuleSets.size(); ++i) {
    mRuleSets[i].Print(mLog);
    mLog("\n");
  }
  mLog("\n");
#endif
}

bool SecMap::IsEmpty() { return mRuleSets.empty(); }

size_t SecMap::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
  size_t n = aMallocSizeOf(this);

  // It's conceivable that these calls would be unsafe with some
  // implementations of std::vector, but it seems to be working for now...
  n += aMallocSizeOf(mRuleSets.data());
  n += aMallocSizeOf(mPfxInstrs.data());

  return n;
}

////////////////////////////////////////////////////////////////
// SegArray                                                   //
////////////////////////////////////////////////////////////////

// A SegArray holds a set of address ranges that together exactly
// cover an address range, with no overlaps or holes.  Each range has
// an associated value, which in this case has been specialised to be
// a simple boolean.  The representation is kept to minimal canonical
// form in which adjacent ranges with the same associated value are
// merged together.  Each range is represented by a |struct Seg|.
//
// SegArrays are used to keep track of which parts of the address
// space are known to contain instructions.
class SegArray {
 public:
  void add(uintptr_t lo, uintptr_t hi, bool val) {
    if (lo > hi) {
      return;
    }
    split_at(lo);
    if (hi < UINTPTR_MAX) {
      split_at(hi + 1);
    }
    std::vector<Seg>::size_type iLo, iHi, i;
    iLo = find(lo);
    iHi = find(hi);
    for (i = iLo; i <= iHi; ++i) {
      mSegs[i].val = val;
    }
    preen();
  }

  // RUNS IN NO-MALLOC CONTEXT
  bool getBoundingCodeSegment(/*OUT*/ uintptr_t* rx_min,
                              /*OUT*/ uintptr_t* rx_max, uintptr_t addr) {
    std::vector<Seg>::size_type i = find(addr);
    if (!mSegs[i].val) {
      return false;
    }
    *rx_min = mSegs[i].lo;
    *rx_max = mSegs[i].hi;
    return true;
  }

  SegArray() {
    Seg s(0, UINTPTR_MAX, false);
    mSegs.push_back(s);
  }

 private:
  struct Seg {
    Seg(uintptr_t lo, uintptr_t hi, bool val) : lo(lo), hi(hi), val(val) {}
    uintptr_t lo;
    uintptr_t hi;
    bool val;
  };

  void preen() {
    for (std::vector<Seg>::iterator iter = mSegs.begin();
         iter < mSegs.end() - 1; ++iter) {
      if (iter[0].val != iter[1].val) {
        continue;
      }
      iter[0].hi = iter[1].hi;
      mSegs.erase(iter + 1);
      // Back up one, so as not to miss an opportunity to merge
      // with the entry after this one.
      --iter;
    }
  }

  // RUNS IN NO-MALLOC CONTEXT
  std::vector<Seg>::size_type find(uintptr_t a) {
    long int lo = 0;
    long int hi = (long int)mSegs.size();
    while (true) {
      // The unsearched space is lo .. hi inclusive.
      if (lo > hi) {
        // Not found.  This can't happen.
        return (std::vector<Seg>::size_type)(-1);
      }
      long int mid = lo + ((hi - lo) / 2);
      uintptr_t mid_lo = mSegs[mid].lo;
      uintptr_t mid_hi = mSegs[mid].hi;
      if (a < mid_lo) {
        hi = mid - 1;
        continue;
      }
      if (a > mid_hi) {
        lo = mid + 1;
        continue;
      }
      return (std::vector<Seg>::size_type)mid;
    }
  }

  void split_at(uintptr_t a) {
    std::vector<Seg>::size_type i = find(a);
    if (mSegs[i].lo == a) {
      return;
    }
    mSegs.insert(mSegs.begin() + i + 1, mSegs[i]);
    mSegs[i].hi = a - 1;
    mSegs[i + 1].lo = a;
  }

  void show() {
    printf("<< %d entries:\n", (int)mSegs.size());
    for (std::vector<Seg>::iterator iter = mSegs.begin(); iter < mSegs.end();
         ++iter) {
      printf("  %016llx  %016llx  %s\n", (unsigned long long int)(*iter).lo,
             (unsigned long long int)(*iter).hi,
             (*iter).val ? "true" : "false");
    }
    printf(">>\n");
  }

  std::vector<Seg> mSegs;
};

////////////////////////////////////////////////////////////////
// PriMap                                                     //
////////////////////////////////////////////////////////////////

class PriMap {
 public:
  explicit PriMap(void (*aLog)(const char*)) : mLog(aLog) {}

  // RUNS IN NO-MALLOC CONTEXT
  pair<const RuleSet*, const vector<PfxInstr>*> Lookup(uintptr_t ia) {
    SecMap* sm = FindSecMap(ia);
    return pair<const RuleSet*, const vector<PfxInstr>*>(
        sm ? sm->FindRuleSet(ia) : nullptr, sm ? sm->GetPfxInstrs() : nullptr);
  }

  // Add a secondary map.  No overlaps allowed w.r.t. existing
  // secondary maps.
  void AddSecMap(mozilla::UniquePtr<SecMap>&& aSecMap) {
    // We can't add an empty SecMap to the PriMap.  But that's OK
    // since we'd never be able to find anything in it anyway.
    if (aSecMap->IsEmpty()) {
      return;
    }

    // Iterate through the SecMaps and find the right place for this
    // one.  At the same time, ensure that the in-order
    // non-overlapping invariant is preserved (and, generally, holds).
    // FIXME: this gives a cost that is O(N^2) in the total number of
    // shared objects in the system.  ToDo: better.
    MOZ_ASSERT(aSecMap->mSummaryMinAddr <= aSecMap->mSummaryMaxAddr);

    size_t num_secMaps = mSecMaps.size();
    uintptr_t i;
    for (i = 0; i < num_secMaps; ++i) {
      mozilla::UniquePtr<SecMap>& sm_i = mSecMaps[i];
      MOZ_ASSERT(sm_i->mSummaryMinAddr <= sm_i->mSummaryMaxAddr);
      if (aSecMap->mSummaryMinAddr < sm_i->mSummaryMaxAddr) {
        // |aSecMap| needs to be inserted immediately before mSecMaps[i].
        break;
      }
    }
    MOZ_ASSERT(i <= num_secMaps);
    if (i == num_secMaps) {
      // It goes at the end.
      mSecMaps.push_back(std::move(aSecMap));
    } else {
      std::vector<mozilla::UniquePtr<SecMap>>::iterator iter =
          mSecMaps.begin() + i;
      mSecMaps.insert(iter, std::move(aSecMap));
    }
    char buf[100];
    SprintfLiteral(buf, "AddSecMap: now have %d SecMaps\n",
                   (int)mSecMaps.size());
    buf[sizeof(buf) - 1] = 0;
    mLog(buf);
  }

  // Remove and delete any SecMaps in the mapping, that intersect
  // with the specified address range.
  void RemoveSecMapsInRange(uintptr_t avma_min, uintptr_t avma_max) {
    MOZ_ASSERT(avma_min <= avma_max);
    size_t num_secMaps = mSecMaps.size();
    if (num_secMaps > 0) {
      intptr_t i;
      // Iterate from end to start over the vector, so as to ensure
      // that the special case where |avma_min| and |avma_max| denote
      // the entire address space, can be completed in time proportional
      // to the number of elements in the map.
      for (i = (intptr_t)num_secMaps - 1; i >= 0; i--) {
        mozilla::UniquePtr<SecMap>& sm_i = mSecMaps[i];
        if (sm_i->mSummaryMaxAddr < avma_min ||
            avma_max < sm_i->mSummaryMinAddr) {
          // There's no overlap.  Move on.
          continue;
        }
        // We need to remove mSecMaps[i] and slide all those above it
        // downwards to cover the hole.
        mSecMaps.erase(mSecMaps.begin() + i);
      }
    }
  }

  // Return the number of currently contained SecMaps.
  size_t CountSecMaps() { return mSecMaps.size(); }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
    size_t n = aMallocSizeOf(this);

    // It's conceivable that this call would be unsafe with some
    // implementations of std::vector, but it seems to be working for now...
    n += aMallocSizeOf(mSecMaps.data());

    for (size_t i = 0; i < mSecMaps.size(); i++) {
      n += mSecMaps[i]->SizeOfIncludingThis(aMallocSizeOf);
    }

    return n;
  }

 private:
  // RUNS IN NO-MALLOC CONTEXT
  SecMap* FindSecMap(uintptr_t ia) {
    // Binary search mSecMaps to find one that brackets |ia|.
    // lo and hi need to be signed, else the loop termination tests
    // don't work properly.
    long int lo = 0;
    long int hi = (long int)mSecMaps.size() - 1;
    while (true) {
      // current unsearched space is from lo to hi, inclusive.
      if (lo > hi) {
        // not found
        return nullptr;
      }
      long int mid = lo + ((hi - lo) / 2);
      mozilla::UniquePtr<SecMap>& mid_secMap = mSecMaps[mid];
      uintptr_t mid_minAddr = mid_secMap->mSummaryMinAddr;
      uintptr_t mid_maxAddr = mid_secMap->mSummaryMaxAddr;
      if (ia < mid_minAddr) {
        hi = mid - 1;
        continue;
      }
      if (ia > mid_maxAddr) {
        lo = mid + 1;
        continue;
      }
      MOZ_ASSERT(mid_minAddr <= ia && ia <= mid_maxAddr);
      return mid_secMap.get();
    }
    // NOTREACHED
  }

 private:
  // sorted array of per-object ranges, non overlapping, non empty
  std::vector<mozilla::UniquePtr<SecMap>> mSecMaps;

  // a logging sink, for debugging.
  void (*mLog)(const char*);
};

////////////////////////////////////////////////////////////////
// LUL                                                        //
////////////////////////////////////////////////////////////////

#define LUL_LOG(_str)                                           \
  do {                                                          \
    char buf[200];                                              \
    SprintfLiteral(buf, "LUL: pid %d tid %d lul-obj %p: %s",    \
                   profiler_current_process_id(),               \
                   profiler_current_thread_id(), this, (_str)); \
    buf[sizeof(buf) - 1] = 0;                                   \
    mLog(buf);                                                  \
  } while (0)

LUL::LUL(void (*aLog)(const char*))
    : mLog(aLog),
      mAdminMode(true),
      mAdminThreadId(profiler_current_thread_id()),
      mPriMap(new PriMap(aLog)),
      mSegArray(new SegArray()),
      mUSU(new UniqueStringUniverse()) {
  LUL_LOG("LUL::LUL: Created object");
}

LUL::~LUL() {
  LUL_LOG("LUL::~LUL: Destroyed object");
  delete mPriMap;
  delete mSegArray;
  mLog = nullptr;
  delete mUSU;
}

void LUL::MaybeShowStats() {
  // This is racey in the sense that it can't guarantee that
  //   n_new == n_new_Context + n_new_CFI + n_new_Scanned
  // if it should happen that mStats is updated by some other thread
  // in between computation of n_new and n_new_{Context,CFI,FP}.
  // But it's just stats printing, so we don't really care.
  uint32_t n_new = mStats - mStatsPrevious;
  if (n_new >= 5000) {
    uint32_t n_new_Context = mStats.mContext - mStatsPrevious.mContext;
    uint32_t n_new_CFI = mStats.mCFI - mStatsPrevious.mCFI;
    uint32_t n_new_FP = mStats.mFP - mStatsPrevious.mFP;
    mStatsPrevious = mStats;
    char buf[200];
    SprintfLiteral(buf,
                   "LUL frame stats: TOTAL %5u"
                   "    CTX %4u    CFI %4u    FP %4u",
                   n_new, n_new_Context, n_new_CFI, n_new_FP);
    buf[sizeof(buf) - 1] = 0;
    mLog(buf);
  }
}

size_t LUL::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
  size_t n = aMallocSizeOf(this);
  n += mPriMap->SizeOfIncludingThis(aMallocSizeOf);

  // Measurement of the following members may be added later if DMD finds it
  // is worthwhile:
  // - mSegArray
  // - mUSU

  return n;
}

void LUL::EnableUnwinding() {
  LUL_LOG("LUL::EnableUnwinding");
  // Don't assert for Admin mode here.  That is, tolerate a call here
  // if we are already in Unwinding mode.
  MOZ_RELEASE_ASSERT(profiler_current_thread_id() == mAdminThreadId);

  mAdminMode = false;
}

void LUL::NotifyAfterMap(uintptr_t aRXavma, size_t aSize, const char* aFileName,
                         const void* aMappedImage) {
  MOZ_RELEASE_ASSERT(mAdminMode);
  MOZ_RELEASE_ASSERT(profiler_current_thread_id() == mAdminThreadId);

  mLog(":\n");
  char buf[200];
  SprintfLiteral(buf, "NotifyMap %llx %llu %s\n",
                 (unsigned long long int)aRXavma, (unsigned long long int)aSize,
                 aFileName);
  buf[sizeof(buf) - 1] = 0;
  mLog(buf);

  // Ignore obviously-stupid notifications.
  if (aSize > 0) {
    // Here's a new mapping, for this object.
    mozilla::UniquePtr<SecMap> smap = mozilla::MakeUnique<SecMap>(mLog);

    // Read CFI or EXIDX unwind data into |smap|.
    if (!aMappedImage) {
      (void)lul::ReadSymbolData(string(aFileName), std::vector<string>(),
                                smap.get(), (void*)aRXavma, aSize, mUSU, mLog);
    } else {
      (void)lul::ReadSymbolDataInternal(
          (const uint8_t*)aMappedImage, string(aFileName),
          std::vector<string>(), smap.get(), (void*)aRXavma, aSize, mUSU, mLog);
    }

    mLog("NotifyMap .. preparing entries\n");

    smap->PrepareRuleSets(aRXavma, aSize);

    SprintfLiteral(buf, "NotifyMap got %lld entries\n",
                   (long long int)smap->Size());
    buf[sizeof(buf) - 1] = 0;
    mLog(buf);

    // Add it to the primary map (the top level set of mapped objects).
    mPriMap->AddSecMap(std::move(smap));

    // Tell the segment array about the mapping, so that the stack
    // scan and __kernel_syscall mechanisms know where valid code is.
    mSegArray->add(aRXavma, aRXavma + aSize - 1, true);
  }
}

void LUL::NotifyExecutableArea(uintptr_t aRXavma, size_t aSize) {
  MOZ_RELEASE_ASSERT(mAdminMode);
  MOZ_RELEASE_ASSERT(profiler_current_thread_id() == mAdminThreadId);

  mLog(":\n");
  char buf[200];
  SprintfLiteral(buf, "NotifyExecutableArea %llx %llu\n",
                 (unsigned long long int)aRXavma,
                 (unsigned long long int)aSize);
  buf[sizeof(buf) - 1] = 0;
  mLog(buf);

  // Ignore obviously-stupid notifications.
  if (aSize > 0) {
    // Tell the segment array about the mapping, so that the stack
    // scan and __kernel_syscall mechanisms know where valid code is.
    mSegArray->add(aRXavma, aRXavma + aSize - 1, true);
  }
}

void LUL::NotifyBeforeUnmap(uintptr_t aRXavmaMin, uintptr_t aRXavmaMax) {
  MOZ_RELEASE_ASSERT(mAdminMode);
  MOZ_RELEASE_ASSERT(profiler_current_thread_id() == mAdminThreadId);

  mLog(":\n");
  char buf[100];
  SprintfLiteral(buf, "NotifyUnmap %016llx-%016llx\n",
                 (unsigned long long int)aRXavmaMin,
                 (unsigned long long int)aRXavmaMax);
  buf[sizeof(buf) - 1] = 0;
  mLog(buf);

  MOZ_ASSERT(aRXavmaMin <= aRXavmaMax);

  // Remove from the primary map, any secondary maps that intersect
  // with the address range.  Also delete the secondary maps.
  mPriMap->RemoveSecMapsInRange(aRXavmaMin, aRXavmaMax);

  // Tell the segment array that the address range no longer
  // contains valid code.
  mSegArray->add(aRXavmaMin, aRXavmaMax, false);

  SprintfLiteral(buf, "NotifyUnmap: now have %d SecMaps\n",
                 (int)mPriMap->CountSecMaps());
  buf[sizeof(buf) - 1] = 0;
  mLog(buf);
}

size_t LUL::CountMappings() {
  MOZ_RELEASE_ASSERT(mAdminMode);
  MOZ_RELEASE_ASSERT(profiler_current_thread_id() == mAdminThreadId);

  return mPriMap->CountSecMaps();
}

// RUNS IN NO-MALLOC CONTEXT
static TaggedUWord DerefTUW(TaggedUWord aAddr, const StackImage* aStackImg) {
  if (!aAddr.Valid()) {
    return TaggedUWord();
  }

  // Lower limit check.  |aAddr.Value()| is the lowest requested address
  // and |aStackImg->mStartAvma| is the lowest address we actually have,
  // so the comparison is straightforward.
  if (aAddr.Value() < aStackImg->mStartAvma) {
    return TaggedUWord();
  }

  // Upper limit check.  We must compute the highest requested address
  // and the highest address we actually have, but being careful to
  // avoid overflow.  In particular if |aAddr| is 0xFFF...FFF or the
  // 3/7 values below that, then we will get overflow.  See bug #1245477.
  typedef CheckedInt<uintptr_t> CheckedUWord;
  CheckedUWord highest_requested_plus_one =
      CheckedUWord(aAddr.Value()) + CheckedUWord(sizeof(uintptr_t));
  CheckedUWord highest_available_plus_one =
      CheckedUWord(aStackImg->mStartAvma) + CheckedUWord(aStackImg->mLen);
  if (!highest_requested_plus_one.isValid()     // overflow?
      || !highest_available_plus_one.isValid()  // overflow?
      || (highest_requested_plus_one.value() >
          highest_available_plus_one.value())) {  // in range?
    return TaggedUWord();
  }

  return TaggedUWord(
      *(uintptr_t*)(&aStackImg
                         ->mContents[aAddr.Value() - aStackImg->mStartAvma]));
}

// RUNS IN NO-MALLOC CONTEXT
static TaggedUWord EvaluateReg(int16_t aReg, const UnwindRegs* aOldRegs,
                               TaggedUWord aCFA) {
  switch (aReg) {
    case DW_REG_CFA:
      return aCFA;
#if defined(GP_ARCH_amd64) || defined(GP_ARCH_x86)
    case DW_REG_INTEL_XBP:
      return aOldRegs->xbp;
    case DW_REG_INTEL_XSP:
      return aOldRegs->xsp;
    case DW_REG_INTEL_XIP:
      return aOldRegs->xip;
#elif defined(GP_ARCH_arm)
    case DW_REG_ARM_R7:
      return aOldRegs->r7;
    case DW_REG_ARM_R11:
      return aOldRegs->r11;
    case DW_REG_ARM_R12:
      return aOldRegs->r12;
    case DW_REG_ARM_R13:
      return aOldRegs->r13;
    case DW_REG_ARM_R14:
      return aOldRegs->r14;
    case DW_REG_ARM_R15:
      return aOldRegs->r15;
#elif defined(GP_ARCH_arm64)
    case DW_REG_AARCH64_X29:
      return aOldRegs->x29;
    case DW_REG_AARCH64_X30:
      return aOldRegs->x30;
    case DW_REG_AARCH64_SP:
      return aOldRegs->sp;
#elif defined(GP_ARCH_mips64)
    case DW_REG_MIPS_SP:
      return aOldRegs->sp;
    case DW_REG_MIPS_FP:
      return aOldRegs->fp;
    case DW_REG_MIPS_PC:
      return aOldRegs->pc;
#else
#  error "Unsupported arch"
#endif
    default:
      MOZ_ASSERT(0);
      return TaggedUWord();
  }
}

// RUNS IN NO-MALLOC CONTEXT
// See prototype for comment.
TaggedUWord EvaluatePfxExpr(int32_t start, const UnwindRegs* aOldRegs,
                            TaggedUWord aCFA, const StackImage* aStackImg,
                            const vector<PfxInstr>& aPfxInstrs) {
  // A small evaluation stack, and a stack pointer, which points to
  // the highest numbered in-use element.
  const int N_STACK = 10;
  TaggedUWord stack[N_STACK];
  int stackPointer = -1;
  for (int i = 0; i < N_STACK; i++) stack[i] = TaggedUWord();

#define PUSH(_tuw)                                             \
  do {                                                         \
    if (stackPointer >= N_STACK - 1) goto fail; /* overflow */ \
    stack[++stackPointer] = (_tuw);                            \
  } while (0)

#define POP(_lval)                                   \
  do {                                               \
    if (stackPointer < 0) goto fail; /* underflow */ \
    _lval = stack[stackPointer--];                   \
  } while (0)

  // Cursor in the instruction sequence.
  size_t curr = start + 1;

  // Check the start point is sane.
  size_t nInstrs = aPfxInstrs.size();
  if (start < 0 || (size_t)start >= nInstrs) goto fail;

  {
    // The instruction sequence must start with PX_Start.  If not,
    // something is seriously wrong.
    PfxInstr first = aPfxInstrs[start];
    if (first.mOpcode != PX_Start) goto fail;

    // Push the CFA on the stack to start with (or not), as required by
    // the original DW_OP_*expression* CFI.
    if (first.mOperand != 0) PUSH(aCFA);
  }

  while (true) {
    if (curr >= nInstrs) goto fail;  // ran off the end of the sequence

    PfxInstr pfxi = aPfxInstrs[curr++];
    if (pfxi.mOpcode == PX_End) break;  // we're done

    switch (pfxi.mOpcode) {
      case PX_Start:
        // This should appear only at the start of the sequence.
        goto fail;
      case PX_End:
        // We just took care of that, so we shouldn't see it again.
        MOZ_ASSERT(0);
        goto fail;
      case PX_SImm32:
        PUSH(TaggedUWord((intptr_t)pfxi.mOperand));
        break;
      case PX_DwReg: {
        DW_REG_NUMBER reg = (DW_REG_NUMBER)pfxi.mOperand;
        MOZ_ASSERT(reg != DW_REG_CFA);
        PUSH(EvaluateReg(reg, aOldRegs, aCFA));
        break;
      }
      case PX_Deref: {
        TaggedUWord addr;
        POP(addr);
        PUSH(DerefTUW(addr, aStackImg));
        break;
      }
      case PX_Add: {
        TaggedUWord x, y;
        POP(x);
        POP(y);
        PUSH(y + x);
        break;
      }
      case PX_Sub: {
        TaggedUWord x, y;
        POP(x);
        POP(y);
        PUSH(y - x);
        break;
      }
      case PX_And: {
        TaggedUWord x, y;
        POP(x);
        POP(y);
        PUSH(y & x);
        break;
      }
      case PX_Or: {
        TaggedUWord x, y;
        POP(x);
        POP(y);
        PUSH(y | x);
        break;
      }
      case PX_CmpGES: {
        TaggedUWord x, y;
        POP(x);
        POP(y);
        PUSH(y.CmpGEs(x));
        break;
      }
      case PX_Shl: {
        TaggedUWord x, y;
        POP(x);
        POP(y);
        PUSH(y << x);
        break;
      }
      default:
        MOZ_ASSERT(0);
        goto fail;
    }
  }  // while (true)

  // Evaluation finished.  The top value on the stack is the result.
  if (stackPointer >= 0) {
    return stack[stackPointer];
  }
  // Else fall through

fail:
  return TaggedUWord();

#undef PUSH
#undef POP
}

// RUNS IN NO-MALLOC CONTEXT
TaggedUWord LExpr::EvaluateExpr(const UnwindRegs* aOldRegs, TaggedUWord aCFA,
                                const StackImage* aStackImg,
                                const vector<PfxInstr>* aPfxInstrs) const {
  switch (mHow) {
    case UNKNOWN:
      return TaggedUWord();
    case NODEREF: {
      TaggedUWord tuw = EvaluateReg(mReg, aOldRegs, aCFA);
      tuw = tuw + TaggedUWord((intptr_t)mOffset);
      return tuw;
    }
    case DEREF: {
      TaggedUWord tuw = EvaluateReg(mReg, aOldRegs, aCFA);
      tuw = tuw + TaggedUWord((intptr_t)mOffset);
      return DerefTUW(tuw, aStackImg);
    }
    case PFXEXPR: {
      MOZ_ASSERT(aPfxInstrs);
      if (!aPfxInstrs) {
        return TaggedUWord();
      }
      return EvaluatePfxExpr(mOffset, aOldRegs, aCFA, aStackImg, *aPfxInstrs);
    }
    default:
      MOZ_ASSERT(0);
      return TaggedUWord();
  }
}

// RUNS IN NO-MALLOC CONTEXT
static void UseRuleSet(/*MOD*/ UnwindRegs* aRegs, const StackImage* aStackImg,
                       const RuleSet* aRS, const vector<PfxInstr>* aPfxInstrs) {
  // Take a copy of regs, since we'll need to refer to the old values
  // whilst computing the new ones.
  UnwindRegs old_regs = *aRegs;

  // Mark all the current register values as invalid, so that the
  // caller can see, on our return, which ones have been computed
  // anew.  If we don't even manage to compute a new PC value, then
  // the caller will have to abandon the unwind.
  // FIXME: Create and use instead: aRegs->SetAllInvalid();
#if defined(GP_ARCH_amd64) || defined(GP_ARCH_x86)
  aRegs->xbp = TaggedUWord();
  aRegs->xsp = TaggedUWord();
  aRegs->xip = TaggedUWord();
#elif defined(GP_ARCH_arm)
  aRegs->r7 = TaggedUWord();
  aRegs->r11 = TaggedUWord();
  aRegs->r12 = TaggedUWord();
  aRegs->r13 = TaggedUWord();
  aRegs->r14 = TaggedUWord();
  aRegs->r15 = TaggedUWord();
#elif defined(GP_ARCH_arm64)
  aRegs->x29 = TaggedUWord();
  aRegs->x30 = TaggedUWord();
  aRegs->sp = TaggedUWord();
  aRegs->pc = TaggedUWord();
#elif defined(GP_ARCH_mips64)
  aRegs->sp = TaggedUWord();
  aRegs->fp = TaggedUWord();
  aRegs->pc = TaggedUWord();
#else
#  error "Unsupported arch"
#endif

  // This is generally useful.
  const TaggedUWord inval = TaggedUWord();

  // First, compute the CFA.
  TaggedUWord cfa = aRS->mCfaExpr.EvaluateExpr(&old_regs, inval /*old cfa*/,
                                               aStackImg, aPfxInstrs);

  // If we didn't manage to compute the CFA, well .. that's ungood,
  // but keep going anyway.  It'll be OK provided none of the register
  // value rules mention the CFA.  In any case, compute the new values
  // for each register that we're tracking.

#if defined(GP_ARCH_amd64) || defined(GP_ARCH_x86)
  aRegs->xbp =
      aRS->mXbpExpr.EvaluateExpr(&old_regs, cfa, aStackImg, aPfxInstrs);
  aRegs->xsp =
      aRS->mXspExpr.EvaluateExpr(&old_regs, cfa, aStackImg, aPfxInstrs);
  aRegs->xip =
      aRS->mXipExpr.EvaluateExpr(&old_regs, cfa, aStackImg, aPfxInstrs);
#elif defined(GP_ARCH_arm)
  aRegs->r7 = aRS->mR7expr.EvaluateExpr(&old_regs, cfa, aStackImg, aPfxInstrs);
  aRegs->r11 =
      aRS->mR11expr.EvaluateExpr(&old_regs, cfa, aStackImg, aPfxInstrs);
  aRegs->r12 =
      aRS->mR12expr.EvaluateExpr(&old_regs, cfa, aStackImg, aPfxInstrs);
  aRegs->r13 =
      aRS->mR13expr.EvaluateExpr(&old_regs, cfa, aStackImg, aPfxInstrs);
  aRegs->r14 =
      aRS->mR14expr.EvaluateExpr(&old_regs, cfa, aStackImg, aPfxInstrs);
  aRegs->r15 =
      aRS->mR15expr.EvaluateExpr(&old_regs, cfa, aStackImg, aPfxInstrs);
#elif defined(GP_ARCH_arm64)
  aRegs->x29 =
      aRS->mX29expr.EvaluateExpr(&old_regs, cfa, aStackImg, aPfxInstrs);
  aRegs->x30 =
      aRS->mX30expr.EvaluateExpr(&old_regs, cfa, aStackImg, aPfxInstrs);
  aRegs->sp = aRS->mSPexpr.EvaluateExpr(&old_regs, cfa, aStackImg, aPfxInstrs);
#elif defined(GP_ARCH_mips64)
  aRegs->sp = aRS->mSPexpr.EvaluateExpr(&old_regs, cfa, aStackImg, aPfxInstrs);
  aRegs->fp = aRS->mFPexpr.EvaluateExpr(&old_regs, cfa, aStackImg, aPfxInstrs);
  aRegs->pc = aRS->mPCexpr.EvaluateExpr(&old_regs, cfa, aStackImg, aPfxInstrs);
#else
#  error "Unsupported arch"
#endif

  // We're done.  Any regs for which we didn't manage to compute a
  // new value will now be marked as invalid.
}

// RUNS IN NO-MALLOC CONTEXT
void LUL::Unwind(/*OUT*/ uintptr_t* aFramePCs,
                 /*OUT*/ uintptr_t* aFrameSPs,
                 /*OUT*/ size_t* aFramesUsed,
                 /*OUT*/ size_t* aFramePointerFramesAcquired,
                 size_t aFramesAvail, UnwindRegs* aStartRegs,
                 StackImage* aStackImg) {
  MOZ_RELEASE_ASSERT(!mAdminMode);

  /////////////////////////////////////////////////////////
  // BEGIN UNWIND

  *aFramesUsed = 0;

  UnwindRegs regs = *aStartRegs;
  TaggedUWord last_valid_sp = TaggedUWord();

  while (true) {
    if (DEBUG_MAIN) {
      char buf[300];
      mLog("\n");
#if defined(GP_ARCH_amd64) || defined(GP_ARCH_x86)
      SprintfLiteral(
          buf, "LoopTop: rip %d/%llx  rsp %d/%llx  rbp %d/%llx\n",
          (int)regs.xip.Valid(), (unsigned long long int)regs.xip.Value(),
          (int)regs.xsp.Valid(), (unsigned long long int)regs.xsp.Value(),
          (int)regs.xbp.Valid(), (unsigned long long int)regs.xbp.Value());
      buf[sizeof(buf) - 1] = 0;
      mLog(buf);
#elif defined(GP_ARCH_arm)
      SprintfLiteral(
          buf,
          "LoopTop: r15 %d/%llx  r7 %d/%llx  r11 %d/%llx"
          "  r12 %d/%llx  r13 %d/%llx  r14 %d/%llx\n",
          (int)regs.r15.Valid(), (unsigned long long int)regs.r15.Value(),
          (int)regs.r7.Valid(), (unsigned long long int)regs.r7.Value(),
          (int)regs.r11.Valid(), (unsigned long long int)regs.r11.Value(),
          (int)regs.r12.Valid(), (unsigned long long int)regs.r12.Value(),
          (int)regs.r13.Valid(), (unsigned long long int)regs.r13.Value(),
          (int)regs.r14.Valid(), (unsigned long long int)regs.r14.Value());
      buf[sizeof(buf) - 1] = 0;
      mLog(buf);
#elif defined(GP_ARCH_arm64)
      SprintfLiteral(
          buf,
          "LoopTop: pc %d/%llx  x29 %d/%llx  x30 %d/%llx"
          "  sp %d/%llx\n",
          (int)regs.pc.Valid(), (unsigned long long int)regs.pc.Value(),
          (int)regs.x29.Valid(), (unsigned long long int)regs.x29.Value(),
          (int)regs.x30.Valid(), (unsigned long long int)regs.x30.Value(),
          (int)regs.sp.Valid(), (unsigned long long int)regs.sp.Value());
      buf[sizeof(buf) - 1] = 0;
      mLog(buf);
#elif defined(GP_ARCH_mips64)
      SprintfLiteral(
          buf, "LoopTop: pc %d/%llx  sp %d/%llx  fp %d/%llx\n",
          (int)regs.pc.Valid(), (unsigned long long int)regs.pc.Value(),
          (int)regs.sp.Valid(), (unsigned long long int)regs.sp.Value(),
          (int)regs.fp.Valid(), (unsigned long long int)regs.fp.Value());
      buf[sizeof(buf) - 1] = 0;
      mLog(buf);
#else
#  error "Unsupported arch"
#endif
    }

#if defined(GP_ARCH_amd64) || defined(GP_ARCH_x86)
    TaggedUWord ia = regs.xip;
    TaggedUWord sp = regs.xsp;
#elif defined(GP_ARCH_arm)
    TaggedUWord ia = (*aFramesUsed == 0 ? regs.r15 : regs.r14);
    TaggedUWord sp = regs.r13;
#elif defined(GP_ARCH_arm64)
    TaggedUWord ia = (*aFramesUsed == 0 ? regs.pc : regs.x30);
    TaggedUWord sp = regs.sp;
#elif defined(GP_ARCH_mips64)
    TaggedUWord ia = regs.pc;
    TaggedUWord sp = regs.sp;
#else
#  error "Unsupported arch"
#endif

    if (*aFramesUsed >= aFramesAvail) {
      break;
    }

    // If we don't have a valid value for the PC, give up.
    if (!ia.Valid()) {
      break;
    }

    // If this is the innermost frame, record the SP value, which
    // presumably is valid.  If this isn't the innermost frame, and we
    // have a valid SP value, check that its SP value isn't less that
    // the one we've seen so far, so as to catch potential SP value
    // cycles.
    if (*aFramesUsed == 0) {
      last_valid_sp = sp;
    } else {
      MOZ_ASSERT(last_valid_sp.Valid());
      if (sp.Valid()) {
        if (sp.Value() < last_valid_sp.Value()) {
          // Hmm, SP going in the wrong direction.  Let's stop.
          break;
        }
        // Remember where we got to.
        last_valid_sp = sp;
      }
    }

    // For the innermost frame, the IA value is what we need.  For all
    // other frames, it's actually the return address, so back up one
    // byte so as to get it into the calling instruction.
    aFramePCs[*aFramesUsed] = ia.Value() - (*aFramesUsed == 0 ? 0 : 1);
    aFrameSPs[*aFramesUsed] = sp.Valid() ? sp.Value() : 0;
    (*aFramesUsed)++;

    // Find the RuleSet for the current IA, if any.  This will also
    // query the backing (secondary) maps if it isn't found in the
    // thread-local cache.

    // If this isn't the innermost frame, back up into the calling insn.
    if (*aFramesUsed > 1) {
      ia = ia + TaggedUWord((uintptr_t)(-1));
    }

    pair<const RuleSet*, const vector<PfxInstr>*> ruleset_and_pfxinstrs =
        mPriMap->Lookup(ia.Value());
    const RuleSet* ruleset = ruleset_and_pfxinstrs.first;
    const vector<PfxInstr>* pfxinstrs = ruleset_and_pfxinstrs.second;

    if (DEBUG_MAIN) {
      char buf[100];
      SprintfLiteral(buf, "ruleset for 0x%llx = %p\n",
                     (unsigned long long int)ia.Value(), ruleset);
      buf[sizeof(buf) - 1] = 0;
      mLog(buf);
    }

#if defined(GP_PLAT_x86_android) || defined(GP_PLAT_x86_linux)
    /////////////////////////////////////////////
    ////
    // On 32 bit x86-linux, syscalls are often done via the VDSO
    // function __kernel_vsyscall, which doesn't have a corresponding
    // object that we can read debuginfo from.  That effectively kills
    // off all stack traces for threads blocked in syscalls.  Hence
    // special-case by looking at the code surrounding the program
    // counter.
    //
    // 0xf7757420 <__kernel_vsyscall+0>:	push   %ecx
    // 0xf7757421 <__kernel_vsyscall+1>:	push   %edx
    // 0xf7757422 <__kernel_vsyscall+2>:	push   %ebp
    // 0xf7757423 <__kernel_vsyscall+3>:	mov    %esp,%ebp
    // 0xf7757425 <__kernel_vsyscall+5>:	sysenter
    // 0xf7757427 <__kernel_vsyscall+7>:	nop
    // 0xf7757428 <__kernel_vsyscall+8>:	nop
    // 0xf7757429 <__kernel_vsyscall+9>:	nop
    // 0xf775742a <__kernel_vsyscall+10>:	nop
    // 0xf775742b <__kernel_vsyscall+11>:	nop
    // 0xf775742c <__kernel_vsyscall+12>:	nop
    // 0xf775742d <__kernel_vsyscall+13>:	nop
    // 0xf775742e <__kernel_vsyscall+14>:	int    $0x80
    // 0xf7757430 <__kernel_vsyscall+16>:	pop    %ebp
    // 0xf7757431 <__kernel_vsyscall+17>:	pop    %edx
    // 0xf7757432 <__kernel_vsyscall+18>:	pop    %ecx
    // 0xf7757433 <__kernel_vsyscall+19>:	ret
    //
    // In cases where the sampled thread is blocked in a syscall, its
    // program counter will point at "pop %ebp".  Hence we look for
    // the sequence "int $0x80; pop %ebp; pop %edx; pop %ecx; ret", and
    // the corresponding register-recovery actions are:
    //    new_ebp = *(old_esp + 0)
    //    new eip = *(old_esp + 12)
    //    new_esp = old_esp + 16
    //
    // It may also be the case that the program counter points two
    // nops before the "int $0x80", viz, is __kernel_vsyscall+12, in
    // the case where the syscall has been restarted but the thread
    // hasn't been rescheduled.  The code below doesn't handle that;
    // it could easily be made to.
    //
    if (!ruleset && *aFramesUsed == 1 && ia.Valid() && sp.Valid()) {
      uintptr_t insns_min, insns_max;
      uintptr_t eip = ia.Value();
      bool b = mSegArray->getBoundingCodeSegment(&insns_min, &insns_max, eip);
      if (b && eip - 2 >= insns_min && eip + 3 <= insns_max) {
        uint8_t* eipC = (uint8_t*)eip;
        if (eipC[-2] == 0xCD && eipC[-1] == 0x80 && eipC[0] == 0x5D &&
            eipC[1] == 0x5A && eipC[2] == 0x59 && eipC[3] == 0xC3) {
          TaggedUWord sp_plus_0 = sp;
          TaggedUWord sp_plus_12 = sp;
          TaggedUWord sp_plus_16 = sp;
          sp_plus_12 = sp_plus_12 + TaggedUWord(12);
          sp_plus_16 = sp_plus_16 + TaggedUWord(16);
          TaggedUWord new_ebp = DerefTUW(sp_plus_0, aStackImg);
          TaggedUWord new_eip = DerefTUW(sp_plus_12, aStackImg);
          TaggedUWord new_esp = sp_plus_16;
          if (new_ebp.Valid() && new_eip.Valid() && new_esp.Valid()) {
            regs.xbp = new_ebp;
            regs.xip = new_eip;
            regs.xsp = new_esp;
            continue;
          }
        }
      }
    }
    ////
    /////////////////////////////////////////////
#endif  // defined(GP_PLAT_x86_android) || defined(GP_PLAT_x86_linux)

    // So, do we have a ruleset for this address?  If so, use it now.
    if (ruleset) {
      if (DEBUG_MAIN) {
        ruleset->Print(mLog);
        mLog("\n");
      }
      // Use the RuleSet to compute the registers for the previous
      // frame.  |regs| is modified in-place.
      UseRuleSet(&regs, aStackImg, ruleset, pfxinstrs);
      continue;
    }

#if defined(GP_PLAT_amd64_linux) || defined(GP_PLAT_x86_linux) || \
    defined(GP_PLAT_amd64_android) || defined(GP_PLAT_x86_android)
    // There's no RuleSet for the specified address.  On amd64/x86_linux, see if
    // it's possible to recover the caller's frame by using the frame pointer.

    // We seek to compute (new_IP, new_SP, new_BP) from (old_BP, stack image),
    // and assume the following layout:
    //
    //                 <--- new_SP
    //   +----------+
    //   |  new_IP  |  (return address)
    //   +----------+
    //   |  new_BP  |  <--- old_BP
    //   +----------+
    //   |   ....   |
    //   |   ....   |
    //   |   ....   |
    //   +----------+  <---- old_SP (arbitrary, but must be <= old_BP)

    const size_t wordSzB = sizeof(uintptr_t);
    TaggedUWord old_xsp = regs.xsp;

    // points at new_BP ?
    TaggedUWord old_xbp = regs.xbp;
    // points at new_IP ?
    TaggedUWord old_xbp_plus1 = regs.xbp + TaggedUWord(1 * wordSzB);
    // is the new_SP ?
    TaggedUWord old_xbp_plus2 = regs.xbp + TaggedUWord(2 * wordSzB);

    if (old_xbp.Valid() && old_xbp.IsAligned() && old_xsp.Valid() &&
        old_xsp.IsAligned() && old_xsp.Value() <= old_xbp.Value()) {
      // We don't need to do any range, alignment or validity checks for
      // addresses passed to DerefTUW, since that performs them itself, and
      // returns an invalid value on failure.  Any such value will poison
      // subsequent uses, and we do a final check for validity before putting
      // the computed values into |regs|.
      TaggedUWord new_xbp = DerefTUW(old_xbp, aStackImg);
      if (new_xbp.Valid() && new_xbp.IsAligned() &&
          old_xbp.Value() < new_xbp.Value()) {
        TaggedUWord new_xip = DerefTUW(old_xbp_plus1, aStackImg);
        TaggedUWord new_xsp = old_xbp_plus2;
        if (new_xbp.Valid() && new_xip.Valid() && new_xsp.Valid()) {
          regs.xbp = new_xbp;
          regs.xip = new_xip;
          regs.xsp = new_xsp;
          (*aFramePointerFramesAcquired)++;
          continue;
        }
      }
    }
#elif defined(GP_ARCH_arm64)
    // Here is an example of generated code for prologue and epilogue..
    //
    // stp     x29, x30, [sp, #-16]!
    // mov     x29, sp
    // ...
    // ldp     x29, x30, [sp], #16
    // ret
    //
    // Next is another example of generated code.
    //
    // stp     x20, x19, [sp, #-32]!
    // stp     x29, x30, [sp, #16]
    // add     x29, sp, #0x10
    // ...
    // ldp     x29, x30, [sp, #16]
    // ldp     x20, x19, [sp], #32
    // ret
    //
    // Previous x29 and x30 register are stored in the address of x29 register.
    // But since sp register value depends on local variables, we cannot compute
    // previous sp register from current sp/fp/lr register and there is no
    // regular rule for sp register in prologue. But since return address is lr
    // register, if x29 is valid, we will get return address without sp
    // register.
    //
    // So we assume the following layout that if no rule set. x29 is frame
    // pointer, so we will be able to compute x29 and x30 .
    //
    //   +----------+  <--- new_sp (cannot compute)
    //   |   ....   |
    //   +----------+
    //   |  new_lr  |  (return address)
    //   +----------+
    //   |  new_fp  |  <--- old_fp
    //   +----------+
    //   |   ....   |
    //   |   ....   |
    //   +----------+  <---- old_sp (arbitrary, but unused)

    TaggedUWord old_fp = regs.x29;
    if (old_fp.Valid() && old_fp.IsAligned() && last_valid_sp.Valid() &&
        last_valid_sp.Value() <= old_fp.Value()) {
      TaggedUWord new_fp = DerefTUW(old_fp, aStackImg);
      if (new_fp.Valid() && new_fp.IsAligned() &&
          old_fp.Value() < new_fp.Value()) {
        TaggedUWord old_fp_plus1 = old_fp + TaggedUWord(8);
        TaggedUWord new_lr = DerefTUW(old_fp_plus1, aStackImg);
        if (new_lr.Valid()) {
          regs.x29 = new_fp;
          regs.x30 = new_lr;
          // When using frame pointer to walk stack, we cannot compute sp
          // register since we cannot compute sp register from fp/lr/sp
          // register, and there is no regular rule to compute previous sp
          // register. So mark as invalid.
          regs.sp = TaggedUWord();
          (*aFramePointerFramesAcquired)++;
          continue;
        }
      }
    }
#endif  // defined(GP_PLAT_amd64_linux) || defined(GP_PLAT_x86_linux) ||
        // defined(GP_PLAT_amd64_android) || defined(GP_PLAT_x86_android)

    // We failed to recover a frame either using CFI or FP chasing, and we
    // have no other ways to recover the frame.  So we have to give up.
    break;

  }  // top level unwind loop

  // END UNWIND
  /////////////////////////////////////////////////////////
}

////////////////////////////////////////////////////////////////
// LUL Unit Testing                                           //
////////////////////////////////////////////////////////////////

static const int LUL_UNIT_TEST_STACK_SIZE = 32768;

#if defined(GP_ARCH_mips64)
static __attribute__((noinline)) unsigned long __getpc(void) {
  unsigned long rtaddr;
  __asm__ volatile("move %0, $31" : "=r"(rtaddr));
  return rtaddr;
}
#endif

// This function is innermost in the test call sequence.  It uses LUL
// to unwind, and compares the result with the sequence specified in
// the director string.  These need to agree in order for the test to
// pass.  In order not to screw up the results, this function needs
// to have a not-very big stack frame, since we're only presenting
// the innermost LUL_UNIT_TEST_STACK_SIZE bytes of stack to LUL, and
// that chunk unavoidably includes the frame for this function.
//
// This function must not be inlined into its callers.  Doing so will
// cause the expected-vs-actual backtrace consistency checking to
// fail.  Prints summary results to |aLUL|'s logging sink and also
// returns a boolean indicating whether or not the test passed.
static __attribute__((noinline)) bool GetAndCheckStackTrace(
    LUL* aLUL, const char* dstring) {
  // Get hold of the current unwind-start registers.
  UnwindRegs startRegs;
  memset(&startRegs, 0, sizeof(startRegs));
#if defined(GP_ARCH_amd64)
  volatile uintptr_t block[3];
  MOZ_ASSERT(sizeof(block) == 24);
  __asm__ __volatile__(
      "leaq 0(%%rip), %%r15"
      "\n\t"
      "movq %%r15, 0(%0)"
      "\n\t"
      "movq %%rsp, 8(%0)"
      "\n\t"
      "movq %%rbp, 16(%0)"
      "\n"
      :
      : "r"(&block[0])
      : "memory", "r15");
  startRegs.xip = TaggedUWord(block[0]);
  startRegs.xsp = TaggedUWord(block[1]);
  startRegs.xbp = TaggedUWord(block[2]);
  const uintptr_t REDZONE_SIZE = 128;
  uintptr_t start = block[1] - REDZONE_SIZE;
#elif defined(GP_PLAT_x86_linux) || defined(GP_PLAT_x86_android)
  volatile uintptr_t block[3];
  MOZ_ASSERT(sizeof(block) == 12);
  __asm__ __volatile__(
      ".byte 0xE8,0x00,0x00,0x00,0x00" /*call next insn*/
      "\n\t"
      "popl %%edi"
      "\n\t"
      "movl %%edi, 0(%0)"
      "\n\t"
      "movl %%esp, 4(%0)"
      "\n\t"
      "movl %%ebp, 8(%0)"
      "\n"
      :
      : "r"(&block[0])
      : "memory", "edi");
  startRegs.xip = TaggedUWord(block[0]);
  startRegs.xsp = TaggedUWord(block[1]);
  startRegs.xbp = TaggedUWord(block[2]);
  const uintptr_t REDZONE_SIZE = 0;
  uintptr_t start = block[1] - REDZONE_SIZE;
#elif defined(GP_PLAT_arm_linux) || defined(GP_PLAT_arm_android)
  volatile uintptr_t block[6];
  MOZ_ASSERT(sizeof(block) == 24);
  __asm__ __volatile__(
      "mov r0, r15"
      "\n\t"
      "str r0,  [%0, #0]"
      "\n\t"
      "str r14, [%0, #4]"
      "\n\t"
      "str r13, [%0, #8]"
      "\n\t"
      "str r12, [%0, #12]"
      "\n\t"
      "str r11, [%0, #16]"
      "\n\t"
      "str r7,  [%0, #20]"
      "\n"
      :
      : "r"(&block[0])
      : "memory", "r0");
  startRegs.r15 = TaggedUWord(block[0]);
  startRegs.r14 = TaggedUWord(block[1]);
  startRegs.r13 = TaggedUWord(block[2]);
  startRegs.r12 = TaggedUWord(block[3]);
  startRegs.r11 = TaggedUWord(block[4]);
  startRegs.r7 = TaggedUWord(block[5]);
  const uintptr_t REDZONE_SIZE = 0;
  uintptr_t start = block[1] - REDZONE_SIZE;
#elif defined(GP_ARCH_arm64)
  volatile uintptr_t block[4];
  MOZ_ASSERT(sizeof(block) == 32);
  __asm__ __volatile__(
      "adr x0, . \n\t"
      "str x0, [%0, #0] \n\t"
      "str x29, [%0, #8] \n\t"
      "str x30, [%0, #16] \n\t"
      "mov x0, sp \n\t"
      "str x0, [%0, #24] \n\t"
      :
      : "r"(&block[0])
      : "memory", "x0");
  startRegs.pc = TaggedUWord(block[0]);
  startRegs.x29 = TaggedUWord(block[1]);
  startRegs.x30 = TaggedUWord(block[2]);
  startRegs.sp = TaggedUWord(block[3]);
  const uintptr_t REDZONE_SIZE = 0;
  uintptr_t start = block[1] - REDZONE_SIZE;
#elif defined(GP_ARCH_mips64)
  volatile uintptr_t block[3];
  MOZ_ASSERT(sizeof(block) == 24);
  __asm__ __volatile__(
      "sd $29, 8(%0)     \n"
      "sd $30, 16(%0)    \n"
      :
      : "r"(block)
      : "memory");
  block[0] = __getpc();
  startRegs.pc = TaggedUWord(block[0]);
  startRegs.sp = TaggedUWord(block[1]);
  startRegs.fp = TaggedUWord(block[2]);
  const uintptr_t REDZONE_SIZE = 0;
  uintptr_t start = block[1] - REDZONE_SIZE;
#else
#  error "Unsupported platform"
#endif

  // Get hold of the innermost LUL_UNIT_TEST_STACK_SIZE bytes of the
  // stack.
  uintptr_t end = start + LUL_UNIT_TEST_STACK_SIZE;
  uintptr_t ws = sizeof(void*);
  start &= ~(ws - 1);
  end &= ~(ws - 1);
  uintptr_t nToCopy = end - start;
  if (nToCopy > lul::N_STACK_BYTES) {
    nToCopy = lul::N_STACK_BYTES;
  }
  MOZ_ASSERT(nToCopy <= lul::N_STACK_BYTES);
  StackImage* stackImg = new StackImage();
  stackImg->mLen = nToCopy;
  stackImg->mStartAvma = start;
  if (nToCopy > 0) {
    MOZ_MAKE_MEM_DEFINED((void*)start, nToCopy);
    memcpy(&stackImg->mContents[0], (void*)start, nToCopy);
  }

  // Unwind it.
  const int MAX_TEST_FRAMES = 64;
  uintptr_t framePCs[MAX_TEST_FRAMES];
  uintptr_t frameSPs[MAX_TEST_FRAMES];
  size_t framesAvail = mozilla::ArrayLength(framePCs);
  size_t framesUsed = 0;
  size_t framePointerFramesAcquired = 0;
  aLUL->Unwind(&framePCs[0], &frameSPs[0], &framesUsed,
               &framePointerFramesAcquired, framesAvail, &startRegs, stackImg);

  delete stackImg;

  // if (0) {
  //  // Show what we have.
  //  fprintf(stderr, "Got %d frames:\n", (int)framesUsed);
  //  for (size_t i = 0; i < framesUsed; i++) {
  //    fprintf(stderr, "  [%2d]   SP %p   PC %p\n",
  //            (int)i, (void*)frameSPs[i], (void*)framePCs[i]);
  //  }
  //  fprintf(stderr, "\n");
  //}

  // Check to see if there's a consistent binding between digits in
  // the director string ('1' .. '8') and the PC values acquired by
  // the unwind.  If there isn't, the unwinding has failed somehow.
  uintptr_t binding[8];  // binding for '1' .. binding for '8'
  memset((void*)binding, 0, sizeof(binding));

  // The general plan is to work backwards along the director string
  // and forwards along the framePCs array.  Doing so corresponds to
  // working outwards from the innermost frame of the recursive test set.
  const char* cursor = dstring;

  // Find the end.  This leaves |cursor| two bytes past the first
  // character we want to look at -- see comment below.
  while (*cursor) cursor++;

  // Counts the number of consistent frames.
  size_t nConsistent = 0;

  // Iterate back to the start of the director string.  The starting
  // points are a bit complex.  We can't use framePCs[0] because that
  // contains the PC in this frame (above).  We can't use framePCs[1]
  // because that will contain the PC at return point in the recursive
  // test group (TestFn[1-8]) for their call "out" to this function,
  // GetAndCheckStackTrace.  Although LUL will compute a correct
  // return address, that will not be the same return address as for a
  // recursive call out of the the function to another function in the
  // group.  Hence we can only start consistency checking at
  // framePCs[2].
  //
  // To be consistent, then, we must ignore the last element in the
  // director string as that corresponds to framePCs[1].  Hence the
  // start points are: framePCs[2] and the director string 2 bytes
  // before the terminating zero.
  //
  // Also as a result of this, the number of consistent frames counted
  // will always be one less than the length of the director string
  // (not including its terminating zero).
  size_t frameIx;
  for (cursor = cursor - 2, frameIx = 2;
       cursor >= dstring && frameIx < framesUsed; cursor--, frameIx++) {
    char c = *cursor;
    uintptr_t pc = framePCs[frameIx];
    // If this doesn't hold, the director string is ill-formed.
    MOZ_ASSERT(c >= '1' && c <= '8');
    int n = ((int)c) - ((int)'1');
    if (binding[n] == 0) {
      // There's no binding for |c| yet, so install |pc| and carry on.
      binding[n] = pc;
      nConsistent++;
      continue;
    }
    // There's a pre-existing binding for |c|.  Check it's consistent.
    if (binding[n] != pc) {
      // Not consistent.  Give up now.
      break;
    }
    // Consistent.  Keep going.
    nConsistent++;
  }

  // So, did we succeed?
  bool passed = nConsistent + 1 == strlen(dstring);

  // Show the results.
  char buf[200];
  SprintfLiteral(buf, "LULUnitTest:   dstring = %s\n", dstring);
  buf[sizeof(buf) - 1] = 0;
  aLUL->mLog(buf);
  SprintfLiteral(buf, "LULUnitTest:     %d consistent, %d in dstring: %s\n",
                 (int)nConsistent, (int)strlen(dstring),
                 passed ? "PASS" : "FAIL");
  buf[sizeof(buf) - 1] = 0;
  aLUL->mLog(buf);

  return passed;
}

// Macro magic to create a set of 8 mutually recursive functions with
// varying frame sizes.  These will recurse amongst themselves as
// specified by |strP|, the directory string, and call
// GetAndCheckStackTrace when the string becomes empty, passing it the
// original value of the string.  This checks the result, printing
// results on |aLUL|'s logging sink, and also returns a boolean
// indicating whether or not the results are acceptable (correct).

#define DECL_TEST_FN(NAME) \
  bool NAME(LUL* aLUL, const char* strPorig, const char* strP);

#define GEN_TEST_FN(NAME, FRAMESIZE)                                          \
  bool NAME(LUL* aLUL, const char* strPorig, const char* strP) {              \
    /* Create a frame of size (at least) FRAMESIZE, so that the */            \
    /* 8 functions created by this macro offer some variation in frame */     \
    /* sizes.  This isn't as simple as it might seem, since a clever */       \
    /* optimizing compiler (eg, clang-5) detects that the array is unused */  \
    /* and removes it.  We try to defeat this by passing it to a function */  \
    /* in a different compilation unit, and hoping that clang does not */     \
    /* notice that the call is a no-op. */                                    \
    char space[FRAMESIZE];                                                    \
    Unused << write(1, space, 0); /* write zero bytes of |space| to stdout */ \
                                                                              \
    if (*strP == '\0') {                                                      \
      /* We've come to the end of the director string. */                     \
      /* Take a stack snapshot. */                                            \
      return GetAndCheckStackTrace(aLUL, strPorig);                           \
    } else {                                                                  \
      /* Recurse onwards.  This is a bit subtle.  The obvious */              \
      /* thing to do here is call onwards directly, from within the */        \
      /* arms of the case statement.  That gives a problem in that */         \
      /* there will be multiple return points inside each function when */    \
      /* unwinding, so it will be difficult to check for consistency */       \
      /* against the director string.  Instead, we make an indirect */        \
      /* call, so as to guarantee that there is only one call site */         \
      /* within each function.  This does assume that the compiler */         \
      /* won't transform it back to the simple direct-call form. */           \
      /* To discourage it from doing so, the call is bracketed with */        \
      /* __asm__ __volatile__ sections so as to make it not-movable. */       \
      bool (*nextFn)(LUL*, const char*, const char*) = NULL;                  \
      switch (*strP) {                                                        \
        case '1':                                                             \
          nextFn = TestFn1;                                                   \
          break;                                                              \
        case '2':                                                             \
          nextFn = TestFn2;                                                   \
          break;                                                              \
        case '3':                                                             \
          nextFn = TestFn3;                                                   \
          break;                                                              \
        case '4':                                                             \
          nextFn = TestFn4;                                                   \
          break;                                                              \
        case '5':                                                             \
          nextFn = TestFn5;                                                   \
          break;                                                              \
        case '6':                                                             \
          nextFn = TestFn6;                                                   \
          break;                                                              \
        case '7':                                                             \
          nextFn = TestFn7;                                                   \
          break;                                                              \
        case '8':                                                             \
          nextFn = TestFn8;                                                   \
          break;                                                              \
        default:                                                              \
          nextFn = TestFn8;                                                   \
          break;                                                              \
      }                                                                       \
      /* "use" |space| immediately after the recursive call, */               \
      /* so as to dissuade clang from deallocating the space while */         \
      /* the call is active, or otherwise messing with the stack frame. */    \
      __asm__ __volatile__("" ::: "cc", "memory");                            \
      bool passed = nextFn(aLUL, strPorig, strP + 1);                         \
      Unused << write(1, space, 0);                                           \
      __asm__ __volatile__("" ::: "cc", "memory");                            \
      return passed;                                                          \
    }                                                                         \
  }

// The test functions are mutually recursive, so it is necessary to
// declare them before defining them.
DECL_TEST_FN(TestFn1)
DECL_TEST_FN(TestFn2)
DECL_TEST_FN(TestFn3)
DECL_TEST_FN(TestFn4)
DECL_TEST_FN(TestFn5)
DECL_TEST_FN(TestFn6)
DECL_TEST_FN(TestFn7)
DECL_TEST_FN(TestFn8)

GEN_TEST_FN(TestFn1, 123)
GEN_TEST_FN(TestFn2, 456)
GEN_TEST_FN(TestFn3, 789)
GEN_TEST_FN(TestFn4, 23)
GEN_TEST_FN(TestFn5, 47)
GEN_TEST_FN(TestFn6, 117)
GEN_TEST_FN(TestFn7, 1)
GEN_TEST_FN(TestFn8, 99)

// This starts the test sequence going.  Call here to generate a
// sequence of calls as directed by the string |dstring|.  The call
// sequence will, from its innermost frame, finish by calling
// GetAndCheckStackTrace() and passing it |dstring|.
// GetAndCheckStackTrace() will unwind the stack, check consistency
// of those results against |dstring|, and print a pass/fail message
// to aLUL's logging sink.  It also updates the counters in *aNTests
// and aNTestsPassed.
__attribute__((noinline)) void TestUnw(/*OUT*/ int* aNTests,
                                       /*OUT*/ int* aNTestsPassed, LUL* aLUL,
                                       const char* dstring) {
  // Ensure that the stack has at least this much space on it.  This
  // makes it safe to saw off the top LUL_UNIT_TEST_STACK_SIZE bytes
  // and hand it to LUL.  Safe in the sense that no segfault can
  // happen because the stack is at least this big.  This is all
  // somewhat dubious in the sense that a sufficiently clever compiler
  // (clang, for one) can figure out that space[] is unused and delete
  // it from the frame.  Hence the somewhat elaborate hoop jumping to
  // fill it up before the call and to at least appear to use the
  // value afterwards.
  int i;
  volatile char space[LUL_UNIT_TEST_STACK_SIZE];
  for (i = 0; i < LUL_UNIT_TEST_STACK_SIZE; i++) {
    space[i] = (char)(i & 0x7F);
  }

  // Really run the test.
  bool passed = TestFn1(aLUL, dstring, dstring);

  // Appear to use space[], by visiting the value to compute some kind
  // of checksum, and then (apparently) using the checksum.
  int sum = 0;
  for (i = 0; i < LUL_UNIT_TEST_STACK_SIZE; i++) {
    // If this doesn't fool LLVM, I don't know what will.
    sum += space[i] - 3 * i;
  }
  __asm__ __volatile__("" : : "r"(sum));

  // Update the counters.
  (*aNTests)++;
  if (passed) {
    (*aNTestsPassed)++;
  }
}

void RunLulUnitTests(/*OUT*/ int* aNTests, /*OUT*/ int* aNTestsPassed,
                     LUL* aLUL) {
  aLUL->mLog(":\n");
  aLUL->mLog("LULUnitTest: BEGIN\n");
  *aNTests = *aNTestsPassed = 0;
  TestUnw(aNTests, aNTestsPassed, aLUL, "11111111");
  TestUnw(aNTests, aNTestsPassed, aLUL, "11222211");
  TestUnw(aNTests, aNTestsPassed, aLUL, "111222333");
  TestUnw(aNTests, aNTestsPassed, aLUL, "1212121231212331212121212121212");
  TestUnw(aNTests, aNTestsPassed, aLUL, "31415827271828325332173258");
  TestUnw(aNTests, aNTestsPassed, aLUL,
          "123456781122334455667788777777777777777777777");
  aLUL->mLog("LULUnitTest: END\n");
  aLUL->mLog(":\n");
}

}  // namespace lul
