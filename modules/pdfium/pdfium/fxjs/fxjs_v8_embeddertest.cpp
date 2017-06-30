// Copyright 2015 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/js_embedder_test.h"

namespace {

const double kExpected0 = 6.0;
const double kExpected1 = 7.0;
const double kExpected2 = 8.0;

const wchar_t kScript0[] = L"fred = 6";
const wchar_t kScript1[] = L"fred = 7";
const wchar_t kScript2[] = L"fred = 8";

}  // namespace

class FXJSV8EmbedderTest : public JSEmbedderTest {
 public:
  void ExecuteInCurrentContext(const CFX_WideString& script) {
    FXJSErr error;
    int sts = engine()->Execute(script, &error);
    EXPECT_EQ(0, sts);
  }
  void CheckAssignmentInCurrentContext(double expected) {
    v8::Local<v8::Object> This = engine()->GetThisObj();
    v8::Local<v8::Value> fred = engine()->GetObjectProperty(This, L"fred");
    EXPECT_TRUE(fred->IsNumber());
    EXPECT_EQ(expected, engine()->ToDouble(fred));
  }
};

TEST_F(FXJSV8EmbedderTest, Getters) {
  v8::Isolate::Scope isolate_scope(isolate());
  v8::HandleScope handle_scope(isolate());
  v8::Context::Scope context_scope(GetV8Context());

  ExecuteInCurrentContext(CFX_WideString(kScript1));
  CheckAssignmentInCurrentContext(kExpected1);
}

TEST_F(FXJSV8EmbedderTest, MultipleEngines) {
  v8::Isolate::Scope isolate_scope(isolate());
  v8::HandleScope handle_scope(isolate());

  CFXJS_Engine engine1(isolate());
  engine1.InitializeEngine();

  CFXJS_Engine engine2(isolate());
  engine2.InitializeEngine();

  v8::Context::Scope context_scope(GetV8Context());
  ExecuteInCurrentContext(CFX_WideString(kScript0));
  CheckAssignmentInCurrentContext(kExpected0);

  {
    v8::Local<v8::Context> context1 = engine1.NewLocalContext();
    v8::Context::Scope context_scope1(context1);
    ExecuteInCurrentContext(CFX_WideString(kScript1));
    CheckAssignmentInCurrentContext(kExpected1);
  }

  engine1.ReleaseEngine();

  {
    v8::Local<v8::Context> context2 = engine2.NewLocalContext();
    v8::Context::Scope context_scope2(context2);
    ExecuteInCurrentContext(CFX_WideString(kScript2));
    CheckAssignmentInCurrentContext(kExpected2);
  }

  engine2.ReleaseEngine();
  CheckAssignmentInCurrentContext(kExpected0);
}

TEST_F(FXJSV8EmbedderTest, EmptyLocal) {
  v8::Isolate::Scope isolate_scope(isolate());
  v8::HandleScope handle_scope(isolate());
  v8::Context::Scope context_scope(GetV8Context());

  v8::Local<v8::Value> empty;
  EXPECT_FALSE(engine()->ToBoolean(empty));
  EXPECT_EQ(0, engine()->ToInt32(empty));
  EXPECT_EQ(0.0, engine()->ToDouble(empty));
  EXPECT_EQ(L"", engine()->ToWideString(empty));
  EXPECT_TRUE(engine()->ToObject(empty).IsEmpty());
  EXPECT_TRUE(engine()->ToArray(empty).IsEmpty());
}

TEST_F(FXJSV8EmbedderTest, NewNull) {
  v8::Isolate::Scope isolate_scope(isolate());
  v8::HandleScope handle_scope(isolate());
  v8::Context::Scope context_scope(GetV8Context());

  auto nullz = engine()->NewNull();
  EXPECT_FALSE(engine()->ToBoolean(nullz));
  EXPECT_EQ(0, engine()->ToInt32(nullz));
  EXPECT_EQ(0.0, engine()->ToDouble(nullz));
  EXPECT_EQ(L"", engine()->ToWideString(nullz));
  EXPECT_TRUE(engine()->ToObject(nullz).IsEmpty());
  EXPECT_TRUE(engine()->ToArray(nullz).IsEmpty());
}

TEST_F(FXJSV8EmbedderTest, NewBoolean) {
  v8::Isolate::Scope isolate_scope(isolate());
  v8::HandleScope handle_scope(isolate());
  v8::Context::Scope context_scope(GetV8Context());

  auto boolz = engine()->NewBoolean(true);
  EXPECT_TRUE(engine()->ToBoolean(boolz));
  EXPECT_EQ(1, engine()->ToInt32(boolz));
  EXPECT_EQ(1.0, engine()->ToDouble(boolz));
  EXPECT_EQ(L"true", engine()->ToWideString(boolz));
  EXPECT_TRUE(engine()->ToObject(boolz).IsEmpty());
  EXPECT_TRUE(engine()->ToArray(boolz).IsEmpty());
}

TEST_F(FXJSV8EmbedderTest, NewNumber) {
  v8::Isolate::Scope isolate_scope(isolate());
  v8::HandleScope handle_scope(isolate());
  v8::Context::Scope context_scope(GetV8Context());

  auto num = engine()->NewNumber(42.1);
  EXPECT_TRUE(engine()->ToBoolean(num));
  EXPECT_EQ(42, engine()->ToInt32(num));
  EXPECT_EQ(42.1, engine()->ToDouble(num));
  EXPECT_EQ(L"42.1", engine()->ToWideString(num));
  EXPECT_TRUE(engine()->ToObject(num).IsEmpty());
  EXPECT_TRUE(engine()->ToArray(num).IsEmpty());
}

TEST_F(FXJSV8EmbedderTest, NewString) {
  v8::Isolate::Scope isolate_scope(isolate());
  v8::HandleScope handle_scope(isolate());
  v8::Context::Scope context_scope(GetV8Context());

  auto str = engine()->NewString(L"123");
  EXPECT_TRUE(engine()->ToBoolean(str));
  EXPECT_EQ(123, engine()->ToInt32(str));
  EXPECT_EQ(123, engine()->ToDouble(str));
  EXPECT_EQ(L"123", engine()->ToWideString(str));
  EXPECT_TRUE(engine()->ToObject(str).IsEmpty());
  EXPECT_TRUE(engine()->ToArray(str).IsEmpty());
}

TEST_F(FXJSV8EmbedderTest, NewDate) {
  v8::Isolate::Scope isolate_scope(isolate());
  v8::HandleScope handle_scope(isolate());
  v8::Context::Scope context_scope(GetV8Context());

  auto date = engine()->NewDate(1111111111);
  EXPECT_TRUE(engine()->ToBoolean(date));
  EXPECT_EQ(1111111111, engine()->ToInt32(date));
  EXPECT_EQ(1111111111.0, engine()->ToDouble(date));
  EXPECT_NE(L"", engine()->ToWideString(date));  // exact format varies.
  EXPECT_TRUE(engine()->ToObject(date)->IsObject());
  EXPECT_TRUE(engine()->ToArray(date).IsEmpty());
}

TEST_F(FXJSV8EmbedderTest, NewArray) {
  v8::Isolate::Scope isolate_scope(isolate());
  v8::HandleScope handle_scope(isolate());
  v8::Context::Scope context_scope(GetV8Context());

  auto array = engine()->NewArray();
  EXPECT_EQ(0u, engine()->GetArrayLength(array));
  EXPECT_FALSE(engine()->GetArrayElement(array, 2).IsEmpty());
  EXPECT_TRUE(engine()->GetArrayElement(array, 2)->IsUndefined());
  EXPECT_EQ(0u, engine()->GetArrayLength(array));

  engine()->PutArrayElement(array, 3, engine()->NewNumber(12));
  EXPECT_FALSE(engine()->GetArrayElement(array, 2).IsEmpty());
  EXPECT_TRUE(engine()->GetArrayElement(array, 2)->IsUndefined());
  EXPECT_FALSE(engine()->GetArrayElement(array, 3).IsEmpty());
  EXPECT_TRUE(engine()->GetArrayElement(array, 3)->IsNumber());
  EXPECT_EQ(4u, engine()->GetArrayLength(array));

  EXPECT_TRUE(engine()->ToBoolean(array));
  EXPECT_EQ(0, engine()->ToInt32(array));
  double d = engine()->ToDouble(array);
  EXPECT_NE(d, d);  // i.e. NaN.
  EXPECT_EQ(L",,,12", engine()->ToWideString(array));
  EXPECT_TRUE(engine()->ToObject(array)->IsObject());
  EXPECT_TRUE(engine()->ToArray(array)->IsArray());
}

TEST_F(FXJSV8EmbedderTest, NewObject) {
  v8::Isolate::Scope isolate_scope(isolate());
  v8::HandleScope handle_scope(isolate());
  v8::Context::Scope context_scope(GetV8Context());

  auto object = engine()->NewFxDynamicObj(-1);
  EXPECT_EQ(0u, engine()->GetObjectPropertyNames(object).size());
  EXPECT_FALSE(engine()->GetObjectProperty(object, L"clams").IsEmpty());
  EXPECT_TRUE(engine()->GetObjectProperty(object, L"clams")->IsUndefined());
  EXPECT_EQ(0u, engine()->GetObjectPropertyNames(object).size());

  engine()->PutObjectProperty(object, L"clams", engine()->NewNumber(12));
  EXPECT_FALSE(engine()->GetObjectProperty(object, L"clams").IsEmpty());
  EXPECT_TRUE(engine()->GetObjectProperty(object, L"clams")->IsNumber());
  EXPECT_EQ(1u, engine()->GetObjectPropertyNames(object).size());
  EXPECT_EQ(L"clams", engine()->GetObjectPropertyNames(object)[0]);

  EXPECT_TRUE(engine()->ToBoolean(object));
  EXPECT_EQ(0, engine()->ToInt32(object));
  double d = engine()->ToDouble(object);
  EXPECT_NE(d, d);  // i.e. NaN.
  EXPECT_EQ(L"[object Object]", engine()->ToWideString(object));
  EXPECT_TRUE(engine()->ToObject(object)->IsObject());
  EXPECT_TRUE(engine()->ToArray(object).IsEmpty());
}
