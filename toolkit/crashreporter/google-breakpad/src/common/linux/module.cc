// Copyright (c) 2009, Google Inc.
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

#include <cerrno>
#include <cstring>
#include "common/linux/module.h"

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
  for (vector<Function *>::iterator it = functions_.begin();
       it != functions_.end(); it++)
    delete *it;
}

void Module::SetLoadAddress(Address address) {
  load_address_ = address;
}

void Module::AddFunction(Function *function) {
  functions_.push_back(function);
}

void Module::AddFunctions(vector<Function *>::iterator begin,
                          vector<Function *>::iterator end) {
  functions_.insert(functions_.end(), begin, end);
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
    file->name_ = name;
    file->source_id_ = -1;
    destiny = files_.insert(destiny,
                            FileByNameMap::value_type(&file->name_, file));
  }
  return destiny->second;
}

Module::File *Module::FindFile(const char *name) {
  string name_string = name;
  return FindFile(name_string);
}

void Module::AssignSourceIds() {
  // First, give every source file an id of -1.
  for (FileByNameMap::iterator file_it = files_.begin();
       file_it != files_.end(); file_it++)
    file_it->second->source_id_ = -1;

  // Next, mark all files actually cited by our functions' line number
  // info, by setting each one's source id to zero.
  for (vector<Function *>::const_iterator func_it = functions_.begin();
       func_it != functions_.end(); func_it++) {
    Function *func = *func_it;
    for (vector<Line>::iterator line_it = func->lines_.begin();
         line_it != func->lines_.end(); line_it++)
      line_it->file_->source_id_ = 0;
  }

  // Finally, assign source ids to those files that have been marked.
  // We could have just assigned source id numbers while traversing
  // the line numbers, but doing it this way numbers the files in
  // lexicographical order by name, which is neat.
  int next_source_id = 0;
  for (FileByNameMap::iterator file_it = files_.begin();
       file_it != files_.end(); file_it++)
    if (! file_it->second->source_id_)
      file_it->second->source_id_ = next_source_id++;
}

bool Module::ReportError() {
  fprintf(stderr, "error writing symbol file: %s\n",
          strerror (errno));
  return false;
}

bool Module::Write(FILE *stream) {
  if (0 > fprintf(stream, "MODULE %s %s %s %s\n",
                  os_.c_str(), architecture_.c_str(), id_.c_str(),
                  name_.c_str()))
    return ReportError();

  // Write out files.
  AssignSourceIds();
  for (FileByNameMap::iterator file_it = files_.begin();
       file_it != files_.end(); file_it++) {
    File *file = file_it->second;
    if (file->source_id_ >= 0) {
      if (0 > fprintf(stream, "FILE %d %s\n",
                      file->source_id_, file->name_.c_str()))
        return ReportError();
    }
  }

  // Write out functions and their lines.
  for (vector<Function *>::const_iterator func_it = functions_.begin();
       func_it != functions_.end(); func_it++) {
    Function *func = *func_it;
    if (0 > fprintf(stream, "FUNC %lx %lx %lu %s\n",
                    (unsigned long) (func->address_ - load_address_),
                    (unsigned long) func->size_,
                    (unsigned long) func->parameter_size_,
                    func->name_.c_str()))
      return ReportError();
    for (vector<Line>::iterator line_it = func->lines_.begin();
         line_it != func->lines_.end(); line_it++)
      if (0 > fprintf(stream, "%lx %lx %d %d\n",
                      (unsigned long) (line_it->address_ - load_address_),
                      (unsigned long) line_it->size_,
                      line_it->number_,
                      line_it->file_->source_id_))
        return ReportError();
  }

  return true;
}

} // namespace google_breakpad
