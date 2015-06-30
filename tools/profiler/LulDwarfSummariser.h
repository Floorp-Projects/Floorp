/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LulDwarfSummariser_h
#define LulDwarfSummariser_h

#include "LulMainInt.h"

namespace lul {

class Summariser
{
public:
  Summariser(SecMap* aSecMap, uintptr_t aTextBias, void(*aLog)(const char*));

  virtual void Entry(uintptr_t aAddress, uintptr_t aLength);
  virtual void End();

  // Tell the summariser that the value for |aNewReg| at |aAddress| is
  // recovered using the LExpr that can be constructed using the
  // components |how|, |oldReg| and |offset|.  The summariser will
  // inspect the components and may reject them for various reasons,
  // but the hope is that it will find them acceptable and record this
  // rule permanently.
  virtual void Rule(uintptr_t aAddress, int aNewReg,
                    LExprHow how, int16_t oldReg, int64_t offset);

  virtual uint32_t AddPfxInstr(PfxInstr pfxi);

  // Send output to the logging sink, for debugging.
  virtual void Log(const char* str) { mLog(str); }
  
private:
  // The SecMap in which we park the finished summaries (RuleSets) and
  // also any PfxInstrs derived from Dwarf expressions.
  SecMap* mSecMap;

  // Running state for the current summary (RuleSet) under construction.
  RuleSet mCurrRules;

  // The start of the address range to which the RuleSet under
  // construction applies.
  uintptr_t mCurrAddr;

  // The highest address, plus one, for which the RuleSet under
  // construction could possibly apply.  If there are no further
  // incoming events then mCurrRules will eventually be emitted
  // as-is, for the range mCurrAddr.. mMax1Addr - 1, if that is
  // nonempty.
  uintptr_t mMax1Addr;

  // The bias value (to add to the SVMAs, to get AVMAs) to be used
  // when adding entries into mSecMap.
  uintptr_t mTextBias;

  // A logging sink, for debugging.
  void (*mLog)(const char* aFmt);
};

} // namespace lul

#endif // LulDwarfSummariser_h
