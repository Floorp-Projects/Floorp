/*
 * Copyright 2017 WebAssembly Community Group participants
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

#include "src/feature.h"

#include "src/option-parser.h"

namespace wabt {

void Features::AddOptions(OptionParser* parser) {
#define WABT_FEATURE(variable, flag, default_, help)       \
  if (default_ == true) {                                  \
    parser->AddOption("disable-" flag, "Disable " help,    \
                      [this]() { disable_##variable(); }); \
  } else {                                                 \
    parser->AddOption("enable-" flag, "Enable " help,      \
                      [this]() { enable_##variable(); });  \
  }

#include "src/feature.def"
#undef WABT_FEATURE
  parser->AddOption("enable-all", "Enable all features",
                    [this]() { EnableAll(); });
}

void Features::UpdateDependencies() {
  // Exception handling requires reference types.
  if (exceptions_enabled_) {
    reference_types_enabled_ = true;
  }

  // Function references require reference types.
  if (function_references_enabled_) {
    reference_types_enabled_ = true;
  }

  // Reference types requires bulk memory.
  if (reference_types_enabled_) {
    bulk_memory_enabled_ = true;
  }
}

}  // namespace wabt
