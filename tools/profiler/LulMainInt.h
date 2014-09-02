/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LulMainInt_h
#define LulMainInt_h

#include "LulPlatformMacros.h"

#include <vector>

#include "mozilla/Assertions.h"

// This file is provides internal interface inside LUL.  If you are an
// end-user of LUL, do not include it in your code.  The end-user
// interface is in LulMain.h.


namespace lul {

////////////////////////////////////////////////////////////////
// DW_REG_ constants                                          //
////////////////////////////////////////////////////////////////

// These are the Dwarf CFI register numbers, as (presumably) defined
// in the ELF ABI supplements for each architecture.

enum DW_REG_NUMBER {
  // No real register has this number.  It's convenient to be able to
  // treat the CFA (Canonical Frame Address) as "just another
  // register", though.
  DW_REG_CFA = -1,
#if defined(LUL_ARCH_arm)
  // ARM registers
  DW_REG_ARM_R7  = 7,
  DW_REG_ARM_R11 = 11,
  DW_REG_ARM_R12 = 12,
  DW_REG_ARM_R13 = 13,
  DW_REG_ARM_R14 = 14,
  DW_REG_ARM_R15 = 15,
#elif defined(LUL_ARCH_x64)
  // Because the X86 (32 bit) and AMD64 (64 bit) summarisers are
  // combined, a merged set of register constants is needed.
  DW_REG_INTEL_XBP = 6,
  DW_REG_INTEL_XSP = 7,
  DW_REG_INTEL_XIP = 16,
#elif defined(LUL_ARCH_x86)
  DW_REG_INTEL_XBP = 5,
  DW_REG_INTEL_XSP = 4,
  DW_REG_INTEL_XIP = 8,
#else
# error "Unknown arch"
#endif
};


////////////////////////////////////////////////////////////////
// LExpr                                                      //
////////////////////////////////////////////////////////////////

// An expression -- very primitive.  Denotes either "register +
// offset" or a dereferenced version of the same.  So as to allow
// convenient handling of Dwarf-derived unwind info, the register may
// also denote the CFA.  A large number of these need to be stored, so
// we ensure it fits into 8 bytes.  See comment below on RuleSet to
// see how expressions fit into the bigger picture.

struct LExpr {
  // Denotes an expression with no value.
  LExpr()
    : mHow(UNKNOWN)
    , mReg(0)
    , mOffset(0)
  {}

  // Denotes any expressible expression.
  LExpr(uint8_t how, int16_t reg, int32_t offset)
    : mHow(how)
    , mReg(reg)
    , mOffset(offset)
  {}

  // Change the offset for an expression that references memory.
  LExpr add_delta(long delta)
  {
    MOZ_ASSERT(mHow == NODEREF);
    // If this is a non-debug build and the above assertion would have
    // failed, at least return LExpr() so that the machinery that uses
    // the resulting expression fails in a repeatable way.
    return (mHow == NODEREF) ? LExpr(mHow, mReg, mOffset+delta)
                             : LExpr(); // Gone bad
  }

  // Dereference an expression that denotes a memory address.
  LExpr deref()
  {
    MOZ_ASSERT(mHow == NODEREF);
    // Same rationale as for add_delta().
    return (mHow == NODEREF) ? LExpr(DEREF, mReg, mOffset)
                             : LExpr(); // Gone bad
  }

  // Representation of expressions.  If |mReg| is DW_REG_CFA (-1) then
  // it denotes the CFA.  All other allowed values for |mReg| are
  // nonnegative and are DW_REG_ values.

  enum { UNKNOWN=0, // This LExpr denotes no value.
         NODEREF,   // Value is  (mReg + mOffset).
         DEREF };   // Value is *(mReg + mOffset).

  uint8_t mHow;    // UNKNOWN, NODEREF or DEREF
  int16_t mReg;    // A DW_REG_ value
  int32_t mOffset; // 32-bit signed offset should be more than enough.
};

static_assert(sizeof(LExpr) <= 8, "LExpr size changed unexpectedly");


////////////////////////////////////////////////////////////////
// RuleSet                                                    //
////////////////////////////////////////////////////////////////

// This is platform-dependent.  For some address range, describes how
// to recover the CFA and then how to recover the registers for the
// previous frame.
//
// The set of LExprs contained in a given RuleSet describe a DAG which
// says how to compute the caller's registers ("new registers") from
// the callee's registers ("old registers").  The DAG can contain a
// single internal node, which is the value of the CFA for the callee.
// It would be possible to construct a DAG that omits the CFA, but
// including it makes the summarisers simpler, and the Dwarf CFI spec
// has the CFA as a central concept.
//
// For this to make sense, |mCfaExpr| can't have
// |mReg| == DW_REG_CFA since we have no previous value for the CFA.
// All of the other |Expr| fields can -- and usually do -- specify
// |mReg| == DW_REG_CFA.
//
// With that in place, the unwind algorithm proceeds as follows.
//
// (0) Initially: we have values for the old registers, and a memory
//     image.
//
// (1) Compute the CFA by evaluating |mCfaExpr|.  Add the computed
//     value to the set of "old registers".
//
// (2) Compute values for the registers by evaluating all of the other
//     |Expr| fields in the RuleSet.  These can depend on both the old
//     register values and the just-computed CFA.
//
// If we are unwinding without computing a CFA, perhaps because the
// RuleSets are derived from EXIDX instead of Dwarf, then
// |mCfaExpr.mHow| will be LExpr::UNKNOWN, so the computed value will
// be invalid -- that is, TaggedUWord() -- and so any attempt to use
// that will result in the same value.  But that's OK because the
// RuleSet would make no sense if depended on the CFA but specified no
// way to compute it.
//
// A RuleSet is not allowed to cover zero address range.  Having zero
// length would break binary searching in SecMaps and PriMaps.

class RuleSet {
public:
  RuleSet();
  void   Print(void(*aLog)(const char*));

  // Find the LExpr* for a given DW_REG_ value in this class.
  LExpr* ExprForRegno(DW_REG_NUMBER aRegno);

  uintptr_t mAddr;
  uintptr_t mLen;
  // How to compute the CFA.
  LExpr  mCfaExpr;
  // How to compute caller register values.  These may reference the
  // value defined by |mCfaExpr|.
#if defined(LUL_ARCH_x64) || defined(LUL_ARCH_x86)
  LExpr  mXipExpr; // return address
  LExpr  mXspExpr;
  LExpr  mXbpExpr;
#elif defined(LUL_ARCH_arm)
  LExpr  mR15expr; // return address
  LExpr  mR14expr;
  LExpr  mR13expr;
  LExpr  mR12expr;
  LExpr  mR11expr;
  LExpr  mR7expr;
#else
#   error "Unknown arch"
#endif
};


////////////////////////////////////////////////////////////////
// SecMap                                                     //
////////////////////////////////////////////////////////////////

// A SecMap may have zero address range, temporarily, whilst RuleSets
// are being added to it.  But adding a zero-range SecMap to a PriMap
// will make it impossible to maintain the total order of the PriMap
// entries, and so that can't be allowed to happen.

class SecMap {
public:
  // These summarise the contained mRuleSets, in that they give
  // exactly the lowest and highest addresses that any of the entries
  // in this SecMap cover.  Hence invariants:
  //
  // mRuleSets is nonempty
  //    <=> mSummaryMinAddr <= mSummaryMaxAddr
  //        && mSummaryMinAddr == mRuleSets[0].mAddr
  //        && mSummaryMaxAddr == mRuleSets[#rulesets-1].mAddr
  //                              + mRuleSets[#rulesets-1].mLen - 1;
  //
  // This requires that no RuleSet has zero length.
  //
  // mRuleSets is empty
  //    <=> mSummaryMinAddr > mSummaryMaxAddr
  //
  // This doesn't constrain mSummaryMinAddr and mSummaryMaxAddr uniquely,
  // so let's use mSummaryMinAddr == 1 and mSummaryMaxAddr == 0 to denote
  // this case.

  explicit SecMap(void(*aLog)(const char*));
  ~SecMap();

  // Binary search mRuleSets to find one that brackets |ia|, or nullptr
  // if none is found.  It's not allowable to do this until PrepareRuleSets
  // has been called first.
  RuleSet* FindRuleSet(uintptr_t ia);

  // Add a RuleSet to the collection.  The rule is copied in.  Calling
  // this makes the map non-searchable.
  void AddRuleSet(RuleSet* rs);

  // Prepare the map for searching.  Also, remove any rules for code
  // address ranges which don't fall inside [start, +len).  |len| may
  // not be zero.
  void PrepareRuleSets(uintptr_t start, size_t len);

  bool IsEmpty();

  size_t Size() { return mRuleSets.size(); }

  // The min and max addresses of the addresses in the contained
  // RuleSets.  See comment above for invariants.
  uintptr_t mSummaryMinAddr;
  uintptr_t mSummaryMaxAddr;

private:
  // False whilst adding entries; true once it is safe to call FindRuleSet.
  // Transition (false->true) is caused by calling PrepareRuleSets().
  bool mUsable;

  // A vector of RuleSets, sorted, nonoverlapping (post Prepare()).
  std::vector<RuleSet> mRuleSets;

  // A logging sink, for debugging.
  void (*mLog)(const char*);
};

} // namespace lul

#endif // ndef LulMainInt_h
