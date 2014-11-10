/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LulMain.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <algorithm>  // std::sort
#include <string>

#include "mozilla/Assertions.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/MemoryChecking.h"

#include "LulCommonExt.h"
#include "LulElfExt.h"

#include "LulMainInt.h"

// Set this to 1 for verbose logging
#define DEBUG_MAIN 0


namespace lul {

using std::string;
using std::vector;


////////////////////////////////////////////////////////////////
// AutoLulRWLocker                                            //
////////////////////////////////////////////////////////////////

// This is a simple RAII class that manages acquisition and release of
// LulRWLock reader-writer locks.

class AutoLulRWLocker {
public:
  enum AcqMode { FOR_READING, FOR_WRITING };
  AutoLulRWLocker(LulRWLock* aRWLock, AcqMode mode)
    : mRWLock(aRWLock)
  {
    if (mode == FOR_WRITING) {
      aRWLock->WrLock();
    } else {
      aRWLock->RdLock();
    }
  }
  ~AutoLulRWLocker()
  {
    mRWLock->Unlock();
  }

private:
  LulRWLock* mRWLock;
};


////////////////////////////////////////////////////////////////
// RuleSet                                                    //
////////////////////////////////////////////////////////////////

static const char* 
NameOf_DW_REG(int16_t aReg)
{
  switch (aReg) {
    case DW_REG_CFA:       return "cfa";
#if defined(LUL_ARCH_x64) || defined(LUL_ARCH_x86)
    case DW_REG_INTEL_XBP: return "xbp";
    case DW_REG_INTEL_XSP: return "xsp";
    case DW_REG_INTEL_XIP: return "xip";
#elif defined(LUL_ARCH_arm)
    case DW_REG_ARM_R7:    return "r7";
    case DW_REG_ARM_R11:   return "r11";
    case DW_REG_ARM_R12:   return "r12";
    case DW_REG_ARM_R13:   return "r13";
    case DW_REG_ARM_R14:   return "r14";
    case DW_REG_ARM_R15:   return "r15";
#else
# error "Unsupported arch"
#endif
    default: return "???";
  }
}

static string
ShowRule(const char* aNewReg, LExpr aExpr)
{
  char buf[64];
  string res = string(aNewReg) + "=";
  switch (aExpr.mHow) {
    case LExpr::UNKNOWN:
      res += "Unknown";
      break;
    case LExpr::NODEREF:
      sprintf(buf, "%s+%d", NameOf_DW_REG(aExpr.mReg), (int)aExpr.mOffset);
      res += buf;
      break;
    case LExpr::DEREF:
      sprintf(buf, "*(%s+%d)", NameOf_DW_REG(aExpr.mReg), (int)aExpr.mOffset);
      res += buf;
      break;
    default:
      res += "???";
      break;
  }
  return res;
}

void
RuleSet::Print(void(*aLog)(const char*))
{
  char buf[96];
  sprintf(buf, "[%llx .. %llx]: let ",
          (unsigned long long int)mAddr,
          (unsigned long long int)(mAddr + mLen - 1));
  string res = string(buf);
  res += ShowRule("cfa", mCfaExpr);
  res += " in";
  // For each reg we care about, print the recovery expression.
#if defined(LUL_ARCH_x64) || defined(LUL_ARCH_x86)
  res += ShowRule(" RA", mXipExpr);
  res += ShowRule(" SP", mXspExpr);
  res += ShowRule(" BP", mXbpExpr);
#elif defined(LUL_ARCH_arm)
  res += ShowRule(" R15", mR15expr);
  res += ShowRule(" R7",  mR7expr);
  res += ShowRule(" R11", mR11expr);
  res += ShowRule(" R12", mR12expr);
  res += ShowRule(" R13", mR13expr);
  res += ShowRule(" R14", mR14expr);
#else
# error "Unsupported arch"
#endif
  aLog(res.c_str());
}

LExpr*
RuleSet::ExprForRegno(DW_REG_NUMBER aRegno) {
  switch (aRegno) {
    case DW_REG_CFA: return &mCfaExpr;
#   if defined(LUL_ARCH_x64) || defined(LUL_ARCH_x86)
    case DW_REG_INTEL_XIP: return &mXipExpr;
    case DW_REG_INTEL_XSP: return &mXspExpr;
    case DW_REG_INTEL_XBP: return &mXbpExpr;
#   elif defined(LUL_ARCH_arm)
    case DW_REG_ARM_R15:   return &mR15expr;
    case DW_REG_ARM_R14:   return &mR14expr;
    case DW_REG_ARM_R13:   return &mR13expr;
    case DW_REG_ARM_R12:   return &mR12expr;
    case DW_REG_ARM_R11:   return &mR11expr;
    case DW_REG_ARM_R7:    return &mR7expr;
#   else
#     error "Unknown arch"
#   endif
    default: return nullptr;
  }
}

RuleSet::RuleSet()
{
  mAddr = 0;
  mLen  = 0;
  // The only other fields are of type LExpr and those are initialised
  // by LExpr::LExpr().
}


////////////////////////////////////////////////////////////////
// SecMap                                                     //
////////////////////////////////////////////////////////////////

// See header file LulMainInt.h for comments about invariants.

SecMap::SecMap(void(*aLog)(const char*))
  : mSummaryMinAddr(1)
  , mSummaryMaxAddr(0)
  , mUsable(true)
  , mLog(aLog)
{}

SecMap::~SecMap() {
  mRuleSets.clear();
}

RuleSet*
SecMap::FindRuleSet(uintptr_t ia) {
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
    long int  mid         = lo + ((hi - lo) / 2);
    RuleSet*  mid_ruleSet = &mRuleSets[mid];
    uintptr_t mid_minAddr = mid_ruleSet->mAddr;
    uintptr_t mid_maxAddr = mid_minAddr + mid_ruleSet->mLen - 1;
    if (ia < mid_minAddr) { hi = mid-1; continue; }
    if (ia > mid_maxAddr) { lo = mid+1; continue; }
    MOZ_ASSERT(mid_minAddr <= ia && ia <= mid_maxAddr);
    return mid_ruleSet;
  }
  // NOTREACHED
}

// Add a RuleSet to the collection.  The rule is copied in.  Calling
// this makes the map non-searchable.
void
SecMap::AddRuleSet(RuleSet* rs) {
  mUsable = false;
  mRuleSets.push_back(*rs);
}


static bool
CmpRuleSetsByAddrLE(const RuleSet& rs1, const RuleSet& rs2) {
  return rs1.mAddr < rs2.mAddr;
}

// Prepare the map for searching.  Completely remove any which don't
// fall inside the specified range [start, +len).
void
SecMap::PrepareRuleSets(uintptr_t aStart, size_t aLen)
{
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
      RuleSet* prev = &mRuleSets[i-1];
      RuleSet* here = &mRuleSets[i];
      MOZ_ASSERT(prev->mAddr <= here->mAddr);
      if (prev->mAddr + prev->mLen > here->mAddr) {
        prev->mLen = here->mAddr - prev->mAddr;
      }
      if (prev->mLen == 0)
        nZeroLen++;
    }

    if (mRuleSets[n-1].mLen == 0) {
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
      RuleSet* prev = &mRuleSets[i-1];
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
    mSummaryMaxAddr = mRuleSets[n-1].mAddr + mRuleSets[n-1].mLen - 1;
  }
  char buf[150];
  snprintf(buf, sizeof(buf),
           "PrepareRuleSets: %d entries, smin/smax 0x%llx, 0x%llx\n",
           (int)n, (unsigned long long int)mSummaryMinAddr,
                   (unsigned long long int)mSummaryMaxAddr);
  buf[sizeof(buf)-1] = 0;
  mLog(buf);

  // Is now usable for binary search.
  mUsable = true;

  if (0) {
    mLog("\nRulesets after preening\n");
    for (size_t i = 0; i < mRuleSets.size(); ++i) {
      mRuleSets[i].Print(mLog);
      mLog("\n");
    }
    mLog("\n");
  }
}

bool SecMap::IsEmpty() {
  return mRuleSets.empty();
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
      split_at(hi+1);
    }
    std::vector<Seg>::size_type iLo, iHi, i;
    iLo = find(lo);
    iHi = find(hi);
    for (i = iLo; i <= iHi; ++i) {
      mSegs[i].val = val;
    }
    preen();
  }

  bool getBoundingCodeSegment(/*OUT*/uintptr_t* rx_min,
                              /*OUT*/uintptr_t* rx_max, uintptr_t addr) {
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
         iter < mSegs.end()-1;
         ++iter) {
      if (iter[0].val != iter[1].val) {
        continue;
      }
      iter[0].hi = iter[1].hi;
      mSegs.erase(iter+1);
      // Back up one, so as not to miss an opportunity to merge
      // with the entry after this one.
      --iter;
    }
  }

  std::vector<Seg>::size_type find(uintptr_t a) {
    long int lo = 0;
    long int hi = (long int)mSegs.size();
    while (true) {
      // The unsearched space is lo .. hi inclusive.
      if (lo > hi) {
        // Not found.  This can't happen.
        return (std::vector<Seg>::size_type)(-1);
      }
      long int  mid    = lo + ((hi - lo) / 2);
      uintptr_t mid_lo = mSegs[mid].lo;
      uintptr_t mid_hi = mSegs[mid].hi;
      if (a < mid_lo) { hi = mid-1; continue; }
      if (a > mid_hi) { lo = mid+1; continue; }
      return (std::vector<Seg>::size_type)mid;
    }
  }

  void split_at(uintptr_t a) {
    std::vector<Seg>::size_type i = find(a);
    if (mSegs[i].lo == a) {
      return;
    }
    mSegs.insert( mSegs.begin()+i+1, mSegs[i] );
    mSegs[i].hi = a-1;
    mSegs[i+1].lo = a;
  }

  void show() {
    printf("<< %d entries:\n", (int)mSegs.size());
    for (std::vector<Seg>::iterator iter = mSegs.begin();
         iter < mSegs.end();
         ++iter) {
      printf("  %016llx  %016llx  %s\n",
             (unsigned long long int)(*iter).lo,
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
  explicit PriMap(void (*aLog)(const char*))
    : mLog(aLog)
  {}

  ~PriMap() {
    for (std::vector<SecMap*>::iterator iter = mSecMaps.begin();
         iter != mSecMaps.end();
         ++iter) {
      delete *iter;
    }
    mSecMaps.clear();
  }

  // This can happen with the global lock held for reading.
  RuleSet* Lookup(uintptr_t ia) {
    SecMap* sm = FindSecMap(ia);
    return sm ? sm->FindRuleSet(ia) : nullptr;
  }

  // Add a secondary map.  No overlaps allowed w.r.t. existing
  // secondary maps.  Global lock must be held for writing.
  void AddSecMap(SecMap* aSecMap) {
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
      SecMap* sm_i = mSecMaps[i];
      MOZ_ASSERT(sm_i->mSummaryMinAddr <= sm_i->mSummaryMaxAddr);
      if (aSecMap->mSummaryMinAddr < sm_i->mSummaryMaxAddr) {
        // |aSecMap| needs to be inserted immediately before mSecMaps[i].
        break;
      }
    }
    MOZ_ASSERT(i <= num_secMaps);
    if (i == num_secMaps) {
      // It goes at the end.
      mSecMaps.push_back(aSecMap);
    } else {
      std::vector<SecMap*>::iterator iter = mSecMaps.begin() + i;
      mSecMaps.insert(iter, aSecMap);
    }
    char buf[100];
    snprintf(buf, sizeof(buf), "AddSecMap: now have %d SecMaps\n",
             (int)mSecMaps.size());
    buf[sizeof(buf)-1] = 0;
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
      for (i = (intptr_t)num_secMaps-1; i >= 0; i--) {
        SecMap* sm_i = mSecMaps[i];
        if (sm_i->mSummaryMaxAddr < avma_min ||
            avma_max < sm_i->mSummaryMinAddr) {
          // There's no overlap.  Move on.
          continue;
        }
        // We need to remove mSecMaps[i] and slide all those above it
        // downwards to cover the hole.
        mSecMaps.erase(mSecMaps.begin() + i);
        delete sm_i;
      }
    }
  }

  // Return the number of currently contained SecMaps.
  size_t CountSecMaps() {
    return mSecMaps.size();
  }

  // Assess heuristically whether the given address is an instruction
  // immediately following a call instruction.  The caller is required
  // to hold the global lock for reading.
  bool MaybeIsReturnPoint(TaggedUWord aInstrAddr, SegArray* aSegArray) {
    if (!aInstrAddr.Valid()) {
      return false;
    }

    uintptr_t ia = aInstrAddr.Value();

    // Assume that nobody would be crazy enough to put code in the
    // first or last page.
    if (ia < 4096 || ((uintptr_t)(-ia)) < 4096) {
      return false;
    }

    // See if it falls inside a known r-x mapped area.  Poking around
    // outside such places risks segfaulting.
    uintptr_t insns_min, insns_max;
    bool b = aSegArray->getBoundingCodeSegment(&insns_min, &insns_max, ia);
    if (!b) {
      // no code (that we know about) at this address
      return false;
    }

    // |ia| falls within an r-x range.  So we can
    // safely poke around in [insns_min, insns_max].

#if defined(LUL_ARCH_x64) || defined(LUL_ARCH_x86)
    // Is the previous instruction recognisably a CALL?  This is
    // common for the 32- and 64-bit versions, except for the
    // simm32(%rip) case, which is 64-bit only.
    //
    // For all other cases, the 64 bit versions are either identical
    // to the 32 bit versions, or have an optional extra leading REX.W
    // byte (0x41).  Since the extra 0x41 is optional we have to
    // ignore it, with the convenient result that the same matching
    // logic works for both 32- and 64-bit cases.

    uint8_t* p = (uint8_t*)ia;
#   if defined(LUL_ARCH_x64)
    // CALL simm32(%rip)  == FF15 simm32
    if (ia - 6 >= insns_min && p[-6] == 0xFF && p[-5] == 0x15) {
      return true;
    }
#   endif
    // CALL rel32  == E8 rel32  (both 32- and 64-bit)
    if (ia - 5 >= insns_min && p[-5] == 0xE8) {
      return true;
    }
    // CALL *%eax .. CALL *%edi  ==   FFD0 ..   FFD7  (32-bit)
    // CALL *%rax .. CALL *%rdi  ==   FFD0 ..   FFD7  (64-bit)
    // CALL *%r8  .. CALL *%r15  == 41FFD0 .. 41FFD7  (64-bit)
    if (ia - 2 >= insns_min &&
        p[-2] == 0xFF && p[-1] >= 0xD0 && p[-1] <= 0xD7) {
      return true;
    }
    // Almost all of the remaining cases that occur in practice are
    // of the form CALL *simm8(reg) or CALL *simm32(reg).
    //
    // 64 bit cases:
    //
    // call  *simm8(%rax)         FF50   simm8
    // call  *simm8(%rcx)         FF51   simm8
    // call  *simm8(%rdx)         FF52   simm8
    // call  *simm8(%rbx)         FF53   simm8
    // call  *simm8(%rsp)         FF5424 simm8
    // call  *simm8(%rbp)         FF55   simm8
    // call  *simm8(%rsi)         FF56   simm8
    // call  *simm8(%rdi)         FF57   simm8
    //
    // call  *simm8(%r8)        41FF50   simm8
    // call  *simm8(%r9)        41FF51   simm8
    // call  *simm8(%r10)       41FF52   simm8
    // call  *simm8(%r11)       41FF53   simm8
    // call  *simm8(%r12)       41FF5424 simm8
    // call  *simm8(%r13)       41FF55   simm8
    // call  *simm8(%r14)       41FF56   simm8
    // call  *simm8(%r15)       41FF57   simm8
    //
    // call  *simm32(%rax)        FF90   simm32
    // call  *simm32(%rcx)        FF91   simm32
    // call  *simm32(%rdx)        FF92   simm32
    // call  *simm32(%rbx)        FF93   simm32
    // call  *simm32(%rsp)        FF9424 simm32
    // call  *simm32(%rbp)        FF95   simm32
    // call  *simm32(%rsi)        FF96   simm32
    // call  *simm32(%rdi)        FF97   simm32
    //
    // call  *simm32(%r8)       41FF90   simm32
    // call  *simm32(%r9)       41FF91   simm32
    // call  *simm32(%r10)      41FF92   simm32
    // call  *simm32(%r11)      41FF93   simm32
    // call  *simm32(%r12)      41FF9424 simm32
    // call  *simm32(%r13)      41FF95   simm32
    // call  *simm32(%r14)      41FF96   simm32
    // call  *simm32(%r15)      41FF97   simm32
    //
    // 32 bit cases:
    //
    // call  *simm8(%eax)         FF50   simm8
    // call  *simm8(%ecx)         FF51   simm8
    // call  *simm8(%edx)         FF52   simm8
    // call  *simm8(%ebx)         FF53   simm8
    // call  *simm8(%esp)         FF5424 simm8
    // call  *simm8(%ebp)         FF55   simm8
    // call  *simm8(%esi)         FF56   simm8
    // call  *simm8(%edi)         FF57   simm8
    //
    // call  *simm32(%eax)        FF90   simm32
    // call  *simm32(%ecx)        FF91   simm32
    // call  *simm32(%edx)        FF92   simm32
    // call  *simm32(%ebx)        FF93   simm32
    // call  *simm32(%esp)        FF9424 simm32
    // call  *simm32(%ebp)        FF95   simm32
    // call  *simm32(%esi)        FF96   simm32
    // call  *simm32(%edi)        FF97   simm32
    if (ia - 3 >= insns_min &&
        p[-3] == 0xFF &&
        (p[-2] >= 0x50 && p[-2] <= 0x57 && p[-2] != 0x54)) {
      // imm8 case, not including %esp/%rsp
      return true;
    }
    if (ia - 4 >= insns_min &&
        p[-4] == 0xFF && p[-3] == 0x54 && p[-2] == 0x24) {
      // imm8 case for %esp/%rsp
      return true;
    }
    if (ia - 6 >= insns_min &&
        p[-6] == 0xFF &&
        (p[-5] >= 0x90 && p[-5] <= 0x97 && p[-5] != 0x94)) {
      // imm32 case, not including %esp/%rsp
      return true;
    }
    if (ia - 7 >= insns_min &&
        p[-7] == 0xFF && p[-6] == 0x94 && p[-5] == 0x24) {
      // imm32 case for %esp/%rsp
      return true;
    }

#elif defined(LUL_ARCH_arm)
    if (ia & 1) {
      uint16_t w0 = 0, w1 = 0;
      // The return address has its lowest bit set, indicating a return
      // to Thumb code.
      ia &= ~(uintptr_t)1;
      if (ia - 2 >= insns_min && ia - 1 <= insns_max) {
        w1 = *(uint16_t*)(ia - 2);
      }
      if (ia - 4 >= insns_min && ia - 1 <= insns_max) {
        w0 = *(uint16_t*)(ia - 4);
      }
      // Is it a 32-bit Thumb call insn?
      // BL  simm26 (Encoding T1)
      if ((w0 & 0xF800) == 0xF000 && (w1 & 0xC000) == 0xC000) {
        return true;
      }
      // BLX simm26 (Encoding T2)
      if ((w0 & 0xF800) == 0xF000 && (w1 & 0xC000) == 0xC000) {
        return true;
      }
      // Other possible cases:
      // (BLX Rm, Encoding T1).
      // BLX Rm (encoding T1, 16 bit, inspect w1 and ignore w0.)
      // 0100 0111 1 Rm 000
    } else {
      // Returning to ARM code.
      uint32_t a0 = 0;
      if ((ia & 3) == 0 && ia - 4 >= insns_min && ia - 1 <= insns_max) {
        a0 = *(uint32_t*)(ia - 4);
      }
      // Leading E forces unconditional only -- fix.  It could be
      // anything except F, which is the deprecated NV code.
      // BL simm26 (Encoding A1)
      if ((a0 & 0xFF000000) == 0xEB000000) {
        return true;
      }
      // Other possible cases:
      // BLX simm26 (Encoding A2)
      //if ((a0 & 0xFE000000) == 0xFA000000)
      //  return true;
      // BLX (register) (A1): BLX <c> <Rm>
      // cond 0001 0010 1111 1111 1111 0011 Rm
      // again, cond can be anything except NV (0xF)
    }

#else
# error "Unsupported arch"
#endif

    // Not an insn we recognise.
    return false;
  }

 private:
  // FindSecMap's caller must hold the global lock for reading or writing.
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
      long int  mid         = lo + ((hi - lo) / 2);
      SecMap*   mid_secMap  = mSecMaps[mid];
      uintptr_t mid_minAddr = mid_secMap->mSummaryMinAddr;
      uintptr_t mid_maxAddr = mid_secMap->mSummaryMaxAddr;
      if (ia < mid_minAddr) { hi = mid-1; continue; }
      if (ia > mid_maxAddr) { lo = mid+1; continue; }
      MOZ_ASSERT(mid_minAddr <= ia && ia <= mid_maxAddr);
      return mid_secMap;
    }
    // NOTREACHED
  }

 private:
  // sorted array of per-object ranges, non overlapping, non empty
  std::vector<SecMap*> mSecMaps;

  // a logging sink, for debugging.
  void (*mLog)(const char*);
};


////////////////////////////////////////////////////////////////
// CFICache                                                   //
////////////////////////////////////////////////////////////////

// This is the thread-local cache.  It maps individual insn AVMAs to
// the associated CFI record, which live in LUL::mPriMap.
//
// The cache is a direct map hash table indexed by address.
// It has to distinguish 3 cases:
//
// (1) .mRSet == (RuleSet*)0  ==> cache slot not in use
// (2) .mRSet == (RuleSet*)1  ==> slot in use, no RuleSet avail
// (3) .mRSet >  (RuleSet*)1  ==> slot in use, RuleSet* available
//
// Distinguishing between (2) and (3) is important, because if we look
// up an address in LUL::mPriMap and find there is no RuleSet, then
// that fact needs to cached here, so as to avoid potentially
// expensive repeat lookups.

// A CFICacheEntry::mRSet value of zero indicates that the slot is not
// in use, and a value of one indicates that the slot is in use but
// there is no RuleSet available.
#define ENTRY_NOT_IN_USE      ((RuleSet*)0)
#define NO_RULESET_AVAILABLE  ((RuleSet*)1)

class CFICache {
 public:

  explicit CFICache(PriMap* aPriMap) {
    Invalidate();
    mPriMap = aPriMap;
  }

  void Invalidate() {
    for (int i = 0; i < N_ENTRIES; ++i) {
      mCache[i].mAVMA = 0;
      mCache[i].mRSet = ENTRY_NOT_IN_USE;
    }
  }

  RuleSet* Lookup(uintptr_t ia) {
    uintptr_t hash = ia % (uintptr_t)N_ENTRIES;
    CFICacheEntry* ce = &mCache[hash];
    if (ce->mAVMA == ia) {
      // The cache has an entry for |ia|.  Interpret it.
      if (ce->mRSet > NO_RULESET_AVAILABLE) {
        // There's a RuleSet.  So return it.
        return ce->mRSet;
      }
      if (ce->mRSet == NO_RULESET_AVAILABLE) {
        // There's no RuleSet for this address.  Don't update
        // the cache, since we might get queried again.
        return nullptr;
      }
      // Otherwise, the slot is not in use.  Fall through to
      // the 'miss' case.
    }

    // The cache entry is for some other address, or is not in use.
    // Update it.  If it can be found in the priMap then install it
    // as-is.  Else put NO_RULESET_AVAILABLE in, so as to indicate
    // there's no info for this address.
    RuleSet* fallback  = mPriMap->Lookup(ia);
    mCache[hash].mAVMA = ia;
    mCache[hash].mRSet = fallback ? fallback : NO_RULESET_AVAILABLE;
    return fallback;
  }

 private:
  // This should be a prime number.
  static const int N_ENTRIES = 509;

  // See comment above for the meaning of these entries.
  struct CFICacheEntry {
    uintptr_t mAVMA; // AVMA of the associated instruction
    RuleSet*  mRSet; // RuleSet* for the instruction
  };
  CFICacheEntry mCache[N_ENTRIES];

  // Need to have a pointer to the PriMap, so as to be able
  // to service misses.
  PriMap* mPriMap;
};

#undef ENTRY_NOT_IN_USE
#undef NO_RULESET_AVAILABLE


////////////////////////////////////////////////////////////////
// LUL                                                        //
////////////////////////////////////////////////////////////////

LUL::LUL(void (*aLog)(const char*))
{
  mRWlock = new LulRWLock();
  AutoLulRWLocker lock(mRWlock, AutoLulRWLocker::FOR_WRITING);
  mLog = aLog;
  mPriMap = new PriMap(aLog);
  mSegArray = new SegArray();
}


LUL::~LUL()
{
  // The auto-locked section must have its own scope, so that the
  // unlock is performed before the mRWLock is deleted.
  {
    AutoLulRWLocker lock(mRWlock, AutoLulRWLocker::FOR_WRITING);
    for (std::map<pthread_t,CFICache*>::iterator iter = mCaches.begin();
         iter != mCaches.end();
         ++iter) {
      delete iter->second;
    }
    delete mPriMap;
    delete mSegArray;
    mLog = nullptr;
  }
  // Now we don't hold the lock.  Hence it is safe to delete it.
  delete mRWlock;
}


void
LUL::RegisterUnwinderThread()
{
  AutoLulRWLocker lock(mRWlock, AutoLulRWLocker::FOR_WRITING);

  pthread_t me = pthread_self();
  CFICache* cache = new CFICache(mPriMap);

  std::pair<std::map<pthread_t,CFICache*>::iterator, bool> res
    = mCaches.insert(std::pair<pthread_t,CFICache*>(me, cache));
  // "this thread is not already registered"
  MOZ_ASSERT(res.second); // "new element was inserted"
  // Using mozilla::DebugOnly to declare |res| leads to compilation error
  (void)res.second;
}

void
LUL::NotifyAfterMap(uintptr_t aRXavma, size_t aSize,
                    const char* aFileName, const void* aMappedImage)
{
  AutoLulRWLocker lock(mRWlock, AutoLulRWLocker::FOR_WRITING);

  mLog(":\n");
  char buf[200];
  snprintf(buf, sizeof(buf), "NotifyMap %llx %llu %s\n",
           (unsigned long long int)aRXavma, (unsigned long long int)aSize,
           aFileName);
  buf[sizeof(buf)-1] = 0;
  mLog(buf);

  InvalidateCFICaches();

  // Ignore obviously-stupid notifications.
  if (aSize > 0) {

    // Here's a new mapping, for this object.
    SecMap* smap = new SecMap(mLog);

    // Read CFI or EXIDX unwind data into |smap|.
    if (!aMappedImage) {
      (void)lul::ReadSymbolData(
              string(aFileName), std::vector<string>(), smap,
              (void*)aRXavma, aSize, mLog);
    } else {
      (void)lul::ReadSymbolDataInternal(
              (const uint8_t*)aMappedImage,
              string(aFileName), std::vector<string>(), smap,
              (void*)aRXavma, aSize, mLog);
    }

    mLog("NotifyMap .. preparing entries\n");

    smap->PrepareRuleSets(aRXavma, aSize);

    snprintf(buf, sizeof(buf),
             "NotifyMap got %lld entries\n", (long long int)smap->Size());
    buf[sizeof(buf)-1] = 0;
    mLog(buf);

    // Add it to the primary map (the top level set of mapped objects).
    mPriMap->AddSecMap(smap);

    // Tell the segment array about the mapping, so that the stack
    // scan and __kernel_syscall mechanisms know where valid code is.
    mSegArray->add(aRXavma, aRXavma + aSize - 1, true);
  }
}


void
LUL::NotifyExecutableArea(uintptr_t aRXavma, size_t aSize)
{
  AutoLulRWLocker lock(mRWlock, AutoLulRWLocker::FOR_WRITING);

  mLog(":\n");
  char buf[200];
  snprintf(buf, sizeof(buf), "NotifyExecutableArea %llx %llu\n",
           (unsigned long long int)aRXavma, (unsigned long long int)aSize);
  buf[sizeof(buf)-1] = 0;
  mLog(buf);

  InvalidateCFICaches();

  // Ignore obviously-stupid notifications.
  if (aSize > 0) {
    // Tell the segment array about the mapping, so that the stack
    // scan and __kernel_syscall mechanisms know where valid code is.
    mSegArray->add(aRXavma, aRXavma + aSize - 1, true);
  }
}


void
LUL::NotifyBeforeUnmap(uintptr_t aRXavmaMin, uintptr_t aRXavmaMax)
{
  AutoLulRWLocker lock(mRWlock, AutoLulRWLocker::FOR_WRITING);

  mLog(":\n");
  char buf[100];
  snprintf(buf, sizeof(buf), "NotifyUnmap %016llx-%016llx\n",
           (unsigned long long int)aRXavmaMin,
           (unsigned long long int)aRXavmaMax);
  buf[sizeof(buf)-1] = 0;
  mLog(buf);

  MOZ_ASSERT(aRXavmaMin <= aRXavmaMax);

  InvalidateCFICaches();

  // Remove from the primary map, any secondary maps that intersect
  // with the address range.  Also delete the secondary maps.
  mPriMap->RemoveSecMapsInRange(aRXavmaMin, aRXavmaMax);

  // Tell the segment array that the address range no longer
  // contains valid code.
  mSegArray->add(aRXavmaMin, aRXavmaMax, false);

  snprintf(buf, sizeof(buf), "NotifyUnmap: now have %d SecMaps\n",
           (int)mPriMap->CountSecMaps());
  buf[sizeof(buf)-1] = 0;
  mLog(buf);
}


size_t
LUL::CountMappings()
{
  AutoLulRWLocker lock(mRWlock, AutoLulRWLocker::FOR_WRITING);
  return mPriMap->CountSecMaps();
}


static
TaggedUWord DerefTUW(TaggedUWord aAddr, StackImage* aStackImg)
{
  if (!aAddr.Valid()) {
    return TaggedUWord();
  }
  if (aAddr.Value() < aStackImg->mStartAvma) {
    return TaggedUWord();
  }
  if (aAddr.Value() + sizeof(uintptr_t) > aStackImg->mStartAvma
                                          + aStackImg->mLen) {
    return TaggedUWord();
  }
  return TaggedUWord(*(uintptr_t*)(aStackImg->mContents + aAddr.Value()
                                   - aStackImg->mStartAvma));
}

static
TaggedUWord EvaluateReg(int16_t aReg, UnwindRegs* aOldRegs, TaggedUWord aCFA)
{
  switch (aReg) {
    case DW_REG_CFA:       return aCFA;
#if defined(LUL_ARCH_x64) || defined(LUL_ARCH_x86)
    case DW_REG_INTEL_XBP: return aOldRegs->xbp;
    case DW_REG_INTEL_XSP: return aOldRegs->xsp;
    case DW_REG_INTEL_XIP: return aOldRegs->xip;
#elif defined(LUL_ARCH_arm)
    case DW_REG_ARM_R7:    return aOldRegs->r7;
    case DW_REG_ARM_R11:   return aOldRegs->r11;
    case DW_REG_ARM_R12:   return aOldRegs->r12;
    case DW_REG_ARM_R13:   return aOldRegs->r13;
    case DW_REG_ARM_R14:   return aOldRegs->r14;
    case DW_REG_ARM_R15:   return aOldRegs->r15;
#else
# error "Unsupported arch"
#endif
    default: MOZ_ASSERT(0); return TaggedUWord();
  }
}

static
TaggedUWord EvaluateExpr(LExpr aExpr, UnwindRegs* aOldRegs,
                         TaggedUWord aCFA, StackImage* aStackImg)
{
  switch (aExpr.mHow) {
    case LExpr::UNKNOWN:
      return TaggedUWord();
    case LExpr::NODEREF: {
      TaggedUWord tuw = EvaluateReg(aExpr.mReg, aOldRegs, aCFA);
      tuw.Add(TaggedUWord((intptr_t)aExpr.mOffset));
      return tuw;
    }
    case LExpr::DEREF: {
      TaggedUWord tuw = EvaluateReg(aExpr.mReg, aOldRegs, aCFA);
      tuw.Add(TaggedUWord((intptr_t)aExpr.mOffset));
      return DerefTUW(tuw, aStackImg);
    }
    default:
      MOZ_ASSERT(0);
      return TaggedUWord();
  }
}

static
void UseRuleSet(/*MOD*/UnwindRegs* aRegs,
                StackImage* aStackImg, RuleSet* aRS)
{
  // Take a copy of regs, since we'll need to refer to the old values
  // whilst computing the new ones.
  UnwindRegs old_regs = *aRegs;

  // Mark all the current register values as invalid, so that the
  // caller can see, on our return, which ones have been computed
  // anew.  If we don't even manage to compute a new PC value, then
  // the caller will have to abandon the unwind.
  // FIXME: Create and use instead: aRegs->SetAllInvalid();
#if defined(LUL_ARCH_x64) || defined(LUL_ARCH_x86)
  aRegs->xbp = TaggedUWord();
  aRegs->xsp = TaggedUWord();
  aRegs->xip = TaggedUWord();
#elif defined(LUL_ARCH_arm)
  aRegs->r7  = TaggedUWord();
  aRegs->r11 = TaggedUWord();
  aRegs->r12 = TaggedUWord();
  aRegs->r13 = TaggedUWord();
  aRegs->r14 = TaggedUWord();
  aRegs->r15 = TaggedUWord();
#else
#  error "Unsupported arch"
#endif

  // This is generally useful.
  const TaggedUWord inval = TaggedUWord();

  // First, compute the CFA.
  TaggedUWord cfa = EvaluateExpr(aRS->mCfaExpr, &old_regs,
                                 inval/*old cfa*/, aStackImg);

  // If we didn't manage to compute the CFA, well .. that's ungood,
  // but keep going anyway.  It'll be OK provided none of the register
  // value rules mention the CFA.  In any case, compute the new values
  // for each register that we're tracking.

#if defined(LUL_ARCH_x64) || defined(LUL_ARCH_x86)
  aRegs->xbp = EvaluateExpr(aRS->mXbpExpr, &old_regs, cfa, aStackImg);
  aRegs->xsp = EvaluateExpr(aRS->mXspExpr, &old_regs, cfa, aStackImg);
  aRegs->xip = EvaluateExpr(aRS->mXipExpr, &old_regs, cfa, aStackImg);
#elif defined(LUL_ARCH_arm)
  aRegs->r7  = EvaluateExpr(aRS->mR7expr,  &old_regs, cfa, aStackImg);
  aRegs->r11 = EvaluateExpr(aRS->mR11expr, &old_regs, cfa, aStackImg);
  aRegs->r12 = EvaluateExpr(aRS->mR12expr, &old_regs, cfa, aStackImg);
  aRegs->r13 = EvaluateExpr(aRS->mR13expr, &old_regs, cfa, aStackImg);
  aRegs->r14 = EvaluateExpr(aRS->mR14expr, &old_regs, cfa, aStackImg);
  aRegs->r15 = EvaluateExpr(aRS->mR15expr, &old_regs, cfa, aStackImg);
#else
# error "Unsupported arch"
#endif

  // We're done.  Any regs for which we didn't manage to compute a
  // new value will now be marked as invalid.
}

void
LUL::Unwind(/*OUT*/uintptr_t* aFramePCs,
            /*OUT*/uintptr_t* aFrameSPs,
            /*OUT*/size_t* aFramesUsed, 
            /*OUT*/size_t* aScannedFramesAcquired,
            size_t aFramesAvail,
            size_t aScannedFramesAllowed,
            UnwindRegs* aStartRegs, StackImage* aStackImg)
{
  AutoLulRWLocker lock(mRWlock, AutoLulRWLocker::FOR_READING);

  pthread_t me = pthread_self();
  std::map<pthread_t, CFICache*>::iterator iter = mCaches.find(me);

  if (iter == mCaches.end()) {
    // The calling thread is not registered for unwinding.
    MOZ_CRASH();
    return;
  }

  CFICache* cache = iter->second;
  MOZ_ASSERT(cache);

  // Do unwindery, possibly modifying |cache|.

  /////////////////////////////////////////////////////////
  // BEGIN UNWIND

  *aFramesUsed = 0;

  UnwindRegs  regs          = *aStartRegs;
  TaggedUWord last_valid_sp = TaggedUWord();

  // Stack-scan control
  unsigned int n_scanned_frames      = 0;  // # s-s frames recovered so far
  static const int NUM_SCANNED_WORDS = 50; // max allowed scan length

  while (true) {

    if (DEBUG_MAIN) {
      char buf[300];
      mLog("\n");
#if defined(LUL_ARCH_x64) || defined(LUL_ARCH_x86)
      snprintf(buf, sizeof(buf),
               "LoopTop: rip %d/%llx  rsp %d/%llx  rbp %d/%llx\n",
               (int)regs.xip.Valid(), (unsigned long long int)regs.xip.Value(),
               (int)regs.xsp.Valid(), (unsigned long long int)regs.xsp.Value(),
               (int)regs.xbp.Valid(), (unsigned long long int)regs.xbp.Value());
      buf[sizeof(buf)-1] = 0;
      mLog(buf);
#elif defined(LUL_ARCH_arm)
      snprintf(buf, sizeof(buf),
               "LoopTop: r15 %d/%llx  r7 %d/%llx  r11 %d/%llx"
               "  r12 %d/%llx  r13 %d/%llx  r14 %d/%llx\n",
               (int)regs.r15.Valid(), (unsigned long long int)regs.r15.Value(),
               (int)regs.r7.Valid(),  (unsigned long long int)regs.r7.Value(),
               (int)regs.r11.Valid(), (unsigned long long int)regs.r11.Value(),
               (int)regs.r12.Valid(), (unsigned long long int)regs.r12.Value(),
               (int)regs.r13.Valid(), (unsigned long long int)regs.r13.Value(),
               (int)regs.r14.Valid(), (unsigned long long int)regs.r14.Value());
      buf[sizeof(buf)-1] = 0;
      mLog(buf);
#else
# error "Unsupported arch"
#endif
    }

#if defined(LUL_ARCH_x64) || defined(LUL_ARCH_x86)
    TaggedUWord ia = regs.xip;
    TaggedUWord sp = regs.xsp;
#elif defined(LUL_ARCH_arm)
    TaggedUWord ia = (*aFramesUsed == 0 ? regs.r15 : regs.r14);
    TaggedUWord sp = regs.r13;
#else
# error "Unsupported arch"
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
      ia.Add(TaggedUWord((uintptr_t)(-1)));
    }

    RuleSet* ruleset = cache->Lookup(ia.Value());
    if (DEBUG_MAIN) {
      char buf[100];
      snprintf(buf, sizeof(buf), "ruleset for 0x%llx = %p\n",
               (unsigned long long int)ia.Value(), ruleset);
      buf[sizeof(buf)-1] = 0;
      mLog(buf);
    }

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
#if defined(LUL_PLAT_x86_android) || defined(LUL_PLAT_x86_linux)
    if (!ruleset && *aFramesUsed == 1 && ia.Valid() && sp.Valid()) {
      uintptr_t insns_min, insns_max;
      uintptr_t eip = ia.Value();
      bool b = mSegArray->getBoundingCodeSegment(&insns_min, &insns_max, eip);
      if (b && eip - 2 >= insns_min && eip + 3 <= insns_max) {
        uint8_t* eipC = (uint8_t*)eip;
        if (eipC[-2] == 0xCD && eipC[-1] == 0x80 && eipC[0] == 0x5D &&
            eipC[1] == 0x5A && eipC[2] == 0x59 && eipC[3] == 0xC3) {
          TaggedUWord sp_plus_0  = sp;
          TaggedUWord sp_plus_12 = sp;
          TaggedUWord sp_plus_16 = sp;
          sp_plus_12.Add(TaggedUWord(12));
          sp_plus_16.Add(TaggedUWord(16));
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
#endif
    ////
    /////////////////////////////////////////////

    // So, do we have a ruleset for this address?  If so, use it now.
    if (ruleset) {

      if (DEBUG_MAIN) {
        ruleset->Print(mLog); mLog("\n");
      }
      // Use the RuleSet to compute the registers for the previous
      // frame.  |regs| is modified in-place.
      UseRuleSet(&regs, aStackImg, ruleset);

    } else {

      // There's no RuleSet for the specified address, so see if
      // it's possible to get anywhere by stack-scanning.

      // Use stack scanning frugally.
      if (n_scanned_frames++ >= aScannedFramesAllowed) {
        break;
      }

      // We can't scan the stack without a valid, aligned stack pointer.
      if (!sp.IsAligned()) {
        break;
      }

      bool scan_succeeded = false;
      for (int i = 0; i < NUM_SCANNED_WORDS; ++i) {
        TaggedUWord aWord = DerefTUW(sp, aStackImg);
        // aWord is something we fished off the stack.  It should be
        // valid, unless we overran the stack bounds.
        if (!aWord.Valid()) {
          break;
        }

        // Now, does aWord point inside a text section and immediately
        // after something that looks like a call instruction?
        if (mPriMap->MaybeIsReturnPoint(aWord, mSegArray)) {
          // Yes it does.  Update the unwound registers heuristically,
          // using the same schemes as Breakpad does.
          scan_succeeded = true;
          (*aScannedFramesAcquired)++;

#if defined(LUL_ARCH_x64) || defined(LUL_ARCH_x86)
          // The same logic applies for the 32- and 64-bit cases.
          // Register names of the form xsp etc refer to (eg) esp in
          // the 32-bit case and rsp in the 64-bit case.
#         if defined(LUL_ARCH_x64)
          const int wordSize = 8;
#         else
          const int wordSize = 4;
#         endif
          // The return address -- at XSP -- will have been pushed by
          // the CALL instruction.  So the caller's XSP value
          // immediately before and after that CALL instruction is the
          // word above XSP.
          regs.xsp = sp;
          regs.xsp.Add(TaggedUWord(wordSize));

          // aWord points at the return point, so back up one byte
          // to put it in the calling instruction.
          regs.xip = aWord;
          regs.xip.Add(TaggedUWord((uintptr_t)(-1)));

          // Computing a new value from the frame pointer is more tricky.
          if (regs.xbp.Valid() &&
              sp.Valid() && regs.xbp.Value() == sp.Value() - wordSize) {
            // One possibility is that the callee begins with the standard
            // preamble "push %xbp; mov %xsp, %xbp".  In which case, the
            // (1) caller's XBP value will be at the word below XSP, and
            // (2) the current (callee's) XBP will point at that word:
            regs.xbp = DerefTUW(regs.xbp, aStackImg);
          } else if (regs.xbp.Valid() &&
                     sp.Valid() && regs.xbp.Value() >= sp.Value() + wordSize) {
            // If that didn't work out, maybe the callee didn't change
            // XBP, so it still holds the caller's value.  For that to
            // be plausible, XBP will need to have a value at least
            // higher than XSP since that holds the purported return
            // address.  In which case do nothing, since XBP already
            // holds the "right" value.
          } else {
            // Mark XBP as invalid, so that subsequent unwind iterations
            // don't assume it holds valid data.
            regs.xbp = TaggedUWord();
          }

          // Move on to the next word up the stack
          sp.Add(TaggedUWord(wordSize));

#elif defined(LUL_ARCH_arm)
          // Set all registers to be undefined, except for SP(R13) and
          // PC(R15).

          // aWord points either at the return point, if returning to
          // ARM code, or one insn past the return point if returning
          // to Thumb code.  In both cases, aWord-2 is guaranteed to
          // fall within the calling instruction.
          regs.r15 = aWord;
          regs.r15.Add(TaggedUWord((uintptr_t)(-2)));

          // Make SP be the word above the location where the return
          // address was found.
          regs.r13 = sp;
          regs.r13.Add(TaggedUWord(4));

          // All other regs are undefined.
          regs.r7 = regs.r11 = regs.r12 = regs.r14 = TaggedUWord();

          // Move on to the next word up the stack
          sp.Add(TaggedUWord(4));

#else
# error "Unknown plat"
#endif

          break;
        }

      } // for (int i = 0; i < NUM_SCANNED_WORDS; i++)

      // We tried to make progress by scanning the stack, but failed.
      // So give up -- fall out of the top level unwind loop.
      if (!scan_succeeded) {
        break;
      }
    }

  } // top level unwind loop

  // END UNWIND
  /////////////////////////////////////////////////////////
}


void
LUL::InvalidateCFICaches()
{
  // NB: the caller must hold m_rwl for writing.

  // FIXME: this could get expensive.  Use a bool to remember when the
  // caches have been invalidated and hence avoid duplicate invalidations.
  for (std::map<pthread_t,CFICache*>::iterator iter = mCaches.begin();
       iter != mCaches.end();
       ++iter) {
    iter->second->Invalidate();
  }
}


////////////////////////////////////////////////////////////////
// LUL Unit Testing                                           //
////////////////////////////////////////////////////////////////

static const int LUL_UNIT_TEST_STACK_SIZE = 16384;

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
static __attribute__((noinline))
bool GetAndCheckStackTrace(LUL* aLUL, const char* dstring)
{
  // Get hold of the current unwind-start registers.
  UnwindRegs startRegs;
  memset(&startRegs, 0, sizeof(startRegs));
#if defined(LUL_PLAT_x64_linux)
  volatile uintptr_t block[3];
  MOZ_ASSERT(sizeof(block) == 24);
  __asm__ __volatile__(
    "leaq 0(%%rip), %%r15"   "\n\t"
    "movq %%r15, 0(%0)"      "\n\t"
    "movq %%rsp, 8(%0)"      "\n\t"
    "movq %%rbp, 16(%0)"     "\n"
    : : "r"(&block[0]) : "memory", "r15"
  );
  startRegs.xip = TaggedUWord(block[0]);
  startRegs.xsp = TaggedUWord(block[1]);
  startRegs.xbp = TaggedUWord(block[2]);
  const uintptr_t REDZONE_SIZE = 128;
  uintptr_t start = block[1] - REDZONE_SIZE;
#elif defined(LUL_PLAT_x86_linux) || defined(LUL_PLAT_x86_android)
  volatile uintptr_t block[3];
  MOZ_ASSERT(sizeof(block) == 12);
  __asm__ __volatile__(
    ".byte 0xE8,0x00,0x00,0x00,0x00"/*call next insn*/  "\n\t"
    "popl %%edi"             "\n\t"
    "movl %%edi, 0(%0)"      "\n\t"
    "movl %%esp, 4(%0)"      "\n\t"
    "movl %%ebp, 8(%0)"      "\n"
    : : "r"(&block[0]) : "memory", "edi"
  );
  startRegs.xip = TaggedUWord(block[0]);
  startRegs.xsp = TaggedUWord(block[1]);
  startRegs.xbp = TaggedUWord(block[2]);
  const uintptr_t REDZONE_SIZE = 0;
  uintptr_t start = block[1] - REDZONE_SIZE;
#elif defined(LUL_PLAT_arm_android)
  volatile uintptr_t block[6];
  MOZ_ASSERT(sizeof(block) == 24);
  __asm__ __volatile__(
    "mov r0, r15"            "\n\t"
    "str r0,  [%0, #0]"      "\n\t"
    "str r14, [%0, #4]"      "\n\t"
    "str r13, [%0, #8]"      "\n\t"
    "str r12, [%0, #12]"     "\n\t"
    "str r11, [%0, #16]"     "\n\t"
    "str r7,  [%0, #20]"     "\n"
    : : "r"(&block[0]) : "memory", "r0"
  );
  startRegs.r15 = TaggedUWord(block[0]);
  startRegs.r14 = TaggedUWord(block[1]);
  startRegs.r13 = TaggedUWord(block[2]);
  startRegs.r12 = TaggedUWord(block[3]);
  startRegs.r11 = TaggedUWord(block[4]);
  startRegs.r7  = TaggedUWord(block[5]);
  const uintptr_t REDZONE_SIZE = 0;
  uintptr_t start = block[1] - REDZONE_SIZE;
#else
# error "Unsupported platform"
#endif

  // Get hold of the innermost LUL_UNIT_TEST_STACK_SIZE bytes of the
  // stack.
  uintptr_t end = start + LUL_UNIT_TEST_STACK_SIZE;
  uintptr_t ws  = sizeof(void*);
  start &= ~(ws-1);
  end   &= ~(ws-1);
  uintptr_t nToCopy = end - start;
  if (nToCopy > lul::N_STACK_BYTES) {
    nToCopy = lul::N_STACK_BYTES;
  }
  MOZ_ASSERT(nToCopy <= lul::N_STACK_BYTES);
  StackImage* stackImg = new StackImage();
  stackImg->mLen       = nToCopy;
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
  size_t framesUsed  = 0;
  size_t scannedFramesAllowed = 0;
  size_t scannedFramesAcquired = 0;
  aLUL->Unwind( &framePCs[0], &frameSPs[0],
                &framesUsed, &scannedFramesAcquired,
                framesAvail, scannedFramesAllowed,
                &startRegs, stackImg );

  delete stackImg;

  //if (0) {
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
  for (cursor = cursor-2, frameIx = 2;
       cursor >= dstring && frameIx < framesUsed;
       cursor--, frameIx++) {
    char      c  = *cursor;
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
  bool passed = nConsistent+1 == strlen(dstring);

  // Show the results.
  char buf[200];
  snprintf(buf, sizeof(buf), "LULUnitTest:   dstring = %s\n", dstring);
  buf[sizeof(buf)-1] = 0;
  aLUL->mLog(buf);
  snprintf(buf, sizeof(buf),
           "LULUnitTest:     %d consistent, %d in dstring: %s\n",
           (int)nConsistent, (int)strlen(dstring),
           passed ? "PASS" : "FAIL");
  buf[sizeof(buf)-1] = 0;
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

#define GEN_TEST_FN(NAME, FRAMESIZE) \
  bool NAME(LUL* aLUL, const char* strPorig, const char* strP) { \
    volatile char space[FRAMESIZE]; \
    memset((char*)&space[0], 0, sizeof(space)); \
    if (*strP == '\0') { \
      /* We've come to the end of the director string. */ \
      /* Take a stack snapshot. */ \
      return GetAndCheckStackTrace(aLUL, strPorig); \
    } else { \
      /* Recurse onwards.  This is a bit subtle.  The obvious */ \
      /* thing to do here is call onwards directly, from within the */ \
      /* arms of the case statement.  That gives a problem in that */ \
      /* there will be multiple return points inside each function when */ \
      /* unwinding, so it will be difficult to check for consistency */ \
      /* against the director string.  Instead, we make an indirect */ \
      /* call, so as to guarantee that there is only one call site */ \
      /* within each function.  This does assume that the compiler */ \
      /* won't transform it back to the simple direct-call form. */ \
      /* To discourage it from doing so, the call is bracketed with */ \
      /* __asm__ __volatile__ sections so as to make it not-movable. */ \
      bool (*nextFn)(LUL*, const char*, const char*) = NULL; \
      switch (*strP) { \
        case '1': nextFn = TestFn1; break; \
        case '2': nextFn = TestFn2; break; \
        case '3': nextFn = TestFn3; break; \
        case '4': nextFn = TestFn4; break; \
        case '5': nextFn = TestFn5; break; \
        case '6': nextFn = TestFn6; break; \
        case '7': nextFn = TestFn7; break; \
        case '8': nextFn = TestFn8; break; \
        default:  nextFn = TestFn8; break; \
      } \
      __asm__ __volatile__("":::"cc","memory"); \
      bool passed = nextFn(aLUL, strPorig, strP+1); \
      __asm__ __volatile__("":::"cc","memory"); \
      return passed; \
    } \
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
__attribute__((noinline)) void
TestUnw(/*OUT*/int* aNTests, /*OUT*/int*aNTestsPassed,
        LUL* aLUL, const char* dstring)
{
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
    sum += space[i] - 3*i;
  }
  __asm__ __volatile__("" : : "r"(sum));

  // Update the counters.
  (*aNTests)++;
  if (passed) {
    (*aNTestsPassed)++;
  }
}


void
RunLulUnitTests(/*OUT*/int* aNTests, /*OUT*/int*aNTestsPassed, LUL* aLUL)
{
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


} // namespace lul
