/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if XP_WIN && HAVE_64BIT_BUILD

#include "MozStackFrameSymbolizer.h"

#include "MinidumpAnalyzerUtils.h"

#include "processor/cfi_frame_info.h"

#include <iostream>
#include <sstream>
#include <fstream>

namespace CrashReporter {

  extern MinidumpAnalyzerOptions gMinidumpAnalyzerOptions;

  using google_breakpad::CFIFrameInfo;

MozStackFrameSymbolizer::MozStackFrameSymbolizer() :
  StackFrameSymbolizer(nullptr, nullptr)
{
}

MozStackFrameSymbolizer::SymbolizerResult
MozStackFrameSymbolizer::FillSourceLineInfo(const CodeModules* modules,
                                            const CodeModules* unloaded_modules,
                                            const SystemInfo* system_info,
                                            StackFrame* stack_frame)
{
  SymbolizerResult ret = StackFrameSymbolizer::FillSourceLineInfo(
    modules, unloaded_modules, system_info, stack_frame);

  if (ret == kNoError && this->HasImplementation() &&
    stack_frame->function_name.empty()) {
    // Breakpad's Stackwalker::InstructionAddressSeemsValid only considers an
    // address valid if it has associated symbols.
    //
    // This makes sense for complete & accurate symbols, but ours may be
    // incomplete or wrong. Returning a function name tells Breakpad we
    // recognize this address as code, so it's OK to use in stack scanning.
    // This function is only called with addresses that land in this module.
    //
    // This allows us to fall back to stack scanning in the case where we were
    // unable to provide CFI.
    stack_frame->function_name = "<unknown code>";
  }
  return ret;
}

CFIFrameInfo*
MozStackFrameSymbolizer::FindCFIFrameInfo(const StackFrame* frame)
{
  std::string modulePath;

  // For unit testing, support loading a specified module instead of
  // the real one.
  bool moduleHasBeenReplaced = false;
  if (gMinidumpAnalyzerOptions.forceUseModule.size() > 0) {
    modulePath = gMinidumpAnalyzerOptions.forceUseModule;
    moduleHasBeenReplaced = true;
  } else {
    if (!frame->module) {
      return nullptr;
    }
    modulePath = frame->module->code_file();
  }

  // Get/create the unwind parser.
  auto itMod = mModuleMap.find(modulePath);
  std::shared_ptr<ModuleUnwindParser> unwindParser;
  if (itMod != mModuleMap.end()) {
    unwindParser = itMod->second;
  } else {
    unwindParser.reset(new ModuleUnwindParser(modulePath));
    mModuleMap[modulePath] = unwindParser;
  }

  UnwindCFI cfi;
  DWORD offsetAddr;

  if (moduleHasBeenReplaced) {
    // If we are replacing a module, addresses will never line up.
    // So just act like the 1st entry is correct.
    offsetAddr = unwindParser->GetAnyOffsetAddr();
  } else {
    offsetAddr = frame->instruction - frame->module->base_address();
  }

  if (!unwindParser->GetCFI(offsetAddr, cfi)) {
    return nullptr;
  }

  std::unique_ptr<CFIFrameInfo> rules(new CFIFrameInfo());

  static const size_t exprSize = 50;
  char expr[exprSize];
  if (cfi.stackSize == 0) {
    snprintf(expr, exprSize, "$rsp");
  } else {
    snprintf(expr, exprSize, "$rsp %d +", cfi.stackSize);
  }
  rules->SetCFARule(expr);

  if (cfi.ripOffset == 0) {
    snprintf(expr, exprSize, ".cfa ^");
  } else {
    snprintf(expr, exprSize, ".cfa %d - ^", cfi.ripOffset);
  }
  rules->SetRARule(expr);

  return rules.release();
}

} // namespace CrashReporter

#endif // XP_WIN && HAVE_64BIT_BUILD
