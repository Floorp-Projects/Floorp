// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFDOC_CPDF_INTERFORM_H_
#define CORE_FPDFDOC_CPDF_INTERFORM_H_

#include <map>
#include <memory>
#include <vector>

#include "core/fpdfapi/parser/fpdf_parser_decode.h"
#include "core/fpdfdoc/cpdf_defaultappearance.h"
#include "core/fxcrt/fx_string.h"
#include "core/fxcrt/fx_system.h"

class CFieldTree;
class CFDF_Document;
class CPDF_Document;
class CPDF_Dictionary;
class CPDF_Font;
class CPDF_FormControl;
class CPDF_FormField;
class CPDF_Object;
class CPDF_Page;
class IPDF_FormNotify;

CPDF_Font* AddNativeInterFormFont(CPDF_Dictionary*& pFormDict,
                                  CPDF_Document* pDocument,
                                  CFX_ByteString& csNameTag);

class CPDF_InterForm {
 public:
  explicit CPDF_InterForm(CPDF_Document* pDocument);
  ~CPDF_InterForm();

  static void SetUpdateAP(bool bUpdateAP);
  static bool IsUpdateAPEnabled();
  static CFX_ByteString GenerateNewResourceName(const CPDF_Dictionary* pResDict,
                                                const FX_CHAR* csType,
                                                int iMinLen,
                                                const FX_CHAR* csPrefix);
  static CPDF_Font* AddStandardFont(CPDF_Document* pDocument,
                                    CFX_ByteString csFontName);
  static CFX_ByteString GetNativeFont(uint8_t iCharSet, void* pLogFont);
  static uint8_t GetNativeCharSet();
  static CPDF_Font* AddNativeFont(uint8_t iCharSet, CPDF_Document* pDocument);
  static CPDF_Font* AddNativeFont(CPDF_Document* pDocument);

  size_t CountFields(const CFX_WideString& csFieldName) const;
  CPDF_FormField* GetField(uint32_t index,
                           const CFX_WideString& csFieldName) const;
  CPDF_FormField* GetFieldByDict(CPDF_Dictionary* pFieldDict) const;

  CPDF_FormControl* GetControlAtPoint(CPDF_Page* pPage,
                                      const CFX_PointF& point,
                                      int* z_order) const;
  CPDF_FormControl* GetControlByDict(const CPDF_Dictionary* pWidgetDict) const;

  bool NeedConstructAP() const;
  int CountFieldsInCalculationOrder();
  CPDF_FormField* GetFieldInCalculationOrder(int index);
  int FindFieldInCalculationOrder(const CPDF_FormField* pField);

  CPDF_Font* GetFormFont(CFX_ByteString csNameTag);
  CPDF_DefaultAppearance GetDefaultAppearance() const;
  int GetFormAlignment() const;

  bool CheckRequiredFields(const std::vector<CPDF_FormField*>* fields,
                           bool bIncludeOrExclude) const;

  std::unique_ptr<CFDF_Document> ExportToFDF(const CFX_WideStringC& pdf_path,
                                             bool bSimpleFileSpec) const;

  std::unique_ptr<CFDF_Document> ExportToFDF(
      const CFX_WideStringC& pdf_path,
      const std::vector<CPDF_FormField*>& fields,
      bool bIncludeOrExclude,
      bool bSimpleFileSpec) const;

  bool ResetForm(const std::vector<CPDF_FormField*>& fields,
                 bool bIncludeOrExclude,
                 bool bNotify);
  bool ResetForm(bool bNotify);

  void SetFormNotify(IPDF_FormNotify* pNotify);
  bool HasXFAForm() const;
  void FixPageFields(const CPDF_Page* pPage);

 private:
  friend class CPDF_FormControl;
  friend class CPDF_FormField;

  void LoadField(CPDF_Dictionary* pFieldDict, int nLevel);
  CPDF_FormField* AddTerminalField(CPDF_Dictionary* pFieldDict);
  CPDF_FormControl* AddControl(CPDF_FormField* pField,
                               CPDF_Dictionary* pWidgetDict);
  void FDF_ImportField(CPDF_Dictionary* pField,
                       const CFX_WideString& parent_name,
                       bool bNotify = false,
                       int nLevel = 0);
  bool ValidateFieldName(CFX_WideString& csNewFieldName,
                         int iType,
                         const CPDF_FormField* pExcludedField,
                         const CPDF_FormControl* pExcludedControl) const;

  static bool s_bUpdateAP;

  CPDF_Document* const m_pDocument;
  CPDF_Dictionary* m_pFormDict;
  std::map<const CPDF_Dictionary*, std::unique_ptr<CPDF_FormControl>>
      m_ControlMap;
  std::unique_ptr<CFieldTree> m_pFieldTree;
  CFX_ByteString m_bsEncoding;
  IPDF_FormNotify* m_pFormNotify;
};

#endif  // CORE_FPDFDOC_CPDF_INTERFORM_H_
