/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LulMainInt_h
#define LulMainInt_h

#include "PlatformMacros.h"
#include "LulMain.h" // for TaggedUWord

#include <vector>

#include "mozilla/Assertions.h"

// This file is provides internal interface inside LUL.  If you are an
// end-user of LUL, do not include it in your code.  The end-user
// interface is in LulMain.h.


namespace lul {

using std::vector;

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
#if defined(GP_ARCH_arm)
  // ARM registers
  DW_REG_ARM_R7  = 7,
  DW_REG_ARM_R11 = 11,
  DW_REG_ARM_R12 = 12,
  DW_REG_ARM_R13 = 13,
  DW_REG_ARM_R14 = 14,
  DW_REG_ARM_R15 = 15,
#elif defined(GP_ARCH_arm64)
  // aarch64 registers
  DW_REG_AARCH64_X29 = 29,
  DW_REG_AARCH64_X30 = 30,
  DW_REG_AARCH64_SP  = 31,
#elif defined(GP_ARCH_amd64)
  // Because the X86 (32 bit) and AMD64 (64 bit) summarisers are
  // combined, a merged set of register constants is needed.
  DW_REG_INTEL_XBP = 6,
  DW_REG_INTEL_XSP = 7,
  DW_REG_INTEL_XIP = 16,
#elif defined(GP_ARCH_x86)
  DW_REG_INTEL_XBP = 5,
  DW_REG_INTEL_XSP = 4,
  DW_REG_INTEL_XIP = 8,
#elif defined(GP_ARCH_mips64)
  DW_REG_MIPS_SP = 29,
  DW_REG_MIPS_FP = 30,
  DW_REG_MIPS_PC = 34,
#else
# error "Unknown arch"
#endif
};


////////////////////////////////////////////////////////////////
// PfxExpr                                                    //
////////////////////////////////////////////////////////////////

enum PfxExprOp {
  //             meaning of mOperand     effect on stack
  PX_Start,   // bool start-with-CFA?    start, with CFA on stack, or not
  PX_End,     // none                    stop; result is at top of stack
  PX_SImm32,  // int32                   push signed int32
  PX_DwReg,   // DW_REG_NUMBER           push value of the specified reg
  PX_Deref,   // none                    pop X ; push *X
  PX_Add,     // none                    pop X ; pop Y ; push Y + X
  PX_Sub,     // none                    pop X ; pop Y ; push Y - X
  PX_And,     // none                    pop X ; pop Y ; push Y & X
  PX_Or,      // none                    pop X ; pop Y ; push Y | X
  PX_CmpGES,  // none                    pop X ; pop Y ; push (Y >=s X) ? 1 : 0
  PX_Shl      // none                    pop X ; pop Y ; push Y << X
};

struct PfxInstr {
  PfxInstr(PfxExprOp opcode, int32_t operand)
    : mOpcode(opcode)
    , mOperand(operand)
  {}
  explicit PfxInstr(PfxExprOp opcode)
    : mOpcode(opcode)
    , mOperand(0)
  {}
  bool operator==(const PfxInstr& other) const {
    return mOpcode == other.mOpcode && mOperand == other.mOperand;
  }
  PfxExprOp mOpcode;
  int32_t   mOperand;
};

static_assert(sizeof(PfxInstr) <= 8, "PfxInstr size changed unexpectedly");

// Evaluate the prefix expression whose PfxInstrs start at aPfxInstrs[start].
// In the case of any mishap (stack over/underflow, running off the end of
// the instruction vector, obviously malformed sequences),
// return an invalid TaggedUWord.
// RUNS IN NO-MALLOC CONTEXT
TaggedUWord EvaluatePfxExpr(int32_t start,
                            const UnwindRegs* aOldRegs,
                            TaggedUWord aCFA, const StackImage* aStackImg,
                            const vector<PfxInstr>& aPfxInstrs);


////////////////////////////////////////////////////////////////
// LExpr                                                      //
////////////////////////////////////////////////////////////////

// An expression -- very primitive.  Denotes either "register +
// offset", a dereferenced version of the same, or a reference to a
// prefix expression stored elsewhere.  So as to allow convenient
// handling of Dwarf-derived unwind info, the register may also denote
// the CFA.  A large number of these need to be stored, so we ensure
// it fits into 8 bytes.  See comment below on RuleSet to see how
// expressions fit into the bigger picture.

enum LExprHow {
  UNKNOWN=0, // This LExpr denotes no value.
  NODEREF,   // Value is  (mReg + mOffset).
  DEREF,     // Value is *(mReg + mOffset).
  PFXEXPR    // Value is EvaluatePfxExpr(secMap->mPfxInstrs[mOffset])
};

inline static const char* NameOf_LExprHow(LExprHow how) {
  switch (how) {
    case UNKNOWN: return "UNKNOWN";
    case NODEREF: return "NODEREF";
    case DEREF:   return "DEREF";
    case PFXEXPR: return "PFXEXPR";
    default:      return "LExpr-??";
  }
}


struct LExpr {
  // Denotes an expression with no value.
  LExpr()
    : mHow(UNKNOWN)
    , mReg(0)
    , mOffset(0)
  {}

  // Denotes any expressible expression.
  LExpr(LExprHow how, int16_t reg, int32_t offset)
    : mHow(how)
    , mReg(reg)
    , mOffset(offset)
  {
    switch (how) {
      case UNKNOWN: MOZ_ASSERT(reg == 0 && offset == 0); break;
      case NODEREF: break;
      case DEREF:   break;
      case PFXEXPR: MOZ_ASSERT(reg == 0 && offset >= 0); break;
      default:      MOZ_ASSERT(0, "LExpr::LExpr: invalid how");
    }
  }

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

  // Print a rule for recovery of |aNewReg| whose recovered value
  // is this LExpr.
  string ShowRule(const char* aNewReg) const;

  // Evaluate this expression, producing a TaggedUWord.  |aOldRegs|
  // holds register values that may be referred to by the expression.
  // |aCFA| holds the CFA value, if any, that applies.  |aStackImg|
  // contains a chuck of stack that will be consulted if the expression
  // references memory.  |aPfxInstrs| holds the vector of PfxInstrs
  // that will be consulted if this is a PFXEXPR.
  // RUNS IN NO-MALLOC CONTEXT
  TaggedUWord EvaluateExpr(const UnwindRegs* aOldRegs,
                           TaggedUWord aCFA, const StackImage* aStackImg,
                           const vector<PfxInstr>* aPfxInstrs) const;

  // Representation of expressions.  If |mReg| is DW_REG_CFA (-1) then
  // it denotes the CFA.  All other allowed values for |mReg| are
  // nonnegative and are DW_REG_ values.
  LExprHow mHow:8;
  int16_t  mReg;    // A DW_REG_ value
  int32_t  mOffset; // 32-bit signed offset should be more than enough.
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
  void   Print(void(*aLog)(const char*)) const;

  // Find the LExpr* for a given DW_REG_ value in this class.
  LExpr* ExprForRegno(DW_REG_NUMBER aRegno);

  uintptr_t mAddr;
  uintptr_t mLen;
  // How to compute the CFA.
  LExpr  mCfaExpr;
  // How to compute caller register values.  These may reference the
  // value defined by |mCfaExpr|.
#if defined(GP_ARCH_amd64) || defined(GP_ARCH_x86)
  LExpr  mXipExpr; // return address
  LExpr  mXspExpr;
  LExpr  mXbpExpr;
#elif defined(GP_ARCH_arm)
  LExpr  mR15expr; // return address
  LExpr  mR14expr;
  LExpr  mR13expr;
  LExpr  mR12expr;
  LExpr  mR11expr;
  LExpr  mR7expr;
#elif defined(GP_ARCH_arm64)
  LExpr  mX29expr; // frame pointer register
  LExpr  mX30expr; // link register
  LExpr  mSPexpr;
#elif defined(GP_ARCH_mips64)
  LExpr  mPCexpr;
  LExpr  mFPexpr;
  LExpr  mSPexpr;
#else
#   error "Unknown arch"
#endif
};

// Returns |true| for Dwarf register numbers which are members
// of the set of registers that LUL unwinds on this target.
static inline bool registerIsTracked(DW_REG_NUMBER reg) {
  switch (reg) {
#   if defined(GP_ARCH_amd64) || defined(GP_ARCH_x86)
    case DW_REG_INTEL_XBP: case DW_REG_INTEL_XSP: case DW_REG_INTEL_XIP:
      return true;
#   elif defined(GP_ARCH_arm)
    case DW_REG_ARM_R7:  case DW_REG_ARM_R11: case DW_REG_ARM_R12:
    case DW_REG_ARM_R13: case DW_REG_ARM_R14: case DW_REG_ARM_R15:
      return true;
#   elif defined(GP_ARCH_arm64)
    case DW_REG_AARCH64_X29:  case DW_REG_AARCH64_X30: case DW_REG_AARCH64_SP:
      return true;
#elif defined(GP_ARCH_mips64)
    case DW_REG_MIPS_FP:  case DW_REG_MIPS_SP: case DW_REG_MIPS_PC:
      return true;
#   else
#     error "Unknown arch"
#   endif
    default:
      return false;
  }
}


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
  void AddRuleSet(const RuleSet* rs);

  // Add a PfxInstr to the vector of such instrs, and return the index
  // in the vector.  Calling this makes the map non-searchable.
  uint32_t AddPfxInstr(PfxInstr pfxi);

  // Returns the entire vector of PfxInstrs.
  const vector<PfxInstr>* GetPfxInstrs() { return &mPfxInstrs; }

  // Prepare the map for searching.  Also, remove any rules for code
  // address ranges which don't fall inside [start, +len).  |len| may
  // not be zero.
  void PrepareRuleSets(uintptr_t start, size_t len);

  bool IsEmpty();

  size_t Size() { return mRuleSets.size(); }

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  // The min and max addresses of the addresses in the contained
  // RuleSets.  See comment above for invariants.
  uintptr_t mSummaryMinAddr;
  uintptr_t mSummaryMaxAddr;

private:
  // False whilst adding entries; true once it is safe to call FindRuleSet.
  // Transition (false->true) is caused by calling PrepareRuleSets().
  bool mUsable;

  // A vector of RuleSets, sorted, nonoverlapping (post Prepare()).
  vector<RuleSet> mRuleSets;

  // A vector of PfxInstrs, which are referred to by the RuleSets.
  // These are provided as a representation of Dwarf expressions
  // (DW_CFA_val_expression, DW_CFA_expression, DW_CFA_def_cfa_expression),
  // are relatively expensive to evaluate, and and are therefore
  // expected to be used only occasionally.
  //
  // The vector holds a bunch of separate PfxInstr programs, each one
  // starting with a PX_Start and terminated by a PX_End, all
  // concatenated together.  When a RuleSet can't recover a value
  // using a self-contained LExpr, it uses a PFXEXPR whose mOffset is
  // the index in this vector of start of the necessary PfxInstr program.
  vector<PfxInstr> mPfxInstrs;

  // A logging sink, for debugging.
  void (*mLog)(const char*);
};

} // namespace lul

#endif // ndef LulMainInt_h
