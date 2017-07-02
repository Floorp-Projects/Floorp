// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/parser/cpdf_object.h"

#include <algorithm>

#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_indirect_object_holder.h"
#include "core/fpdfapi/parser/cpdf_parser.h"
#include "core/fpdfapi/parser/fpdf_parser_decode.h"
#include "core/fxcrt/fx_string.h"
#include "third_party/base/stl_util.h"

CPDF_Object::~CPDF_Object() {}

CPDF_Object* CPDF_Object::GetDirect() const {
  return const_cast<CPDF_Object*>(this);
}

std::unique_ptr<CPDF_Object> CPDF_Object::CloneObjectNonCyclic(
    bool bDirect) const {
  std::set<const CPDF_Object*> visited_objs;
  return CloneNonCyclic(bDirect, &visited_objs);
}

std::unique_ptr<CPDF_Object> CPDF_Object::CloneDirectObject() const {
  return CloneObjectNonCyclic(true);
}

std::unique_ptr<CPDF_Object> CPDF_Object::CloneNonCyclic(
    bool bDirect,
    std::set<const CPDF_Object*>* pVisited) const {
  return Clone();
}

CFX_ByteString CPDF_Object::GetString() const {
  return CFX_ByteString();
}

CFX_WideString CPDF_Object::GetUnicodeText() const {
  return CFX_WideString();
}

FX_FLOAT CPDF_Object::GetNumber() const {
  return 0;
}

int CPDF_Object::GetInteger() const {
  return 0;
}

CPDF_Dictionary* CPDF_Object::GetDict() const {
  return nullptr;
}

void CPDF_Object::SetString(const CFX_ByteString& str) {
  ASSERT(false);
}

bool CPDF_Object::IsArray() const {
  return false;
}

bool CPDF_Object::IsBoolean() const {
  return false;
}

bool CPDF_Object::IsDictionary() const {
  return false;
}

bool CPDF_Object::IsName() const {
  return false;
}

bool CPDF_Object::IsNumber() const {
  return false;
}

bool CPDF_Object::IsReference() const {
  return false;
}

bool CPDF_Object::IsStream() const {
  return false;
}

bool CPDF_Object::IsString() const {
  return false;
}

CPDF_Array* CPDF_Object::AsArray() {
  return nullptr;
}

const CPDF_Array* CPDF_Object::AsArray() const {
  return nullptr;
}

CPDF_Boolean* CPDF_Object::AsBoolean() {
  return nullptr;
}

const CPDF_Boolean* CPDF_Object::AsBoolean() const {
  return nullptr;
}

CPDF_Dictionary* CPDF_Object::AsDictionary() {
  return nullptr;
}

const CPDF_Dictionary* CPDF_Object::AsDictionary() const {
  return nullptr;
}

CPDF_Name* CPDF_Object::AsName() {
  return nullptr;
}

const CPDF_Name* CPDF_Object::AsName() const {
  return nullptr;
}

CPDF_Number* CPDF_Object::AsNumber() {
  return nullptr;
}

const CPDF_Number* CPDF_Object::AsNumber() const {
  return nullptr;
}

CPDF_Reference* CPDF_Object::AsReference() {
  return nullptr;
}

const CPDF_Reference* CPDF_Object::AsReference() const {
  return nullptr;
}

CPDF_Stream* CPDF_Object::AsStream() {
  return nullptr;
}

const CPDF_Stream* CPDF_Object::AsStream() const {
  return nullptr;
}

CPDF_String* CPDF_Object::AsString() {
  return nullptr;
}

const CPDF_String* CPDF_Object::AsString() const {
  return nullptr;
}
