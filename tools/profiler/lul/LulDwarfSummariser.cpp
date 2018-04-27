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

// Do |s64|'s lowest 32 bits sign extend back to |s64| itself?
static inline bool fitsIn32Bits(int64 s64) {
  return s64 == ((s64 & 0xffffffff) ^ 0x80000000) - 0x80000000;
}

// Check a LExpr prefix expression, starting at pfxInstrs[start] up to
// the next PX_End instruction, to ensure that:
// * It only mentions registers that are tracked on this target
// * The start point is sane
// If the expression is ok, return NULL.  Else return a pointer
// a const char* holding a bit of text describing the problem.
static const char*
checkPfxExpr(const vector<PfxInstr>* pfxInstrs, int64_t start)
{
  size_t nInstrs = pfxInstrs->size();
  if (start < 0 || start >= (ssize_t)nInstrs) {
    return "bogus start point";
  }
  size_t i;
  for (i = start; i < nInstrs; i++) {
    PfxInstr pxi = (*pfxInstrs)[i];
    if (pxi.mOpcode == PX_End)
      break;
    if (pxi.mOpcode == PX_DwReg &&
        !registerIsTracked((DW_REG_NUMBER)pxi.mOperand)) {
      return "uses untracked reg";
    }
  }
  return nullptr; // success
}


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
    SprintfLiteral(buf,
                   "LUL Entry(%llx, %llu)\n",
                   (unsigned long long int)aAddress,
                   (unsigned long long int)aLength);
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
Summariser::Rule(uintptr_t aAddress, int aNewReg,
                 LExprHow how, int16_t oldReg, int64_t offset)
{
  aAddress += mTextBias;
  if (DEBUG_SUMMARISER) {
    char buf[100];
    if (how == NODEREF || how == DEREF) {
      bool deref = how == DEREF;
      SprintfLiteral(buf,
                     "LUL  0x%llx  old-r%d = %sr%d + %lld%s\n",
                     (unsigned long long int)aAddress, aNewReg,
                     deref ? "*(" : "", (int)oldReg, (long long int)offset,
                     deref ? ")" : "");
    } else if (how == PFXEXPR) {
      SprintfLiteral(buf,
                     "LUL  0x%llx  old-r%d = pfx-expr-at %lld\n",
                     (unsigned long long int)aAddress, aNewReg,
                     (long long int)offset);
    } else {
      SprintfLiteral(buf,
                     "LUL  0x%llx  old-r%d = (invalid LExpr!)\n",
                     (unsigned long long int)aAddress, aNewReg);
    }
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

  // If for some reason summarisation fails, either or both of these
  // become non-null and point at constant text describing the
  // problem.  Using two rather than just one avoids complications of
  // having to concatenate two strings to produce a complete error message.
  const char* reason1 = nullptr;
  const char* reason2 = nullptr;

  // |offset| needs to be a 32 bit value that sign extends to 64 bits
  // on a 64 bit target.  We will need to incorporate |offset| into
  // any LExpr made here.  So we may as well check it right now.
  if (!fitsIn32Bits(offset)) {
    reason1 = "offset not in signed 32-bit range";
    goto cant_summarise;
  }

  // FIXME: factor out common parts of the arch-dependent summarisers.

#if defined(GP_ARCH_arm)

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
      if (how != NODEREF) {
        reason1 = "rule for DW_REG_CFA: invalid |how|";
        goto cant_summarise;
      }
      switch (oldReg) {
        case DW_REG_ARM_R7:  case DW_REG_ARM_R11:
        case DW_REG_ARM_R12: case DW_REG_ARM_R13:
          break;
        default:
          reason1 = "rule for DW_REG_CFA: invalid |oldReg|";
          goto cant_summarise;
      }
      mCurrRules.mCfaExpr = LExpr(how, oldReg, offset);
      break;

    case DW_REG_ARM_R7:  case DW_REG_ARM_R11: case DW_REG_ARM_R12:
    case DW_REG_ARM_R13: case DW_REG_ARM_R14: case DW_REG_ARM_R15: {
      // This is a new rule for R7, R11, R12, R13 (SP), R14 (LR) or
      // R15 (the return address).
      switch (how) {
        case NODEREF: case DEREF:
          // Check the old register is one we're tracking.
          if (!registerIsTracked((DW_REG_NUMBER)oldReg) &&
              oldReg != DW_REG_CFA) {
            reason1 = "rule for R7/11/12/13/14/15: uses untracked reg";
            goto cant_summarise;
          }
          break;
        case PFXEXPR: {
          // Check that the prefix expression only mentions tracked registers.
          const vector<PfxInstr>* pfxInstrs = mSecMap->GetPfxInstrs();
          reason2 = checkPfxExpr(pfxInstrs, offset);
          if (reason2) {
            reason1 = "rule for R7/11/12/13/14/15: ";
            goto cant_summarise;
          }
          break;
        }
        default:
          goto cant_summarise;
      }
      LExpr expr = LExpr(how, oldReg, offset);
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
      // Leave |reason1| and |reason2| unset here.  This program point
      // is reached so often that it causes a flood of "Can't
      // summarise" messages.  In any case, we don't really care about
      // the fact that this summary would produce a new value for a
      // register that we're not tracking.  We do on the other hand
      // care if the summary's expression *uses* a register that we're
      // not tracking.  But in that case one of the above failures
      // should tell us which.
      goto cant_summarise;
  }

  // Mark callee-saved registers (r4 .. r11) as unchanged, if there is
  // no other information about them.  FIXME: do this just once, at
  // the point where the ruleset is committed.
  if (mCurrRules.mR7expr.mHow == UNKNOWN) {
    mCurrRules.mR7expr = LExpr(NODEREF, DW_REG_ARM_R7, 0);
  }
  if (mCurrRules.mR11expr.mHow == UNKNOWN) {
    mCurrRules.mR11expr = LExpr(NODEREF, DW_REG_ARM_R11, 0);
  }
  if (mCurrRules.mR12expr.mHow == UNKNOWN) {
    mCurrRules.mR12expr = LExpr(NODEREF, DW_REG_ARM_R12, 0);
  }

  // The old r13 (SP) value before the call is always the same as the
  // CFA.
  mCurrRules.mR13expr = LExpr(NODEREF, DW_REG_CFA, 0);

  // If there's no information about R15 (the return address), say
  // it's a copy of R14 (the link register).
  if (mCurrRules.mR15expr.mHow == UNKNOWN) {
    mCurrRules.mR15expr = LExpr(NODEREF, DW_REG_ARM_R14, 0);
  }

#elif defined(GP_ARCH_arm64)

  // ----------------- arm64 ----------------- //

  switch (aNewReg) {
    case DW_REG_CFA:
      if (how != NODEREF) {
        reason1 = "rule for DW_REG_CFA: invalid |how|";
        goto cant_summarise;
      }
      switch (oldReg) {
        case DW_REG_AARCH64_X29:
        case DW_REG_AARCH64_SP:
          break;
        default:
          reason1 = "rule for DW_REG_CFA: invalid |oldReg|";
          goto cant_summarise;
      }
      mCurrRules.mCfaExpr = LExpr(how, oldReg, offset);
      break;

    case DW_REG_AARCH64_X29:
    case DW_REG_AARCH64_X30:
    case DW_REG_AARCH64_SP: {
      switch (how) {
        case NODEREF:
        case DEREF:
          // Check the old register is one we're tracking.
          if (!registerIsTracked((DW_REG_NUMBER)oldReg) &&
              oldReg != DW_REG_CFA) {
            reason1 = "rule for X29/X30/SP: uses untracked reg";
            goto cant_summarise;
          }
          break;
        case PFXEXPR: {
          // Check that the prefix expression only mentions tracked registers.
          const vector<PfxInstr>* pfxInstrs = mSecMap->GetPfxInstrs();
          reason2 = checkPfxExpr(pfxInstrs, offset);
          if (reason2) {
            reason1 = "rule for X29/X30/SP: ";
            goto cant_summarise;
          }
          break;
        }
        default:
          goto cant_summarise;
      }
      LExpr expr = LExpr(how, oldReg, offset);
      switch (aNewReg) {
        case DW_REG_AARCH64_X29: mCurrRules.mX29expr = expr; break;
        case DW_REG_AARCH64_X30: mCurrRules.mX30expr = expr; break;
        case DW_REG_AARCH64_SP:  mCurrRules.mSPexpr  = expr; break;
        default: MOZ_ASSERT(0);
      }
      break;
    }
    default:
     // Leave |reason1| and |reason2| unset here, for the reasons explained
     // in the analogous point
     goto cant_summarise;
  }

  if (mCurrRules.mX29expr.mHow == UNKNOWN) {
    mCurrRules.mX29expr = LExpr(NODEREF, DW_REG_AARCH64_X29, 0);
  }
  if (mCurrRules.mX30expr.mHow == UNKNOWN) {
    mCurrRules.mX30expr = LExpr(NODEREF, DW_REG_AARCH64_X30, 0);
  }
  // On aarch64, it seems the old SP value before the call is always the
  // same as the CFA.  Therefore, in the absence of any other way to
  // recover the SP, specify that the CFA should be copied.
  if (mCurrRules.mSPexpr.mHow == UNKNOWN) {
    mCurrRules.mSPexpr = LExpr(NODEREF, DW_REG_CFA, 0);
  }
#elif defined(GP_ARCH_amd64) || defined(GP_ARCH_x86)

  // ---------------- x64/x86 ---------------- //

  // Now, can we add the rule to our summary?  This depends on whether
  // the registers and the overall expression are representable.  This
  // is the heart of the summarisation process.
  switch (aNewReg) {

    case DW_REG_CFA: {
      // This is a rule that defines the CFA.  The only forms we choose to
      // represent are: = SP+offset, = FP+offset, or =prefix-expr.
      switch (how) {
        case NODEREF:
          if (oldReg != DW_REG_INTEL_XSP && oldReg != DW_REG_INTEL_XBP) {
            reason1 = "rule for DW_REG_CFA: invalid |oldReg|";
            goto cant_summarise;
          }
          break;
        case DEREF:
          reason1 = "rule for DW_REG_CFA: invalid |how|";
          goto cant_summarise;
        case PFXEXPR: {
          // Check that the prefix expression only mentions tracked registers.
          const vector<PfxInstr>* pfxInstrs = mSecMap->GetPfxInstrs();
          reason2 = checkPfxExpr(pfxInstrs, offset);
          if (reason2) {
            reason1 = "rule for CFA: ";
            goto cant_summarise;
          }
          break;
        }
        default:
          goto cant_summarise;
      }
      mCurrRules.mCfaExpr = LExpr(how, oldReg, offset);
      break;
    }

    case DW_REG_INTEL_XSP: case DW_REG_INTEL_XBP: case DW_REG_INTEL_XIP: {
      // This is a new rule for XSP, XBP or XIP (the return address).
      switch (how) {
        case NODEREF: case DEREF:
          // Check the old register is one we're tracking.
          if (!registerIsTracked((DW_REG_NUMBER)oldReg) &&
              oldReg != DW_REG_CFA) {
            reason1 = "rule for XSP/XBP/XIP: uses untracked reg";
            goto cant_summarise;
          }
          break;
        case PFXEXPR: {
          // Check that the prefix expression only mentions tracked registers.
          const vector<PfxInstr>* pfxInstrs = mSecMap->GetPfxInstrs();
          reason2 = checkPfxExpr(pfxInstrs, offset);
          if (reason2) {
            reason1 = "rule for XSP/XBP/XIP: ";
            goto cant_summarise;
          }
          break;
        }
        default:
          goto cant_summarise;
      }
      LExpr expr = LExpr(how, oldReg, offset);
      switch (aNewReg) {
        case DW_REG_INTEL_XBP: mCurrRules.mXbpExpr = expr; break;
        case DW_REG_INTEL_XSP: mCurrRules.mXspExpr = expr; break;
        case DW_REG_INTEL_XIP: mCurrRules.mXipExpr = expr; break;
        default: MOZ_CRASH("impossible value for aNewReg");
      }
      break;
    }

    default:
      // Leave |reason1| and |reason2| unset here, for the reasons
      // explained in the analogous point in the ARM case just above.
      goto cant_summarise;

  }

  // On Intel, it seems the old SP value before the call is always the
  // same as the CFA.  Therefore, in the absence of any other way to
  // recover the SP, specify that the CFA should be copied.
  if (mCurrRules.mXspExpr.mHow == UNKNOWN) {
    mCurrRules.mXspExpr = LExpr(NODEREF, DW_REG_CFA, 0);
  }

  // Also, gcc says "Undef" for BP when it is unchanged.
  if (mCurrRules.mXbpExpr.mHow == UNKNOWN) {
    mCurrRules.mXbpExpr = LExpr(NODEREF, DW_REG_INTEL_XBP, 0);
  }

#elif defined(GP_ARCH_mips64)
  // ---------------- mips ---------------- //
  //
  // Now, can we add the rule to our summary?  This depends on whether
  // the registers and the overall expression are representable.  This
  // is the heart of the summarisation process.
   switch (aNewReg) {

    case DW_REG_CFA:
      // This is a rule that defines the CFA.  The only forms we can
      // represent are: = SP+offset or = FP+offset.
      if (how != NODEREF) {
        reason1 = "rule for DW_REG_CFA: invalid |how|";
        goto cant_summarise;
      }
      if (oldReg != DW_REG_MIPS_SP && oldReg != DW_REG_MIPS_FP) {
        reason1 = "rule for DW_REG_CFA: invalid |oldReg|";
        goto cant_summarise;
      }
      mCurrRules.mCfaExpr = LExpr(how, oldReg, offset);
      break;

   case DW_REG_MIPS_SP: case DW_REG_MIPS_FP: case DW_REG_MIPS_PC: {
      // This is a new rule for SP, FP or PC (the return address).
      switch (how) {
        case NODEREF: case DEREF:
          // Check the old register is one we're tracking.
          if (!registerIsTracked((DW_REG_NUMBER)oldReg) &&
              oldReg != DW_REG_CFA) {
            reason1 = "rule for SP/FP/PC: uses untracked reg";
            goto cant_summarise;
          }
          break;
        case PFXEXPR: {
          // Check that the prefix expression only mentions tracked registers.
          const vector<PfxInstr>* pfxInstrs = mSecMap->GetPfxInstrs();
          reason2 = checkPfxExpr(pfxInstrs, offset);
          if (reason2) {
            reason1 = "rule for SP/FP/PC: ";
            goto cant_summarise;
          }
          break;
        }
        default:
          goto cant_summarise;
      }
      LExpr expr = LExpr(how, oldReg, offset);
      switch (aNewReg) {
        case DW_REG_MIPS_FP: mCurrRules.mFPexpr = expr; break;
        case DW_REG_MIPS_SP: mCurrRules.mSPexpr = expr; break;
        case DW_REG_MIPS_PC: mCurrRules.mPCexpr = expr; break;
        default: MOZ_CRASH("impossible value for aNewReg");
      }
      break;
    }
    default:
      // Leave |reason1| and |reason2| unset here, for the reasons
      // explained in the analogous point in the ARM case just above.
      goto cant_summarise;
  }

  // On MIPS, it seems the old SP value before the call is always the
  // same as the CFA.  Therefore, in the absence of any other way to
  // recover the SP, specify that the CFA should be copied.
  if (mCurrRules.mSPexpr.mHow == UNKNOWN) {
    mCurrRules.mSPexpr = LExpr(NODEREF, DW_REG_CFA, 0);
  }

  // Also, gcc says "Undef" for FP when it is unchanged.
  if (mCurrRules.mFPexpr.mHow == UNKNOWN) {
    mCurrRules.mFPexpr = LExpr(NODEREF, DW_REG_MIPS_FP, 0);
  }

#else

# error "Unsupported arch"
#endif

  return;

 cant_summarise:
  if (reason1 || reason2) {
    char buf[200];
    SprintfLiteral(buf, "LUL  can't summarise: "
                        "SVMA=0x%llx: %s%s, expr=LExpr(%s,%u,%lld)\n",
                   (unsigned long long int)(aAddress - mTextBias),
                   reason1 ? reason1 : "", reason2 ? reason2 : "",
                   NameOf_LExprHow(how),
                   (unsigned int)oldReg, (long long int)offset);
    mLog(buf);
  }
}

uint32_t
Summariser::AddPfxInstr(PfxInstr pfxi)
{
  return mSecMap->AddPfxInstr(pfxi);
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
