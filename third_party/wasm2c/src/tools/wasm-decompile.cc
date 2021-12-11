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

#include <cassert>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>

#include "src/apply-names.h"
#include "src/binary-reader.h"
#include "src/binary-reader-ir.h"
#include "src/error-formatter.h"
#include "src/feature.h"
#include "src/generate-names.h"
#include "src/ir.h"
#include "src/option-parser.h"
#include "src/stream.h"
#include "src/validator.h"
#include "src/wast-lexer.h"
#include "src/decompiler.h"

using namespace wabt;

int ProgramMain(int argc, char** argv) {
  InitStdio();

  std::string infile;
  std::string outfile;
  Features features;
  DecompileOptions decompile_options;
  bool fail_on_custom_section_error = true;

  {
    const char s_description[] =
      "  Read a file in the WebAssembly binary format, and convert it to\n"
      "  a decompiled text file.\n"
      "\n"
      "examples:\n"
      "  # parse binary file test.wasm and write text file test.dcmp\n"
      "  $ wasm-decompile test.wasm -o test.dcmp\n";
    OptionParser parser("wasm-decompile", s_description);
    parser.AddOption(
        'o', "output", "FILENAME",
        "Output file for the decompiled file, by default use stdout",
        [&](const char* argument) {
          outfile = argument;
          ConvertBackslashToSlash(&outfile);
        });
    features.AddOptions(&parser);
    parser.AddOption("ignore-custom-section-errors",
                     "Ignore errors in custom sections",
                     [&]() { fail_on_custom_section_error = false; });
    parser.AddArgument("filename", OptionParser::ArgumentCount::One,
                       [&](const char* argument) {
                         infile = argument;
                         ConvertBackslashToSlash(&infile);
                       });
    parser.Parse(argc, argv);
  }

  std::vector<uint8_t> file_data;
  Result result = ReadFile(infile.c_str(), &file_data);
  if (Succeeded(result)) {
    Errors errors;
    Module module;
    const bool kStopOnFirstError = true;
    ReadBinaryOptions options(features, nullptr,
                              true, kStopOnFirstError,
                              fail_on_custom_section_error);
    result = ReadBinaryIr(infile.c_str(), file_data.data(), file_data.size(),
                          options, &errors, &module);
    if (Succeeded(result)) {
      ValidateOptions options(features);
      result = ValidateModule(&module, &errors, options);
      if (Succeeded(result)) {
        result = GenerateNames(&module,
                               static_cast<NameOpts>(NameOpts::AlphaNames));
      }
      if (Succeeded(result)) {
        // Must be called after ReadBinaryIr & GenerateNames, and before
        // ApplyNames, see comments at definition.
        RenameAll(module);
      }
      if (Succeeded(result)) {
        /* TODO(binji): This shouldn't fail; if a name can't be applied
         * (because the index is invalid, say) it should just be skipped. */
        Result dummy_result = ApplyNames(&module);
        WABT_USE(dummy_result);
      }
      if (Succeeded(result)) {
        auto s = Decompile(module, decompile_options);
        FileStream stream(!outfile.empty() ? FileStream(outfile)
                                             : FileStream(stdout));
        stream.WriteData(s.data(), s.size());
      }
    }
    FormatErrorsToFile(errors, Location::Type::Binary);
  }
  return result != Result::Ok;
}

int main(int argc, char** argv) {
  WABT_TRY
  return ProgramMain(argc, argv);
  WABT_CATCH_BAD_ALLOC_AND_EXIT
}
