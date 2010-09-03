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

// Original author: Jim Blandy <jimb@mozilla.com> <jimb@red-bean.com>

// module.cc: Implement google_breakpad::Module.  See module.h.

#include "common/module.h"

#include <errno.h>
#include <string.h>

namespace google_breakpad {

Module::Module(const string &name, const string &os,
               const string &architecture, const string &id) :
    name_(name),
    os_(os),
    architecture_(architecture),
    id_(id),
    load_address_(0) { }

Module::~Module() {
  for (FileByNameMap::iterator it = files_.begin(); it != files_.end(); it++)
    delete it->second;
  for (FunctionSet::iterator it = functions_.begin();
       it != functions_.end(); it++)
    delete *it;
  for (vector<StackFrameEntry *>::iterator it = stack_frame_entries_.begin();
       it != stack_frame_entries_.end(); it++)
    delete *it;
}

void Module::SetLoadAddress(Address address) {
  load_address_ = address;
}

void Module::AddFunction(Function *function) {
  std::pair<FunctionSet::iterator,bool> ret = functions_.insert(function);
  if (!ret.second) {
    // Free the duplicate we failed to insert because we own it.
    delete function;
  }
}

void Module::AddFunctions(vector<Function *>::iterator begin,
                          vector<Function *>::iterator end) {
  for (vector<Function *>::iterator it = begin; it != end; it++)
    AddFunction(*it);
}

void Module::AddStackFrameEntry(StackFrameEntry *stack_frame_entry) {
  stack_frame_entries_.push_back(stack_frame_entry);
}

void Module::GetFunctions(vector<Function *> *vec,
                          vector<Function *>::iterator i) {
  vec->insert(i, functions_.begin(), functions_.end());
}

Module::File *Module::FindFile(const string &name) {
  // A tricky bit here.  The key of each map entry needs to be a
  // pointer to the entry's File's name string.  This means that we
  // can't do the initial lookup with any operation that would create
  // an empty entry for us if the name isn't found (like, say,
  // operator[] or insert do), because such a created entry's key will
  // be a pointer the string passed as our argument.  Since the key of
  // a map's value type is const, we can't fix it up once we've
  // created our file.  lower_bound does the lookup without doing an
  // insertion, and returns a good hint iterator to pass to insert.
  // Our "destiny" is where we belong, whether we're there or not now.
  FileByNameMap::iterator destiny = files_.lower_bound(&name);
  if (destiny == files_.end()
      || *destiny->first != name) {  // Repeated string comparison, boo hoo.
    File *file = new File;
    file->name = name;
    file->source_id = -1;
    destiny = files_.insert(destiny,
                            FileByNameMap::value_type(&file->name, file));
  }
  return destiny->second;
}

Module::File *Module::FindFile(const char *name) {
  string name_string = name;
  return FindFile(name_string);
}

Module::File *Module::FindExistingFile(const string &name) {
  FileByNameMap::iterator it = files_.find(&name);
  return (it == files_.end()) ? NULL : it->second;
}

void Module::GetFiles(vector<File *> *vec) {
  vec->clear();
  for (FileByNameMap::iterator it = files_.begin(); it != files_.end(); it++)
    vec->push_back(it->second);
}

void Module::GetStackFrameEntries(vector<StackFrameEntry *> *vec) {
  *vec = stack_frame_entries_;
}

void Module::AssignSourceIds() {
  // First, give every source file an id of -1.
  for (FileByNameMap::iterator file_it = files_.begin();
       file_it != files_.end(); file_it++)
    file_it->second->source_id = -1;

  // Next, mark all files actually cited by our functions' line number
  // info, by setting each one's source id to zero.
  for (FunctionSet::const_iterator func_it = functions_.begin();
       func_it != functions_.end(); func_it++) {
    Function *func = *func_it;
    for (vector<Line>::iterator line_it = func->lines.begin();
         line_it != func->lines.end(); line_it++)
      line_it->file->source_id = 0;
  }

  // Finally, assign source ids to those files that have been marked.
  // We could have just assigned source id numbers while traversing
  // the line numbers, but doing it this way numbers the files in
  // lexicographical order by name, which is neat.
  int next_source_id = 0;
  for (FileByNameMap::iterator file_it = files_.begin();
       file_it != files_.end(); file_it++)
    if (!file_it->second->source_id)
      file_it->second->source_id = next_source_id++;
}

bool Module::ReportError() {
  fprintf(stderr, "error writing symbol file: %s\n",
          strerror(errno));
  return false;
}

bool Module::WriteRuleMap(const RuleMap &rule_map, FILE *stream) {
  for (RuleMap::const_iterator it = rule_map.begin();
       it != rule_map.end(); it++) {
    if (it != rule_map.begin() &&
        0 > putc(' ', stream))
      return false;
    if (0 > fprintf(stream, "%s: %s", it->first.c_str(), it->second.c_str()))
      return false;
  }
  return true;
}

bool Module::Write(FILE *stream) {
  if (0 > fprintf(stream, "MODULE %s %s %s %s\n",
                  os_.c_str(), architecture_.c_str(), id_.c_str(),
                  name_.c_str()))
    return ReportError();

  AssignSourceIds();

  // Write out files.
  for (FileByNameMap::iterator file_it = files_.begin();
       file_it != files_.end(); file_it++) {
    File *file = file_it->second;
    if (file->source_id >= 0) {
      if (0 > fprintf(stream, "FILE %d %s\n",
                      file->source_id, file->name.c_str()))
        return ReportError();
    }
  }

  // Write out functions and their lines.
  for (FunctionSet::const_iterator func_it = functions_.begin();
       func_it != functions_.end(); func_it++) {
    Function *func = *func_it;
    if (0 > fprintf(stream, "FUNC %llx %llx %llx %s\n",
                    (unsigned long long) (func->address - load_address_),
                    (unsigned long long) func->size,
                    (unsigned long long) func->parameter_size,
                    func->name.c_str()))
      return ReportError();
    for (vector<Line>::iterator line_it = func->lines.begin();
         line_it != func->lines.end(); line_it++)
      if (0 > fprintf(stream, "%llx %llx %d %d\n",
                      (unsigned long long) (line_it->address - load_address_),
                      (unsigned long long) line_it->size,
                      line_it->number,
                      line_it->file->source_id))
        return ReportError();
  }

  // Write out 'STACK CFI INIT' and 'STACK CFI' records.
  vector<StackFrameEntry *>::const_iterator frame_it;
  for (frame_it = stack_frame_entries_.begin();
       frame_it != stack_frame_entries_.end(); frame_it++) {
    StackFrameEntry *entry = *frame_it;
    if (0 > fprintf(stream, "STACK CFI INIT %llx %llx ",
                    (unsigned long long) entry->address - load_address_,
                    (unsigned long long) entry->size)
        || !WriteRuleMap(entry->initial_rules, stream)
        || 0 > putc('\n', stream))
      return ReportError();

    // Write out this entry's delta rules as 'STACK CFI' records.
    for (RuleChangeMap::const_iterator delta_it = entry->rule_changes.begin();
         delta_it != entry->rule_changes.end(); delta_it++) {
      if (0 > fprintf(stream, "STACK CFI %llx ",
                      (unsigned long long) delta_it->first - load_address_)
          || !WriteRuleMap(delta_it->second, stream)
          || 0 > putc('\n', stream))
        return ReportError();
    }
  }

  return true;
}

} // namespace google_breakpad
