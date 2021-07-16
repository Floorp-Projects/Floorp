/*
 * Copyright 2020 WebAssembly Community Group participants
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

#include "gtest/gtest.h"

#include "src/binary-reader.h"
#include "src/error-formatter.h"

#include "src/interp/binary-reader-interp.h"
#include "src/interp/interp.h"

using namespace wabt;
using namespace wabt::interp;

class InterpTest : public ::testing::Test {
 public:
  void ReadModule(const std::vector<u8>& data) {
    Errors errors;
    ReadBinaryOptions options;
    Result result = ReadBinaryInterp(data.data(), data.size(), options, &errors,
                                     &module_desc_);
    ASSERT_EQ(Result::Ok, result)
        << FormatErrorsToString(errors, Location::Type::Binary);
  }

  void Instantiate(const RefVec& imports = RefVec{}) {
    mod_ = Module::New(store_, module_desc_);
    RefPtr<Trap> trap;
    inst_ = Instance::Instantiate(store_, mod_.ref(), imports, &trap);
    ASSERT_TRUE(inst_) << trap->message();
  }

  DefinedFunc::Ptr GetFuncExport(interp::Index index) {
    EXPECT_LT(index, inst_->exports().size());
    return store_.UnsafeGet<DefinedFunc>(inst_->exports()[index]);
  }

  void ExpectBufferStrEq(OutputBuffer& buf, const char* str) {
    std::string buf_str(buf.data.begin(), buf.data.end());
    EXPECT_STREQ(buf_str.c_str(), str);
  }

  Store store_;
  ModuleDesc module_desc_;
  Module::Ptr mod_;
  Instance::Ptr inst_;
};


TEST_F(InterpTest, Empty) {
  ASSERT_TRUE(mod_.empty());
  ReadModule({0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00});
  Instantiate();
  ASSERT_FALSE(mod_.empty());
}

TEST_F(InterpTest, MVP) {
  // (module
  //   (type (;0;) (func (param i32) (result i32)))
  //   (type (;1;) (func (param f32) (result f32)))
  //   (type (;2;) (func))
  //   (import "foo" "bar" (func (;0;) (type 0)))
  //   (func (;1;) (type 1) (param f32) (result f32)
  //     (f32.const 0x1.5p+5 (;=42;)))
  //   (func (;2;) (type 2))
  //   (table (;0;) 1 2 funcref)
  //   (memory (;0;) 1)
  //   (global (;0;) i32 (i32.const 1))
  //   (export "quux" (func 1))
  //   (start 2)
  //   (elem (;0;) (i32.const 0) 0 1)
  //   (data (;0;) (i32.const 2) "hello"))
  ReadModule({
      0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x0e, 0x03, 0x60,
      0x01, 0x7f, 0x01, 0x7f, 0x60, 0x01, 0x7d, 0x01, 0x7d, 0x60, 0x00, 0x00,
      0x02, 0x0b, 0x01, 0x03, 0x66, 0x6f, 0x6f, 0x03, 0x62, 0x61, 0x72, 0x00,
      0x00, 0x03, 0x03, 0x02, 0x01, 0x02, 0x04, 0x05, 0x01, 0x70, 0x01, 0x01,
      0x02, 0x05, 0x03, 0x01, 0x00, 0x01, 0x06, 0x06, 0x01, 0x7f, 0x00, 0x41,
      0x01, 0x0b, 0x07, 0x08, 0x01, 0x04, 0x71, 0x75, 0x75, 0x78, 0x00, 0x01,
      0x08, 0x01, 0x02, 0x09, 0x08, 0x01, 0x00, 0x41, 0x00, 0x0b, 0x02, 0x00,
      0x01, 0x0a, 0x0c, 0x02, 0x07, 0x00, 0x43, 0x00, 0x00, 0x28, 0x42, 0x0b,
      0x02, 0x00, 0x0b, 0x0b, 0x0b, 0x01, 0x00, 0x41, 0x02, 0x0b, 0x05, 0x68,
      0x65, 0x6c, 0x6c, 0x6f,
  });

  EXPECT_EQ(3u, module_desc_.func_types.size());
  EXPECT_EQ(1u, module_desc_.imports.size());
  EXPECT_EQ(2u, module_desc_.funcs.size());
  EXPECT_EQ(1u, module_desc_.tables.size());
  EXPECT_EQ(1u, module_desc_.memories.size());
  EXPECT_EQ(1u, module_desc_.globals.size());
  EXPECT_EQ(0u, module_desc_.events.size());
  EXPECT_EQ(1u, module_desc_.exports.size());
  EXPECT_EQ(1u, module_desc_.starts.size());
  EXPECT_EQ(1u, module_desc_.elems.size());
  EXPECT_EQ(1u, module_desc_.datas.size());
}

namespace {

// (func (export "fac") (param $n i32) (result i32)
//   (local $result i32)
//   (local.set $result (i32.const 1))
//   (loop (result i32)
//     (local.set $result
//       (i32.mul
//         (br_if 1 (local.get $result) (i32.eqz (local.get $n)))
//         (local.get $n)))
//     (local.set $n (i32.sub (local.get $n) (i32.const 1)))
//     (br 0)))
const std::vector<u8> s_fac_module = {
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x06, 0x01,
    0x60, 0x01, 0x7f, 0x01, 0x7f, 0x03, 0x02, 0x01, 0x00, 0x07, 0x07,
    0x01, 0x03, 0x66, 0x61, 0x63, 0x00, 0x00, 0x0a, 0x22, 0x01, 0x20,
    0x01, 0x01, 0x7f, 0x41, 0x01, 0x21, 0x01, 0x03, 0x7f, 0x20, 0x01,
    0x20, 0x00, 0x45, 0x0d, 0x01, 0x20, 0x00, 0x6c, 0x21, 0x01, 0x20,
    0x00, 0x41, 0x01, 0x6b, 0x21, 0x00, 0x0c, 0x00, 0x0b, 0x0b,
};

}  // namespace

TEST_F(InterpTest, Disassemble) {
  ReadModule(s_fac_module);

  MemoryStream stream;
  module_desc_.istream.Disassemble(&stream);
  auto buf = stream.ReleaseOutputBuffer();

  ExpectBufferStrEq(*buf,
R"(   0| alloca 1
   8| i32.const 1
  16| local.set $2, %[-1]
  24| local.get $1
  32| local.get $3
  40| i32.eqz %[-1]
  44| br_unless @60, %[-1]
  52| br @116
  60| local.get $3
  68| i32.mul %[-2], %[-1]
  72| local.set $2, %[-1]
  80| local.get $2
  88| i32.const 1
  96| i32.sub %[-2], %[-1]
 100| local.set $3, %[-1]
 108| br @24
 116| drop_keep $2 $1
 128| return
)");
}

TEST_F(InterpTest, Fac) {
  ReadModule(s_fac_module);
  Instantiate();
  auto func = GetFuncExport(0);

  Values results;
  Trap::Ptr trap;
  Result result = func->Call(store_, {Value::Make(5)}, results, &trap);

  ASSERT_EQ(Result::Ok, result);
  EXPECT_EQ(1u, results.size());
  EXPECT_EQ(120u, results[0].Get<u32>());
}

TEST_F(InterpTest, Fac_Trace) {
  ReadModule(s_fac_module);
  Instantiate();
  auto func = GetFuncExport(0);

  Values results;
  Trap::Ptr trap;
  MemoryStream stream;
  Result result = func->Call(store_, {Value::Make(2)}, results, &trap, &stream);
  ASSERT_EQ(Result::Ok, result);

  auto buf = stream.ReleaseOutputBuffer();
  ExpectBufferStrEq(*buf,
R"(#0.    0: V:1  | alloca 1
#0.    8: V:2  | i32.const 1
#0.   16: V:3  | local.set $2, 1
#0.   24: V:2  | local.get $1
#0.   32: V:3  | local.get $3
#0.   40: V:4  | i32.eqz 2
#0.   44: V:4  | br_unless @60, 0
#0.   60: V:3  | local.get $3
#0.   68: V:4  | i32.mul 1, 2
#0.   72: V:3  | local.set $2, 2
#0.   80: V:2  | local.get $2
#0.   88: V:3  | i32.const 1
#0.   96: V:4  | i32.sub 2, 1
#0.  100: V:3  | local.set $3, 1
#0.  108: V:2  | br @24
#0.   24: V:2  | local.get $1
#0.   32: V:3  | local.get $3
#0.   40: V:4  | i32.eqz 1
#0.   44: V:4  | br_unless @60, 0
#0.   60: V:3  | local.get $3
#0.   68: V:4  | i32.mul 2, 1
#0.   72: V:3  | local.set $2, 2
#0.   80: V:2  | local.get $2
#0.   88: V:3  | i32.const 1
#0.   96: V:4  | i32.sub 1, 1
#0.  100: V:3  | local.set $3, 0
#0.  108: V:2  | br @24
#0.   24: V:2  | local.get $1
#0.   32: V:3  | local.get $3
#0.   40: V:4  | i32.eqz 0
#0.   44: V:4  | br_unless @60, 1
#0.   52: V:3  | br @116
#0.  116: V:3  | drop_keep $2 $1
#0.  128: V:1  | return
)");
}

TEST_F(InterpTest, Local_Trace) {
  // (func (export "a")
  //   (local i32 i64 f32 f64)
  //   (local.set 0 (i32.const 0))
  //   (local.set 1 (i64.const 1))
  //   (local.set 2 (f32.const 2))
  //   (local.set 3 (f64.const 3)))
  ReadModule({
      0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x04, 0x01,
      0x60, 0x00, 0x00, 0x03, 0x02, 0x01, 0x00, 0x07, 0x05, 0x01, 0x01,
      0x61, 0x00, 0x00, 0x0a, 0x26, 0x01, 0x24, 0x04, 0x01, 0x7f, 0x01,
      0x7e, 0x01, 0x7d, 0x01, 0x7c, 0x41, 0x00, 0x21, 0x00, 0x42, 0x01,
      0x21, 0x01, 0x43, 0x00, 0x00, 0x00, 0x40, 0x21, 0x02, 0x44, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x40, 0x21, 0x03, 0x0b,
  });

  Instantiate();
  auto func = GetFuncExport(0);

  Values results;
  Trap::Ptr trap;
  MemoryStream stream;
  Result result = func->Call(store_, {}, results, &trap, &stream);
  ASSERT_EQ(Result::Ok, result);

  auto buf = stream.ReleaseOutputBuffer();
  ExpectBufferStrEq(*buf,
R"(#0.    0: V:0  | alloca 4
#0.    8: V:4  | i32.const 0
#0.   16: V:5  | local.set $5, 0
#0.   24: V:4  | i64.const 1
#0.   36: V:5  | local.set $4, 1
#0.   44: V:4  | f32.const 2
#0.   52: V:5  | local.set $3, 2
#0.   60: V:4  | f64.const 3
#0.   72: V:5  | local.set $2, 3
#0.   80: V:4  | drop_keep $4 $0
#0.   92: V:0  | return
)");
}

TEST_F(InterpTest, HostFunc) {
  // (import "" "f" (func $f (param i32) (result i32)))
  // (func (export "g") (result i32)
  //   (call $f (i32.const 1)))
  ReadModule({
      0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x0a,
      0x02, 0x60, 0x01, 0x7f, 0x01, 0x7f, 0x60, 0x00, 0x01, 0x7f,
      0x02, 0x06, 0x01, 0x00, 0x01, 0x66, 0x00, 0x00, 0x03, 0x02,
      0x01, 0x01, 0x07, 0x05, 0x01, 0x01, 0x67, 0x00, 0x01, 0x0a,
      0x08, 0x01, 0x06, 0x00, 0x41, 0x01, 0x10, 0x00, 0x0b,
  });

  auto host_func =
      HostFunc::New(store_, FuncType{{ValueType::I32}, {ValueType::I32}},
                    [](Thread& thread, const Values& params, Values& results,
                       Trap::Ptr* out_trap) -> Result {
                      results[0] = Value::Make(params[0].Get<u32>() + 1);
                      return Result::Ok;
                    });

  Instantiate({host_func->self()});

  Values results;
  Trap::Ptr trap;
  Result result = GetFuncExport(0)->Call(store_, {}, results, &trap);

  ASSERT_EQ(Result::Ok, result);
  EXPECT_EQ(1u, results.size());
  EXPECT_EQ(2u, results[0].Get<u32>());
}

TEST_F(InterpTest, HostFunc_PingPong) {
  // (import "" "f" (func $f (param i32) (result i32)))
  // (func (export "g") (param i32) (result i32)
  //   (call $f (i32.add (local.get 0) (i32.const 1))))
  ReadModule({
      0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x06, 0x01, 0x60,
      0x01, 0x7f, 0x01, 0x7f, 0x02, 0x06, 0x01, 0x00, 0x01, 0x66, 0x00, 0x00,
      0x03, 0x02, 0x01, 0x00, 0x07, 0x05, 0x01, 0x01, 0x67, 0x00, 0x01, 0x0a,
      0x0b, 0x01, 0x09, 0x00, 0x20, 0x00, 0x41, 0x01, 0x6a, 0x10, 0x00, 0x0b,
  });

  auto host_func = HostFunc::New(
      store_, FuncType{{ValueType::I32}, {ValueType::I32}},
      [&](Thread& thread, const Values& params, Values& results,
          Trap::Ptr* out_trap) -> Result {
        auto val = params[0].Get<u32>();
        if (val < 10) {
          return GetFuncExport(0)->Call(store_, {Value::Make(val * 2)}, results,
                                        out_trap);
        }
        results[0] = Value::Make(val);
        return Result::Ok;
      });

  Instantiate({host_func->self()});

  // Should produce the following calls:
  //  g(1) -> f(2) -> g(4) -> f(5) -> g(10) -> f(11) -> return 11

  Values results;
  Trap::Ptr trap;
  Result result = GetFuncExport(0)->Call(store_, {Value::Make(1)}, results, &trap);

  ASSERT_EQ(Result::Ok, result);
  EXPECT_EQ(1u, results.size());
  EXPECT_EQ(11u, results[0].Get<u32>());
}

TEST_F(InterpTest, HostFunc_PingPong_SameThread) {
  // (import "" "f" (func $f (param i32) (result i32)))
  // (func (export "g") (param i32) (result i32)
  //   (call $f (i32.add (local.get 0) (i32.const 1))))
  ReadModule({
      0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x06, 0x01, 0x60,
      0x01, 0x7f, 0x01, 0x7f, 0x02, 0x06, 0x01, 0x00, 0x01, 0x66, 0x00, 0x00,
      0x03, 0x02, 0x01, 0x00, 0x07, 0x05, 0x01, 0x01, 0x67, 0x00, 0x01, 0x0a,
      0x0b, 0x01, 0x09, 0x00, 0x20, 0x00, 0x41, 0x01, 0x6a, 0x10, 0x00, 0x0b,
  });

  auto thread = Thread::New(store_, {});

  auto host_func =
      HostFunc::New(store_, FuncType{{ValueType::I32}, {ValueType::I32}},
                    [&](Thread& t, const Values& params, Values& results,
                        Trap::Ptr* out_trap) -> Result {
                      auto val = params[0].Get<u32>();
                      if (val < 10) {
                        return GetFuncExport(0)->Call(t, {Value::Make(val * 2)},
                                                      results, out_trap);
                      }
                      results[0] = Value::Make(val);
                      return Result::Ok;
                    });

  Instantiate({host_func->self()});

  // Should produce the following calls:
  //  g(1) -> f(2) -> g(4) -> f(5) -> g(10) -> f(11) -> return 11

  Values results;
  Trap::Ptr trap;
  Result result = GetFuncExport(0)->Call(*thread, {Value::Make(1)}, results, &trap);

  ASSERT_EQ(Result::Ok, result);
  EXPECT_EQ(1u, results.size());
  EXPECT_EQ(11u, results[0].Get<u32>());
}

TEST_F(InterpTest, HostTrap) {
  // (import "host" "a" (func $0))
  // (func $1 call $0)
  // (start $1)
  ReadModule({
      0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x04, 0x01,
      0x60, 0x00, 0x00, 0x02, 0x0a, 0x01, 0x04, 0x68, 0x6f, 0x73, 0x74,
      0x01, 0x61, 0x00, 0x00, 0x03, 0x02, 0x01, 0x00, 0x08, 0x01, 0x01,
      0x0a, 0x06, 0x01, 0x04, 0x00, 0x10, 0x00, 0x0b,
  });

  auto host_func =
      HostFunc::New(store_, FuncType{{}, {}},
                    [&](Thread& thread, const Values& params, Values& results,
                        Trap::Ptr* out_trap) -> Result {
                      *out_trap = Trap::New(store_, "boom");
                      return Result::Error;
                    });

  mod_ = Module::New(store_, module_desc_);
  RefPtr<Trap> trap;
  Instance::Instantiate(store_, mod_.ref(), {host_func->self()}, &trap);

  ASSERT_TRUE(trap);
  ASSERT_EQ("boom", trap->message());
}

TEST_F(InterpTest, Rot13) {
  // (import "host" "mem" (memory $mem 1))
  // (import "host" "fill_buf" (func $fill_buf (param i32 i32) (result i32)))
  // (import "host" "buf_done" (func $buf_done (param i32 i32)))
  //
  // (func $rot13c (param $c i32) (result i32)
  //   (local $uc i32)
  //
  //   ;; No change if < 'A'.
  //   (if (i32.lt_u (get_local $c) (i32.const 65))
  //     (return (get_local $c)))
  //
  //   ;; Clear 5th bit of c, to force uppercase. 0xdf = 0b11011111
  //   (set_local $uc (i32.and (get_local $c) (i32.const 0xdf)))
  //
  //   ;; In range ['A', 'M'] return |c| + 13.
  //   (if (i32.le_u (get_local $uc) (i32.const 77))
  //     (return (i32.add (get_local $c) (i32.const 13))))
  //
  //   ;; In range ['N', 'Z'] return |c| - 13.
  //   (if (i32.le_u (get_local $uc) (i32.const 90))
  //     (return (i32.sub (get_local $c) (i32.const 13))))
  //
  //   ;; No change for everything else.
  //   (return (get_local $c))
  // )
  //
  // (func (export "rot13")
  //   (local $size i32)
  //   (local $i i32)
  //
  //   ;; Ask host to fill memory [0, 1024) with data.
  //   (call $fill_buf (i32.const 0) (i32.const 1024))
  //
  //   ;; The host returns the size filled.
  //   (set_local $size)
  //
  //   ;; Loop over all bytes and rot13 them.
  //   (block $exit
  //     (loop $top
  //       ;; if (i >= size) break
  //       (if (i32.ge_u (get_local $i) (get_local $size)) (br $exit))
  //
  //       ;; mem[i] = rot13c(mem[i])
  //       (i32.store8
  //         (get_local $i)
  //         (call $rot13c
  //           (i32.load8_u (get_local $i))))
  //
  //       ;; i++
  //       (set_local $i (i32.add (get_local $i) (i32.const 1)))
  //       (br $top)
  //     )
  //   )
  //
  //   (call $buf_done (i32.const 0) (get_local $size))
  // )
  ReadModule({
      0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x14, 0x04, 0x60,
      0x02, 0x7f, 0x7f, 0x01, 0x7f, 0x60, 0x02, 0x7f, 0x7f, 0x00, 0x60, 0x01,
      0x7f, 0x01, 0x7f, 0x60, 0x00, 0x00, 0x02, 0x2d, 0x03, 0x04, 0x68, 0x6f,
      0x73, 0x74, 0x03, 0x6d, 0x65, 0x6d, 0x02, 0x00, 0x01, 0x04, 0x68, 0x6f,
      0x73, 0x74, 0x08, 0x66, 0x69, 0x6c, 0x6c, 0x5f, 0x62, 0x75, 0x66, 0x00,
      0x00, 0x04, 0x68, 0x6f, 0x73, 0x74, 0x08, 0x62, 0x75, 0x66, 0x5f, 0x64,
      0x6f, 0x6e, 0x65, 0x00, 0x01, 0x03, 0x03, 0x02, 0x02, 0x03, 0x07, 0x09,
      0x01, 0x05, 0x72, 0x6f, 0x74, 0x31, 0x33, 0x00, 0x03, 0x0a, 0x74, 0x02,
      0x39, 0x01, 0x01, 0x7f, 0x20, 0x00, 0x41, 0xc1, 0x00, 0x49, 0x04, 0x40,
      0x20, 0x00, 0x0f, 0x0b, 0x20, 0x00, 0x41, 0xdf, 0x01, 0x71, 0x21, 0x01,
      0x20, 0x01, 0x41, 0xcd, 0x00, 0x4d, 0x04, 0x40, 0x20, 0x00, 0x41, 0x0d,
      0x6a, 0x0f, 0x0b, 0x20, 0x01, 0x41, 0xda, 0x00, 0x4d, 0x04, 0x40, 0x20,
      0x00, 0x41, 0x0d, 0x6b, 0x0f, 0x0b, 0x20, 0x00, 0x0f, 0x0b, 0x38, 0x01,
      0x02, 0x7f, 0x41, 0x00, 0x41, 0x80, 0x08, 0x10, 0x00, 0x21, 0x00, 0x02,
      0x40, 0x03, 0x40, 0x20, 0x01, 0x20, 0x00, 0x4f, 0x04, 0x40, 0x0c, 0x02,
      0x0b, 0x20, 0x01, 0x20, 0x01, 0x2d, 0x00, 0x00, 0x10, 0x02, 0x3a, 0x00,
      0x00, 0x20, 0x01, 0x41, 0x01, 0x6a, 0x21, 0x01, 0x0c, 0x00, 0x0b, 0x0b,
      0x41, 0x00, 0x20, 0x00, 0x10, 0x01, 0x0b,
  });

  auto host_func =
      HostFunc::New(store_, FuncType{{ValueType::I32}, {ValueType::I32}},
                    [](Thread& thread, const Values& params, Values& results,
                       Trap::Ptr* out_trap) -> Result {
                      results[0] = Value::Make(params[0].Get<u32>() + 1);
                      return Result::Ok;
                    });

  std::string string_data = "Hello, WebAssembly!";

  auto memory = Memory::New(store_, MemoryType{Limits{1}});

  auto fill_buf = [&](Thread& thread, const Values& params, Values& results,
                      Trap::Ptr* out_trap) -> Result {
    // (param $ptr i32) (param $max_size i32) (result $size i32)
    EXPECT_EQ(2u, params.size());
    EXPECT_EQ(1u, results.size());

    u32 ptr = params[0].Get<u32>();
    u32 max_size = params[1].Get<u32>();
    u32 size = std::min(max_size, u32(string_data.size()));

    EXPECT_LT(ptr + size, memory->ByteSize());

    std::copy(string_data.begin(), string_data.begin() + size,
              memory->UnsafeData() + ptr);

    results[0].Set(size);
    return Result::Ok;
  };
  auto fill_buf_func = HostFunc::New(
      store_, FuncType{{ValueType::I32, ValueType::I32}, {ValueType::I32}},
      fill_buf);

  auto buf_done = [&](Thread& thread, const Values& params, Values& results,
                      Trap::Ptr* out_trap) -> Result {
    // (param $ptr i32) (param $size i32)
    EXPECT_EQ(2u, params.size());
    EXPECT_EQ(0u, results.size());

    u32 ptr = params[0].Get<u32>();
    u32 size = params[1].Get<u32>();

    EXPECT_LT(ptr + size, memory->ByteSize());

    string_data.resize(size);
    std::copy(memory->UnsafeData() + ptr, memory->UnsafeData() + ptr + size,
              string_data.begin());

    return Result::Ok;
  };
  auto buf_done_func = HostFunc::New(
      store_, FuncType{{ValueType::I32, ValueType::I32}, {}}, buf_done);

  Instantiate({memory->self(), fill_buf_func->self(), buf_done_func->self()});

  auto rot13 = GetFuncExport(0);

  Values results;
  Trap::Ptr trap;
  ASSERT_EQ(Result::Ok, rot13->Call(store_, {}, results, &trap));

  ASSERT_EQ("Uryyb, JroNffrzoyl!", string_data);

  ASSERT_EQ(Result::Ok, rot13->Call(store_, {}, results, &trap));

  ASSERT_EQ("Hello, WebAssembly!", string_data);
}

class InterpGCTest : public InterpTest {
 public:
  void SetUp() override {
    before_new = store_.object_count();
  }

  void TearDown() override {
    // Reset instance and module, in case they were allocated.
    inst_.reset();
    mod_.reset();

    store_.Collect();
    EXPECT_EQ(before_new, store_.object_count());
  }

  size_t before_new;
};

TEST_F(InterpGCTest, Collect_Basic) {
  auto foreign = Foreign::New(store_, nullptr);
  auto after_new = store_.object_count();
  EXPECT_EQ(before_new + 1, after_new);

  // Remove root, but object is not destroyed until collect.
  foreign.reset();
  EXPECT_EQ(after_new, store_.object_count());
}

TEST_F(InterpGCTest, Collect_GlobalCycle) {
  auto gt = GlobalType{ValueType::ExternRef, Mutability::Var};
  auto g1 = Global::New(store_, gt, Value::Make(Ref::Null));
  auto g2 = Global::New(store_, gt, Value::Make(g1->self()));
  g1->Set(store_, g2->self());

  auto after_new = store_.object_count();
  EXPECT_EQ(before_new + 2, after_new);

  // Remove g1 root, but it's kept alive by g2.
  g1.reset();
  store_.Collect();
  EXPECT_EQ(after_new, store_.object_count());

  // Remove g2 root, now both should be removed.
  g2.reset();
}

TEST_F(InterpGCTest, Collect_TableCycle) {
  auto tt = TableType{ValueType::ExternRef, Limits{2}};
  auto t1 = Table::New(store_, tt);
  auto t2 = Table::New(store_, tt);
  auto t3 = Table::New(store_, tt);

  t1->Set(store_, 0, t1->self());  // t1 references itself.
  t2->Set(store_, 0, t3->self());
  t3->Set(store_, 0, t2->self());  // t2 and t3 reference each other.
  t3->Set(store_, 1, t1->self());  // t3 also references t1.

  auto after_new = store_.object_count();
  EXPECT_EQ(before_new + 3, after_new);

  // Remove t1 and t2 roots, but their kept alive by t3.
  t1.reset();
  t2.reset();
  store_.Collect();
  EXPECT_EQ(after_new, store_.object_count());

  // Remove t3 root, now all should be removed.
  t3.reset();
}

TEST_F(InterpGCTest, Collect_Func) {
  ReadModule(s_fac_module);
  Instantiate();
  auto func = GetFuncExport(0);

  auto after_new = store_.object_count();
  EXPECT_EQ(before_new + 3, after_new);  // module, instance, func.

  // Reset module and instance roots, but they'll be kept alive by the func.
  mod_.reset();
  inst_.reset();
  store_.Collect();
  EXPECT_EQ(after_new, store_.object_count());
}

TEST_F(InterpGCTest, Collect_InstanceImport) {
  // (import "" "f" (func))
  // (import "" "t" (table 0 funcref))
  // (import "" "m" (memory 0))
  // (import "" "g" (global i32))
  ReadModule({
      0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x04, 0x01,
      0x60, 0x00, 0x00, 0x02, 0x19, 0x04, 0x00, 0x01, 0x66, 0x00, 0x00,
      0x00, 0x01, 0x74, 0x01, 0x70, 0x00, 0x00, 0x00, 0x01, 0x6d, 0x02,
      0x00, 0x00, 0x00, 0x01, 0x67, 0x03, 0x7f, 0x00,
  });
  auto f = HostFunc::New(store_, FuncType{{}, {}},
                         [](Thread& thread, const Values&, Values&,
                            Trap::Ptr*) -> Result { return Result::Ok; });
  auto t = Table::New(store_, TableType{ValueType::FuncRef, Limits{0}});
  auto m = Memory::New(store_, MemoryType{Limits{0}});
  auto g = Global::New(store_, GlobalType{ValueType::I32, Mutability::Const},
                       Value::Make(5));

  Instantiate({f->self(), t->self(), m->self(), g->self()});
  auto after_new = store_.object_count();
  EXPECT_EQ(before_new + 6, after_new);  // module, instance, f, t, m, g

  // Instance keeps all imports alive.
  f.reset();
  t.reset();
  m.reset();
  g.reset();
  store_.Collect();
  EXPECT_EQ(after_new, store_.object_count());
}

TEST_F(InterpGCTest, Collect_InstanceExport) {
  // (func (export "f"))
  // (global (export "g") i32 (i32.const 0))
  // (table (export "t") 0 funcref)
  // (memory (export "m") 0)
  ReadModule({
      0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x04, 0x01,
      0x60, 0x00, 0x00, 0x03, 0x02, 0x01, 0x00, 0x04, 0x04, 0x01, 0x70,
      0x00, 0x00, 0x05, 0x03, 0x01, 0x00, 0x00, 0x06, 0x06, 0x01, 0x7f,
      0x00, 0x41, 0x00, 0x0b, 0x07, 0x11, 0x04, 0x01, 0x66, 0x00, 0x00,
      0x01, 0x67, 0x03, 0x00, 0x01, 0x74, 0x01, 0x00, 0x01, 0x6d, 0x02,
      0x00, 0x0a, 0x04, 0x01, 0x02, 0x00, 0x0b,
  });
  Instantiate();
  auto after_new = store_.object_count();
  EXPECT_EQ(before_new + 6, after_new);  // module, instance, f, t, m, g

  // Instance keeps all exports alive.
  store_.Collect();
  EXPECT_EQ(after_new, store_.object_count());
}

// TODO: Test for Thread keeping references alive as locals/params/stack values.
// This requires better tracking of references than currently exists in the
// interpreter. (see TODOs in Select/LocalGet/GlobalGet)
