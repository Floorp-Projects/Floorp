/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PROCESSOR_LOCAL_DEBUG_INFO_SYMBOLIZER_H_
#define PROCESSOR_LOCAL_DEBUG_INFO_SYMBOLIZER_H_

#include "google_breakpad/processor/stack_frame_symbolizer.h"

#include <map>
#include <vector>

namespace google_breakpad {

class Module;

class LocalDebugInfoSymbolizer : public StackFrameSymbolizer {
 public:
  using StackFrameSymbolizer::SymbolizerResult;
  LocalDebugInfoSymbolizer(const std::vector<string>& debug_dirs) :
      StackFrameSymbolizer(NULL, NULL),
      debug_dirs_(debug_dirs) {}
  virtual ~LocalDebugInfoSymbolizer();

  virtual SymbolizerResult FillSourceLineInfo(const CodeModules* modules,
                                              const SystemInfo* system_info,
                                              StackFrame* stack_frame);

  virtual WindowsFrameInfo* FindWindowsFrameInfo(const StackFrame* frame);

  virtual CFIFrameInfo* FindCFIFrameInfo(const StackFrame* frame);

  // Lie to the stackwalker to short-circuit stack-scanning heuristics.
  virtual bool HasImplementation() { return false; }

 private:
  typedef std::map<string, Module*> SymbolMap;
  SymbolMap symbols_;
  std::vector<string> debug_dirs_;
};

}  // namespace google_breakpad

#endif  // PROCESSOR_LOCAL_DEBUG_INFO_SYMBOLIZER_H_
