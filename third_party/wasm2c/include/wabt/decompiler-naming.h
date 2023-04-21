/*
 * Copyright 2019 WebAssembly Community Group participants
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef WABT_DECOMPILER_NAMING_H_
#define WABT_DECOMPILER_NAMING_H_

#include "wabt/decompiler-ast.h"

#include <set>

namespace wabt {

inline void RenameToIdentifier(std::string& name,
                               Index i,
                               BindingHash& bh,
                               const std::set<std::string_view>* filter) {
  // Filter out non-identifier characters, and try to reduce the size of
  // gigantic C++ signature names.
  std::string s;
  size_t nesting = 0;
  size_t read = 0;
  size_t word_start = 0;
  for (auto c : name) {
    read++;
    // We most certainly don't want to parse the entirety of C++ signatures,
    // but these names are sometimes several lines long, so would be great
    // to trim down. One quick way to do that is to remove anything between
    // nested (), which usually means the parameter list.
    if (c == '(') {
      nesting++;
    }
    if (c == ')') {
      nesting--;
    }
    if (nesting) {
      continue;
    }
    if (!isalnum(static_cast<unsigned char>(c))) {
      c = '_';
    }
    if (c == '_') {
      if (s.empty()) {
        continue;  // Skip leading.
      }
      if (s.back() == '_') {
        continue;  // Consecutive.
      }
    }
    s += c;
    if (filter && (c == '_' || read == name.size())) {
      // We found a "word" inside a snake_case identifier.
      auto word_end = s.size();
      if (c == '_') {
        word_end--;
      }
      assert(word_end > word_start);
      auto word =
          std::string_view(s.c_str() + word_start, word_end - word_start);
      if (filter->find(word) != filter->end()) {
        s.resize(word_start);
      }
      word_start = s.size();
    }
  }
  if (!s.empty() && s.back() == '_') {
    s.pop_back();  // Trailing.
  }
  // If after all this culling, we're still gigantic (STL identifier can
  // easily be hundreds of chars in size), just cut the identifier
  // down, it will be disambiguated below, if needed.
  const size_t max_identifier_length = 100;
  if (s.size() > max_identifier_length) {
    s.resize(max_identifier_length);
  }
  if (s.empty()) {
    s = "__empty";
  }
  // Remove original binding first, such that it doesn't match with our
  // new name.
  bh.erase(name);
  // Find a unique name.
  Index disambiguator = 0;
  auto base_len = s.size();
  for (;;) {
    if (bh.count(s) == 0) {
      break;
    }
    disambiguator++;
    s.resize(base_len);
    s += '_';
    s += std::to_string(disambiguator);
  }
  // Replace name in bindings.
  name = s;
  bh.emplace(s, Binding(i));
}

template <typename T>
void RenameToIdentifiers(std::vector<T*>& things,
                         BindingHash& bh,
                         const std::set<std::string_view>* filter) {
  Index i = 0;
  for (auto thing : things) {
    RenameToIdentifier(thing->name, i++, bh, filter);
  }
}

enum {
  // This a bit arbitrary, change at will.
  min_content_identifier_size = 7,
  max_content_identifier_size = 30
};

void RenameToContents(std::vector<DataSegment*>& segs, BindingHash& bh) {
  std::string s;
  for (auto seg : segs) {
    if (seg->name.substr(0, 2) != "d_") {
      // This segment was named explicitly by a symbol.
      // FIXME: this is not a great check, a symbol could start with d_.
      continue;
    }
    s = "d_";
    for (auto c : seg->data) {
      if (isalnum(c) || c == '_') {
        s += static_cast<char>(c);
      }
      if (s.size() >= max_content_identifier_size) {
        // We truncate any very long names, since those make for hard to
        // format output. They can be somewhat long though, since data segment
        // references tend to not occur that often.
        break;
      }
    }
    if (s.size() < min_content_identifier_size) {
      // It is useful to have a minimum, since if there few printable characters
      // in a data section, that is probably a sign of binary, and those few
      // characters are not going to be very significant.
      continue;
    }
    // We could do the same disambiguition as RenameToIdentifier and
    // GenerateNames do, but if we come up with a clashing name here it is
    // likely a sign of not very meaningful binary data, so it is easier to
    // just keep the original generated name in that case.
    if (bh.count(s) != 0) {
      continue;
    }
    // Remove original entry.
    bh.erase(seg->name);
    seg->name = s;
    bh.emplace(s, Binding(static_cast<Index>(&seg - &segs[0])));
  }
}

// Function names may contain arbitrary C++ syntax, so we want to
// filter those to look like identifiers. A function name may be set
// by a name section (applied in ReadBinaryIr, called before this function)
// or by an export (applied by GenerateNames, called before this function),
// to both the Func and func_bindings.
// Those names then further perculate down the IR in ApplyNames (called after
// this function).
// To not have to add too many decompiler-specific code into those systems
// (using a callback??) we instead rename everything here.
// Also do data section renaming here.
void RenameAll(Module& module) {
  // We also filter common C++ keywords/STL idents that make for huge
  // identifiers.
  // FIXME: this can obviously give bad results if the input is not C++..
  std::set<std::string_view> filter = {
      {"const"},    {"std"},   {"allocator"}, {"char"},  {"basic"},
      {"traits"},   {"wchar"}, {"t"},         {"void"},  {"int"},
      {"unsigned"}, {"2"},     {"cxxabiv1"},  {"short"}, {"4096ul"},
  };
  RenameToIdentifiers(module.funcs, module.func_bindings, &filter);
  // Also do this for some other kinds of names, but without the keyword
  // substitution.
  RenameToIdentifiers(module.globals, module.global_bindings, nullptr);
  RenameToIdentifiers(module.tables, module.table_bindings, nullptr);
  RenameToIdentifiers(module.tags, module.tag_bindings, nullptr);
  RenameToIdentifiers(module.exports, module.export_bindings, nullptr);
  RenameToIdentifiers(module.types, module.type_bindings, nullptr);
  RenameToIdentifiers(module.memories, module.memory_bindings, nullptr);
  RenameToIdentifiers(module.data_segments, module.data_segment_bindings,
                      nullptr);
  RenameToIdentifiers(module.elem_segments, module.elem_segment_bindings,
                      nullptr);
  // Special purpose naming for data segments.
  RenameToContents(module.data_segments, module.data_segment_bindings);
}

}  // namespace wabt

#endif  // WABT_DECOMPILER_NAMING_H_
