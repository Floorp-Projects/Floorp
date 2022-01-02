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

#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <string>

#include "config.h"

#include "src/binary-writer.h"
#include "src/common.h"
#include "src/error-formatter.h"
#include "src/feature.h"
#include "src/filenames.h"
#include "src/ir.h"
#include "src/option-parser.h"
#include "src/resolve-names.h"
#include "src/stream.h"
#include "src/validator.h"
#include "src/wast-parser.h"

using namespace wabt;

static const char* s_infile;
static std::string s_outfile;
static bool s_dump_module;
static int s_verbose;
static WriteBinaryOptions s_write_binary_options;
static bool s_validate = true;
static bool s_debug_parsing;
static Features s_features;

static std::unique_ptr<FileStream> s_log_stream;

static const char s_description[] =
R"(  read a file in the wasm text format, check it for errors, and
  convert it to the wasm binary format.

examples:
  # parse and typecheck test.wat
  $ wat2wasm test.wat

  # parse test.wat and write to binary file test.wasm
  $ wat2wasm test.wat -o test.wasm

  # parse spec-test.wast, and write verbose output to stdout (including
  # the meaning of every byte)
  $ wat2wasm spec-test.wast -v
)";

static void ParseOptions(int argc, char* argv[]) {
  OptionParser parser("wat2wasm", s_description);

  parser.AddOption('v', "verbose", "Use multiple times for more info", []() {
    s_verbose++;
    s_log_stream = FileStream::CreateStderr();
  });
  parser.AddOption("debug-parser", "Turn on debugging the parser of wat files",
                   []() { s_debug_parsing = true; });
  parser.AddOption('d', "dump-module",
                   "Print a hexdump of the module to stdout",
                   []() { s_dump_module = true; });
  s_features.AddOptions(&parser);
  parser.AddOption('o', "output", "FILE", "output wasm binary file",
                   [](const char* argument) { s_outfile = argument; });
  parser.AddOption(
      'r', "relocatable",
      "Create a relocatable wasm binary (suitable for linking with e.g. lld)",
      []() { s_write_binary_options.relocatable = true; });
  parser.AddOption(
      "no-canonicalize-leb128s",
      "Write all LEB128 sizes as 5-bytes instead of their minimal size",
      []() { s_write_binary_options.canonicalize_lebs = false; });
  parser.AddOption("debug-names",
                   "Write debug names to the generated binary file",
                   []() { s_write_binary_options.write_debug_names = true; });
  parser.AddOption("no-check", "Don't check for invalid modules",
                   []() { s_validate = false; });
  parser.AddArgument("filename", OptionParser::ArgumentCount::One,
                     [](const char* argument) { s_infile = argument; });

  parser.Parse(argc, argv);
}

static void WriteBufferToFile(string_view filename,
                              const OutputBuffer& buffer) {
  if (s_dump_module) {
    std::unique_ptr<FileStream> stream = FileStream::CreateStdout();
    if (s_verbose) {
      stream->Writef(";; dump\n");
    }
    if (!buffer.data.empty()) {
      stream->WriteMemoryDump(buffer.data.data(), buffer.data.size());
    }
  }

  buffer.WriteToFile(filename);
}

static std::string DefaultOuputName(string_view input_name) {
  // Strip existing extension and add .wasm
  std::string result(StripExtension(GetBasename(input_name)));
  result += kWasmExtension;

  return result;
}

int ProgramMain(int argc, char** argv) {
  InitStdio();

  ParseOptions(argc, argv);

  std::vector<uint8_t> file_data;
  Result result = ReadFile(s_infile, &file_data);
  std::unique_ptr<WastLexer> lexer = WastLexer::CreateBufferLexer(
      s_infile, file_data.data(), file_data.size());
  if (Failed(result)) {
    WABT_FATAL("unable to read file: %s\n", s_infile);
  }

  Errors errors;
  std::unique_ptr<Module> module;
  WastParseOptions parse_wast_options(s_features);
  result = ParseWatModule(lexer.get(), &module, &errors, &parse_wast_options);

  if (Succeeded(result) && s_validate) {
    ValidateOptions options(s_features);
    result = ValidateModule(module.get(), &errors, options);
  }

  if (Succeeded(result)) {
    MemoryStream stream(s_log_stream.get());
    s_write_binary_options.features = s_features;
    result = WriteBinaryModule(&stream, module.get(), s_write_binary_options);

    if (Succeeded(result)) {
      if (s_outfile.empty()) {
        s_outfile = DefaultOuputName(s_infile);
      }
      WriteBufferToFile(s_outfile.c_str(), stream.output_buffer());
    }
  }

  auto line_finder = lexer->MakeLineFinder();
  FormatErrorsToFile(errors, Location::Type::Text, line_finder.get());

  return result != Result::Ok;
}

int main(int argc, char** argv) {
  WABT_TRY
  return ProgramMain(argc, argv);
  WABT_CATCH_BAD_ALLOC_AND_EXIT
}
