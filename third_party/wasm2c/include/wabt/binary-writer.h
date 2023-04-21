/*
 * Copyright 2016 WebAssembly Community Group participants
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

#ifndef WABT_BINARY_WRITER_H_
#define WABT_BINARY_WRITER_H_

#include "wabt/common.h"
#include "wabt/feature.h"
#include "wabt/opcode.h"
#include "wabt/stream.h"

namespace wabt {

struct Module;
struct Script;

struct WriteBinaryOptions {
  WriteBinaryOptions() = default;
  WriteBinaryOptions(const Features& features,
                     bool canonicalize_lebs,
                     bool relocatable,
                     bool write_debug_names)
      : features(features),
        canonicalize_lebs(canonicalize_lebs),
        relocatable(relocatable),
        write_debug_names(write_debug_names) {}

  Features features;
  bool canonicalize_lebs = true;
  bool relocatable = false;
  bool write_debug_names = false;
};

Result WriteBinaryModule(Stream*, const Module*, const WriteBinaryOptions&);

void WriteType(Stream* stream, Type type, const char* desc = nullptr);

void WriteStr(Stream* stream,
              std::string_view s,
              const char* desc,
              PrintChars print_chars = PrintChars::No);

void WriteOpcode(Stream* stream, Opcode opcode);

void WriteLimits(Stream* stream, const Limits* limits);

}  // namespace wabt

#endif /* WABT_BINARY_WRITER_H_ */
