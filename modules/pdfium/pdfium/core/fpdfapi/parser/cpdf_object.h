// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_PARSER_CPDF_OBJECT_H_
#define CORE_FPDFAPI_PARSER_CPDF_OBJECT_H_

#include <memory>
#include <set>
#include <type_traits>

#include "core/fxcrt/fx_string.h"
#include "core/fxcrt/fx_system.h"

class CPDF_Array;
class CPDF_Boolean;
class CPDF_Dictionary;
class CPDF_Name;
class CPDF_Null;
class CPDF_Number;
class CPDF_Reference;
class CPDF_Stream;
class CPDF_String;

class CPDF_Object {
 public:
  static const uint32_t kInvalidObjNum = static_cast<uint32_t>(-1);
  enum Type {
    BOOLEAN = 1,
    NUMBER,
    STRING,
    NAME,
    ARRAY,
    DICTIONARY,
    STREAM,
    NULLOBJ,
    REFERENCE
  };

  virtual ~CPDF_Object();

  virtual Type GetType() const = 0;
  uint32_t GetObjNum() const { return m_ObjNum; }
  uint32_t GetGenNum() const { return m_GenNum; }
  bool IsInline() const { return m_ObjNum == 0; }

  // Create a deep copy of the object.
  virtual std::unique_ptr<CPDF_Object> Clone() const = 0;

  // Create a deep copy of the object except any reference object be
  // copied to the object it points to directly.
  virtual std::unique_ptr<CPDF_Object> CloneDirectObject() const;

  virtual CPDF_Object* GetDirect() const;
  virtual CFX_ByteString GetString() const;
  virtual CFX_WideString GetUnicodeText() const;
  virtual FX_FLOAT GetNumber() const;
  virtual int GetInteger() const;
  virtual CPDF_Dictionary* GetDict() const;

  virtual void SetString(const CFX_ByteString& str);

  virtual bool IsArray() const;
  virtual bool IsBoolean() const;
  virtual bool IsDictionary() const;
  virtual bool IsName() const;
  virtual bool IsNumber() const;
  virtual bool IsReference() const;
  virtual bool IsStream() const;
  virtual bool IsString() const;

  virtual CPDF_Array* AsArray();
  virtual const CPDF_Array* AsArray() const;
  virtual CPDF_Boolean* AsBoolean();
  virtual const CPDF_Boolean* AsBoolean() const;
  virtual CPDF_Dictionary* AsDictionary();
  virtual const CPDF_Dictionary* AsDictionary() const;
  virtual CPDF_Name* AsName();
  virtual const CPDF_Name* AsName() const;
  virtual CPDF_Number* AsNumber();
  virtual const CPDF_Number* AsNumber() const;
  virtual CPDF_Reference* AsReference();
  virtual const CPDF_Reference* AsReference() const;
  virtual CPDF_Stream* AsStream();
  virtual const CPDF_Stream* AsStream() const;
  virtual CPDF_String* AsString();
  virtual const CPDF_String* AsString() const;

 protected:
  friend class CPDF_Array;
  friend class CPDF_Dictionary;
  friend class CPDF_IndirectObjectHolder;
  friend class CPDF_Parser;
  friend class CPDF_Reference;
  friend class CPDF_Stream;

  CPDF_Object() : m_ObjNum(0), m_GenNum(0) {}

  std::unique_ptr<CPDF_Object> CloneObjectNonCyclic(bool bDirect) const;

  // Create a deep copy of the object with the option to either
  // copy a reference object or directly copy the object it refers to
  // when |bDirect| is true.
  // Also check cyclic reference against |pVisited|, no copy if it is found.
  // Complex objects should implement their own CloneNonCyclic()
  // function to properly check for possible loop.
  virtual std::unique_ptr<CPDF_Object> CloneNonCyclic(
      bool bDirect,
      std::set<const CPDF_Object*>* pVisited) const;

  uint32_t m_ObjNum;
  uint32_t m_GenNum;

 private:
  CPDF_Object(const CPDF_Object& src) {}
};

template <typename T>
struct CanInternStrings {
  static const bool value = std::is_same<T, CPDF_Array>::value ||
                            std::is_same<T, CPDF_Dictionary>::value ||
                            std::is_same<T, CPDF_Name>::value ||
                            std::is_same<T, CPDF_String>::value;
};

#endif  // CORE_FPDFAPI_PARSER_CPDF_OBJECT_H_
