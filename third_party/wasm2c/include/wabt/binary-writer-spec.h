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

#ifndef WABT_BINARY_WRITER_SPEC_H_
#define WABT_BINARY_WRITER_SPEC_H_

#include <functional>
#include <utility>
#include <vector>

#include "wabt/binary-writer.h"
#include "wabt/common.h"
#include "wabt/ir.h"

namespace wabt {

struct FilenameMemoryStreamPair {
  FilenameMemoryStreamPair(std::string_view filename,
                           std::unique_ptr<MemoryStream> stream)
      : filename(filename), stream(std::move(stream)) {}
  std::string filename;
  std::unique_ptr<MemoryStream> stream;
};

using WriteBinarySpecStreamFactory =
    std::function<Stream*(std::string_view filename)>;

Result WriteBinarySpecScript(Stream* json_stream,
                             WriteBinarySpecStreamFactory module_stream_factory,
                             Script*,
                             std::string_view source_filename,
                             std::string_view module_filename_noext,
                             const WriteBinaryOptions&);

// Convenience function for producing MemoryStream outputs all modules.
Result WriteBinarySpecScript(
    Stream* json_stream,
    Script*,
    std::string_view source_filename,
    std::string_view module_filename_noext,
    const WriteBinaryOptions&,
    std::vector<FilenameMemoryStreamPair>* out_module_streams,
    Stream* log_stream = nullptr);

}  // namespace wabt

#endif /* WABT_BINARY_WRITER_SPEC_H_ */
