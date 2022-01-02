/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/JSONWriter.h"
#include "mozilla/UniquePtr.h"
#include <stdio.h>
#include <string>
#include <string.h>

using mozilla::JSONWriteFunc;
using mozilla::JSONWriter;
using mozilla::MakeStringSpan;
using mozilla::MakeUnique;
using mozilla::Span;

// This writes all the output into a big buffer.
struct StringWriteFunc : public JSONWriteFunc {
  std::string mString;

  void Write(const mozilla::Span<const char>& aStr) override {
    mString.append(aStr.data(), aStr.size());
  }
};

void Check(JSONWriteFunc* aFunc, const char* aExpected) {
  const std::string& actual = static_cast<StringWriteFunc*>(aFunc)->mString;
  if (strcmp(aExpected, actual.c_str()) != 0) {
    fprintf(stderr,
            "---- EXPECTED ----\n<<<%s>>>\n"
            "---- ACTUAL ----\n<<<%s>>>\n",
            aExpected, actual.c_str());
    MOZ_RELEASE_ASSERT(false, "expected and actual output don't match");
  }
}

// Note: to convert actual output into |expected| strings that C++ can handle,
// apply the following substitutions, in order, to each line.
// - s/\\/\\\\/g    # escapes backslashes
// - s/"/\\"/g      # escapes quotes
// - s/$/\\n\\/     # adds a newline and string continuation char to each line

void TestBasicProperties() {
  const char* expected =
      "\
{\n\
 \"null\": null,\n\
 \"bool1\": true,\n\
 \"bool2\": false,\n\
 \"int1\": 123,\n\
 \"int2\": -123,\n\
 \"int3\": -123456789000,\n\
 \"double1\": 1.2345,\n\
 \"double2\": -3,\n\
 \"double3\": 1e-7,\n\
 \"double4\": 1.1111111111111111e+21,\n\
 \"string1\": \"\",\n\
 \"string2\": \"1234\",\n\
 \"string3\": \"hello\",\n\
 \"string4\": \"\\\" \\\\ \\u0007 \\b \\t \\n \\u000b \\f \\r\",\n\
 \"string5\": \"hello\",\n\
 \"string6\": \"\\\" \\\\ \\u0007 \\b \\t \",\n\
 \"span1\": \"buf1\",\n\
 \"span2\": \"buf2\",\n\
 \"span3\": \"buf3\",\n\
 \"span4\": \"buf\\n4\",\n\
 \"span5\": \"MakeStringSpan\",\n\
 \"len 0 array, multi-line\": [\n\
 ],\n\
 \"len 0 array, single-line\": [],\n\
 \"len 1 array\": [\n\
  1\n\
 ],\n\
 \"len 5 array, multi-line\": [\n\
  1,\n\
  2,\n\
  3,\n\
  4,\n\
  5\n\
 ],\n\
 \"len 3 array, single-line\": [1, [{}, 2, []], 3],\n\
 \"len 0 object, multi-line\": {\n\
 },\n\
 \"len 0 object, single-line\": {},\n\
 \"len 1 object\": {\n\
  \"one\": 1\n\
 },\n\
 \"len 5 object\": {\n\
  \"one\": 1,\n\
  \"two\": 2,\n\
  \"three\": 3,\n\
  \"four\": 4,\n\
  \"five\": 5\n\
 },\n\
 \"len 3 object, single-line\": {\"a\": 1, \"b\": [{}, 2, []], \"c\": 3}\n\
}\n\
";

  JSONWriter w(MakeUnique<StringWriteFunc>());

  w.Start();
  {
    w.NullProperty("null");

    w.BoolProperty("bool1", true);
    w.BoolProperty("bool2", false);

    w.IntProperty("int1", 123);
    w.IntProperty("int2", -0x7b);
    w.IntProperty("int3", -123456789000ll);

    w.DoubleProperty("double1", 1.2345);
    w.DoubleProperty("double2", -3);
    w.DoubleProperty("double3", 1e-7);
    w.DoubleProperty("double4", 1.1111111111111111e+21);

    w.StringProperty("string1", "");
    w.StringProperty("string2", "1234");
    w.StringProperty("string3", "hello");
    w.StringProperty("string4", "\" \\ \a \b \t \n \v \f \r");
    w.StringProperty("string5", "hello\0cut");  // '\0' marks the end.
    w.StringProperty("string6", "\" \\ \a \b \t \0 \n \v \f \r");

    const char buf1[] = {'b', 'u', 'f', '1'};
    w.StringProperty("span1", buf1);
    const char buf2[] = {'b', 'u', 'f', '2', '\0'};
    w.StringProperty("span2", buf2);
    const char buf3[] = {'b', 'u', 'f', '3', '\0', '?'};
    w.StringProperty("span3", buf3);
    const char buf4[] = {'b', 'u', 'f', '\n', '4', '\0', '?'};
    w.StringProperty("span4", buf4);
    w.StringProperty("span5", MakeStringSpan("MakeStringSpan"));

    w.StartArrayProperty("len 0 array, multi-line", w.MultiLineStyle);
    w.EndArray();

    w.StartArrayProperty("len 0 array, single-line", w.SingleLineStyle);
    w.EndArray();

    w.StartArrayProperty("len 1 array");
    { w.IntElement(1); }
    w.EndArray();

    w.StartArrayProperty("len 5 array, multi-line", w.MultiLineStyle);
    {
      w.IntElement(1);
      w.IntElement(2);
      w.IntElement(3);
      w.IntElement(4);
      w.IntElement(5);
    }
    w.EndArray();

    w.StartArrayProperty("len 3 array, single-line", w.SingleLineStyle);
    {
      w.IntElement(1);
      w.StartArrayElement();
      {
        w.StartObjectElement(w.SingleLineStyle);
        w.EndObject();

        w.IntElement(2);

        w.StartArrayElement(w.MultiLineStyle);  // style overridden from above
        w.EndArray();
      }
      w.EndArray();
      w.IntElement(3);
    }
    w.EndArray();

    w.StartObjectProperty("len 0 object, multi-line");
    w.EndObject();

    w.StartObjectProperty("len 0 object, single-line", w.SingleLineStyle);
    w.EndObject();

    w.StartObjectProperty("len 1 object");
    { w.IntProperty("one", 1); }
    w.EndObject();

    w.StartObjectProperty("len 5 object");
    {
      w.IntProperty("one", 1);
      w.IntProperty("two", 2);
      w.IntProperty("three", 3);
      w.IntProperty("four", 4);
      w.IntProperty("five", 5);
    }
    w.EndObject();

    w.StartObjectProperty("len 3 object, single-line", w.SingleLineStyle);
    {
      w.IntProperty("a", 1);
      w.StartArrayProperty("b");
      {
        w.StartObjectElement();
        w.EndObject();

        w.IntElement(2);

        w.StartArrayElement(w.SingleLineStyle);
        w.EndArray();
      }
      w.EndArray();
      w.IntProperty("c", 3);
    }
    w.EndObject();
  }
  w.End();

  Check(w.WriteFunc(), expected);
}

void TestBasicElements() {
  const char* expected =
      "\
{\n\
 \"array\": [\n\
  null,\n\
  true,\n\
  false,\n\
  123,\n\
  -123,\n\
  -123456789000,\n\
  1.2345,\n\
  -3,\n\
  1e-7,\n\
  1.1111111111111111e+21,\n\
  \"\",\n\
  \"1234\",\n\
  \"hello\",\n\
  \"\\\" \\\\ \\u0007 \\b \\t \\n \\u000b \\f \\r\",\n\
  \"hello\",\n\
  \"\\\" \\\\ \\u0007 \\b \\t \",\n\
  \"buf1\",\n\
  \"buf2\",\n\
  \"buf3\",\n\
  \"buf\\n4\",\n\
  \"MakeStringSpan\",\n\
  [\n\
  ],\n\
  [],\n\
  [\n\
   1\n\
  ],\n\
  [\n\
   1,\n\
   2,\n\
   3,\n\
   4,\n\
   5\n\
  ],\n\
  [1, [{}, 2, []], 3],\n\
  {\n\
  },\n\
  {},\n\
  {\n\
   \"one\": 1\n\
  },\n\
  {\n\
   \"one\": 1,\n\
   \"two\": 2,\n\
   \"three\": 3,\n\
   \"four\": 4,\n\
   \"five\": 5\n\
  },\n\
  {\"a\": 1, \"b\": [{}, 2, []], \"c\": 3}\n\
 ]\n\
}\n\
";

  JSONWriter w(MakeUnique<StringWriteFunc>());

  w.Start();
  w.StartArrayProperty("array");
  {
    w.NullElement();

    w.BoolElement(true);
    w.BoolElement(false);

    w.IntElement(123);
    w.IntElement(-0x7b);
    w.IntElement(-123456789000ll);

    w.DoubleElement(1.2345);
    w.DoubleElement(-3);
    w.DoubleElement(1e-7);
    w.DoubleElement(1.1111111111111111e+21);

    w.StringElement("");
    w.StringElement("1234");
    w.StringElement("hello");
    w.StringElement("\" \\ \a \b \t \n \v \f \r");
    w.StringElement("hello\0cut");  // '\0' marks the end.
    w.StringElement("\" \\ \a \b \t \0 \n \v \f \r");

    const char buf1[] = {'b', 'u', 'f', '1'};
    w.StringElement(buf1);
    const char buf2[] = {'b', 'u', 'f', '2', '\0'};
    w.StringElement(buf2);
    const char buf3[] = {'b', 'u', 'f', '3', '\0', '?'};
    w.StringElement(buf3);
    const char buf4[] = {'b', 'u', 'f', '\n', '4', '\0', '?'};
    w.StringElement(buf4);
    w.StringElement(MakeStringSpan("MakeStringSpan"));

    w.StartArrayElement();
    w.EndArray();

    w.StartArrayElement(w.SingleLineStyle);
    w.EndArray();

    w.StartArrayElement();
    { w.IntElement(1); }
    w.EndArray();

    w.StartArrayElement();
    {
      w.IntElement(1);
      w.IntElement(2);
      w.IntElement(3);
      w.IntElement(4);
      w.IntElement(5);
    }
    w.EndArray();

    w.StartArrayElement(w.SingleLineStyle);
    {
      w.IntElement(1);
      w.StartArrayElement();
      {
        w.StartObjectElement(w.SingleLineStyle);
        w.EndObject();

        w.IntElement(2);

        w.StartArrayElement(w.MultiLineStyle);  // style overridden from above
        w.EndArray();
      }
      w.EndArray();
      w.IntElement(3);
    }
    w.EndArray();

    w.StartObjectElement();
    w.EndObject();

    w.StartObjectElement(w.SingleLineStyle);
    w.EndObject();

    w.StartObjectElement();
    { w.IntProperty("one", 1); }
    w.EndObject();

    w.StartObjectElement();
    {
      w.IntProperty("one", 1);
      w.IntProperty("two", 2);
      w.IntProperty("three", 3);
      w.IntProperty("four", 4);
      w.IntProperty("five", 5);
    }
    w.EndObject();

    w.StartObjectElement(w.SingleLineStyle);
    {
      w.IntProperty("a", 1);
      w.StartArrayProperty("b");
      {
        w.StartObjectElement();
        w.EndObject();

        w.IntElement(2);

        w.StartArrayElement(w.SingleLineStyle);
        w.EndArray();
      }
      w.EndArray();
      w.IntProperty("c", 3);
    }
    w.EndObject();
  }
  w.EndArray();
  w.End();

  Check(w.WriteFunc(), expected);
}

void TestOneLineObject() {
  const char* expected =
      "\
{\"i\": 1, \"array\": [null, [{}], {\"o\": {}}, \"s\"], \"d\": 3.33}\n\
";

  JSONWriter w(MakeUnique<StringWriteFunc>());

  w.Start(w.SingleLineStyle);

  w.IntProperty("i", 1);

  w.StartArrayProperty("array");
  {
    w.NullElement();

    w.StartArrayElement(w.MultiLineStyle);  // style overridden from above
    {
      w.StartObjectElement();
      w.EndObject();
    }
    w.EndArray();

    w.StartObjectElement();
    {
      w.StartObjectProperty("o");
      w.EndObject();
    }
    w.EndObject();

    w.StringElement("s");
  }
  w.EndArray();

  w.DoubleProperty("d", 3.33);

  w.End();

  Check(w.WriteFunc(), expected);
}

void TestStringEscaping() {
  // This test uses hexadecimal character escapes because UTF8 literals cause
  // problems for some compilers (see bug 1069726).
  const char* expected =
      "\
{\n\
 \"ascii\": \"\x7F~}|{zyxwvutsrqponmlkjihgfedcba`_^]\\\\[ZYXWVUTSRQPONMLKJIHGFEDCBA@?>=<;:9876543210/.-,+*)('&%$#\\\"! \\u001f\\u001e\\u001d\\u001c\\u001b\\u001a\\u0019\\u0018\\u0017\\u0016\\u0015\\u0014\\u0013\\u0012\\u0011\\u0010\\u000f\\u000e\\r\\f\\u000b\\n\\t\\b\\u0007\\u0006\\u0005\\u0004\\u0003\\u0002\\u0001\",\n\
 \"\xD9\x85\xD8\xB1\xD8\xAD\xD8\xA8\xD8\xA7 \xD9\x87\xD9\x86\xD8\xA7\xD9\x83\": true,\n\
 \"\xD5\xA2\xD5\xA1\xD6\x80\xD5\xA5\xD6\x82 \xD5\xB9\xD5\xAF\xD5\xA1\": -123,\n\
 \"\xE4\xBD\xA0\xE5\xA5\xBD\": 1.234,\n\
 \"\xCE\xB3\xCE\xB5\xCE\xB9\xCE\xB1 \xCE\xB5\xCE\xBA\xCE\xB5\xCE\xAF\": \"\xD8\xB3\xD9\x84\xD8\xA7\xD9\x85\",\n\
 \"hall\xC3\xB3 \xC3\xBE"
      "arna\": 4660,\n\
 \"\xE3\x81\x93\xE3\x82\x93\xE3\x81\xAB\xE3\x81\xA1\xE3\x81\xAF\": {\n\
  \"\xD0\xBF\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82\": [\n\
  ]\n\
 }\n\
}\n\
";

  JSONWriter w(MakeUnique<StringWriteFunc>());

  // Test the string escaping behaviour.
  w.Start();
  {
    // Test all 127 ascii values. Do it in reverse order so that the 0
    // at the end serves as the null char.
    char buf[128];
    for (int i = 0; i < 128; i++) {
      buf[i] = 127 - i;
    }
    w.StringProperty("ascii", buf);

    // Test lots of unicode stuff. Note that this file is encoded as UTF-8.
    w.BoolProperty(
        "\xD9\x85\xD8\xB1\xD8\xAD\xD8\xA8\xD8\xA7 "
        "\xD9\x87\xD9\x86\xD8\xA7\xD9\x83",
        true);
    w.IntProperty(
        "\xD5\xA2\xD5\xA1\xD6\x80\xD5\xA5\xD6\x82 \xD5\xB9\xD5\xAF\xD5\xA1",
        -123);
    w.DoubleProperty("\xE4\xBD\xA0\xE5\xA5\xBD", 1.234);
    w.StringProperty(
        "\xCE\xB3\xCE\xB5\xCE\xB9\xCE\xB1 \xCE\xB5\xCE\xBA\xCE\xB5\xCE\xAF",
        "\xD8\xB3\xD9\x84\xD8\xA7\xD9\x85");
    w.IntProperty(
        "hall\xC3\xB3 \xC3\xBE"
        "arna",
        0x1234);
    w.StartObjectProperty(
        "\xE3\x81\x93\xE3\x82\x93\xE3\x81\xAB\xE3\x81\xA1\xE3\x81\xAF");
    {
      w.StartArrayProperty("\xD0\xBF\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82");
      w.EndArray();
    }
    w.EndObject();
  }
  w.End();

  Check(w.WriteFunc(), expected);
}

void TestDeepNesting() {
  const char* expected =
      "\
{\n\
 \"a\": [\n\
  {\n\
   \"a\": [\n\
    {\n\
     \"a\": [\n\
      {\n\
       \"a\": [\n\
        {\n\
         \"a\": [\n\
          {\n\
           \"a\": [\n\
            {\n\
             \"a\": [\n\
              {\n\
               \"a\": [\n\
                {\n\
                 \"a\": [\n\
                  {\n\
                   \"a\": [\n\
                    {\n\
                    }\n\
                   ]\n\
                  }\n\
                 ]\n\
                }\n\
               ]\n\
              }\n\
             ]\n\
            }\n\
           ]\n\
          }\n\
         ]\n\
        }\n\
       ]\n\
      }\n\
     ]\n\
    }\n\
   ]\n\
  }\n\
 ]\n\
}\n\
";

  JSONWriter w(MakeUnique<StringWriteFunc>());

  w.Start();
  {
    static const int n = 10;
    for (int i = 0; i < n; i++) {
      w.StartArrayProperty("a");
      w.StartObjectElement();
    }
    for (int i = 0; i < n; i++) {
      w.EndObject();
      w.EndArray();
    }
  }
  w.End();

  Check(w.WriteFunc(), expected);
}

void TestEscapedPropertyNames() {
  const char* expected =
      "\
{\"i\\t\": 1, \"array\\t\": [null, [{}], {\"o\\t\": {}}, \"s\"], \"d\": 3.33}\n\
";

  JSONWriter w(MakeUnique<StringWriteFunc>());

  w.Start(w.SingleLineStyle);

  w.IntProperty("i\t\0cut", 1);  // '\0' marks the end.

  w.StartArrayProperty("array\t");
  {
    w.NullElement();

    w.StartArrayElement(w.MultiLineStyle);  // style overridden from above
    {
      w.StartObjectElement();
      w.EndObject();
    }
    w.EndArray();

    w.StartObjectElement();
    {
      w.StartObjectProperty("o\t");
      w.EndObject();
    }
    w.EndObject();

    w.StringElement("s");
  }
  w.EndArray();

  w.DoubleProperty("d\0\t", 3.33);

  w.End();

  Check(w.WriteFunc(), expected);
}

int main(void) {
  TestBasicProperties();
  TestBasicElements();
  TestOneLineObject();
  TestStringEscaping();
  TestDeepNesting();
  TestEscapedPropertyNames();

  return 0;
}
