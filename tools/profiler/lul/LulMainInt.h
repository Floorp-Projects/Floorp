/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LulMainInt_h
#define LulMainInt_h

#include "PlatformMacros.h"
#include "LulMain.h"  // for TaggedUWord

#include <string>
#include <vector>

#include "mozilla/Assertions.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/HashTable.h"
#include "mozilla/Sprintf.h"

// This file provides an internal interface inside LUL.  If you are an
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
  DW_REG_ARM_R7 = 7,
  DW_REG_ARM_R11 = 11,
  DW_REG_ARM_R12 = 12,
  DW_REG_ARM_R13 = 13,
  DW_REG_ARM_R14 = 14,
  DW_REG_ARM_R15 = 15,
#elif defined(GP_ARCH_arm64)
  // aarch64 registers
  DW_REG_AARCH64_X29 = 29,
  DW_REG_AARCH64_X30 = 30,
  DW_REG_AARCH64_SP = 31,
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
#  error "Unknown arch"
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
      : mOpcode(opcode), mOperand(operand) {}
  explicit PfxInstr(PfxExprOp opcode) : mOpcode(opcode), mOperand(0) {}
  bool operator==(const PfxInstr& other) const {
    return mOpcode == other.mOpcode && mOperand == other.mOperand;
  }
  PfxExprOp mOpcode;
  int32_t mOperand;
};

static_assert(sizeof(PfxInstr) <= 8, "PfxInstr size changed unexpectedly");

// Evaluate the prefix expression whose PfxInstrs start at aPfxInstrs[start].
// In the case of any mishap (stack over/underflow, running off the end of
// the instruction vector, obviously malformed sequences),
// return an invalid TaggedUWord.
// RUNS IN NO-MALLOC CONTEXT
TaggedUWord EvaluatePfxExpr(int32_t start, const UnwindRegs* aOldRegs,
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
  UNKNOWN = 0,  // This LExpr denotes no value.
  NODEREF,      // Value is  (mReg + mOffset).
  DEREF,        // Value is *(mReg + mOffset).
  PFXEXPR       // Value is EvaluatePfxExpr(secMap->mPfxInstrs[mOffset])
};

inline static const char* NameOf_LExprHow(LExprHow how) {
  switch (how) {
    case UNKNOWN:
      return "UNKNOWN";
    case NODEREF:
      return "NODEREF";
    case DEREF:
      return "DEREF";
    case PFXEXPR:
      return "PFXEXPR";
    default:
      return "LExpr-??";
  }
}

struct LExpr {
  // Denotes an expression with no value.
  LExpr() : mHow(UNKNOWN), mReg(0), mOffset(0) {}

  // Denotes any expressible expression.
  LExpr(LExprHow how, int16_t reg, int32_t offset)
      : mHow(how), mReg(reg), mOffset(offset) {
    switch (how) {
      case UNKNOWN:
        MOZ_ASSERT(reg == 0 && offset == 0);
        break;
      case NODEREF:
        break;
      case DEREF:
        break;
      case PFXEXPR:
        MOZ_ASSERT(reg == 0 && offset >= 0);
        break;
      default:
        MOZ_RELEASE_ASSERT(0, "LExpr::LExpr: invalid how");
    }
  }

  // Hash it, carefully looking only at defined parts.
  mozilla::HashNumber hash() const {
    mozilla::HashNumber h = mHow;
    switch (mHow) {
      case UNKNOWN:
        break;
      case NODEREF:
      case DEREF:
        h = mozilla::AddToHash(h, mReg);
        h = mozilla::AddToHash(h, mOffset);
        break;
      case PFXEXPR:
        h = mozilla::AddToHash(h, mOffset);
        break;
      default:
        MOZ_RELEASE_ASSERT(0, "LExpr::hash: invalid how");
    }
    return h;
  }

  // And structural equality.
  bool equals(const LExpr& other) const {
    if (mHow != other.mHow) {
      return false;
    }
    switch (mHow) {
      case UNKNOWN:
        return true;
      case NODEREF:
      case DEREF:
        return mReg == other.mReg && mOffset == other.mOffset;
      case PFXEXPR:
        return mOffset == other.mOffset;
      default:
        MOZ_RELEASE_ASSERT(0, "LExpr::equals: invalid how");
    }
  }

  // Change the offset for an expression that references memory.
  LExpr add_delta(long delta) {
    MOZ_ASSERT(mHow == NODEREF);
    // If this is a non-debug build and the above assertion would have
    // failed, at least return LExpr() so that the machinery that uses
    // the resulting expression fails in a repeatable way.
    return (mHow == NODEREF) ? LExpr(mHow, mReg, mOffset + delta)
                             : LExpr();  // Gone bad
  }

  // Dereference an expression that denotes a memory address.
  LExpr deref() {
    MOZ_ASSERT(mHow == NODEREF);
    // Same rationale as for add_delta().
    return (mHow == NODEREF) ? LExpr(DEREF, mReg, mOffset)
                             : LExpr();  // Gone bad
  }

  // Print a rule for recovery of |aNewReg| whose recovered value
  // is this LExpr.
  std::string ShowRule(const char* aNewReg) const;

  // Evaluate this expression, producing a TaggedUWord.  |aOldRegs|
  // holds register values that may be referred to by the expression.
  // |aCFA| holds the CFA value, if any, that applies.  |aStackImg|
  // contains a chuck of stack that will be consulted if the expression
  // references memory.  |aPfxInstrs| holds the vector of PfxInstrs
  // that will be consulted if this is a PFXEXPR.
  // RUNS IN NO-MALLOC CONTEXT
  TaggedUWord EvaluateExpr(const UnwindRegs* aOldRegs, TaggedUWord aCFA,
                           const StackImage* aStackImg,
                           const vector<PfxInstr>* aPfxInstrs) const;

  // Representation of expressions.  If |mReg| is DW_REG_CFA (-1) then
  // it denotes the CFA.  All other allowed values for |mReg| are
  // nonnegative and are DW_REG_ values.
  LExprHow mHow : 8;
  int16_t mReg;     // A DW_REG_ value
  int32_t mOffset;  // 32-bit signed offset should be more than enough.
};

static_assert(sizeof(LExpr) <= 8, "LExpr size changed unexpectedly");

////////////////////////////////////////////////////////////////
// RuleSet                                                    //
////////////////////////////////////////////////////////////////

// This is platform-dependent.  It describes how to recover the CFA and then
// how to recover the registers for the previous frame.  Such "recipes" are
// specific to particular ranges of machine code, but the associated range
// is not stored in RuleSet, because in general each RuleSet may be used
// for many such range fragments ("extents").  See the comments below for
// Extent and SecMap.
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
  void Print(uintptr_t avma, uintptr_t len, void (*aLog)(const char*)) const;

  // Find the LExpr* for a given DW_REG_ value in this class.
  LExpr* ExprForRegno(DW_REG_NUMBER aRegno);

  // How to compute the CFA.
  LExpr mCfaExpr;
  // How to compute caller register values.  These may reference the
  // value defined by |mCfaExpr|.
#if defined(GP_ARCH_amd64) || defined(GP_ARCH_x86)
  LExpr mXipExpr;  // return address
  LExpr mXspExpr;
  LExpr mXbpExpr;
#elif defined(GP_ARCH_arm)
  LExpr mR15expr;  // return address
  LExpr mR14expr;
  LExpr mR13expr;
  LExpr mR12expr;
  LExpr mR11expr;
  LExpr mR7expr;
#elif defined(GP_ARCH_arm64)
  LExpr mX29expr;  // frame pointer register
  LExpr mX30expr;  // link register
  LExpr mSPexpr;
#elif defined(GP_ARCH_mips64)
  LExpr mPCexpr;
  LExpr mFPexpr;
  LExpr mSPexpr;
#else
#  error "Unknown arch"
#endif

  // Machinery in support of hashing.
  typedef RuleSet Lookup;

  static mozilla::HashNumber hash(RuleSet rs) {
    mozilla::HashNumber h = rs.mCfaExpr.hash();
#if defined(GP_ARCH_amd64) || defined(GP_ARCH_x86)
    h = mozilla::AddToHash(h, rs.mXipExpr.hash());
    h = mozilla::AddToHash(h, rs.mXspExpr.hash());
    h = mozilla::AddToHash(h, rs.mXbpExpr.hash());
#elif defined(GP_ARCH_arm)
    h = mozilla::AddToHash(h, rs.mR15expr.hash());
    h = mozilla::AddToHash(h, rs.mR14expr.hash());
    h = mozilla::AddToHash(h, rs.mR13expr.hash());
    h = mozilla::AddToHash(h, rs.mR12expr.hash());
    h = mozilla::AddToHash(h, rs.mR11expr.hash());
    h = mozilla::AddToHash(h, rs.mR7expr.hash());
#elif defined(GP_ARCH_arm64)
    h = mozilla::AddToHash(h, rs.mX29expr.hash());
    h = mozilla::AddToHash(h, rs.mX30expr.hash());
    h = mozilla::AddToHash(h, rs.mSPexpr.hash());
#elif defined(GP_ARCH_mips64)
    h = mozilla::AddToHash(h, rs.mPCexpr.hash());
    h = mozilla::AddToHash(h, rs.mFPexpr.hash());
    h = mozilla::AddToHash(h, rs.mSPexpr.hash());
#else
#  error "Unknown arch"
#endif
    return h;
  }

  static bool match(const RuleSet& rs1, const RuleSet& rs2) {
    return rs1.mCfaExpr.equals(rs2.mCfaExpr) &&
#if defined(GP_ARCH_amd64) || defined(GP_ARCH_x86)
           rs1.mXipExpr.equals(rs2.mXipExpr) &&
           rs1.mXspExpr.equals(rs2.mXspExpr) &&
           rs1.mXbpExpr.equals(rs2.mXbpExpr);
#elif defined(GP_ARCH_arm)
           rs1.mR15expr.equals(rs2.mR15expr) &&
           rs1.mR14expr.equals(rs2.mR14expr) &&
           rs1.mR13expr.equals(rs2.mR13expr) &&
           rs1.mR12expr.equals(rs2.mR12expr) &&
           rs1.mR11expr.equals(rs2.mR11expr) && rs1.mR7expr.equals(rs2.mR7expr);
#elif defined(GP_ARCH_arm64)
           rs1.mX29expr.equals(rs2.mX29expr) &&
           rs1.mX30expr.equals(rs2.mX30expr) && rs1.mSPexpr.equals(rs2.mSPexpr);
#elif defined(GP_ARCH_mips64)
           rs1.mPCexpr.equals(rs2.mPCexpr) && rs1.mFPexpr.equals(rs2.mFPexpr) &&
           rs1.mSPexpr.equals(rs2.mSPexpr);
#else
#  error "Unknown arch"
#endif
  }
};

// Returns |true| for Dwarf register numbers which are members
// of the set of registers that LUL unwinds on this target.
static inline bool registerIsTracked(DW_REG_NUMBER reg) {
  switch (reg) {
#if defined(GP_ARCH_amd64) || defined(GP_ARCH_x86)
    case DW_REG_INTEL_XBP:
    case DW_REG_INTEL_XSP:
    case DW_REG_INTEL_XIP:
      return true;
#elif defined(GP_ARCH_arm)
    case DW_REG_ARM_R7:
    case DW_REG_ARM_R11:
    case DW_REG_ARM_R12:
    case DW_REG_ARM_R13:
    case DW_REG_ARM_R14:
    case DW_REG_ARM_R15:
      return true;
#elif defined(GP_ARCH_arm64)
    case DW_REG_AARCH64_X29:
    case DW_REG_AARCH64_X30:
    case DW_REG_AARCH64_SP:
      return true;
#elif defined(GP_ARCH_mips64)
    case DW_REG_MIPS_FP:
    case DW_REG_MIPS_SP:
    case DW_REG_MIPS_PC:
      return true;
#else
#  error "Unknown arch"
#endif
    default:
      return false;
  }
}

////////////////////////////////////////////////////////////////
// Extent                                                     //
////////////////////////////////////////////////////////////////

struct Extent {
  // Three fields, which together take 8 bytes.
  uint32_t mOffset;
  uint16_t mLen;
  uint16_t mDictIx;

  // What this means is: suppose we are looking for the unwind rules for some
  // code address (AVMA) `avma`.  If we can find some SecMap `secmap` such
  // that `avma` falls in the range
  //
  //   `[secmap.mMapMinAVMA, secmap.mMapMaxAVMA]`
  //
  // then the RuleSet to use is `secmap.mDictionary[dictIx]` iff we can find
  // an `extent` in `secmap.mExtents` such that `avma` falls into the range
  //
  //   `[secmap.mMapMinAVMA + extent.offset(),
  //     secmap.mMapMinAVMA + extent.offset() + extent.len())`.
  //
  // Packing Extent into the minimum space is important, since there will be
  // huge numbers of Extents -- around 3 million for libxul.so as of Sept
  // 2020.  Here, we aim for an 8-byte size, with the field sizes chosen
  // carefully, as follows:
  //
  // `offset` denotes a byte offset inside the text section for some shared
  // object.  libxul.so is by far the largest.  As of Sept 2020 it has a text
  // size of up to around 120MB, that is, close to 2^27 bytes.  Hence a 32-bit
  // `offset` field gives a safety margin of around a factor of 32
  // (== 2 ^(32 - 27)).
  //
  // `dictIx` indicates a unique `RuleSet` for some code address range.
  // Experimentation on x86_64-linux indicates that only around 300 different
  // `RuleSet`s exist, for libxul.so.  A 16-bit bit field allows up to 65536
  // to be recorded, hence leaving us a generous safety margin.
  //
  // `len` indicates the length of the associated address range.
  //
  // Note the representation becomes unusable if either `offset` overflows 32
  // bits or `dictIx` overflows 16 bits.  On the other hand, it does not
  // matter (although is undesirable) if `len` overflows 16 bits, because in
  // that case we can add multiple size-65535 entries to `secmap.mExtents` to
  // cover the entire range.  Hence the field sizes are biased so as to give a
  // good safety margin for `offset` and `dictIx` at the cost of stealing bits
  // from `len`.  Almost all `len` values we will ever see in practice are
  // 65535 or less, so stealing those bits does not matter much.
  //
  // If further compression is required, it would be feasible to implement
  // Extent using 29 bits for the offset, 8 bits for the length and 11 bits
  // for the dictionary index, giving a total of 6 bytes, provided that the
  // data is packed into 3 uint16_t's.  That would be a bit slower, though,
  // due to the bit packing, and it would be more fragile, in the sense that
  // it would fail for any object with more than 512MB of text segment, or
  // with more than 2048 different `RuleSet`s.  For the current (Sept 2020)
  // libxul.so situation, though, it would work fine.

  Extent(uint32_t offset, uint32_t len, uint32_t dictIx) {
    MOZ_RELEASE_ASSERT(len < (1 << 16));
    MOZ_RELEASE_ASSERT(dictIx < (1 << 16));
    mOffset = offset;
    mLen = len;
    mDictIx = dictIx;
  }
  inline uint32_t offset() const { return mOffset; }
  inline uint32_t len() const { return mLen; }
  inline uint32_t dictIx() const { return mDictIx; }
  void setLen(uint32_t len) {
    MOZ_RELEASE_ASSERT(len < (1 << 16));
    mLen = len;
  }
  void Print(void (*aLog)(const char*)) const {
    char buf[64];
    SprintfLiteral(buf, "Extent(offs=0x%x, len=%u, dictIx=%u)", this->offset(),
                   this->len(), this->dictIx());
    aLog(buf);
  }
};

static_assert(sizeof(Extent) == 8);

////////////////////////////////////////////////////////////////
// SecMap                                                     //
////////////////////////////////////////////////////////////////

// A SecMap may have zero address range, temporarily, whilst RuleSets
// are being added to it.  But adding a zero-range SecMap to a PriMap
// will make it impossible to maintain the total order of the PriMap
// entries, and so that can't be allowed to happen.

class SecMap {
 public:
  // In the constructor, `mapStartAVMA` and `mapLen` define the actual
  // (in-process) virtual addresses covered by the SecMap.  All RuleSets
  // subsequently added to it by calling `AddRuleSet` must fall into this
  // address range, and attempts to add ones outside the range will be
  // ignored.  This restriction exists because the type Extent (see below)
  // indicates an address range for a RuleSet, but for reasons of compactness,
  // it does not contain the start address of the range.  Instead, it contains
  // a 32-bit offset from the base address of the SecMap.  This is also the
  // reason why the map's size is a `uint32_t` and not a `uintptr_t`.
  //
  // The effect is to limit this mechanism to shared objects / executables
  // whose text section size does not exceed 4GB (2^32 bytes).  Given that, as
  // of Sept 2020, libxul.so's text section size is around 120MB, this does
  // not seem like much of a limitation.
  //
  // From the supplied `mapStartAVMA` and `mapLen`, fields `mMapMinAVMA` and
  // `mMapMaxAVMA` are calculated.  It is intended that no two SecMaps owned
  // by the same PriMap contain overlapping address ranges, and the PriMap
  // logic enforces that.
  //
  // Some invariants:
  //
  // mExtents is nonempty
  //    <=> mMapMinAVMA <= mMapMaxAVMA
  //        && mMapMinAVMA <= apply_delta(mExtents[0].offset())
  //        && apply_delta(mExtents[#rulesets-1].offset()
  //             + mExtents[#rulesets-1].len() - 1) <= mMapMaxAVMA
  //        where
  //           apply_delta(off) = off + mMapMinAVMA
  //
  //        This requires that no RuleSet has zero length.
  //
  // mExtents is empty
  //    <=> mMapMinAVMA > mMapMaxAVMA
  //
  // This doesn't constrain mMapMinAVMA and mMapMaxAVMA uniquely, so let's use
  // mMapMinAVMA == 1 and mMapMaxAVMA == 0 to denote this case.

  SecMap(uintptr_t mapStartAVMA, uint32_t mapLen, void (*aLog)(const char*));
  ~SecMap();

  // Binary search mRuleSets to find one that brackets |ia|, or nullptr
  // if none is found.  It's not allowable to do this until PrepareRuleSets
  // has been called first.
  RuleSet* FindRuleSet(uintptr_t ia);

  // Add a RuleSet to the collection.  The rule is copied in.  Calling
  // this makes the map non-searchable.
  void AddRuleSet(const RuleSet* rs, uintptr_t avma, uintptr_t len);

  // Add a PfxInstr to the vector of such instrs, and return the index
  // in the vector.  Calling this makes the map non-searchable.
  uint32_t AddPfxInstr(PfxInstr pfxi);

  // Returns the entire vector of PfxInstrs.
  const vector<PfxInstr>* GetPfxInstrs() { return &mPfxInstrs; }

  // Prepare the map for searching, by sorting it, de-overlapping entries and
  // removing any resulting zero-length entries.  At the start of this
  // routine, all Extents should fall within [mMapMinAVMA, mMapMaxAVMA] and
  // not have zero length, as a result of the checks in AddRuleSet().
  void PrepareRuleSets();

  bool IsEmpty();

  size_t Size() { return mExtents.size() + mDictionary.size(); }

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  // The extent of this SecMap as a whole.  The extents of all contained
  // RuleSets must fall inside this.  See comment above for details.
  uintptr_t mMapMinAVMA;
  uintptr_t mMapMaxAVMA;

 private:
  // False whilst adding entries; true once it is safe to call FindRuleSet.
  // Transition (false->true) is caused by calling PrepareRuleSets().
  bool mUsable;

  // This is used to find and remove duplicate RuleSets while we are adding
  // them to the SecMap.  Almost all RuleSets are duplicates, so de-duping
  // them is a huge space win.  This is non-null while `mUsable` is false, and
  // becomes null (is discarded) after the call to PrepareRuleSets, which
  // copies all the entries into `mDictionary`.
  mozilla::UniquePtr<
      mozilla::HashMap<RuleSet, uint32_t, RuleSet, InfallibleAllocPolicy>>
      mUniqifier;

  // This will contain final contents of `mUniqifier`, but ordered
  // (implicitly) by the `uint32_t` value fields, for fast access.
  vector<RuleSet> mDictionary;

  // A vector of Extents, sorted by offset value, nonoverlapping (post
  // PrepareRuleSets()).
  vector<Extent> mExtents;

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

}  // namespace lul

#endif  // ndef LulMainInt_h
