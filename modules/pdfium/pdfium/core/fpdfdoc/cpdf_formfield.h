// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFDOC_CPDF_FORMFIELD_H_
#define CORE_FPDFDOC_CPDF_FORMFIELD_H_

#include <vector>

#include "core/fpdfdoc/cpdf_aaction.h"
#include "core/fpdfdoc/cpdf_formfield.h"
#include "core/fxcrt/fx_basic.h"
#include "core/fxcrt/fx_string.h"
#include "core/fxcrt/fx_system.h"
#include "third_party/base/stl_util.h"

#define FIELDTYPE_UNKNOWN 0
#define FIELDTYPE_PUSHBUTTON 1
#define FIELDTYPE_CHECKBOX 2
#define FIELDTYPE_RADIOBUTTON 3
#define FIELDTYPE_COMBOBOX 4
#define FIELDTYPE_LISTBOX 5
#define FIELDTYPE_TEXTFIELD 6
#define FIELDTYPE_SIGNATURE 7

#define FORMFLAG_READONLY 0x01
#define FORMFLAG_REQUIRED 0x02
#define FORMFLAG_NOEXPORT 0x04

class CPDF_Dictionary;
class CPDF_Font;
class CPDF_FormControl;
class CPDF_InterForm;
class CPDF_String;

CPDF_Object* FPDF_GetFieldAttr(CPDF_Dictionary* pFieldDict,
                               const FX_CHAR* name,
                               int nLevel = 0);
CFX_WideString FPDF_GetFullName(CPDF_Dictionary* pFieldDict);

class CPDF_FormField {
 public:
  enum Type {
    Unknown,
    PushButton,
    RadioButton,
    CheckBox,
    Text,
    RichText,
    File,
    ListBox,
    ComboBox,
    Sign
  };

  CFX_WideString GetFullName() const;

  Type GetType() const { return m_Type; }
  uint32_t GetFlags() const { return m_Flags; }

  CPDF_Dictionary* GetFieldDict() const { return m_pDict; }
  void SetFieldDict(CPDF_Dictionary* pDict) { m_pDict = pDict; }

  bool ResetField(bool bNotify = false);

  int CountControls() const {
    return pdfium::CollectionSize<int>(m_ControlList);
  }

  CPDF_FormControl* GetControl(int index) const { return m_ControlList[index]; }

  int GetControlIndex(const CPDF_FormControl* pControl) const;
  int GetFieldType() const;

  CPDF_AAction GetAdditionalAction() const;
  CFX_WideString GetAlternateName() const;
  CFX_WideString GetMappingName() const;

  uint32_t GetFieldFlags() const;
  CFX_ByteString GetDefaultStyle() const;
  CFX_WideString GetRichTextString() const;

  CFX_WideString GetValue() const;
  CFX_WideString GetDefaultValue() const;
  bool SetValue(const CFX_WideString& value, bool bNotify = false);

  int GetMaxLen() const;
  int CountSelectedItems() const;
  int GetSelectedIndex(int index) const;

  bool ClearSelection(bool bNotify = false);
  bool IsItemSelected(int index) const;
  bool SetItemSelection(int index, bool bSelected, bool bNotify = false);

  bool IsItemDefaultSelected(int index) const;

  int GetDefaultSelectedItem() const;
  int CountOptions() const;

  CFX_WideString GetOptionLabel(int index) const;
  CFX_WideString GetOptionValue(int index) const;

  int FindOption(CFX_WideString csOptLabel) const;
  int FindOptionValue(const CFX_WideString& csOptValue) const;

  bool CheckControl(int iControlIndex, bool bChecked, bool bNotify = false);

  int GetTopVisibleIndex() const;
  int CountSelectedOptions() const;

  int GetSelectedOptionIndex(int index) const;
  bool IsOptionSelected(int iOptIndex) const;

  bool SelectOption(int iOptIndex, bool bSelected, bool bNotify = false);

  bool ClearSelectedOptions(bool bNotify = false);

#ifdef PDF_ENABLE_XFA
  bool ClearOptions(bool bNotify = false);

  int InsertOption(CFX_WideString csOptLabel,
                   int index = -1,
                   bool bNotify = false);
#endif  // PDF_ENABLE_XFA

  FX_FLOAT GetFontSize() const { return m_FontSize; }
  CPDF_Font* GetFont() const { return m_pFont; }

 private:
  friend class CPDF_InterForm;
  friend class CPDF_FormControl;

  CPDF_FormField(CPDF_InterForm* pForm, CPDF_Dictionary* pDict);
  ~CPDF_FormField();

  CFX_WideString GetValue(bool bDefault) const;
  bool SetValue(const CFX_WideString& value, bool bDefault, bool bNotify);

  void SyncFieldFlags();
  int FindListSel(CPDF_String* str);
  CFX_WideString GetOptionText(int index, int sub_index) const;

  void LoadDA();
  CFX_WideString GetCheckValue(bool bDefault) const;
  bool SetCheckValue(const CFX_WideString& value, bool bDefault, bool bNotify);

  bool NotifyBeforeSelectionChange(const CFX_WideString& value);
  void NotifyAfterSelectionChange();

  bool NotifyBeforeValueChange(const CFX_WideString& value);
  void NotifyAfterValueChange();

  bool NotifyListOrComboBoxBeforeChange(const CFX_WideString& value);
  void NotifyListOrComboBoxAfterChange();

  CPDF_FormField::Type m_Type;
  uint32_t m_Flags;
  CPDF_InterForm* const m_pForm;
  CPDF_Dictionary* m_pDict;
  std::vector<CPDF_FormControl*> m_ControlList;  // Owned by InterForm parent.
  FX_FLOAT m_FontSize;
  CPDF_Font* m_pFont;
};

#endif  // CORE_FPDFDOC_CPDF_FORMFIELD_H_
