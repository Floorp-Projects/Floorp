/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MozStackFrameSymbolizer_h
#define MozStackFrameSymbolizer_h

#if XP_WIN && HAVE_64BIT_BUILD

#include "Win64ModuleUnwindMetadata.h"

#include "google_breakpad/processor/stack_frame_symbolizer.h"
#include "google_breakpad/processor/stack_frame.h"

#include <memory>

namespace CrashReporter {

using google_breakpad::CodeModule;
using google_breakpad::CodeModules;
using google_breakpad::SourceLineResolverInterface;
using google_breakpad::StackFrame;
using google_breakpad::StackFrameSymbolizer;
using google_breakpad::SymbolSupplier;
using google_breakpad::SystemInfo;

class MozStackFrameSymbolizer : public StackFrameSymbolizer {
  using google_breakpad::StackFrameSymbolizer::SymbolizerResult;

  std::map<std::string, std::shared_ptr<ModuleUnwindParser>> mModuleMap;

public:
  MozStackFrameSymbolizer();

  virtual SymbolizerResult FillSourceLineInfo(const CodeModules* modules,
    const CodeModules* unloaded_modules,
    const SystemInfo* system_info,
    StackFrame* stack_frame);

  virtual class google_breakpad::CFIFrameInfo* FindCFIFrameInfo(
    const StackFrame* frame);
};

} // namespace CrashReporter

#endif // XP_WIN && HAVE_64BIT_BUILD

#endif // MozStackFrameSymbolizer_h
