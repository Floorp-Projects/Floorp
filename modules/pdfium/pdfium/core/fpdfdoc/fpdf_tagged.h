// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFDOC_FPDF_TAGGED_H_
#define CORE_FPDFDOC_FPDF_TAGGED_H_

#include <memory>

#include "core/fxge/fx_dib.h"

class CPDF_Dictionary;
class CPDF_Document;
class IPDF_StructElement;

class IPDF_StructTree {
 public:
  static std::unique_ptr<IPDF_StructTree> LoadPage(
      const CPDF_Document* pDoc,
      const CPDF_Dictionary* pPageDict);


  virtual int CountTopElements() const = 0;
  virtual IPDF_StructElement* GetTopElement(int i) const = 0;

 protected:
  friend std::default_delete<IPDF_StructTree>;
  virtual ~IPDF_StructTree() {}
};

class IPDF_StructElement {
 public:
  virtual IPDF_StructTree* GetTree() const = 0;
  virtual const CFX_ByteString& GetType() const = 0;
  virtual IPDF_StructElement* GetParent() const = 0;
  virtual CPDF_Dictionary* GetDict() const = 0;
  virtual int CountKids() const = 0;
  virtual IPDF_StructElement* GetKidIfElement(int index) const = 0;

  virtual CPDF_Object* GetAttr(const CFX_ByteStringC& owner,
                               const CFX_ByteStringC& name,
                               bool bInheritable = false,
                               FX_FLOAT fLevel = 0.0F) = 0;

  virtual CFX_ByteString GetName(const CFX_ByteStringC& owner,
                                 const CFX_ByteStringC& name,
                                 const CFX_ByteStringC& default_value,
                                 bool bInheritable = false,
                                 int subindex = -1) = 0;

  virtual FX_ARGB GetColor(const CFX_ByteStringC& owner,
                           const CFX_ByteStringC& name,
                           FX_ARGB default_value,
                           bool bInheritable = false,
                           int subindex = -1) = 0;

  virtual FX_FLOAT GetNumber(const CFX_ByteStringC& owner,
                             const CFX_ByteStringC& name,
                             FX_FLOAT default_value,
                             bool bInheritable = false,
                             int subindex = -1) = 0;

  virtual int GetInteger(const CFX_ByteStringC& owner,
                         const CFX_ByteStringC& name,
                         int default_value,
                         bool bInheritable = false,
                         int subindex = -1) = 0;

 protected:
  virtual ~IPDF_StructElement() {}
};

#endif  // CORE_FPDFDOC_FPDF_TAGGED_H_
