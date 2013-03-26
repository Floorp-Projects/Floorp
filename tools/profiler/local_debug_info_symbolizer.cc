
#include "PlatformMacros.h"
#include "nsAutoPtr.h"

#if !defined(SPS_OS_windows)
# include "common/module.h"
# include "processor/cfi_frame_info.h"
#endif
#include "google_breakpad/processor/code_module.h"
#include "google_breakpad/processor/code_modules.h"
#include "google_breakpad/processor/stack_frame.h"
#include "processor/logging.h"

#if defined(SPS_PLAT_amd64_linux) || defined(SPS_PLAT_arm_android) \
    || defined(SPS_PLAT_x86_linux) || defined(SPS_PLAT_x86_android)
# include "common/linux/dump_symbols.h"
#elif defined(SPS_PLAT_amd64_darwin) || defined(SPS_PLAT_x86_darwin)
# include "shim_mac_dump_syms.h"
#elif defined(SPS_OS_windows)
  /* This is all stubbed out anyway, so don't do anything. */
#else
# error "Unknown platform"
#endif

#include "platform.h"
#include "local_debug_info_symbolizer.h"

namespace google_breakpad {

LocalDebugInfoSymbolizer::~LocalDebugInfoSymbolizer() {
# if !defined(SPS_OS_windows)
  for (SymbolMap::iterator it = symbols_.begin();
       it != symbols_.end();
       ++it) {
    delete it->second;
  }
# endif
}

StackFrameSymbolizer::SymbolizerResult
LocalDebugInfoSymbolizer::FillSourceLineInfo(const CodeModules* modules,
                                             const SystemInfo* system_info,
                                             StackFrame* frame) {
  if (!modules) {
    return kError;
  }
  const CodeModule* module = modules->GetModuleForAddress(frame->instruction);
  if (!module) {
    return kError;
  }
  frame->module = module;

# if !defined(SPS_OS_windows)
  Module* debug_info_module = NULL;
  SymbolMap::const_iterator it = symbols_.find(module->code_file());
  if (it == symbols_.end()) {
    if (no_symbol_modules_.find(module->code_file()) !=
        no_symbol_modules_.end()) {
      return kNoError;
    }
    LOG("BPUnw:");
    LOGF("BPUnw: ReadSymbolData: BEGIN   %s", module->code_file().c_str());
    if (!ReadSymbolData(module->code_file(),
                        debug_dirs_,
                        ONLY_CFI,
                        &debug_info_module)) {
      BPLOG(ERROR) << "ReadSymbolData failed for " << module->code_file();
      LOGF("BPUnw: ReadSymbolData: FAIL    %s", module->code_file().c_str());
      if (debug_info_module)
        delete debug_info_module;
      no_symbol_modules_.insert(module->code_file());
      return kNoError;
    }

    LOGF("BPUnw: ReadSymbolData: SUCCESS %s", module->code_file().c_str());
    symbols_[module->code_file()] = debug_info_module;
  } else {
    debug_info_module = it->second;
  }

  u_int64_t address = frame->instruction - frame->module->base_address();
  Module::Function* function =
      debug_info_module->FindFunctionByAddress(address);
  if (function) {
    frame->function_name = function->name;
    //TODO: line info: function->lines
  } else {
    Module::Extern* ex = debug_info_module->FindExternByAddress(address);
    if (ex) {
      frame->function_name = ex->name;
    }
  }
# endif /* !defined(SPS_OS_windows) */
  return kNoError;
}


WindowsFrameInfo* LocalDebugInfoSymbolizer::FindWindowsFrameInfo(
    const StackFrame* frame) {
  // Not currently implemented, would require PDBSourceLineWriter to
  // implement an API to return symbol data.
  return NULL;
}

#if !defined(SPS_OS_windows)
// Taken wholesale from source_line_resolver_base.cc
bool ParseCFIRuleSet(const string& rule_set, CFIFrameInfo* frame_info) {
  CFIFrameInfoParseHandler handler(frame_info);
  CFIRuleParser parser(&handler);
  return parser.Parse(rule_set);
}

static void ConvertCFI(const UniqueString* name, const Module::Expr& rule,
                       CFIFrameInfo* frame_info) {
  if (name == ustr__ZDcfa()) frame_info->SetCFARule(rule);
  else if (name == ustr__ZDra()) frame_info->SetRARule(rule);
  else frame_info->SetRegisterRule(name, rule);
}


static void ConvertCFI(const Module::RuleMap& rule_map,
                       CFIFrameInfo* frame_info) {
  for (Module::RuleMap::const_iterator it = rule_map.begin();
       it != rule_map.end(); ++it) {
    ConvertCFI(it->first, it->second, frame_info);
  }
}
#endif

CFIFrameInfo* LocalDebugInfoSymbolizer::FindCFIFrameInfo(
    const StackFrame* frame) {
#if defined(SPS_OS_windows)
  return NULL;
#else
  if (!frame || !frame->module) return NULL;

  SymbolMap::const_iterator it = symbols_.find(frame->module->code_file());
  if (it == symbols_.end()) return NULL;

  Module* module = it->second;
  u_int64_t address = frame->instruction - frame->module->base_address();
  Module::StackFrameEntry* entry =
      module->FindStackFrameEntryByAddress(address);
  if (!entry)
    return NULL;

  //TODO: can we cache this data per-address? does that make sense?
  // TODO: Maybe this should use google_breakpad::scoped_ptr, since we're in
  // "namespace google_breakpad". Not using scoped_ptr currently, because its
  // header triggers build warnings -- see bug 855010.
  nsAutoPtr<CFIFrameInfo> rules(new CFIFrameInfo());
  ConvertCFI(entry->initial_rules, rules);
  for (Module::RuleChangeMap::const_iterator delta_it =
           entry->rule_changes.begin();
       delta_it != entry->rule_changes.end() && delta_it->first < address;
       ++delta_it) {
    ConvertCFI(delta_it->second, rules);
  }
  return rules.forget();
#endif /* defined(SPS_OS_windows) */
}

}  // namespace google_breakpad
