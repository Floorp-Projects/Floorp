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

#ifndef WABT_C_WRITER_H_
#define WABT_C_WRITER_H_

#include <functional>
#include "wabt/common.h"
#include "wabt/feature.h"
#include "wabt/ir.h"

namespace wabt {

struct Module;
class Stream;

struct WriteCOptions {
  std::string_view module_name;
  /*
   * name_to_output_file_index takes const iterators to begin and end of a list
   * of all functions in the module, number of imported functions, and number of
   * .c outputs as argument, returns a vector where vector[i] the index of the
   * .c output that funcs_begin + i goes into. Only called when --num-outputs is
   * used.
   */
  std::function<std::vector<size_t>(
      std::vector<Func*>::const_iterator funcs_begin,
      std::vector<Func*>::const_iterator funcs_end,
      size_t num_imported_functions,
      size_t num_outputs)>
      name_to_output_file_index;
};

Result WriteC(std::vector<Stream*>&& c_streams,
              Stream* h_stream,
              Stream* h_impl_stream,
              const char* header_name,
              const char* header_impl_name,
              const Module*,
              const WriteCOptions&);

}  // namespace wabt

#endif /* WABT_C_WRITER_H_ */
