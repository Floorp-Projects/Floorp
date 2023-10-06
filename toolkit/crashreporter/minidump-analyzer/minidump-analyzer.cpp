/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdio>
#include <cstring>
#include <string>
#include <sstream>

#include "json/json.h"
#include "google_breakpad/processor/basic_source_line_resolver.h"
#include "google_breakpad/processor/call_stack.h"
#include "google_breakpad/processor/code_module.h"
#include "google_breakpad/processor/code_modules.h"
#include "google_breakpad/processor/minidump.h"
#include "google_breakpad/processor/minidump_processor.h"
#include "google_breakpad/processor/process_state.h"
#include "google_breakpad/processor/stack_frame.h"
#include "processor/pathname_stripper.h"

#include "mozilla/FStream.h"
#include "mozilla/Unused.h"

#if defined(XP_WIN)

#  include <windows.h>
#  include "mozilla/glue/WindowsDllServices.h"

#elif defined(XP_UNIX) || defined(XP_MACOSX)

#  include <sys/types.h>
#  include <sys/stat.h>
#  include <unistd.h>

#endif

#include "MinidumpAnalyzerUtils.h"

#if XP_WIN && HAVE_64BIT_BUILD && defined(_M_X64)
#  include "MozStackFrameSymbolizer.h"
#endif

namespace CrashReporter {

#if defined(XP_WIN)

static mozilla::glue::BasicDllServices gDllServices;

#endif

using std::hex;
using std::ios;
using std::ios_base;
using std::map;
using std::showbase;
using std::string;
using std::stringstream;
using std::wstring;

using google_breakpad::BasicSourceLineResolver;
using google_breakpad::CallStack;
using google_breakpad::CodeModule;
using google_breakpad::CodeModules;
using google_breakpad::Minidump;
using google_breakpad::MinidumpProcessor;
using google_breakpad::PathnameStripper;
using google_breakpad::ProcessResult;
using google_breakpad::ProcessState;
using google_breakpad::StackFrame;

using mozilla::IFStream;
using mozilla::OFStream;
using mozilla::Unused;

MinidumpAnalyzerOptions gMinidumpAnalyzerOptions;

// Path of the minidump to be analyzed.
static string gMinidumpPath;

struct ModuleCompare {
  bool operator()(const CodeModule* aLhs, const CodeModule* aRhs) const {
    return aLhs->base_address() < aRhs->base_address();
  }
};

typedef map<const CodeModule*, unsigned int, ModuleCompare> OrderedModulesMap;

static void AddModulesFromCallStack(OrderedModulesMap& aOrderedModules,
                                    const CallStack* aStack) {
  int frameCount = aStack->frames()->size();

  for (int frameIndex = 0; frameIndex < frameCount; ++frameIndex) {
    const StackFrame* frame = aStack->frames()->at(frameIndex);

    if (frame->module) {
      aOrderedModules.insert(
          std::pair<const CodeModule*, unsigned int>(frame->module, 0));
    }
  }
}

static void PopulateModuleList(const ProcessState& aProcessState,
                               OrderedModulesMap& aOrderedModules,
                               bool aFullStacks) {
  int threadCount = aProcessState.threads()->size();
  int requestingThread = aProcessState.requesting_thread();

  if (!aFullStacks && (requestingThread != -1)) {
    AddModulesFromCallStack(aOrderedModules,
                            aProcessState.threads()->at(requestingThread));
  } else {
    for (int threadIndex = 0; threadIndex < threadCount; ++threadIndex) {
      AddModulesFromCallStack(aOrderedModules,
                              aProcessState.threads()->at(threadIndex));
    }
  }

  int moduleCount = 0;
  for (auto& itr : aOrderedModules) {
    itr.second = moduleCount++;
  }
}

static const char kExtraDataExtension[] = ".extra";

static string ToHex(uint64_t aValue) {
  stringstream output;

  output << hex << showbase << aValue;

  return output.str();
}

// Convert the stack frame trust value into a readable string.

static string FrameTrust(const StackFrame::FrameTrust aTrust) {
  switch (aTrust) {
    case StackFrame::FRAME_TRUST_NONE:
      return "none";
    case StackFrame::FRAME_TRUST_SCAN:
      return "scan";
    case StackFrame::FRAME_TRUST_CFI_SCAN:
      return "cfi_scan";
    case StackFrame::FRAME_TRUST_FP:
      return "frame_pointer";
    case StackFrame::FRAME_TRUST_CFI:
      return "cfi";
    case StackFrame::FRAME_TRUST_PREWALKED:
      return "prewalked";
    case StackFrame::FRAME_TRUST_CONTEXT:
      return "context";
  }

  return "none";
}

// Convert the result value of the minidump processing step into a readable
// string.

static string ResultString(ProcessResult aResult) {
  switch (aResult) {
    case google_breakpad::PROCESS_OK:
      return "OK";
    case google_breakpad::PROCESS_ERROR_MINIDUMP_NOT_FOUND:
      return "ERROR_MINIDUMP_NOT_FOUND";
    case google_breakpad::PROCESS_ERROR_NO_MINIDUMP_HEADER:
      return "ERROR_NO_MINIDUMP_HEADER";
    case google_breakpad::PROCESS_ERROR_NO_THREAD_LIST:
      return "ERROR_NO_THREAD_LIST";
    case google_breakpad::PROCESS_ERROR_GETTING_THREAD:
      return "ERROR_GETTING_THREAD";
    case google_breakpad::PROCESS_ERROR_GETTING_THREAD_ID:
      return "ERROR_GETTING_THREAD_ID";
    case google_breakpad::PROCESS_ERROR_DUPLICATE_REQUESTING_THREADS:
      return "ERROR_DUPLICATE_REQUESTING_THREADS";
    case google_breakpad::PROCESS_SYMBOL_SUPPLIER_INTERRUPTED:
      return "SYMBOL_SUPPLIER_INTERRUPTED";
    default:
      return "";
  }
}

// Convert the list of stack frames to JSON and append them to the array
// specified in the |aNode| parameter.

static void ConvertStackToJSON(const ProcessState& aProcessState,
                               const OrderedModulesMap& aOrderedModules,
                               const CallStack* aStack, Json::Value& aNode) {
  int frameCount = aStack->frames()->size();

  for (int frameIndex = 0; frameIndex < frameCount; ++frameIndex) {
    const StackFrame* frame = aStack->frames()->at(frameIndex);
    Json::Value frameNode;

    if (frame->module) {
      const auto& itr = aOrderedModules.find(frame->module);

      if (itr != aOrderedModules.end()) {
        frameNode["module_index"] = (*itr).second;
      }
    }

    frameNode["trust"] = FrameTrust(frame->trust);
    // The 'ip' field is equivalent to socorro's 'offset' field
    frameNode["ip"] = ToHex(frame->instruction);

    aNode.append(frameNode);
  }
}

// Extract the list of certifications subjects from the list of modules and
// store it in the |aCertSubjects| parameter

static void RetrieveCertSubjects(const CodeModules* modules,
                                 Json::Value& aCertSubjects) {
#if defined(XP_WIN)
  if (modules) {
    for (size_t i = 0; i < modules->module_count(); i++) {
      const CodeModule* module = modules->GetModuleAtIndex(i);
      auto certSubject = gDllServices.GetBinaryOrgName(
          UTF8ToWide(module->code_file()).c_str());
      if (certSubject) {
        string strSubject(WideToUTF8(certSubject.get()));
        // Json::Value::operator[] creates and returns a null member if the key
        // does not exist.
        Json::Value& subjectNode = aCertSubjects[strSubject];
        if (!subjectNode) {
          // If the member is null, we want to convert that to an array.
          subjectNode = Json::Value(Json::arrayValue);
        }

        // Now we're guaranteed that subjectNode is an array. Add the new entry.
        subjectNode.append(PathnameStripper::File(module->code_file()));
      }
    }
  }
#endif  // defined(XP_WIN)
}

// Convert the list of modules to JSON and append them to the array specified
// in the |aNode| parameter.

static int ConvertModulesToJSON(const ProcessState& aProcessState,
                                const OrderedModulesMap& aOrderedModules,
                                Json::Value& aNode) {
  const CodeModules* modules = aProcessState.modules();

  if (!modules) {
    return -1;
  }

  uint64_t mainAddress = 0;
  const CodeModule* mainModule = modules->GetMainModule();

  if (mainModule) {
    mainAddress = mainModule->base_address();
  }

  int mainModuleIndex = -1;

  for (const auto& itr : aOrderedModules) {
    const CodeModule* module = itr.first;

    if ((module->base_address() == mainAddress) && mainModule) {
      mainModuleIndex = itr.second;
    }

    Json::Value moduleNode;
    moduleNode["filename"] = PathnameStripper::File(module->code_file());
    moduleNode["code_id"] = PathnameStripper::File(module->code_identifier());
    moduleNode["version"] = module->version();
    moduleNode["debug_file"] = PathnameStripper::File(module->debug_file());
    moduleNode["debug_id"] = module->debug_identifier();
    moduleNode["base_addr"] = ToHex(module->base_address());
    moduleNode["end_addr"] = ToHex(module->base_address() + module->size());

    aNode.append(moduleNode);
  }

  return mainModuleIndex;
}

// Convert the list of unloaded modules to JSON and append them to the array
// specified in the |aNode| parameter. Return the number of unloaded modules
// that were found.

static size_t ConvertUnloadedModulesToJSON(const ProcessState& aProcessState,
                                           Json::Value& aNode) {
  const CodeModules* unloadedModules = aProcessState.unloaded_modules();
  if (!unloadedModules) {
    return 0;
  }

  const size_t unloadedModulesLen = unloadedModules->module_count();
  for (size_t i = 0; i < unloadedModulesLen; i++) {
    const CodeModule* unloadedModule = unloadedModules->GetModuleAtIndex(i);

    Json::Value unloadedModuleNode;
    unloadedModuleNode["filename"] =
        PathnameStripper::File(unloadedModule->code_file());
    unloadedModuleNode["code_id"] =
        PathnameStripper::File(unloadedModule->code_identifier());
    unloadedModuleNode["base_addr"] = ToHex(unloadedModule->base_address());
    unloadedModuleNode["end_addr"] =
        ToHex(unloadedModule->base_address() + unloadedModule->size());

    aNode.append(unloadedModuleNode);
  }

  return unloadedModulesLen;
}

// Convert the process state to JSON, this includes information about the
// crash, the module list and stack traces for every thread

static void ConvertProcessStateToJSON(const ProcessState& aProcessState,
                                      Json::Value& aStackTraces,
                                      const bool aFullStacks,
                                      Json::Value& aCertSubjects) {
  // Crash info
  Json::Value crashInfo;
  int requestingThread = aProcessState.requesting_thread();

  if (aProcessState.crashed()) {
    crashInfo["type"] = aProcessState.crash_reason();
    crashInfo["address"] = ToHex(aProcessState.crash_address());

    if (requestingThread != -1) {
      // Record the crashing thread index only if this is a full minidump
      // and all threads' stacks are present, otherwise only the crashing
      // thread stack is written out and this field is set to 0.
      crashInfo["crashing_thread"] = aFullStacks ? requestingThread : 0;
    }
  } else {
    crashInfo["type"] = Json::Value(Json::nullValue);
    // Add assertion info, if available
    string assertion = aProcessState.assertion();

    if (!assertion.empty()) {
      crashInfo["assertion"] = assertion;
    }
  }

  aStackTraces["crash_info"] = crashInfo;

  // Modules
  OrderedModulesMap orderedModules;
  PopulateModuleList(aProcessState, orderedModules, aFullStacks);

  Json::Value modules(Json::arrayValue);
  int mainModule = ConvertModulesToJSON(aProcessState, orderedModules, modules);

  if (mainModule != -1) {
    aStackTraces["main_module"] = mainModule;
  }

  aStackTraces["modules"] = modules;

  Json::Value unloadedModules(Json::arrayValue);
  size_t unloadedModulesLen =
      ConvertUnloadedModulesToJSON(aProcessState, unloadedModules);

  if (unloadedModulesLen > 0) {
    aStackTraces["unloaded_modules"] = unloadedModules;
  }

  RetrieveCertSubjects(aProcessState.modules(), aCertSubjects);
  RetrieveCertSubjects(aProcessState.unloaded_modules(), aCertSubjects);

  // Threads
  Json::Value threads(Json::arrayValue);
  int threadCount = aProcessState.threads()->size();

  if (!aFullStacks && (requestingThread != -1)) {
    // Only add the crashing thread
    Json::Value thread;
    Json::Value stack(Json::arrayValue);
    const CallStack* rawStack = aProcessState.threads()->at(requestingThread);

    ConvertStackToJSON(aProcessState, orderedModules, rawStack, stack);
    thread["frames"] = stack;
    threads.append(thread);
  } else {
    for (int threadIndex = 0; threadIndex < threadCount; ++threadIndex) {
      Json::Value thread;
      Json::Value stack(Json::arrayValue);
      const CallStack* rawStack = aProcessState.threads()->at(threadIndex);

      ConvertStackToJSON(aProcessState, orderedModules, rawStack, stack);
      thread["frames"] = stack;
      threads.append(thread);
    }
  }

  aStackTraces["threads"] = threads;
}

// Process the minidump file and append the JSON-formatted stack traces to
// the node specified in |aStackTraces|. We also populate |aCertSubjects| with
// information about the certificates used to sign modules, when present and
// supported by the underlying OS.
static bool ProcessMinidump(Json::Value& aStackTraces,
                            Json::Value& aCertSubjects, const string& aDumpFile,
                            const bool aFullStacks) {
#if XP_WIN && HAVE_64BIT_BUILD && defined(_M_X64)
  MozStackFrameSymbolizer symbolizer;
  MinidumpProcessor minidumpProcessor(&symbolizer, false);
#else
  BasicSourceLineResolver resolver;
  // We don't have a valid symbol resolver so we pass nullptr instead.
  MinidumpProcessor minidumpProcessor(nullptr, &resolver);
#endif

  // Process the minidump.
#if defined(XP_WIN)
  // Breakpad invokes std::ifstream directly, so this path needs to be ANSI
  Minidump dump(UTF8ToMBCS(aDumpFile));
#else
  Minidump dump(aDumpFile);
#endif  // defined(XP_WIN)
  if (!dump.Read()) {
    return false;
  }

  ProcessResult rv;
  ProcessState processState;
  rv = minidumpProcessor.Process(&dump, &processState);
  aStackTraces["status"] = ResultString(rv);

  ConvertProcessStateToJSON(processState, aStackTraces, aFullStacks,
                            aCertSubjects);

  return true;
}

static bool ReadExtraFile(const string& aExtraDataPath, Json::Value& aExtra) {
  IFStream f(
#if defined(XP_WIN)
      UTF8ToWide(aExtraDataPath).c_str(),
#else
      aExtraDataPath.c_str(),
#endif  // defined(XP_WIN)
      ios::in);
  if (!f.is_open()) {
    return false;
  }

  Json::CharReaderBuilder builder;
  return parseFromStream(builder, f, &aExtra, nullptr);
}

// Update the extra data file by adding the StackTraces and ModuleSignatureInfo
// fields that contain the JSON outputs of this program.
static bool UpdateExtraDataFile(const string& aDumpPath,
                                const Json::Value& aStackTraces,
                                const Json::Value& aCertSubjects) {
  string extraDataPath(aDumpPath);
  int dot = extraDataPath.rfind('.');

  if (dot < 0) {
    return false;  // Not a valid dump path
  }

  extraDataPath.replace(dot, extraDataPath.length() - dot, kExtraDataExtension);

  Json::Value extra;
  if (!ReadExtraFile(extraDataPath, extra)) {
    return false;
  }

  OFStream f(
#if defined(XP_WIN)
      UTF8ToWide(extraDataPath).c_str(),
#else
      extraDataPath.c_str(),
#endif  // defined(XP_WIN)
      ios::out | ios::trunc);

  bool res = false;
  if (f.is_open()) {
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";

    // The StackTraces field is not stored as a string because it's not a
    // crash annotation. It's only used by the crash reporter client which
    // strips it before submitting the other annotations to Socorro.
    extra["StackTraces"] = aStackTraces;

    if (!!aCertSubjects) {
      extra["ModuleSignatureInfo"] = Json::writeString(builder, aCertSubjects);
    }

    std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
    writer->write(extra, &f);
    f << "\n";
    res = !f.fail();
    f.close();
  }

  return res;
}

static bool GenerateStacks(const string& aDumpPath, const bool aFullStacks) {
  Json::Value stackTraces;
  Json::Value certSubjects;

  if (!ProcessMinidump(stackTraces, certSubjects, aDumpPath, aFullStacks)) {
    return false;
  }

  return UpdateExtraDataFile(aDumpPath, stackTraces, certSubjects);
}

}  // namespace CrashReporter

using namespace CrashReporter;

#if defined(XP_WIN)
#  define XP_LITERAL(s) L##s
#else
#  define XP_LITERAL(s) s
#endif

template <typename CharT>
struct CharTraits;

template <>
struct CharTraits<char> {
  static int compare(const char* left, const char* right) {
    return strcmp(left, right);
  }

  static string& assign(string& left, const char* right) {
    left = right;
    return left;
  }
};

#if defined(XP_WIN)

template <>
struct CharTraits<wchar_t> {
  static int compare(const wchar_t* left, const wchar_t* right) {
    return wcscmp(left, right);
  }

  static string& assign(string& left, const wchar_t* right) {
    left = WideToUTF8(right);
    return left;
  }
};

#endif  // defined(XP_WIN)

static void LowerPriority() {
#if defined(XP_WIN)
  Unused << SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
#else  // Linux, MacOS X, etc...
  Unused << nice(20);
#endif
}

template <typename CharT, typename Traits = CharTraits<CharT>>
static void ParseArguments(int argc, CharT** argv) {
  if (argc <= 1) {
    exit(EXIT_FAILURE);
  }

  for (int i = 1; i < argc - 1; i++) {
    if (!Traits::compare(argv[i], XP_LITERAL("--full"))) {
      gMinidumpAnalyzerOptions.fullMinidump = true;
    } else if (!Traits::compare(argv[i], XP_LITERAL("--force-use-module")) &&
               (i < argc - 2)) {
      Traits::assign(gMinidumpAnalyzerOptions.forceUseModule, argv[i + 1]);
      ++i;
    } else {
      exit(EXIT_FAILURE);
    }
  }

  Traits::assign(gMinidumpPath, argv[argc - 1]);
}

#if defined(XP_WIN)
// WARNING: Windows does *NOT* use UTF8 for char strings off the command line!
// Using wmain here so that the CRT doesn't need to perform a wasteful and
// lossy UTF-16 to MBCS conversion; ParseArguments will convert to UTF8
// directly.
extern "C" int wmain(int argc, wchar_t** argv)
#else
int main(int argc, char** argv)
#endif
{
  LowerPriority();
  ParseArguments(argc, argv);

  if (!GenerateStacks(gMinidumpPath, gMinidumpAnalyzerOptions.fullMinidump)) {
    exit(EXIT_FAILURE);
  }

  exit(EXIT_SUCCESS);
}
