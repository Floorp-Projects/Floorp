/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include <sstream>
#include "JSStreamWriter.h"

TEST(JSStreamWriter, NoOutput) {
  std::stringstream ss;
  JSStreamWriter b(ss);
  ASSERT_TRUE(ss.str().compare("") == 0);
}

TEST(JSStreamWriter, EmptyObject) {
  std::stringstream ss;
  JSStreamWriter b(ss);
  b.BeginObject();
  b.EndObject();
  ASSERT_TRUE(ss.str().compare("{}") == 0);
}

TEST(JSStreamWriter, OnePropertyObject) {
  std::stringstream ss;
  JSStreamWriter b(ss);
  b.BeginObject();
  b.Name("a");
  b.Value(1);
  b.EndObject();
  ASSERT_TRUE(ss.str().compare("{\"a\":1}") == 0);
}

TEST(JSStreamWriter, MultiPropertyObject) {
  std::stringstream ss;
  JSStreamWriter b(ss);
  b.BeginObject();
  b.Name("a");
  b.Value(1);
  b.Name("b");
  b.Value(2);
  b.EndObject();
  ASSERT_TRUE(ss.str().compare("{\"a\":1,\"b\":2}") == 0);
}

TEST(JSStreamWriter, OnePropertyArray) {
  std::stringstream ss;
  JSStreamWriter b(ss);
  b.BeginArray();
  b.Value(1);
  b.EndArray();
  ASSERT_TRUE(ss.str().compare("[1]") == 0);
}

TEST(JSStreamWriter, MultiPropertyArray) {
  std::stringstream ss;
  JSStreamWriter b(ss);
  b.BeginArray();
  b.Value(1);
  b.Value(2);
  b.EndArray();
  ASSERT_TRUE(ss.str().compare("[1,2]") == 0);
}

TEST(JSStreamWriter, NestedObject) {
  std::stringstream ss;
  JSStreamWriter b(ss);
  b.BeginObject();
  b.Name("a");
  b.BeginObject();
  b.Name("b");
  b.Value(1);
  b.EndObject();
  b.EndObject();
  ASSERT_TRUE(ss.str().compare("{\"a\":{\"b\":1}}") == 0);
}

TEST(JSStreamWriter, NestedObjectInArray) {
  std::stringstream ss;
  JSStreamWriter b(ss);
  b.BeginArray();
  b.BeginObject();
  b.Name("a");
  b.Value(1);
  b.EndObject();
  b.EndArray();
  ASSERT_TRUE(ss.str().compare("[{\"a\":1}]") == 0);
}

TEST(JSStreamWriter, NestedArrayInObject) {
  std::stringstream ss;
  JSStreamWriter b(ss);
  b.BeginObject();
  b.Name("a");
  b.BeginArray();
  b.Value(1);
  b.EndArray();
  b.EndObject();
  ASSERT_TRUE(ss.str().compare("{\"a\":[1]}") == 0);
}

TEST(JSStreamWriter, StingEscaping) {
  std::stringstream ss;
  JSStreamWriter b(ss);
  b.Value("a\"a");
  ASSERT_TRUE(ss.str().compare("\"a\\\"a\"") == 0);

  std::stringstream ss2;
  JSStreamWriter b2(ss2);
  b2.Value("a\na");
  ASSERT_TRUE(ss2.str().compare("\"a\\u000Aa\"") == 0);
}

TEST(JSStreamWriter, ArrayOfOjects) {
  std::stringstream ss;
  JSStreamWriter b(ss);
  b.BeginArray();
    b.BeginObject();
    b.EndObject();

    b.BeginObject();
    b.EndObject();
  b.EndArray();
  ASSERT_TRUE(ss.str().compare("[{},{}]") == 0);
}

TEST(JSStreamWriter, Complex) {
  std::stringstream ss;
  JSStreamWriter b(ss);
  b.BeginObject();
    b.Name("a");
      b.BeginArray();
        b.Value(1);

        b.BeginObject();
        b.EndObject();

        b.BeginObject();
          b.Name("b");
          b.Value("c");
        b.EndObject();
      b.EndArray();

    b.Name("b");
      b.BeginArray();
        b.BeginArray();
        b.EndArray();
      b.EndArray();
  b.EndObject();
  ASSERT_TRUE(ss.str().compare("{\"a\":[1,{},{\"b\":\"c\"}],\"b\":[[]]}") == 0);
}

TEST(JSStreamWriter, Complex2) {
  std::stringstream ss;
  JSStreamWriter b(ss);
  b.BeginObject();
    b.Name("a");
      b.BeginArray();
        b.BeginObject();
          b.Name("b");
            b.Value("c");
          b.Name("d");
            b.BeginArray();
              b.BeginObject();
                b.Name("e");
                  b.BeginArray();
                    b.BeginObject();
                      b.Name("f");
                        b.Value("g");
                    b.EndObject();
                    b.BeginObject();
                      b.Name("h");
                        b.Value("i");
                    b.EndObject();
                  b.EndArray();
              b.EndObject();
            b.EndArray();
        b.EndObject();
      b.EndArray();
  b.EndObject();
  ASSERT_TRUE(ss.str().compare("{\"a\":[{\"b\":\"c\",\"d\":[{\"e\":[{\"f\":\"g\"},{\"h\":\"i\"}]}]}]}") == 0);
}
