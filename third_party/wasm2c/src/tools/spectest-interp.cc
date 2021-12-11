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

#include <algorithm>
#include <cassert>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "src/binary-reader.h"
#include "src/cast.h"
#include "src/common.h"
#include "src/error-formatter.h"
#include "src/feature.h"
#include "src/interp/binary-reader-interp.h"
#include "src/interp/interp-util.h"
#include "src/interp/interp.h"
#include "src/literal.h"
#include "src/option-parser.h"
#include "src/stream.h"
#include "src/validator.h"
#include "src/wast-lexer.h"
#include "src/wast-parser.h"

using namespace wabt;
using namespace wabt::interp;

static int s_verbose;
static const char* s_infile;
static Thread::Options s_thread_options;
static Stream* s_trace_stream;
static Features s_features;

static std::unique_ptr<FileStream> s_log_stream;
static std::unique_ptr<FileStream> s_stdout_stream;

enum class RunVerbosity {
  Quiet = 0,
  Verbose = 1,
};

static const char s_description[] =
    R"(  read a Spectest JSON file, and run its tests in the interpreter.

examples:
  # parse test.json and run the spec tests
  $ spectest-interp test.json
)";

static void ParseOptions(int argc, char** argv) {
  OptionParser parser("spectest-interp", s_description);

  parser.AddOption('v', "verbose", "Use multiple times for more info", []() {
    s_verbose++;
    s_log_stream = FileStream::CreateStderr();
  });
  s_features.AddOptions(&parser);
  parser.AddOption('V', "value-stack-size", "SIZE",
                   "Size in elements of the value stack",
                   [](const std::string& argument) {
                     // TODO(binji): validate.
                     s_thread_options.value_stack_size = atoi(argument.c_str());
                   });
  parser.AddOption('C', "call-stack-size", "SIZE",
                   "Size in elements of the call stack",
                   [](const std::string& argument) {
                     // TODO(binji): validate.
                     s_thread_options.call_stack_size = atoi(argument.c_str());
                   });
  parser.AddOption('t', "trace", "Trace execution",
                   []() { s_trace_stream = s_stdout_stream.get(); });

  parser.AddArgument("filename", OptionParser::ArgumentCount::One,
                     [](const char* argument) { s_infile = argument; });
  parser.Parse(argc, argv);
}

namespace spectest {

class Command;
typedef std::unique_ptr<Command> CommandPtr;
typedef std::vector<CommandPtr> CommandPtrVector;

class Script {
 public:
  std::string filename;
  CommandPtrVector commands;
};

class Command {
 public:
  WABT_DISALLOW_COPY_AND_ASSIGN(Command);
  Command() = delete;
  virtual ~Command() = default;

  CommandType type;
  uint32_t line = 0;

 protected:
  explicit Command(CommandType type) : type(type) {}
};

template <CommandType TypeEnum>
class CommandMixin : public Command {
 public:
  static bool classof(const Command* cmd) { return cmd->type == TypeEnum; }
  CommandMixin() : Command(TypeEnum) {}
};

enum class ModuleType {
  Text,
  Binary,
};

class ModuleCommand : public CommandMixin<CommandType::Module> {
 public:
  ModuleType module = ModuleType::Binary;
  std::string filename;
  std::string name;
};

class Action {
 public:
  ActionType type = ActionType::Invoke;
  std::string module_name;
  std::string field_name;
  ValueTypes types;
  Values args;
};

template <CommandType TypeEnum>
class ActionCommandBase : public CommandMixin<TypeEnum> {
 public:
  Action action;
};

typedef ActionCommandBase<CommandType::Action> ActionCommand;

class RegisterCommand : public CommandMixin<CommandType::Register> {
 public:
  std::string as;
  std::string name;
};

struct ExpectedValue {
  TypedValue value;
  Type lane_type;             // Only valid if value.type == Type::V128.
  // Up to 4 NaN values used, depending on |value.type| and |lane_type|:
  //   | type  | lane_type | valid                 |
  //   | f32   |           | nan[0]                |
  //   | f64   |           | nan[0]                |
  //   | v128  | f32       | nan[0] through nan[3] |
  //   | v128  | f64       | nan[0],nan[1]         |
  //   | *     | *         | none valid            |
  ExpectedNan nan[4];
};

int LaneCountFromType(Type type) {
  switch (type) {
    case Type::I8: return 16;
    case Type::I16: return 8;
    case Type::I32: return 4;
    case Type::I64: return 2;
    case Type::F32: return 4;
    case Type::F64: return 2;
    default: assert(false); return 0;
  }
}

ExpectedValue GetLane(const ExpectedValue& ev, int lane) {
  int lane_count = LaneCountFromType(ev.lane_type);
  assert(ev.value.type == Type::V128);
  assert(lane < lane_count);

  ExpectedValue result;
  result.value.type = ev.lane_type;

  v128 vec = ev.value.value.Get<v128>();

  for (int lane = 0; lane < lane_count; ++lane) {
    switch (ev.lane_type) {
      case Type::I8:
        result.nan[0] = ExpectedNan::None;
        result.value.value.Set<u32>(vec.u8(lane));
        break;

      case Type::I16:
        result.nan[0] = ExpectedNan::None;
        result.value.value.Set<u32>(vec.u16(lane));
        break;

      case Type::I32:
        result.nan[0] = ExpectedNan::None;
        result.value.value.Set<u32>(vec.u32(lane));
        break;

      case Type::I64:
        result.nan[0] = ExpectedNan::None;
        result.value.value.Set<u64>(vec.u64(lane));
        break;

      case Type::F32:
        result.nan[0] = ev.nan[lane];
        result.value.value.Set<f32>(Bitcast<f32>(vec.f32_bits(lane)));
        break;

      case Type::F64:
        result.nan[0] = ev.nan[lane];
        result.value.value.Set<f64>(Bitcast<f64>(vec.f64_bits(lane)));
        break;

      default:
        WABT_UNREACHABLE;
    }
  }
  return result;
}

TypedValue GetLane(const TypedValue& tv, Type lane_type, int lane) {
  int lane_count = LaneCountFromType(lane_type);
  assert(tv.type == Type::V128);
  assert(lane < lane_count);

  TypedValue result;
  result.type = lane_type;

  v128 vec = tv.value.Get<v128>();

  for (int lane = 0; lane < lane_count; ++lane) {
    switch (lane_type) {
      case Type::I8:
        result.value.Set<u32>(vec.u8(lane));
        break;

      case Type::I16:
        result.value.Set<u32>(vec.u16(lane));
        break;

      case Type::I32:
        result.value.Set<u32>(vec.u32(lane));
        break;

      case Type::I64:
        result.value.Set<u64>(vec.u64(lane));
        break;

      case Type::F32:
        result.value.Set<f32>(Bitcast<f32>(vec.f32_bits(lane)));
        break;

      case Type::F64:
        result.value.Set<f64>(Bitcast<f64>(vec.f64_bits(lane)));
        break;

      default:
        WABT_UNREACHABLE;
    }
  }
  return result;
}

class AssertReturnCommand : public CommandMixin<CommandType::AssertReturn> {
 public:
  Action action;
  std::vector<ExpectedValue> expected;
};

template <CommandType TypeEnum>
class AssertTrapCommandBase : public CommandMixin<TypeEnum> {
 public:
  Action action;
  std::string text;
};

typedef AssertTrapCommandBase<CommandType::AssertTrap> AssertTrapCommand;
typedef AssertTrapCommandBase<CommandType::AssertExhaustion>
    AssertExhaustionCommand;

template <CommandType TypeEnum>
class AssertModuleCommand : public CommandMixin<TypeEnum> {
 public:
  ModuleType type = ModuleType::Binary;
  std::string filename;
  std::string text;
};

typedef AssertModuleCommand<CommandType::AssertMalformed>
    AssertMalformedCommand;
typedef AssertModuleCommand<CommandType::AssertInvalid> AssertInvalidCommand;
typedef AssertModuleCommand<CommandType::AssertUnlinkable>
    AssertUnlinkableCommand;
typedef AssertModuleCommand<CommandType::AssertUninstantiable>
    AssertUninstantiableCommand;

// An extremely simple JSON parser that only knows how to parse the expected
// format from wat2wasm.
class JSONParser {
 public:
  JSONParser() {}

  wabt::Result ReadFile(string_view spec_json_filename);
  wabt::Result ParseScript(Script* out_script);

 private:
  void WABT_PRINTF_FORMAT(2, 3) PrintError(const char* format, ...);

  // Whether to allow parsing of expectation-only forms (e.g. `nan:canonical`,
  // `nan:arithmetic`, etc.)
  enum class AllowExpected { No, Yes };

  void PutbackChar();
  int ReadChar();
  void SkipWhitespace();
  bool Match(const char* s);
  wabt::Result Expect(const char* s);
  wabt::Result ExpectKey(const char* key);
  wabt::Result ParseUint32(uint32_t* out_int);
  wabt::Result ParseString(std::string* out_string);
  wabt::Result ParseKeyStringValue(const char* key, std::string* out_string);
  wabt::Result ParseOptNameStringValue(std::string* out_string);
  wabt::Result ParseLine(uint32_t* out_line_number);
  wabt::Result ParseType(Type* out_type);
  wabt::Result ParseTypeObject(Type* out_type);
  wabt::Result ParseTypeVector(TypeVector* out_types);
  wabt::Result ParseConst(TypedValue* out_value);
  wabt::Result ParseI32Value(uint32_t* out_value, string_view value_str);
  wabt::Result ParseI64Value(uint64_t* out_value, string_view value_str);
  wabt::Result ParseF32Value(uint32_t* out_value,
                             ExpectedNan* out_nan,
                             string_view value_str,
                             AllowExpected);
  wabt::Result ParseF64Value(uint64_t* out_value,
                             ExpectedNan* out_nan,
                             string_view value_str,
                             AllowExpected);
  wabt::Result ParseLaneConstValue(Type lane_type,
                                   int lane,
                                   ExpectedValue* out_value,
                                   string_view value_str,
                                   AllowExpected);
  wabt::Result ParseConstValue(Type type,
                               Value* out_value,
                               ExpectedNan* out_nan,
                               string_view value_str,
                               AllowExpected);
  wabt::Result ParseConstVector(ValueTypes* out_types, Values* out_values);
  wabt::Result ParseExpectedValue(ExpectedValue* out_value, AllowExpected);
  wabt::Result ParseExpectedValues(std::vector<ExpectedValue>* out_values);
  wabt::Result ParseAction(Action* out_action);
  wabt::Result ParseActionResult();
  wabt::Result ParseModuleType(ModuleType* out_type);

  std::string CreateModulePath(string_view filename);
  wabt::Result ParseFilename(std::string* out_filename);
  wabt::Result ParseCommand(CommandPtr* out_command);

  // Parsing info.
  std::vector<uint8_t> json_data_;
  size_t json_offset_ = 0;
  Location loc_;
  Location prev_loc_;
  bool has_prev_loc_ = false;
};

#define EXPECT(x) CHECK_RESULT(Expect(x))
#define EXPECT_KEY(x) CHECK_RESULT(ExpectKey(x))
#define PARSE_KEY_STRING_VALUE(key, value) \
  CHECK_RESULT(ParseKeyStringValue(key, value))

wabt::Result JSONParser::ReadFile(string_view spec_json_filename) {
  loc_.filename = spec_json_filename;
  loc_.line = 1;
  loc_.first_column = 1;

  return wabt::ReadFile(spec_json_filename, &json_data_);
}

void JSONParser::PrintError(const char* format, ...) {
  WABT_SNPRINTF_ALLOCA(buffer, length, format);
  fprintf(stderr, "%s:%d:%d: %s\n", loc_.filename.to_string().c_str(),
          loc_.line, loc_.first_column, buffer);
}

void JSONParser::PutbackChar() {
  assert(has_prev_loc_);
  json_offset_--;
  loc_ = prev_loc_;
  has_prev_loc_ = false;
}

int JSONParser::ReadChar() {
  if (json_offset_ >= json_data_.size()) {
    return -1;
  }
  prev_loc_ = loc_;
  char c = json_data_[json_offset_++];
  if (c == '\n') {
    loc_.line++;
    loc_.first_column = 1;
  } else {
    loc_.first_column++;
  }
  has_prev_loc_ = true;
  return c;
}

void JSONParser::SkipWhitespace() {
  while (1) {
    switch (ReadChar()) {
      case -1:
        return;

      case ' ':
      case '\t':
      case '\n':
      case '\r':
        break;

      default:
        PutbackChar();
        return;
    }
  }
}

bool JSONParser::Match(const char* s) {
  SkipWhitespace();
  Location start_loc = loc_;
  size_t start_offset = json_offset_;
  while (*s && *s == ReadChar())
    s++;

  if (*s == 0) {
    return true;
  } else {
    json_offset_ = start_offset;
    loc_ = start_loc;
    return false;
  }
}

wabt::Result JSONParser::Expect(const char* s) {
  if (Match(s)) {
    return wabt::Result::Ok;
  } else {
    PrintError("expected %s", s);
    return wabt::Result::Error;
  }
}

wabt::Result JSONParser::ExpectKey(const char* key) {
  size_t keylen = strlen(key);
  size_t quoted_len = keylen + 2 + 1;
  char* quoted = static_cast<char*>(alloca(quoted_len));
  snprintf(quoted, quoted_len, "\"%s\"", key);
  EXPECT(quoted);
  EXPECT(":");
  return wabt::Result::Ok;
}

wabt::Result JSONParser::ParseUint32(uint32_t* out_int) {
  uint32_t result = 0;
  SkipWhitespace();
  while (1) {
    int c = ReadChar();
    if (c >= '0' && c <= '9') {
      uint32_t last_result = result;
      result = result * 10 + static_cast<uint32_t>(c - '0');
      if (result < last_result) {
        PrintError("uint32 overflow");
        return wabt::Result::Error;
      }
    } else {
      PutbackChar();
      break;
    }
  }
  *out_int = result;
  return wabt::Result::Ok;
}

wabt::Result JSONParser::ParseString(std::string* out_string) {
  out_string->clear();

  SkipWhitespace();
  if (ReadChar() != '"') {
    PrintError("expected string");
    return wabt::Result::Error;
  }

  while (1) {
    int c = ReadChar();
    if (c == '"') {
      break;
    } else if (c == '\\') {
      /* The only escape supported is \uxxxx. */
      c = ReadChar();
      if (c != 'u') {
        PrintError("expected escape: \\uxxxx");
        return wabt::Result::Error;
      }
      uint16_t code = 0;
      for (int i = 0; i < 4; ++i) {
        c = ReadChar();
        int cval;
        if (c >= '0' && c <= '9') {
          cval = c - '0';
        } else if (c >= 'a' && c <= 'f') {
          cval = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
          cval = c - 'A' + 10;
        } else {
          PrintError("expected hex char");
          return wabt::Result::Error;
        }
        code = (code << 4) + cval;
      }

      if (code < 256) {
        *out_string += code;
      } else {
        PrintError("only escape codes < 256 allowed, got %u\n", code);
      }
    } else {
      *out_string += c;
    }
  }
  return wabt::Result::Ok;
}

wabt::Result JSONParser::ParseKeyStringValue(const char* key,
                                             std::string* out_string) {
  out_string->clear();
  EXPECT_KEY(key);
  return ParseString(out_string);
}

wabt::Result JSONParser::ParseOptNameStringValue(std::string* out_string) {
  out_string->clear();
  if (Match("\"name\"")) {
    EXPECT(":");
    CHECK_RESULT(ParseString(out_string));
    EXPECT(",");
  }
  return wabt::Result::Ok;
}

wabt::Result JSONParser::ParseLine(uint32_t* out_line_number) {
  EXPECT_KEY("line");
  CHECK_RESULT(ParseUint32(out_line_number));
  return wabt::Result::Ok;
}

wabt::Result JSONParser::ParseType(Type* out_type) {
  std::string type_str;
  CHECK_RESULT(ParseString(&type_str));

  if (type_str == "i32") {
    *out_type = Type::I32;
  } else if (type_str == "f32") {
    *out_type = Type::F32;
  } else if (type_str == "i64") {
    *out_type = Type::I64;
  } else if (type_str == "f64") {
    *out_type = Type::F64;
  } else if (type_str == "v128") {
    *out_type = Type::V128;
  } else if (type_str == "i8") {
    *out_type = Type::I8;
  } else if (type_str == "i16") {
    *out_type = Type::I16;
  } else if (type_str == "funcref") {
    *out_type = Type::FuncRef;
  } else if (type_str == "externref") {
    *out_type = Type::ExternRef;
  } else {
    PrintError("unknown type: \"%s\"", type_str.c_str());
    return wabt::Result::Error;
  }
  return wabt::Result::Ok;
}

wabt::Result JSONParser::ParseTypeObject(Type* out_type) {
  EXPECT("{");
  EXPECT_KEY("type");
  CHECK_RESULT(ParseType(out_type));
  EXPECT("}");
  return wabt::Result::Ok;
}

wabt::Result JSONParser::ParseTypeVector(TypeVector* out_types) {
  out_types->clear();
  EXPECT("[");
  bool first = true;
  while (!Match("]")) {
    if (!first) {
      EXPECT(",");
    }
    Type type;
    CHECK_RESULT(ParseTypeObject(&type));
    first = false;
    out_types->push_back(type);
  }
  return wabt::Result::Ok;
}

wabt::Result JSONParser::ParseConst(TypedValue* out_value) {
  ExpectedValue expected;
  CHECK_RESULT(ParseExpectedValue(&expected, AllowExpected::No));
  *out_value = expected.value;
  return wabt::Result::Ok;
}

wabt::Result JSONParser::ParseI32Value(uint32_t* out_value,
                                       string_view value_str) {
  if (Failed(ParseInt32(value_str.begin(), value_str.end(), out_value,
                        ParseIntType::UnsignedOnly))) {
    PrintError("invalid i32 literal");
    return wabt::Result::Error;
  }
  return wabt::Result::Ok;
}

wabt::Result JSONParser::ParseI64Value(uint64_t* out_value,
                                       string_view value_str) {
  if (Failed(ParseInt64(value_str.begin(), value_str.end(), out_value,
                        ParseIntType::UnsignedOnly))) {
    PrintError("invalid i64 literal");
    return wabt::Result::Error;
  }
  return wabt::Result::Ok;
}

wabt::Result JSONParser::ParseF32Value(uint32_t* out_value,
                                       ExpectedNan* out_nan,
                                       string_view value_str,
                                       AllowExpected allow_expected) {
  if (allow_expected == AllowExpected::Yes) {
    *out_value = 0;
    if (value_str == "nan:canonical") {
      *out_nan = ExpectedNan::Canonical;
      return wabt::Result::Ok;
    } else if (value_str == "nan:arithmetic") {
      *out_nan = ExpectedNan::Arithmetic;
      return wabt::Result::Ok;
    }
  }

  *out_nan = ExpectedNan::None;
  if (Failed(ParseInt32(value_str.begin(), value_str.end(), out_value,
                        ParseIntType::UnsignedOnly))) {
    PrintError("invalid f32 literal");
    return wabt::Result::Error;
  }
  return wabt::Result::Ok;
}

wabt::Result JSONParser::ParseF64Value(uint64_t* out_value,
                                       ExpectedNan* out_nan,
                                       string_view value_str,
                                       AllowExpected allow_expected) {
  if (allow_expected == AllowExpected::Yes) {
    *out_value = 0;
    if (value_str == "nan:canonical") {
      *out_nan = ExpectedNan::Canonical;
      return wabt::Result::Ok;
    } else if (value_str == "nan:arithmetic") {
      *out_nan = ExpectedNan::Arithmetic;
      return wabt::Result::Ok;
    }
  }

  *out_nan = ExpectedNan::None;
  if (Failed(ParseInt64(value_str.begin(), value_str.end(), out_value,
                        ParseIntType::UnsignedOnly))) {
    PrintError("invalid f64 literal");
    return wabt::Result::Error;
  }
  return wabt::Result::Ok;
}

wabt::Result JSONParser::ParseLaneConstValue(Type lane_type,
                                             int lane,
                                             ExpectedValue* out_value,
                                             string_view value_str,
                                             AllowExpected allow_expected) {
  v128 v = out_value->value.value.Get<v128>();

  switch (lane_type) {
    case Type::I8: {
      uint32_t value;
      CHECK_RESULT(ParseI32Value(&value, value_str));
      v.set_u8(lane, value);
      break;
    }

    case Type::I16: {
      uint32_t value;
      CHECK_RESULT(ParseI32Value(&value, value_str));
      v.set_u16(lane, value);
      break;
    }

    case Type::I32: {
      uint32_t value;
      CHECK_RESULT(ParseI32Value(&value, value_str));
      v.set_u32(lane, value);
      break;
    }

    case Type::I64: {
      uint64_t value;
      CHECK_RESULT(ParseI64Value(&value, value_str));
      v.set_u64(lane, value);
      break;
    }

    case Type::F32: {
      ExpectedNan nan;
      uint32_t value_bits;
      CHECK_RESULT(ParseF32Value(&value_bits, &nan, value_str, allow_expected));
      v.set_f32_bits(lane, value_bits);
      assert(lane < 4);
      out_value->nan[lane] = nan;
      break;
    }

    case Type::F64: {
      ExpectedNan nan;
      uint64_t value_bits;
      CHECK_RESULT(ParseF64Value(&value_bits, &nan, value_str, allow_expected));
      v.set_f64_bits(lane, value_bits);
      assert(lane < 2);
      out_value->nan[lane] = nan;
      break;
    }

    default:
      PrintError("unknown concrete type: \"%s\"", lane_type.GetName());
      return wabt::Result::Error;
  }

  out_value->value.value.Set<v128>(v);
  return wabt::Result::Ok;
}

wabt::Result JSONParser::ParseConstValue(Type type,
                                         Value* out_value,
                                         ExpectedNan* out_nan,
                                         string_view value_str,
                                         AllowExpected allow_expected) {
  *out_nan = ExpectedNan::None;

  switch (type) {
    case Type::I32: {
      uint32_t value;
      CHECK_RESULT(ParseI32Value(&value, value_str));
      out_value->Set(value);
      break;
    }

    case Type::F32: {
      uint32_t value_bits;
      CHECK_RESULT(
          ParseF32Value(&value_bits, out_nan, value_str, allow_expected));
      out_value->Set(Bitcast<f32>(value_bits));
      break;
    }

    case Type::I64: {
      uint64_t value;
      CHECK_RESULT(ParseI64Value(&value, value_str));
      out_value->Set(value);
      break;
    }

    case Type::F64: {
      uint64_t value_bits;
      CHECK_RESULT(
          ParseF64Value(&value_bits, out_nan, value_str, allow_expected));
      out_value->Set(Bitcast<f64>(value_bits));
      break;
    }

    case Type::V128:
      assert(false);  // Should use ParseLaneConstValue instead.
      break;

    case Type::FuncRef:
      if (value_str == "null") {
        out_value->Set(Ref::Null);
      } else {
        assert(allow_expected == AllowExpected::Yes);
        out_value->Set(Ref{1});
      }
      break;

    case Type::ExternRef:
      if (value_str == "null") {
        out_value->Set(Ref::Null);
      } else {
        uint32_t value;
        CHECK_RESULT(ParseI32Value(&value, value_str));
        // TODO: hack, just whatever ref is at this index; but skip null (which
        // is always 0).
        out_value->Set(Ref{value + 1});
      }
      break;

    default:
      PrintError("unknown concrete type: \"%s\"", type.GetName());
      return wabt::Result::Error;
  }

  return wabt::Result::Ok;
}

wabt::Result JSONParser::ParseExpectedValue(ExpectedValue* out_value,
                                            AllowExpected allow_expected) {
  Type type;
  std::string value_str;
  EXPECT("{");
  EXPECT_KEY("type");
  CHECK_RESULT(ParseType(&type));
  EXPECT(",");
  if (type == Type::V128) {
    Type lane_type;
    EXPECT_KEY("lane_type");
    CHECK_RESULT(ParseType(&lane_type));
    EXPECT(",");
    EXPECT_KEY("value");
    EXPECT("[");

    int lane_count = LaneCountFromType(lane_type);
    for (int lane = 0; lane < lane_count; ++lane) {
      CHECK_RESULT(ParseString(&value_str));
      CHECK_RESULT(ParseLaneConstValue(lane_type, lane, out_value, value_str,
                                       allow_expected));
      if (lane < lane_count - 1) {
        EXPECT(",");
      }
    }
    EXPECT("]");
    out_value->value.type = type;
    out_value->lane_type = lane_type;
  } else {
    PARSE_KEY_STRING_VALUE("value", &value_str);
    CHECK_RESULT(ParseConstValue(type, &out_value->value.value,
                                 &out_value->nan[0], value_str,
                                 allow_expected));
    out_value->value.type = type;
  }
  EXPECT("}");

  return wabt::Result::Ok;
}

wabt::Result JSONParser::ParseExpectedValues(
    std::vector<ExpectedValue>* out_values) {
  out_values->clear();
  EXPECT("[");
  bool first = true;
  while (!Match("]")) {
    if (!first) {
      EXPECT(",");
    }
    ExpectedValue value;
    CHECK_RESULT(ParseExpectedValue(&value, AllowExpected::Yes));
    out_values->push_back(value);
    first = false;
  }
  return wabt::Result::Ok;
}

wabt::Result JSONParser::ParseConstVector(ValueTypes* out_types, Values* out_values) {
  out_values->clear();
  EXPECT("[");
  bool first = true;
  while (!Match("]")) {
    if (!first) {
      EXPECT(",");
    }
    TypedValue tv;
    CHECK_RESULT(ParseConst(&tv));
    out_types->push_back(tv.type);
    out_values->push_back(tv.value);
    first = false;
  }
  return wabt::Result::Ok;
}

wabt::Result JSONParser::ParseAction(Action* out_action) {
  EXPECT_KEY("action");
  EXPECT("{");
  EXPECT_KEY("type");
  if (Match("\"invoke\"")) {
    out_action->type = ActionType::Invoke;
  } else {
    EXPECT("\"get\"");
    out_action->type = ActionType::Get;
  }
  EXPECT(",");
  if (Match("\"module\"")) {
    EXPECT(":");
    CHECK_RESULT(ParseString(&out_action->module_name));
    EXPECT(",");
  }
  PARSE_KEY_STRING_VALUE("field", &out_action->field_name);
  if (out_action->type == ActionType::Invoke) {
    EXPECT(",");
    EXPECT_KEY("args");
    CHECK_RESULT(ParseConstVector(&out_action->types, &out_action->args));
  }
  EXPECT("}");
  return wabt::Result::Ok;
}

wabt::Result JSONParser::ParseActionResult() {
  // Not needed for wabt-interp, but useful for other parsers.
  EXPECT_KEY("expected");
  TypeVector expected;
  CHECK_RESULT(ParseTypeVector(&expected));
  return wabt::Result::Ok;
}

wabt::Result JSONParser::ParseModuleType(ModuleType* out_type) {
  std::string module_type_str;

  PARSE_KEY_STRING_VALUE("module_type", &module_type_str);
  if (module_type_str == "text") {
    *out_type = ModuleType::Text;
    return wabt::Result::Ok;
  } else if (module_type_str == "binary") {
    *out_type = ModuleType::Binary;
    return wabt::Result::Ok;
  } else {
    PrintError("unknown module type: \"%s\"", module_type_str.c_str());
    return wabt::Result::Error;
  }
}

static string_view GetDirname(string_view path) {
  // Strip everything after and including the last slash (or backslash), e.g.:
  //
  // s = "foo/bar/baz", => "foo/bar"
  // s = "/usr/local/include/stdio.h", => "/usr/local/include"
  // s = "foo.bar", => ""
  // s = "some\windows\directory", => "some\windows"
  size_t last_slash = path.find_last_of('/');
  size_t last_backslash = path.find_last_of('\\');
  if (last_slash == string_view::npos) {
    last_slash = 0;
  }
  if (last_backslash == string_view::npos) {
    last_backslash = 0;
  }

  return path.substr(0, std::max(last_slash, last_backslash));
}

std::string JSONParser::CreateModulePath(string_view filename) {
  string_view spec_json_filename = loc_.filename;
  string_view dirname = GetDirname(spec_json_filename);
  std::string path;

  if (dirname.size() == 0) {
    path = filename.to_string();
  } else {
    path = dirname.to_string();
    path += '/';
    path += filename.to_string();
  }

  ConvertBackslashToSlash(&path);
  return path;
}

wabt::Result JSONParser::ParseFilename(std::string* out_filename) {
  PARSE_KEY_STRING_VALUE("filename", out_filename);
  *out_filename = CreateModulePath(*out_filename);
  return wabt::Result::Ok;
}

wabt::Result JSONParser::ParseCommand(CommandPtr* out_command) {
  EXPECT("{");
  EXPECT_KEY("type");
  if (Match("\"module\"")) {
    auto command = MakeUnique<ModuleCommand>();
    EXPECT(",");
    CHECK_RESULT(ParseLine(&command->line));
    EXPECT(",");
    CHECK_RESULT(ParseOptNameStringValue(&command->name));
    CHECK_RESULT(ParseFilename(&command->filename));
    *out_command = std::move(command);
  } else if (Match("\"action\"")) {
    auto command = MakeUnique<ActionCommand>();
    EXPECT(",");
    CHECK_RESULT(ParseLine(&command->line));
    EXPECT(",");
    CHECK_RESULT(ParseAction(&command->action));
    EXPECT(",");
    CHECK_RESULT(ParseActionResult());
    *out_command = std::move(command);
  } else if (Match("\"register\"")) {
    auto command = MakeUnique<RegisterCommand>();
    EXPECT(",");
    CHECK_RESULT(ParseLine(&command->line));
    EXPECT(",");
    CHECK_RESULT(ParseOptNameStringValue(&command->name));
    PARSE_KEY_STRING_VALUE("as", &command->as);
    *out_command = std::move(command);
  } else if (Match("\"assert_malformed\"")) {
    auto command = MakeUnique<AssertMalformedCommand>();
    EXPECT(",");
    CHECK_RESULT(ParseLine(&command->line));
    EXPECT(",");
    CHECK_RESULT(ParseFilename(&command->filename));
    EXPECT(",");
    PARSE_KEY_STRING_VALUE("text", &command->text);
    EXPECT(",");
    CHECK_RESULT(ParseModuleType(&command->type));
    *out_command = std::move(command);
  } else if (Match("\"assert_invalid\"")) {
    auto command = MakeUnique<AssertInvalidCommand>();
    EXPECT(",");
    CHECK_RESULT(ParseLine(&command->line));
    EXPECT(",");
    CHECK_RESULT(ParseFilename(&command->filename));
    EXPECT(",");
    PARSE_KEY_STRING_VALUE("text", &command->text);
    EXPECT(",");
    CHECK_RESULT(ParseModuleType(&command->type));
    *out_command = std::move(command);
  } else if (Match("\"assert_unlinkable\"")) {
    auto command = MakeUnique<AssertUnlinkableCommand>();
    EXPECT(",");
    CHECK_RESULT(ParseLine(&command->line));
    EXPECT(",");
    CHECK_RESULT(ParseFilename(&command->filename));
    EXPECT(",");
    PARSE_KEY_STRING_VALUE("text", &command->text);
    EXPECT(",");
    CHECK_RESULT(ParseModuleType(&command->type));
    *out_command = std::move(command);
  } else if (Match("\"assert_uninstantiable\"")) {
    auto command = MakeUnique<AssertUninstantiableCommand>();
    EXPECT(",");
    CHECK_RESULT(ParseLine(&command->line));
    EXPECT(",");
    CHECK_RESULT(ParseFilename(&command->filename));
    EXPECT(",");
    PARSE_KEY_STRING_VALUE("text", &command->text);
    EXPECT(",");
    CHECK_RESULT(ParseModuleType(&command->type));
    *out_command = std::move(command);
  } else if (Match("\"assert_return\"")) {
    auto command = MakeUnique<AssertReturnCommand>();
    EXPECT(",");
    CHECK_RESULT(ParseLine(&command->line));
    EXPECT(",");
    CHECK_RESULT(ParseAction(&command->action));
    EXPECT(",");
    EXPECT_KEY("expected");
    CHECK_RESULT(ParseExpectedValues(&command->expected));
    *out_command = std::move(command);
  } else if (Match("\"assert_trap\"")) {
    auto command = MakeUnique<AssertTrapCommand>();
    EXPECT(",");
    CHECK_RESULT(ParseLine(&command->line));
    EXPECT(",");
    CHECK_RESULT(ParseAction(&command->action));
    EXPECT(",");
    PARSE_KEY_STRING_VALUE("text", &command->text);
    EXPECT(",");
    CHECK_RESULT(ParseActionResult());
    *out_command = std::move(command);
  } else if (Match("\"assert_exhaustion\"")) {
    auto command = MakeUnique<AssertExhaustionCommand>();
    EXPECT(",");
    CHECK_RESULT(ParseLine(&command->line));
    EXPECT(",");
    CHECK_RESULT(ParseAction(&command->action));
    EXPECT(",");
    PARSE_KEY_STRING_VALUE("text", &command->text);
    EXPECT(",");
    CHECK_RESULT(ParseActionResult());
    *out_command = std::move(command);
  } else {
    PrintError("unknown command type");
    return wabt::Result::Error;
  }
  EXPECT("}");
  return wabt::Result::Ok;
}

wabt::Result JSONParser::ParseScript(Script* out_script) {
  EXPECT("{");
  PARSE_KEY_STRING_VALUE("source_filename", &out_script->filename);
  EXPECT(",");
  EXPECT_KEY("commands");
  EXPECT("[");
  bool first = true;
  while (!Match("]")) {
    CommandPtr command;
    if (!first) {
      EXPECT(",");
    }
    CHECK_RESULT(ParseCommand(&command));
    out_script->commands.push_back(std::move(command));
    first = false;
  }
  EXPECT("}");
  return wabt::Result::Ok;
}

struct ActionResult {
  ValueTypes types;
  Values values;
  Trap::Ptr trap;
};

class CommandRunner {
 public:
  CommandRunner();
  wabt::Result Run(const Script& script);

  int passed() const { return passed_; }
  int total() const { return total_; }

 private:
  using ExportMap = std::map<std::string, Extern::Ptr>;
  using Registry = std::map<std::string, ExportMap>;

  void WABT_PRINTF_FORMAT(3, 4)
      PrintError(uint32_t line_number, const char* format, ...);
  ActionResult RunAction(int line_number,
                         const Action* action,
                         RunVerbosity verbose);

  interp::Module::Ptr ReadModule(string_view module_filename, Errors* errors);
  Extern::Ptr GetImport(const std::string&, const std::string&);
  void PopulateImports(const interp::Module::Ptr&, RefVec*);
  void PopulateExports(const Instance::Ptr&, ExportMap*);

  wabt::Result OnModuleCommand(const ModuleCommand*);
  wabt::Result OnActionCommand(const ActionCommand*);
  wabt::Result OnRegisterCommand(const RegisterCommand*);
  wabt::Result OnAssertMalformedCommand(const AssertMalformedCommand*);
  wabt::Result OnAssertUnlinkableCommand(const AssertUnlinkableCommand*);
  wabt::Result OnAssertInvalidCommand(const AssertInvalidCommand*);
  wabt::Result OnAssertUninstantiableCommand(
      const AssertUninstantiableCommand*);
  wabt::Result OnAssertReturnCommand(const AssertReturnCommand*);
  wabt::Result OnAssertTrapCommand(const AssertTrapCommand*);
  wabt::Result OnAssertExhaustionCommand(const AssertExhaustionCommand*);

  wabt::Result CheckAssertReturnResult(const AssertReturnCommand* command,
                                       int index,
                                       ExpectedValue expected,
                                       TypedValue actual,
                                       bool print_error);

  void TallyCommand(wabt::Result);

  wabt::Result ReadInvalidTextModule(string_view module_filename,
                                     const std::string& header);
  wabt::Result ReadInvalidModule(int line_number,
                           string_view module_filename,
                           ModuleType module_type,
                           const char* desc);
  wabt::Result ReadUnlinkableModule(int line_number,
                              string_view module_filename,
                              ModuleType module_type,
                              const char* desc);

  Store store_;
  Registry registry_;  // Used when importing.
  Registry instances_;  // Used when referencing module by name in invoke.
  ExportMap last_instance_;
  int passed_ = 0;
  int total_ = 0;

  std::string source_filename_;
};

CommandRunner::CommandRunner() : store_(s_features) {
  auto&& spectest = registry_["spectest"];

  // Initialize print functions for the spec test.
  struct {
    const char* name;
    interp::FuncType type;
  } const print_funcs[] = {
      {"print", interp::FuncType{{}, {}}},
      {"print_i32", interp::FuncType{{ValueType::I32}, {}}},
      {"print_f32", interp::FuncType{{ValueType::F32}, {}}},
      {"print_f64", interp::FuncType{{ValueType::F64}, {}}},
      {"print_i32_f32", interp::FuncType{{ValueType::I32, ValueType::F32}, {}}},
      {"print_f64_f64", interp::FuncType{{ValueType::F64, ValueType::F64}, {}}},
  };

  for (auto&& print : print_funcs) {
    auto import_name = StringPrintf("spectest.%s", print.name);
    spectest[print.name] = HostFunc::New(
        store_, print.type,
        [=](Thread& inst, const Values& params, Values& results,
            Trap::Ptr* trap) -> wabt::Result {
          printf("called host ");
          WriteCall(s_stdout_stream.get(), import_name, print.type, params,
                    results, *trap);
          return wabt::Result::Ok;
        });
  }

  spectest["table"] =
      interp::Table::New(store_, TableType{ValueType::FuncRef, Limits{10, 20}});

  spectest["memory"] = interp::Memory::New(store_, MemoryType{Limits{1, 2}});

  spectest["global_i32"] = interp::Global::New(
      store_, GlobalType{ValueType::I32, Mutability::Const}, Value::Make(u32{666}));
  spectest["global_i64"] = interp::Global::New(
      store_, GlobalType{ValueType::I64, Mutability::Const}, Value::Make(u64{666}));
  spectest["global_f32"] = interp::Global::New(
      store_, GlobalType{ValueType::F32, Mutability::Const}, Value::Make(f32{666}));
  spectest["global_f64"] = interp::Global::New(
      store_, GlobalType{ValueType::F64, Mutability::Const}, Value::Make(f64{666}));
}

wabt::Result CommandRunner::Run(const Script& script) {
  source_filename_ = script.filename;

  for (const CommandPtr& command : script.commands) {
    switch (command->type) {
      case CommandType::Module:
        OnModuleCommand(cast<ModuleCommand>(command.get()));
        break;

      case CommandType::Action:
        TallyCommand(OnActionCommand(cast<ActionCommand>(command.get())));
        break;

      case CommandType::Register:
        OnRegisterCommand(cast<RegisterCommand>(command.get()));
        break;

      case CommandType::AssertMalformed:
        TallyCommand(OnAssertMalformedCommand(
            cast<AssertMalformedCommand>(command.get())));
        break;

      case CommandType::AssertInvalid:
        TallyCommand(
            OnAssertInvalidCommand(cast<AssertInvalidCommand>(command.get())));
        break;

      case CommandType::AssertUnlinkable:
        TallyCommand(OnAssertUnlinkableCommand(
            cast<AssertUnlinkableCommand>(command.get())));
        break;

      case CommandType::AssertUninstantiable:
        TallyCommand(OnAssertUninstantiableCommand(
            cast<AssertUninstantiableCommand>(command.get())));
        break;

      case CommandType::AssertReturn:
        TallyCommand(
            OnAssertReturnCommand(cast<AssertReturnCommand>(command.get())));
        break;

      case CommandType::AssertTrap:
        TallyCommand(
            OnAssertTrapCommand(cast<AssertTrapCommand>(command.get())));
        break;

      case CommandType::AssertExhaustion:
        TallyCommand(OnAssertExhaustionCommand(
            cast<AssertExhaustionCommand>(command.get())));
        break;
    }
  }

  return wabt::Result::Ok;
}

void CommandRunner::PrintError(uint32_t line_number, const char* format, ...) {
  WABT_SNPRINTF_ALLOCA(buffer, length, format);
  printf("%s:%u: %s\n", source_filename_.c_str(), line_number, buffer);
}

ActionResult CommandRunner::RunAction(int line_number,
                                      const Action* action,
                                      RunVerbosity verbose) {
  ExportMap& module = !action->module_name.empty()
                          ? instances_[action->module_name]
                          : last_instance_;
  Extern::Ptr extern_ = module[action->field_name];
  if (!extern_) {
    PrintError(line_number, "unknown invoke \"%s.%s\"",
               action->module_name.c_str(), action->field_name.c_str());
    return {};
  }

  ActionResult result;

  switch (action->type) {
    case ActionType::Invoke: {
      auto* func = cast<interp::Func>(extern_.get());
      func->Call(store_, action->args, result.values, &result.trap,
                 s_trace_stream);
      result.types = func->type().results;
      if (verbose == RunVerbosity::Verbose) {
        WriteCall(s_stdout_stream.get(), action->field_name, func->type(),
                  action->args, result.values, result.trap);
      }
      break;
    }

    case ActionType::Get: {
      auto* global = cast<interp::Global>(extern_.get());
      result.values.push_back(global->Get());
      result.types.push_back(global->type().type);
      break;
    }

    default:
      WABT_UNREACHABLE;
  }

  return result;
}

wabt::Result CommandRunner::ReadInvalidTextModule(string_view module_filename,
                                            const std::string& header) {
  std::vector<uint8_t> file_data;
  wabt::Result result = ReadFile(module_filename, &file_data);
  std::unique_ptr<WastLexer> lexer = WastLexer::CreateBufferLexer(
      module_filename, file_data.data(), file_data.size());
  Errors errors;
  if (Succeeded(result)) {
    std::unique_ptr<wabt::Module> module;
    WastParseOptions options(s_features);
    result = ParseWatModule(lexer.get(), &module, &errors, &options);
  }

  auto line_finder = lexer->MakeLineFinder();
  FormatErrorsToFile(errors, Location::Type::Text, line_finder.get(), stdout,
                     header, PrintHeader::Once);
  return result;
}

interp::Module::Ptr CommandRunner::ReadModule(string_view module_filename,
                                               Errors* errors) {
  std::vector<uint8_t> file_data;

  if (Failed(ReadFile(module_filename, &file_data))) {
    return {};
  }

  const bool kReadDebugNames = true;
  const bool kStopOnFirstError = true;
  const bool kFailOnCustomSectionError = true;
  ReadBinaryOptions options(s_features, s_log_stream.get(), kReadDebugNames,
                            kStopOnFirstError, kFailOnCustomSectionError);
  ModuleDesc module_desc;
  if (Failed(ReadBinaryInterp(file_data.data(), file_data.size(), options,
                              errors, &module_desc))) {
    return {};
  }

  if (s_verbose) {
    module_desc.istream.Disassemble(s_stdout_stream.get());
  }

  return interp::Module::New(store_, module_desc);
}

wabt::Result CommandRunner::ReadInvalidModule(int line_number,
                                        string_view module_filename,
                                        ModuleType module_type,
                                        const char* desc) {
  std::string header = StringPrintf(
      "%s:%d: %s passed", source_filename_.c_str(), line_number, desc);

  switch (module_type) {
    case ModuleType::Text: {
      return ReadInvalidTextModule(module_filename, header);
    }

    case ModuleType::Binary: {
      Errors errors;
      auto module = ReadModule(module_filename, &errors);
      if (!module) {
        FormatErrorsToFile(errors, Location::Type::Binary, {}, stdout, header,
                           PrintHeader::Once);
        return wabt::Result::Error;
      } else {
        return wabt::Result::Ok;
      }
    }
  }

  WABT_UNREACHABLE;
}

Extern::Ptr CommandRunner::GetImport(const std::string& module,
                                     const std::string& name) {
  auto mod_iter = registry_.find(module);
  if (mod_iter != registry_.end()) {
    auto extern_iter = mod_iter->second.find(name);
    if (extern_iter != mod_iter->second.end()) {
      return extern_iter->second;
    }
  }
  return {};
}

void CommandRunner::PopulateImports(const interp::Module::Ptr& module,
                                    RefVec* imports) {
  for (auto&& import : module->desc().imports) {
    auto extern_ = GetImport(import.type.module, import.type.name);
    imports->push_back(extern_ ? extern_.ref() : Ref::Null);
  }
}

void CommandRunner::PopulateExports(const Instance::Ptr& instance,
                                    ExportMap* map) {
  map->clear();
  interp::Module::Ptr module{store_, instance->module()};
  for (size_t i = 0; i < module->export_types().size(); ++i) {
    const ExportType& export_type = module->export_types()[i];
    (*map)[export_type.name] = store_.UnsafeGet<Extern>(instance->exports()[i]);
  }
}

wabt::Result CommandRunner::OnModuleCommand(const ModuleCommand* command) {
  Errors errors;
  auto module = ReadModule(command->filename, &errors);
  FormatErrorsToFile(errors, Location::Type::Binary);

  if (!module) {
    PrintError(command->line, "error reading module: \"%s\"",
               command->filename.c_str());
    return wabt::Result::Error;
  }

  RefVec imports;
  PopulateImports(module, &imports);

  Trap::Ptr trap;
  auto instance = Instance::Instantiate(store_, module.ref(), imports, &trap);
  if (trap) {
    assert(!instance);
    PrintError(command->line, "error instantiating module: \"%s\"",
               trap->message().c_str());
    return wabt::Result::Error;
  }

  PopulateExports(instance, &last_instance_);
  if (!command->name.empty()) {
    instances_[command->name] = last_instance_;
  }

  return wabt::Result::Ok;
}

wabt::Result CommandRunner::OnActionCommand(const ActionCommand* command) {
  ActionResult result =
      RunAction(command->line, &command->action, RunVerbosity::Verbose);

  if (result.trap) {
    PrintError(command->line, "unexpected trap: %s",
               result.trap->message().c_str());
    return wabt::Result::Error;
  }

  return wabt::Result::Ok;
}

wabt::Result CommandRunner::OnAssertMalformedCommand(
    const AssertMalformedCommand* command) {
  wabt::Result result = ReadInvalidModule(command->line, command->filename,
                                    command->type, "assert_malformed");
  if (Succeeded(result)) {
    PrintError(command->line, "expected module to be malformed: \"%s\"",
               command->filename.c_str());
    return wabt::Result::Error;
  }

  return wabt::Result::Ok;
}

wabt::Result CommandRunner::OnRegisterCommand(const RegisterCommand* command) {
  if (!command->name.empty()) {
    auto instance_iter = instances_.find(command->name);
    if (instance_iter == instances_.end()) {
      PrintError(command->line, "unknown module in register");
      return wabt::Result::Error;
    }
    registry_[command->as] = instance_iter->second;
  } else {
    registry_[command->as] = last_instance_;
  }

  return wabt::Result::Ok;
}

wabt::Result CommandRunner::OnAssertUnlinkableCommand(
    const AssertUnlinkableCommand* command) {
  Errors errors;
  auto module = ReadModule(command->filename, &errors);

  if (!module) {
    PrintError(command->line, "unable to compile unlinkable module: \"%s\"",
               command->filename.c_str());
    return wabt::Result::Error;
  }

  RefVec imports;
  PopulateImports(module, &imports);

  Trap::Ptr trap;
  auto instance = Instance::Instantiate(store_, module.ref(), imports, &trap);
  if (!trap) {
    PrintError(command->line, "expected module to be unlinkable: \"%s\"",
               command->filename.c_str());
    return wabt::Result::Error;
  }

  // TODO: Change to one-line error.
  PrintError(command->line, "assert_unlinkable passed:\n  error: %s",
             trap->message().c_str());
  return wabt::Result::Ok;
}

wabt::Result CommandRunner::OnAssertInvalidCommand(
    const AssertInvalidCommand* command) {
  wabt::Result result = ReadInvalidModule(command->line, command->filename,
                                    command->type, "assert_invalid");
  if (Succeeded(result)) {
    PrintError(command->line, "expected module to be invalid: \"%s\"",
               command->filename.c_str());
    return wabt::Result::Error;
  }

  return wabt::Result::Ok;
}

wabt::Result CommandRunner::OnAssertUninstantiableCommand(
    const AssertUninstantiableCommand* command) {
  Errors errors;
  auto module = ReadModule(command->filename, &errors);

  if (!module) {
    PrintError(command->line, "unable to compile uninstantiable module: \"%s\"",
               command->filename.c_str());
    return wabt::Result::Error;
  }

  RefVec imports;
  PopulateImports(module, &imports);

  Trap::Ptr trap;
  auto instance = Instance::Instantiate(store_, module.ref(), imports, &trap);
  if (!trap) {
    PrintError(command->line, "expected module to be uninstantiable: \"%s\"",
               command->filename.c_str());
    return wabt::Result::Error;
  }

  // TODO: print error when assertion passes.
#if 0
  PrintError(command->line, "assert_uninstantiable passed: %s",
             trap->message().c_str());
#endif
  return wabt::Result::Ok;
}

static bool WABT_VECTORCALL IsCanonicalNan(f32 val) {
  const u32 kQuietNan = 0x7fc00000U;
  const u32 kQuietNegNan = 0xffc00000U;
  u32 bits = Bitcast<u32>(val);
  return bits == kQuietNan || bits == kQuietNegNan;
}

static bool WABT_VECTORCALL IsCanonicalNan(f64 val) {
  const u64 kQuietNan = 0x7ff8000000000000ULL;
  const u64 kQuietNegNan = 0xfff8000000000000ULL;
  u64 bits = Bitcast<u64>(val);
  return bits == kQuietNan || bits == kQuietNegNan;
}

static bool WABT_VECTORCALL IsArithmeticNan(f32 val) {
  const u32 kQuietNan = 0x7fc00000U;
  return (Bitcast<u32>(val) & kQuietNan) == kQuietNan;
}

static bool WABT_VECTORCALL IsArithmeticNan(f64 val) {
  const u64 kQuietNan = 0x7ff8000000000000ULL;
  return (Bitcast<u64>(val) & kQuietNan) == kQuietNan;
}

static std::string ExpectedValueToString(const ExpectedValue& ev) {
  // Extend TypedValueToString to print expected nan values too.
  switch (ev.value.type) {
    case Type::F32:
    case Type::F64:
      switch (ev.nan[0]) {
        case ExpectedNan::None:
          return TypedValueToString(ev.value);

        case ExpectedNan::Arithmetic:
          return StringPrintf("%s:nan:arithmetic", ev.value.type.GetName());

        case ExpectedNan::Canonical:
          return StringPrintf("%s:nan:canonical", ev.value.type.GetName());
      }
      break;

    case Type::V128: {
      int lane_count = LaneCountFromType(ev.lane_type);
      std::string result = "v128 ";
      for (int lane = 0; lane < lane_count; ++lane) {
        result += ExpectedValueToString(GetLane(ev, lane));
      }
      return result;
    }

    default:
      break;
  }
  return TypedValueToString(ev.value);
}

wabt::Result CommandRunner::CheckAssertReturnResult(
    const AssertReturnCommand* command,
    int index,
    ExpectedValue expected,
    TypedValue actual,
    bool print_error) {
  assert(expected.value.type == actual.type ||
         IsReference(expected.value.type));
  bool ok = true;
  switch (expected.value.type) {
    case Type::I8:
    case Type::I16:
    case Type::I32:
      ok = expected.value.value.Get<u32>() == actual.value.Get<u32>();
      break;

    case Type::I64:
      ok = expected.value.value.Get<u64>() == actual.value.Get<u64>();
      break;

    case Type::F32:
      switch (expected.nan[0]) {
        case ExpectedNan::Arithmetic:
          ok = IsArithmeticNan(actual.value.Get<f32>());
          break;

        case ExpectedNan::Canonical:
          ok = IsCanonicalNan(actual.value.Get<f32>());
          break;

        case ExpectedNan::None:
          ok = Bitcast<u32>(expected.value.value.Get<f32>()) ==
               Bitcast<u32>(actual.value.Get<f32>());
          break;
      }
      break;

    case Type::F64:
      switch (expected.nan[0]) {
        case ExpectedNan::Arithmetic:
          ok = IsArithmeticNan(actual.value.Get<f64>());
          break;

        case ExpectedNan::Canonical:
          ok = IsCanonicalNan(actual.value.Get<f64>());
          break;

        case ExpectedNan::None:
          ok = Bitcast<u64>(expected.value.value.Get<f64>()) ==
               Bitcast<u64>(actual.value.Get<f64>());
          break;
      }
      break;

    case Type::V128: {
      // Compare each lane as if it were its own value.
      for (int lane = 0; lane < LaneCountFromType(expected.lane_type); ++lane) {
        ExpectedValue lane_expected = GetLane(expected, lane);
        TypedValue lane_actual = GetLane(actual, expected.lane_type, lane);

        if (Failed(CheckAssertReturnResult(command, index, lane_expected,
                                           lane_actual, false))) {
          PrintError(command->line,
                     "mismatch in lane %u of result %u of assert_return: "
                     "expected %s, got %s",
                     lane, index, ExpectedValueToString(lane_expected).c_str(),
                     TypedValueToString(lane_actual).c_str());
          ok = false;
        }
      }
      break;
    }

    case Type::FuncRef:
      // A funcref expectation only requires that the reference be a function,
      // but it doesn't check the actual index.
      ok = (actual.type == Type::FuncRef);
      break;

    case Type::ExternRef:
      ok = expected.value.value.Get<Ref>() == actual.value.Get<Ref>();
      break;

    default:
      WABT_UNREACHABLE;
  }

  if (!ok && print_error) {
    PrintError(command->line,
               "mismatch in result %u of assert_return: expected %s, got %s",
               index, ExpectedValueToString(expected).c_str(),
               TypedValueToString(actual).c_str());
  }
  return ok ? wabt::Result::Ok : wabt::Result::Error;
}

wabt::Result CommandRunner::OnAssertReturnCommand(
    const AssertReturnCommand* command) {
  ActionResult action_result =
      RunAction(command->line, &command->action, RunVerbosity::Quiet);

  if (action_result.trap) {
    PrintError(command->line, "unexpected trap: %s",
               action_result.trap->message().c_str());
    return wabt::Result::Error;
  }

  if (action_result.values.size() != command->expected.size()) {
    PrintError(command->line,
               "result length mismatch in assert_return: expected %" PRIzd
               ", got %" PRIzd,
               command->expected.size(), action_result.values.size());
    return wabt::Result::Error;
  }

  wabt::Result result = wabt::Result::Ok;
  for (size_t i = 0; i < action_result.values.size(); ++i) {
    const ExpectedValue& expected = command->expected[i];
    TypedValue actual{action_result.types[i], action_result.values[i]};

    result |= CheckAssertReturnResult(command, i, expected, actual, true);
  }

  return result;
}

wabt::Result CommandRunner::OnAssertTrapCommand(
    const AssertTrapCommand* command) {
  ActionResult result =
      RunAction(command->line, &command->action, RunVerbosity::Quiet);
  if (!result.trap) {
    PrintError(command->line, "expected trap: \"%s\"", command->text.c_str());
    return wabt::Result::Error;
  }

  PrintError(command->line, "assert_trap passed: %s",
             result.trap->message().c_str());
  return wabt::Result::Ok;
}

wabt::Result CommandRunner::OnAssertExhaustionCommand(
    const AssertExhaustionCommand* command) {
  ActionResult result =
      RunAction(command->line, &command->action, RunVerbosity::Quiet);
  if (!result.trap || result.trap->message() != "call stack exhausted") {
    PrintError(command->line, "expected trap: \"%s\"", command->text.c_str());
    return wabt::Result::Error;
  }

  // TODO: print message when assertion passes.
#if 0
  PrintError(command->line, "assert_exhaustion passed: %s",
             result.trap->message().c_str());
#endif
  return wabt::Result::Ok;
}

void CommandRunner::TallyCommand(wabt::Result result) {
  if (Succeeded(result)) {
    passed_++;
  }
  total_++;
}

static int ReadAndRunSpecJSON(string_view spec_json_filename) {
  JSONParser parser;
  if (parser.ReadFile(spec_json_filename) == wabt::Result::Error) {
    return 1;
  }

  Script script;
  if (parser.ParseScript(&script) == wabt::Result::Error) {
    return 1;
  }

  CommandRunner runner;
  if (runner.Run(script) == wabt::Result::Error) {
    return 1;
  }

  printf("%d/%d tests passed.\n", runner.passed(), runner.total());
  const int failed = runner.total() - runner.passed();
  return failed;
}

}  // namespace spectest

int ProgramMain(int argc, char** argv) {
  InitStdio();
  s_stdout_stream = FileStream::CreateStdout();

  ParseOptions(argc, argv);
  return spectest::ReadAndRunSpecJSON(s_infile);
}

int main(int argc, char** argv) {
  WABT_TRY
  return ProgramMain(argc, argv);
  WABT_CATCH_BAD_ALLOC_AND_EXIT
}
