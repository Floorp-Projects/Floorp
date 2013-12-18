/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LulDwarfSummariser.h"

#include "mozilla/Assertions.h"

// Set this to 1 for verbose logging
#define DEBUG_SUMMARISER 0

namespace lul {

Summariser::Summariser(SecMap* aSecMap, uintptr_t aTextBias,
                       void(*aLog)(const char*))
  : mSecMap(aSecMap)
  , mTextBias(aTextBias)
  , mLog(aLog)
{
  mCurrAddr = 0;
  mMax1Addr = 0; // Gives an empty range.

  // Initialise the running RuleSet to "haven't got a clue" status.
  new (&mCurrRules) RuleSet();
}

void
Summariser::Entry(uintptr_t aAddress, uintptr_t aLength)
{
  aAddress += mTextBias;
  if (DEBUG_SUMMARISER) {
    char buf[100];
    snprintf(buf, sizeof(buf), "LUL Entry(%llx, %llu)\n",
             (unsigned long long int)aAddress,
             (unsigned long long int)aLength);
    buf[sizeof(buf)-1] = 0;
    mLog(buf);
  }
  // This throws away any previous summary, that is, assumes
  // that the previous summary, if any, has been properly finished
  // by a call to End().
  mCurrAddr = aAddress;
  mMax1Addr = aAddress + aLength;
  new (&mCurrRules) RuleSet();
}

void
Summariser::Rule(uintptr_t aAddress,
                 int aNewReg, int aOldReg, intptr_t aOffset, bool aDeref)
{
  aAddress += mTextBias;
  if (DEBUG_SUMMARISER) {
    char buf[100];
    snprintf(buf, sizeof(buf),
             "LUL  0x%llx  old-r%d = %sr%d + %ld%s\n",
             (unsigned long long int)aAddress, aNewReg,
             aDeref ? "*(" : "", aOldReg, (long)aOffset, aDeref ? ")" : "");
    buf[sizeof(buf)-1] = 0;
    mLog(buf);
  }
  if (mCurrAddr < aAddress) {
    // Flush the existing summary first.
    mCurrRules.mAddr = mCurrAddr;
    mCurrRules.mLen  = aAddress - mCurrAddr;
    mSecMap->AddRuleSet(&mCurrRules);
    if (DEBUG_SUMMARISER) {
      mLog("LUL  "); mCurrRules.Print(mLog);
      mLog("\n");
    }
    mCurrAddr = aAddress;
  }

  // FIXME: factor out common parts of the arch-dependent summarisers.

#if defined(LUL_ARCH_arm)

  // ----------------- arm ----------------- //

  // Now, can we add the rule to our summary?  This depends on whether
  // the registers and the overall expression are representable.  This
  // is the heart of the summarisation process.
  switch (aNewReg) {

    case DW_REG_CFA:
      // This is a rule that defines the CFA.  The only forms we
      // choose to represent are: r7/11/12/13 + offset.  The offset
      // must fit into 32 bits since 'uintptr_t' is 32 bit on ARM,
      // hence there is no need to check it for overflow.
      if (aDeref) {
        goto cant_summarise;
      }
      switch (aOldReg) {
        case DW_REG_ARM_R7:  case DW_REG_ARM_R11:
        case DW_REG_ARM_R12: case DW_REG_ARM_R13:
          break;
        default:
          goto cant_summarise;
      }
      mCurrRules.mCfaExpr = LExpr(LExpr::NODEREF, aOldReg, aOffset);
      break;

    case DW_REG_ARM_R7:  case DW_REG_ARM_R11: case DW_REG_ARM_R12:
    case DW_REG_ARM_R13: case DW_REG_ARM_R14: case DW_REG_ARM_R15: {
      // Check the aOldReg is valid.
      switch (aOldReg) {
        case DW_REG_CFA:
        case DW_REG_ARM_R7:  case DW_REG_ARM_R11: case DW_REG_ARM_R12:
        case DW_REG_ARM_R13: case DW_REG_ARM_R14: case DW_REG_ARM_R15:
          break;
        default:
          goto cant_summarise;
      }
      // This is a new rule for one of r{7,11,12,13,14,15} and has a
      // representable offset.  In particular the new value of r15 is
      // going to be the return address.
      LExpr expr = LExpr(aDeref ? LExpr::DEREF : LExpr::NODEREF,
                         aOldReg, aOffset);
      switch (aNewReg) {
        case DW_REG_ARM_R7:  mCurrRules.mR7expr  = expr; break;
        case DW_REG_ARM_R11: mCurrRules.mR11expr = expr; break;
        case DW_REG_ARM_R12: mCurrRules.mR12expr = expr; break;
        case DW_REG_ARM_R13: mCurrRules.mR13expr = expr; break;
        case DW_REG_ARM_R14: mCurrRules.mR14expr = expr; break;
        case DW_REG_ARM_R15: mCurrRules.mR15expr = expr; break;
        default: MOZ_ASSERT(0);
      }
      break;
    }

    default:
      goto cant_summarise;
  }

  // Mark callee-saved registers (r4 .. r11) as unchanged, if there is
  // no other information about them.  FIXME: do this just once, at
  // the point where the ruleset is committed.
  if (mCurrRules.mR7expr.mHow == LExpr::UNKNOWN) {
    mCurrRules.mR7expr = LExpr(LExpr::NODEREF, DW_REG_ARM_R7, 0);
  }
  if (mCurrRules.mR11expr.mHow == LExpr::UNKNOWN) {
    mCurrRules.mR11expr = LExpr(LExpr::NODEREF, DW_REG_ARM_R11, 0);
  }
  if (mCurrRules.mR12expr.mHow == LExpr::UNKNOWN) {
    mCurrRules.mR12expr = LExpr(LExpr::NODEREF, DW_REG_ARM_R12, 0);
  }

  // The old r13 (SP) value before the call is always the same as the
  // CFA.
  mCurrRules.mR13expr = LExpr(LExpr::NODEREF, DW_REG_CFA, 0);

  // If there's no information about R15 (the return address), say
  // it's a copy of R14 (the link register).
  if (mCurrRules.mR15expr.mHow == LExpr::UNKNOWN) {
    mCurrRules.mR15expr = LExpr(LExpr::NODEREF, DW_REG_ARM_R14, 0);
  }

#elif defined(LUL_ARCH_x64) || defined(LUL_ARCH_x86)

  // ---------------- x64/x86 ---------------- //

  // Now, can we add the rule to our summary?  This depends on whether
  // the registers and the overall expression are representable.  This
  // is the heart of the summarisation process.  In the 64 bit case
  // we need to check that aOffset will fit into an int32_t.  In the
  // 32 bit case it is expected that the compiler will fold out the
  // test since it always succeeds.
  if (aNewReg == DW_REG_CFA) {
    // This is a rule that defines the CFA.  The only forms we can
    // represent are: = SP+offset or = FP+offset.
    if (!aDeref && aOffset == (intptr_t)(int32_t)aOffset &&
        (aOldReg == DW_REG_INTEL_XSP || aOldReg == DW_REG_INTEL_XBP)) {
      mCurrRules.mCfaExpr = LExpr(LExpr::NODEREF, aOldReg, aOffset);
    } else {
      goto cant_summarise;
    }
  }
  else
  if ((aNewReg == DW_REG_INTEL_XSP ||
       aNewReg == DW_REG_INTEL_XBP || aNewReg == DW_REG_INTEL_XIP) &&
      (aOldReg == DW_REG_CFA ||
       aOldReg == DW_REG_INTEL_XSP ||
       aOldReg == DW_REG_INTEL_XBP || aOldReg == DW_REG_INTEL_XIP) &&
      aOffset == (intptr_t)(int32_t)aOffset) {
    // This is a new rule for SP, BP or the return address
    // respectively, and has a representable offset.
    LExpr expr = LExpr(aDeref ? LExpr::DEREF : LExpr::NODEREF,
                       aOldReg, aOffset);
    switch (aNewReg) {
      case DW_REG_INTEL_XBP: mCurrRules.mXbpExpr = expr; break;
      case DW_REG_INTEL_XSP: mCurrRules.mXspExpr = expr; break;
      case DW_REG_INTEL_XIP: mCurrRules.mXipExpr = expr; break;
      default: MOZ_CRASH("impossible value for aNewReg");
    }
  }
  else {
    goto cant_summarise;
  }

  // On Intel, it seems the old SP value before the call is always the
  // same as the CFA.  Therefore, in the absence of any other way to
  // recover the SP, specify that the CFA should be copied.
  if (mCurrRules.mXspExpr.mHow == LExpr::UNKNOWN) {
    mCurrRules.mXspExpr = LExpr(LExpr::NODEREF, DW_REG_CFA, 0);
  }

  // Also, gcc says "Undef" for BP when it is unchanged.
  if (mCurrRules.mXbpExpr.mHow == LExpr::UNKNOWN) {
    mCurrRules.mXbpExpr = LExpr(LExpr::NODEREF, DW_REG_INTEL_XBP, 0);
  }

#else

# error "Unsupported arch"
#endif

  return;
 cant_summarise:
  if (0) {
    mLog("LUL  can't summarise\n");
  }
}

void
Summariser::End()
{
  if (DEBUG_SUMMARISER) {
    mLog("LUL End\n");
  }
  if (mCurrAddr < mMax1Addr) {
    mCurrRules.mAddr = mCurrAddr;
    mCurrRules.mLen  = mMax1Addr - mCurrAddr;
    mSecMap->AddRuleSet(&mCurrRules);
    if (DEBUG_SUMMARISER) {
      mLog("LUL  "); mCurrRules.Print(mLog);
      mLog("\n");
    }
  }
}

} // namespace lul
