// Copyright (c) 2006, Google Inc.
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

#include <map>
#include <utility>
#include <vector>

#include "processor/address_map-inl.h"
#include "processor/contained_range_map-inl.h"
#include "processor/range_map-inl.h"

#include "google_breakpad/processor/basic_source_line_resolver.h"
#include "google_breakpad/processor/code_module.h"
#include "google_breakpad/processor/stack_frame.h"
#include "processor/linked_ptr.h"
#include "processor/scoped_ptr.h"
#include "processor/stack_frame_info.h"

using std::map;
using std::vector;
using std::make_pair;
using __gnu_cxx::hash;

namespace google_breakpad {

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

  // Loads the given map file, returning true on success.
  bool LoadMap(const string &map_file);

  // Looks up the given relative address, and fills the StackFrame struct
  // with the result.  Additional debugging information, if available, is
  // returned.  If no additional information is available, returns NULL.
  // A NULL return value is not an error.  The caller takes ownership of
  // any returned StackFrameInfo object.
  StackFrameInfo* LookupAddress(StackFrame *frame) const;

 private:
  friend class BasicSourceLineResolver;
  typedef hash_map<int, string> FileMap;

  // The types for stack_info_.  This is equivalent to MS DIA's
  // StackFrameTypeEnum.  Each identifies a different type of frame
  // information, although all are represented in the symbol file in the
  // same format.  These are used as indices to the stack_info_ array.
  enum StackInfoTypes {
    STACK_INFO_FPO = 0,
    STACK_INFO_TRAP,  // not used here
    STACK_INFO_TSS,   // not used here
    STACK_INFO_STANDARD,
    STACK_INFO_FRAME_DATA,
    STACK_INFO_LAST,  // must be the last sequentially-numbered item
    STACK_INFO_UNKNOWN = -1
  };

  // Splits line into at most max_tokens space-separated tokens, placing
  // them in the tokens vector.  line is a 0-terminated string that
  // optionally ends with a newline character or combination, which will
  // be removed.  line must not contain any embedded '\n' or '\r' characters.
  // If more tokens than max_tokens are present, the final token is placed
  // into the vector without splitting it up at all.  This modifies line as
  // a side effect.  Returns true if exactly max_tokens tokens are returned,
  // and false if fewer are returned.  This is not considered a failure of
  // Tokenize, but may be treated as a failure if the caller expects an
  // exact, as opposed to maximum, number of tokens.
  static bool Tokenize(char *line, int max_tokens, vector<char*> *tokens);

  // Parses a file declaration
  void ParseFile(char *file_line);

  // Parses a function declaration, returning a new Function object.
  Function* ParseFunction(char *function_line);

  // Parses a line declaration, returning a new Line object.
  Line* ParseLine(char *line_line);

  // Parses a PUBLIC symbol declaration, storing it in public_symbols_.
  // Returns false if an error occurs.
  bool ParsePublicSymbol(char *public_line);

  // Parses a stack frame info declaration, storing it in stack_info_.
  bool ParseStackInfo(char *stack_info_line);

  string name_;
  FileMap files_;
  RangeMap< MemAddr, linked_ptr<Function> > functions_;
  AddressMap< MemAddr, linked_ptr<PublicSymbol> > public_symbols_;

  // Each element in the array is a ContainedRangeMap for a type listed in
  // StackInfoTypes.  These are split by type because there may be overlaps
  // between maps of different types, but some information is only available
  // as certain types.
  ContainedRangeMap< MemAddr, linked_ptr<StackFrameInfo> >
      stack_info_[STACK_INFO_LAST];
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

bool BasicSourceLineResolver::LoadModule(const string &module_name,
                                         const string &map_file) {
  // Make sure we don't already have a module with the given name.
  if (modules_->find(module_name) != modules_->end()) {
    return false;
  }

  Module *module = new Module(module_name);
  if (!module->LoadMap(map_file)) {
    delete module;
    return false;
  }

  modules_->insert(make_pair(module_name, module));
  return true;
}

bool BasicSourceLineResolver::HasModule(const string &module_name) const {
  return modules_->find(module_name) != modules_->end();
}

StackFrameInfo* BasicSourceLineResolver::FillSourceLineInfo(
    StackFrame *frame) const {
  if (frame->module) {
    ModuleMap::const_iterator it = modules_->find(frame->module->code_file());
    if (it != modules_->end()) {
      return it->second->LookupAddress(frame);
    }
  }
  return NULL;
}

bool BasicSourceLineResolver::Module::LoadMap(const string &map_file) {
  FILE *f = fopen(map_file.c_str(), "r");
  if (!f) {
    return false;
  }

  // TODO(mmentovai): this might not be large enough to handle really long
  // lines, which might be present for FUNC lines of highly-templatized
  // code.
  char buffer[8192];
  linked_ptr<Function> cur_func;

  while (fgets(buffer, sizeof(buffer), f)) {
    if (strncmp(buffer, "FILE ", 5) == 0) {
      ParseFile(buffer);
    } else if (strncmp(buffer, "STACK ", 6) == 0) {
      if (!ParseStackInfo(buffer)) {
        return false;
      }
    } else if (strncmp(buffer, "FUNC ", 5) == 0) {
      cur_func.reset(ParseFunction(buffer));
      if (!cur_func.get()) {
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
        return false;
      }
      Line *line = ParseLine(buffer);
      if (!line) {
        return false;
      }
      cur_func->lines.StoreRange(line->address, line->size,
                                 linked_ptr<Line>(line));
    }
  }

  fclose(f);
  return true;
}

StackFrameInfo* BasicSourceLineResolver::Module::LookupAddress(
    StackFrame *frame) const {
  MemAddr address = frame->instruction - frame->module->base_address();

  linked_ptr<StackFrameInfo> retrieved_info;
  // Check for debugging info first, before any possible early returns.
  //
  // We only know about STACK_INFO_FRAME_DATA and STACK_INFO_FPO.  Prefer
  // them in this order.  STACK_INFO_FRAME_DATA is the newer type that
  // includes its own program string.  STACK_INFO_FPO is the older type
  // corresponding to the FPO_DATA struct.  See stackwalker_x86.cc.
  if (!stack_info_[STACK_INFO_FRAME_DATA].RetrieveRange(address,
                                                        &retrieved_info)) {
    stack_info_[STACK_INFO_FPO].RetrieveRange(address, &retrieved_info);
  }

  scoped_ptr<StackFrameInfo> frame_info;
  if (retrieved_info.get()) {
    frame_info.reset(new StackFrameInfo());
    frame_info->CopyFrom(*retrieved_info.get());
  }

  // First, look for a matching FUNC range.  Use RetrieveNearestRange instead
  // of RetrieveRange so that the nearest function can be compared to the
  // nearest PUBLIC symbol if the address does not lie within the function.
  // Having access to the highest function below address, even when address
  // is outside of the function, is useful: if the function is higher than
  // the nearest PUBLIC symbol, then it means that the PUBLIC symbols is not
  // valid for the address, and no function information should be filled in.
  // Using RetrieveNearestRange instead of RetrieveRange means that we need
  // to verify that address is within the range before using a FUNC.
  //
  // If no FUNC containing the address is found, look for the nearest PUBLIC
  // symbol, being careful not to use a public symbol at a lower address than
  // the nearest FUNC.
  int parameter_size = 0;
  linked_ptr<Function> func;
  linked_ptr<PublicSymbol> public_symbol;
  MemAddr function_base;
  MemAddr function_size;
  MemAddr public_address;
  if (functions_.RetrieveNearestRange(address, &func,
                                      &function_base, &function_size) &&
      address >= function_base && address < function_base + function_size) {
    parameter_size = func->parameter_size;

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
             (!func.get() || public_address > function_base + function_size)) {
    parameter_size = public_symbol->parameter_size;

    frame->function_name = public_symbol->name;
    frame->function_base = frame->module->base_address() + public_address;
  } else {
    // No FUNC or PUBLIC data available.
    return frame_info.release();
  }

  if (!frame_info.get()) {
    // Even without a relevant STACK line, many functions contain information
    // about how much space their parameters consume on the stack.  Prefer
    // the STACK stuff (above), but if it's not present, take the
    // information from the FUNC or PUBLIC line.
    frame_info.reset(new StackFrameInfo());
    frame_info->parameter_size = parameter_size;
    frame_info->valid |= StackFrameInfo::VALID_PARAMETER_SIZE;
  }

  return frame_info.release();
}

// static
bool BasicSourceLineResolver::Module::Tokenize(char *line, int max_tokens,
                                               vector<char*> *tokens) {
  tokens->clear();
  tokens->reserve(max_tokens);

  int remaining = max_tokens;

  // Split tokens on the space character.  Look for newlines too to
  // strip them out before exhausting max_tokens.
  char *save_ptr;
  char *token = strtok_r(line, " \r\n", &save_ptr);
  while (token && --remaining > 0) {
    tokens->push_back(token);
    if (remaining > 1)
      token = strtok_r(NULL, " \r\n", &save_ptr);
  }

  // If there's anything left, just add it as a single token.
  if (!remaining > 0) {
    if ((token = strtok_r(NULL, "\r\n", &save_ptr))) {
      tokens->push_back(token);
    }
  }

  return tokens->size() == static_cast<unsigned int>(max_tokens);
}

void BasicSourceLineResolver::Module::ParseFile(char *file_line) {
  // FILE <id> <filename>
  file_line += 5;  // skip prefix

  vector<char*> tokens;
  if (!Tokenize(file_line, 2, &tokens)) {
    return;
  }

  int index = atoi(tokens[0]);
  if (index < 0) {
    return;
  }

  char *filename = tokens[1];
  if (filename) {
    files_.insert(make_pair(index, string(filename)));
  }
}

BasicSourceLineResolver::Function*
BasicSourceLineResolver::Module::ParseFunction(char *function_line) {
  // FUNC <address> <size> <stack_param_size> <name>
  function_line += 5;  // skip prefix

  vector<char*> tokens;
  if (!Tokenize(function_line, 4, &tokens)) {
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
  if (!Tokenize(line_line, 4, &tokens)) {
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
  if (!Tokenize(public_line, 3, &tokens)) {
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
  // STACK WIN <type> <rva> <code_size> <prolog_size> <epliog_size>
  // <parameter_size> <saved_register_size> <local_size> <max_stack_size>
  // <has_program_string> <program_string_OR_allocates_base_pointer>
  //
  // If has_program_string is 1, the rest of the line is a program string.
  // Otherwise, the final token tells whether the stack info indicates that
  // a base pointer has been allocated.
  //
  // Expect has_program_string to be 1 when type is STACK_INFO_FRAME_DATA and
  // 0 when type is STACK_INFO_FPO, but don't enforce this.

  // Skip "STACK " prefix.
  stack_info_line += 6;

  vector<char*> tokens;
  if (!Tokenize(stack_info_line, 12, &tokens))
    return false;

  // Only MSVC stack frame info is understood for now.
  const char *platform = tokens[0];
  if (strcmp(platform, "WIN") != 0)
    return false;

  int type = strtol(tokens[1], NULL, 16);
  if (type < 0 || type > STACK_INFO_LAST - 1)
    return false;

  u_int64_t rva                 = strtoull(tokens[2],  NULL, 16);
  u_int64_t code_size           = strtoull(tokens[3],  NULL, 16);
  u_int32_t prolog_size         =  strtoul(tokens[4],  NULL, 16);
  u_int32_t epilog_size         =  strtoul(tokens[5],  NULL, 16);
  u_int32_t parameter_size      =  strtoul(tokens[6],  NULL, 16);
  u_int32_t saved_register_size =  strtoul(tokens[7],  NULL, 16);
  u_int32_t local_size          =  strtoul(tokens[8],  NULL, 16);
  u_int32_t max_stack_size      =  strtoul(tokens[9],  NULL, 16);
  int has_program_string        =  strtoul(tokens[10], NULL, 16);

  const char *program_string = "";
  int allocates_base_pointer = 0;
  if (has_program_string) {
    program_string = tokens[11];
  } else {
    allocates_base_pointer = strtoul(tokens[11], NULL, 16);
  }

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

  linked_ptr<StackFrameInfo> stack_frame_info(
      new StackFrameInfo(prolog_size,
                         epilog_size,
                         parameter_size,
                         saved_register_size,
                         local_size,
                         max_stack_size,
                         allocates_base_pointer,
                         program_string));
  stack_info_[type].StoreRange(rva, code_size, stack_frame_info);

  return true;
}

size_t BasicSourceLineResolver::HashString::operator()(const string &s) const {
  return hash<const char*>()(s.c_str());
}

}  // namespace google_breakpad
