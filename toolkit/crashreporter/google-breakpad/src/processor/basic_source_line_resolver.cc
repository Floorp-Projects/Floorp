// Copyright (c) 2010 Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <map>
#include <utility>
#include <vector>

#include "processor/address_map-inl.h"
#include "processor/contained_range_map-inl.h"
#include "processor/range_map-inl.h"

#include "google_breakpad/processor/basic_source_line_resolver.h"
#include "google_breakpad/processor/code_module.h"
#include "google_breakpad/processor/stack_frame.h"
#include "processor/cfi_frame_info.h"
#include "processor/linked_ptr.h"
#include "processor/scoped_ptr.h"
#include "processor/windows_frame_info.h"
#include "processor/tokenize.h"

using std::map;
using std::vector;
using std::make_pair;

namespace google_breakpad {

static const char *kWhitespace = " \r\n";

struct BasicSourceLineResolver::Line {
  Line(MemAddr addr, MemAddr code_size, int file_id, int source_line)
      : address(addr)
      , size(code_size)
      , source_file_id(file_id)
      , line(source_line) { }

  MemAddr address;
  MemAddr size;
  int source_file_id;
  int line;
};

struct BasicSourceLineResolver::Function {
  Function(const string &function_name,
           MemAddr function_address,
           MemAddr code_size,
           int set_parameter_size)
      : name(function_name), address(function_address), size(code_size),
        parameter_size(set_parameter_size) { }

  string name;
  MemAddr address;
  MemAddr size;

  // The size of parameters passed to this function on the stack.
  int parameter_size;

  RangeMap< MemAddr, linked_ptr<Line> > lines;
};

struct BasicSourceLineResolver::PublicSymbol {
  PublicSymbol(const string& set_name,
               MemAddr set_address,
               int set_parameter_size)
      : name(set_name),
        address(set_address),
        parameter_size(set_parameter_size) {}

  string name;
  MemAddr address;

  // If the public symbol is used as a function entry point, parameter_size
  // is set to the size of the parameters passed to the funciton on the
  // stack, if known.
  int parameter_size;
};

class BasicSourceLineResolver::Module {
 public:
  Module(const string &name) : name_(name) { }

  // Loads the given map file, returning true on success.  Reads the
  // map file into memory and calls LoadMapFromBuffer
  bool LoadMap(const string &map_file);

  // Loads a map from the given buffer, returning true on success
  bool LoadMapFromBuffer(const string &map_buffer);

  // Looks up the given relative address, and fills the StackFrame struct
  // with the result.
  void LookupAddress(StackFrame *frame) const;

  // If Windows stack walking information is available covering ADDRESS,
  // return a WindowsFrameInfo structure describing it. If the information
  // is not available, returns NULL. A NULL return value does not indicate
  // an error. The caller takes ownership of any returned WindowsFrameInfo
  // object.
  WindowsFrameInfo *FindWindowsFrameInfo(const StackFrame *frame) const;

  // If CFI stack walking information is available covering ADDRESS,
  // return a CFIFrameInfo structure describing it. If the information
  // is not available, return NULL. The caller takes ownership of any
  // returned CFIFrameInfo object.
  CFIFrameInfo *FindCFIFrameInfo(const StackFrame *frame) const;

 private:
  friend class BasicSourceLineResolver;
  typedef map<int, string> FileMap;

  // Parses a file declaration
  bool ParseFile(char *file_line);

  // Parses a function declaration, returning a new Function object.
  Function* ParseFunction(char *function_line);

  // Parses a line declaration, returning a new Line object.
  Line* ParseLine(char *line_line);

  // Parses a PUBLIC symbol declaration, storing it in public_symbols_.
  // Returns false if an error occurs.
  bool ParsePublicSymbol(char *public_line);

  // Parses a STACK WIN or STACK CFI frame info declaration, storing
  // it in the appropriate table.
  bool ParseStackInfo(char *stack_info_line);

  // Parses a STACK CFI record, storing it in cfi_frame_info_.
  bool ParseCFIFrameInfo(char *stack_info_line);

  // Parse RULE_SET, a series of rules of the sort appearing in STACK
  // CFI records, and store the given rules in FRAME_INFO.
  bool ParseCFIRuleSet(const string &rule_set, CFIFrameInfo *frame_info) const;

  string name_;
  FileMap files_;
  RangeMap< MemAddr, linked_ptr<Function> > functions_;
  AddressMap< MemAddr, linked_ptr<PublicSymbol> > public_symbols_;

  // Each element in the array is a ContainedRangeMap for a type
  // listed in WindowsFrameInfoTypes. These are split by type because
  // there may be overlaps between maps of different types, but some
  // information is only available as certain types.
  ContainedRangeMap< MemAddr, linked_ptr<WindowsFrameInfo> >
    windows_frame_info_[WindowsFrameInfo::STACK_INFO_LAST];

  // DWARF CFI stack walking data. The Module stores the initial rule sets
  // and rule deltas as strings, just as they appear in the symbol file:
  // although the file may contain hundreds of thousands of STACK CFI
  // records, walking a stack will only ever use a few of them, so it's
  // best to delay parsing a record until it's actually needed.

  // STACK CFI INIT records: for each range, an initial set of register
  // recovery rules. The RangeMap's itself gives the starting and ending
  // addresses.
  RangeMap<MemAddr, string> cfi_initial_rules_;

  // STACK CFI records: at a given address, the changes to the register
  // recovery rules that take effect at that address. The map key is the
  // starting address; the ending address is the key of the next entry in
  // this map, or the end of the range as given by the cfi_initial_rules_
  // entry (which FindCFIFrameInfo looks up first).
  map<MemAddr, string> cfi_delta_rules_;
};

BasicSourceLineResolver::BasicSourceLineResolver() : modules_(new ModuleMap) {
}

BasicSourceLineResolver::~BasicSourceLineResolver() {
  ModuleMap::iterator it;
  for (it = modules_->begin(); it != modules_->end(); ++it) {
    delete it->second;
  }
  delete modules_;
}

bool BasicSourceLineResolver::LoadModule(const CodeModule *module,
                                         const string &map_file) {
  if (module == NULL)
    return false;

  // Make sure we don't already have a module with the given name.
  if (modules_->find(module->code_file()) != modules_->end()) {
    BPLOG(INFO) << "Symbols for module " << module->code_file()
                << " already loaded";
    return false;
  }

  BPLOG(INFO) << "Loading symbols for module " << module->code_file()
              << " from " << map_file;

  Module *basic_module = new Module(module->code_file());
  if (!basic_module->LoadMap(map_file)) {
    delete basic_module;
    return false;
  }

  modules_->insert(make_pair(module->code_file(), basic_module));
  return true;
}

bool BasicSourceLineResolver::LoadModuleUsingMapBuffer(
    const CodeModule *module,
    const string &map_buffer) {
  if (!module)
    return false;

  // Make sure we don't already have a module with the given name.
  if (modules_->find(module->code_file()) != modules_->end()) {
    BPLOG(INFO) << "Symbols for module " << module->code_file()
                << " already loaded";
    return false;
  }

  BPLOG(INFO) << "Loading symbols for module " << module->code_file()
              << " from buffer";

  Module *basic_module = new Module(module->code_file());
  if (!basic_module->LoadMapFromBuffer(map_buffer)) {
    delete basic_module;
    return false;
  }

  modules_->insert(make_pair(module->code_file(), basic_module));
  return true;
}

void BasicSourceLineResolver::UnloadModule(const CodeModule *module)
{
  if (!module)
    return;

  ModuleMap::iterator iter = modules_->find(module->code_file());
  if (iter != modules_->end()) {
    modules_->erase(iter);
  }
}

bool BasicSourceLineResolver::HasModule(const CodeModule *module) {
  if (!module)
    return false;
  return modules_->find(module->code_file()) != modules_->end();
}

void BasicSourceLineResolver::FillSourceLineInfo(StackFrame *frame) {
  if (frame->module) {
    ModuleMap::const_iterator it = modules_->find(frame->module->code_file());
    if (it != modules_->end()) {
      it->second->LookupAddress(frame);
    }
  }
}

WindowsFrameInfo *BasicSourceLineResolver::FindWindowsFrameInfo(
    const StackFrame *frame) {
  if (frame->module) {
    ModuleMap::const_iterator it = modules_->find(frame->module->code_file());
    if (it != modules_->end()) {
      return it->second->FindWindowsFrameInfo(frame);
    }
  }
  return NULL;
}

CFIFrameInfo *BasicSourceLineResolver::FindCFIFrameInfo(
    const StackFrame *frame) {
  if (frame->module) {
    ModuleMap::const_iterator it = modules_->find(frame->module->code_file());
    if (it != modules_->end()) {
      return it->second->FindCFIFrameInfo(frame);
    }
  }
  return NULL;
}

class AutoFileCloser {
 public:
  AutoFileCloser(FILE *file) : file_(file) {}
  ~AutoFileCloser() {
    if (file_)
      fclose(file_);
  }

 private:
  FILE *file_;
};

bool BasicSourceLineResolver::Module::LoadMapFromBuffer(
    const string &map_buffer) {
  linked_ptr<Function> cur_func;
  int line_number = 0;
  const char *map_buffer_c_str = map_buffer.c_str();
  char *save_ptr;

  // set up our input buffer as a c-style string so we
  // can we use strtok()
  // have to copy because modifying the result of string::c_str is not
  // permitted
  size_t map_buffer_length = strlen(map_buffer_c_str);

  // If the length is 0, we can still pretend we have a symbol file. This is
  // for scenarios that want to test symbol lookup, but don't necessarily care if
  // certain modules do not have any information, like system libraries.
  if (map_buffer_length == 0) {
    return true;
  }

  scoped_array<char> map_buffer_chars(new char[map_buffer_length]);
  if (map_buffer_chars == NULL) {
    BPLOG(ERROR) << "Memory allocation of " << map_buffer_length <<
        " bytes failed";
    return false;
  }

  strncpy(map_buffer_chars.get(), map_buffer_c_str, map_buffer_length);

  if (map_buffer_chars[map_buffer_length - 1] == '\n') {
    map_buffer_chars[map_buffer_length - 1] = '\0';
  }
  char *buffer;
  buffer = strtok_r(map_buffer_chars.get(), "\r\n", &save_ptr);

  while (buffer != NULL) {
    ++line_number;

    if (strncmp(buffer, "FILE ", 5) == 0) {
      if (!ParseFile(buffer)) {
        BPLOG(ERROR) << "ParseFile on buffer failed at " <<
            ":" << line_number;
        return false;
      }
    } else if (strncmp(buffer, "STACK ", 6) == 0) {
      if (!ParseStackInfo(buffer)) {
        BPLOG(ERROR) << "ParseStackInfo failed at " <<
            ":" << line_number;
        return false;
      }
    } else if (strncmp(buffer, "FUNC ", 5) == 0) {
      cur_func.reset(ParseFunction(buffer));
      if (!cur_func.get()) {
        BPLOG(ERROR) << "ParseFunction failed at " <<
            ":" << line_number;
        return false;
      }
      // StoreRange will fail if the function has an invalid address or size.
      // We'll silently ignore this, the function and any corresponding lines
      // will be destroyed when cur_func is released.
      functions_.StoreRange(cur_func->address, cur_func->size, cur_func);
    } else if (strncmp(buffer, "PUBLIC ", 7) == 0) {
      // Clear cur_func: public symbols don't contain line number information.
      cur_func.reset();

      if (!ParsePublicSymbol(buffer)) {
        BPLOG(ERROR) << "ParsePublicSymbol failed at " <<
            ":" << line_number;
        return false;
      }
    } else if (strncmp(buffer, "MODULE ", 7) == 0) {
      // Ignore these.  They're not of any use to BasicSourceLineResolver,
      // which is fed modules by a SymbolSupplier.  These lines are present to
      // aid other tools in properly placing symbol files so that they can
      // be accessed by a SymbolSupplier.
      //
      // MODULE <guid> <age> <filename>
    } else {
      if (!cur_func.get()) {
        BPLOG(ERROR) << "Found source line data without a function at " <<
            ":" << line_number;
        return false;
      }
      Line *line = ParseLine(buffer);
      if (!line) {
        BPLOG(ERROR) << "ParseLine failed at " << line_number << " for " <<
            buffer;
        return false;
      }
      cur_func->lines.StoreRange(line->address, line->size,
                                 linked_ptr<Line>(line));
    }

    buffer = strtok_r(NULL, "\r\n", &save_ptr);
  }

  return true;
}

bool BasicSourceLineResolver::Module::LoadMap(const string &map_file) {
  struct stat buf;
  int error_code = stat(map_file.c_str(), &buf);
  if (error_code == -1) {
    string error_string;
    int error_code = ErrnoString(&error_string);
    BPLOG(ERROR) << "Could not open " << map_file <<
        ", error " << error_code << ": " << error_string;
    return false;
  }

  off_t file_size = buf.st_size;

  // Allocate memory for file contents, plus a null terminator
  // since we'll use strtok() on the contents.
  char *file_buffer = new char[sizeof(char)*file_size + 1];

  if (file_buffer == NULL) {
    BPLOG(ERROR) << "Could not allocate memory for " << map_file;
    return false;
  }

  BPLOG(INFO) << "Opening " << map_file;

  FILE *f = fopen(map_file.c_str(), "rt");
  if (!f) {
    string error_string;
    int error_code = ErrnoString(&error_string);
    BPLOG(ERROR) << "Could not open " << map_file <<
        ", error " << error_code << ": " << error_string;
    delete [] file_buffer;
    return false;
  }

  AutoFileCloser closer(f);

  int items_read = 0;

  items_read = fread(file_buffer, 1, file_size, f);

  if (items_read != file_size) {
    string error_string;
    int error_code = ErrnoString(&error_string);
    BPLOG(ERROR) << "Could not slurp " << map_file <<
        ", error " << error_code << ": " << error_string;
    delete [] file_buffer;
    return false;
  }
  file_buffer[file_size] = '\0';
  string map_buffer(file_buffer);
  delete [] file_buffer;

  return LoadMapFromBuffer(map_buffer);
}

void BasicSourceLineResolver::Module::LookupAddress(StackFrame *frame) const {
  MemAddr address = frame->instruction - frame->module->base_address();

  // First, look for a FUNC record that covers address. Use
  // RetrieveNearestRange instead of RetrieveRange so that, if there
  // is no such function, we can use the next function to bound the
  // extent of the PUBLIC symbol we find, below. This does mean we
  // need to check that address indeed falls within the function we
  // find; do the range comparison in an overflow-friendly way.
  linked_ptr<Function> func;
  linked_ptr<PublicSymbol> public_symbol;
  MemAddr function_base;
  MemAddr function_size;
  MemAddr public_address;
  if (functions_.RetrieveNearestRange(address, &func,
                                      &function_base, &function_size) &&
      address >= function_base && address - function_base < function_size) {
    frame->function_name = func->name;
    frame->function_base = frame->module->base_address() + function_base;

    linked_ptr<Line> line;
    MemAddr line_base;
    if (func->lines.RetrieveRange(address, &line, &line_base, NULL)) {
      FileMap::const_iterator it = files_.find(line->source_file_id);
      if (it != files_.end()) {
        frame->source_file_name = files_.find(line->source_file_id)->second;
      }
      frame->source_line = line->line;
      frame->source_line_base = frame->module->base_address() + line_base;
    }
  } else if (public_symbols_.Retrieve(address,
                                      &public_symbol, &public_address) &&
             (!func.get() || public_address > function_base)) {
    frame->function_name = public_symbol->name;
    frame->function_base = frame->module->base_address() + public_address;
  }
}

WindowsFrameInfo *BasicSourceLineResolver::Module::FindWindowsFrameInfo(
    const StackFrame *frame) const {
  MemAddr address = frame->instruction - frame->module->base_address();
  scoped_ptr<WindowsFrameInfo> result(new WindowsFrameInfo());

  // We only know about WindowsFrameInfo::STACK_INFO_FRAME_DATA and
  // WindowsFrameInfo::STACK_INFO_FPO. Prefer them in this order.
  // WindowsFrameInfo::STACK_INFO_FRAME_DATA is the newer type that
  // includes its own program string.
  // WindowsFrameInfo::STACK_INFO_FPO is the older type
  // corresponding to the FPO_DATA struct. See stackwalker_x86.cc.
  linked_ptr<WindowsFrameInfo> frame_info;
  if ((windows_frame_info_[WindowsFrameInfo::STACK_INFO_FRAME_DATA]
       .RetrieveRange(address, &frame_info))
      || (windows_frame_info_[WindowsFrameInfo::STACK_INFO_FPO]
          .RetrieveRange(address, &frame_info))) {
    result->CopyFrom(*frame_info.get());
    return result.release();
  }

  // Even without a relevant STACK line, many functions contain
  // information about how much space their parameters consume on the
  // stack. Use RetrieveNearestRange instead of RetrieveRange, so that
  // we can use the function to bound the extent of the PUBLIC symbol,
  // below. However, this does mean we need to check that ADDRESS
  // falls within the retrieved function's range; do the range
  // comparison in an overflow-friendly way.
  linked_ptr<Function> function;
  MemAddr function_base, function_size;
  if (functions_.RetrieveNearestRange(address, &function,
                                      &function_base, &function_size) && 
      address >= function_base && address - function_base < function_size) {
    result->parameter_size = function->parameter_size;
    result->valid |= WindowsFrameInfo::VALID_PARAMETER_SIZE;
    return result.release();
  }

  // PUBLIC symbols might have a parameter size. Use the function we
  // found above to limit the range the public symbol covers.
  linked_ptr<PublicSymbol> public_symbol;
  MemAddr public_address;
  if (public_symbols_.Retrieve(address, &public_symbol, &public_address) &&
      (!function.get() || public_address > function_base)) {
    result->parameter_size = public_symbol->parameter_size;
  }
  
  return NULL;
}

CFIFrameInfo *BasicSourceLineResolver::Module::FindCFIFrameInfo(
    const StackFrame *frame) const {
  MemAddr address = frame->instruction - frame->module->base_address();
  MemAddr initial_base, initial_size;
  string initial_rules;

  // Find the initial rule whose range covers this address. That
  // provides an initial set of register recovery rules. Then, walk
  // forward from the initial rule's starting address to frame's
  // instruction address, applying delta rules.
  if (!cfi_initial_rules_.RetrieveRange(address, &initial_rules,
                                        &initial_base, &initial_size)) {
    return NULL;
  }

  // Create a frame info structure, and populate it with the rules from
  // the STACK CFI INIT record.
  scoped_ptr<CFIFrameInfo> rules(new CFIFrameInfo());
  if (!ParseCFIRuleSet(initial_rules, rules.get()))
    return NULL;

  // Find the first delta rule that falls within the initial rule's range.
  map<MemAddr, string>::const_iterator delta =
    cfi_delta_rules_.lower_bound(initial_base);

  // Apply delta rules up to and including the frame's address.
  while (delta != cfi_delta_rules_.end() && delta->first <= address) {
    ParseCFIRuleSet(delta->second, rules.get());
    delta++;
  }

  return rules.release();
}

bool BasicSourceLineResolver::Module::ParseCFIRuleSet(
    const string &rule_set, CFIFrameInfo *frame_info) const {
  CFIFrameInfoParseHandler handler(frame_info);
  CFIRuleParser parser(&handler);
  return parser.Parse(rule_set);
}

bool BasicSourceLineResolver::Module::ParseFile(char *file_line) {
  // FILE <id> <filename>
  file_line += 5;  // skip prefix

  vector<char*> tokens;
  if (!Tokenize(file_line, kWhitespace, 2, &tokens)) {
    return false;
  }

  int index = atoi(tokens[0]);
  if (index < 0) {
    return false;
  }

  char *filename = tokens[1];
  if (!filename) {
    return false;
  }

  files_.insert(make_pair(index, string(filename)));
  return true;
}

BasicSourceLineResolver::Function*
BasicSourceLineResolver::Module::ParseFunction(char *function_line) {
  // FUNC <address> <size> <stack_param_size> <name>
  function_line += 5;  // skip prefix

  vector<char*> tokens;
  if (!Tokenize(function_line, kWhitespace, 4, &tokens)) {
    return NULL;
  }

  u_int64_t address    = strtoull(tokens[0], NULL, 16);
  u_int64_t size       = strtoull(tokens[1], NULL, 16);
  int stack_param_size = strtoull(tokens[2], NULL, 16);
  char *name           = tokens[3];

  return new Function(name, address, size, stack_param_size);
}

BasicSourceLineResolver::Line* BasicSourceLineResolver::Module::ParseLine(
    char *line_line) {
  // <address> <line number> <source file id>
  vector<char*> tokens;
  if (!Tokenize(line_line, kWhitespace, 4, &tokens)) {
    return NULL;
  }

  u_int64_t address = strtoull(tokens[0], NULL, 16);
  u_int64_t size    = strtoull(tokens[1], NULL, 16);
  int line_number   = atoi(tokens[2]);
  int source_file   = atoi(tokens[3]);
  if (line_number <= 0) {
    return NULL;
  }

  return new Line(address, size, source_file, line_number);
}

bool BasicSourceLineResolver::Module::ParsePublicSymbol(char *public_line) {
  // PUBLIC <address> <stack_param_size> <name>

  // Skip "PUBLIC " prefix.
  public_line += 7;

  vector<char*> tokens;
  if (!Tokenize(public_line, kWhitespace, 3, &tokens)) {
    return false;
  }

  u_int64_t address    = strtoull(tokens[0], NULL, 16);
  int stack_param_size = strtoull(tokens[1], NULL, 16);
  char *name           = tokens[2];

  // A few public symbols show up with an address of 0.  This has been seen
  // in the dumped output of ntdll.pdb for symbols such as _CIlog, _CIpow,
  // RtlDescribeChunkLZNT1, and RtlReserveChunkLZNT1.  They would conflict
  // with one another if they were allowed into the public_symbols_ map,
  // but since the address is obviously invalid, gracefully accept them
  // as input without putting them into the map.
  if (address == 0) {
    return true;
  }

  linked_ptr<PublicSymbol> symbol(new PublicSymbol(name, address,
                                                   stack_param_size));
  return public_symbols_.Store(address, symbol);
}

bool BasicSourceLineResolver::Module::ParseStackInfo(char *stack_info_line) {
  // Skip "STACK " prefix.
  stack_info_line += 6;

  // Find the token indicating what sort of stack frame walking
  // information this is.
  while (*stack_info_line == ' ')
    stack_info_line++;
  const char *platform = stack_info_line;
  while (!strchr(kWhitespace, *stack_info_line))
    stack_info_line++;
  *stack_info_line++ = '\0';

  // MSVC stack frame info.
  if (strcmp(platform, "WIN") == 0) {
    int type = 0;
    u_int64_t rva, code_size;
    linked_ptr<WindowsFrameInfo>
      stack_frame_info(WindowsFrameInfo::ParseFromString(stack_info_line,
                                                         type,
                                                         rva,
                                                         code_size));
    if (stack_frame_info == NULL)
      return false;

    // TODO(mmentovai): I wanted to use StoreRange's return value as this
    // method's return value, but MSVC infrequently outputs stack info that
    // violates the containment rules.  This happens with a section of code
    // in strncpy_s in test_app.cc (testdata/minidump2).  There, problem looks
    // like this:
    //   STACK WIN 4 4242 1a a 0 ...  (STACK WIN 4 base size prolog 0 ...)
    //   STACK WIN 4 4243 2e 9 0 ...
    // ContainedRangeMap treats these two blocks as conflicting.  In reality,
    // when the prolog lengths are taken into account, the actual code of
    // these blocks doesn't conflict.  However, we can't take the prolog lengths
    // into account directly here because we'd wind up with a different set
    // of range conflicts when MSVC outputs stack info like this:
    //   STACK WIN 4 1040 73 33 0 ...
    //   STACK WIN 4 105a 59 19 0 ...
    // because in both of these entries, the beginning of the code after the
    // prolog is at 0x1073, and the last byte of contained code is at 0x10b2.
    // Perhaps we could get away with storing ranges by rva + prolog_size
    // if ContainedRangeMap were modified to allow replacement of
    // already-stored values.

    windows_frame_info_[type].StoreRange(rva, code_size, stack_frame_info);
    return true;
  } else if (strcmp(platform, "CFI") == 0) {
    // DWARF CFI stack frame info
    return ParseCFIFrameInfo(stack_info_line);
  } else {
    // Something unrecognized.
    return false;
  }
}

bool BasicSourceLineResolver::Module::ParseCFIFrameInfo(
    char *stack_info_line) {
  char *cursor;

  // Is this an INIT record or a delta record?
  char *init_or_address = strtok_r(stack_info_line, " \r\n", &cursor);
  if (!init_or_address)
    return false;

  if (strcmp(init_or_address, "INIT") == 0) {
    // This record has the form "STACK INIT <address> <size> <rules...>".
    char *address_field = strtok_r(NULL, " \r\n", &cursor);
    if (!address_field) return false;
    
    char *size_field = strtok_r(NULL, " \r\n", &cursor);
    if (!size_field) return false;

    char *initial_rules = strtok_r(NULL, "\r\n", &cursor);
    if (!initial_rules) return false;

    MemAddr address = strtoul(address_field, NULL, 16);
    MemAddr size    = strtoul(size_field,    NULL, 16);
    cfi_initial_rules_.StoreRange(address, size, initial_rules);
    return true;
  }

  // This record has the form "STACK <address> <rules...>".
  char *address_field = init_or_address;
  char *delta_rules = strtok_r(NULL, "\r\n", &cursor);
  if (!delta_rules) return false;
  MemAddr address = strtoul(address_field, NULL, 16);
  cfi_delta_rules_[address] = delta_rules;
  return true;
}

bool BasicSourceLineResolver::CompareString::operator()(
    const string &s1, const string &s2) const {
  return strcmp(s1.c_str(), s2.c_str()) < 0;
}

}  // namespace google_breakpad
