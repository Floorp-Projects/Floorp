// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfdoc/cpdf_formfield.h"

#include <memory>
#include <set>
#include <utility>

#include "core/fpdfapi/parser/cfdf_document.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/parser/cpdf_name.h"
#include "core/fpdfapi/parser/cpdf_number.h"
#include "core/fpdfapi/parser/cpdf_simple_parser.h"
#include "core/fpdfapi/parser/cpdf_string.h"
#include "core/fpdfapi/parser/fpdf_parser_decode.h"
#include "core/fpdfdoc/cpdf_formcontrol.h"
#include "core/fpdfdoc/cpdf_interform.h"
#include "core/fpdfdoc/cpvt_generateap.h"
#include "third_party/base/stl_util.h"

namespace {

const int kMaxRecursion = 32;

const int kFormListMultiSelect = 0x100;

const int kFormComboEdit = 0x100;

const int kFormRadioNoToggleOff = 0x100;
const int kFormRadioUnison = 0x200;

const int kFormTextMultiLine = 0x100;
const int kFormTextPassword = 0x200;
const int kFormTextNoScroll = 0x400;
const int kFormTextComb = 0x800;

bool IsUnison(CPDF_FormField* pField) {
  if (pField->GetType() == CPDF_FormField::CheckBox)
    return true;
  return (pField->GetFieldFlags() & 0x2000000) != 0;
}

}  // namespace

CPDF_Object* FPDF_GetFieldAttr(CPDF_Dictionary* pFieldDict,
                               const FX_CHAR* name,
                               int nLevel) {
  if (nLevel > kMaxRecursion)
    return nullptr;
  if (!pFieldDict)
    return nullptr;

  CPDF_Object* pAttr = pFieldDict->GetDirectObjectFor(name);
  if (pAttr)
    return pAttr;

  CPDF_Dictionary* pParent = pFieldDict->GetDictFor("Parent");
  if (!pParent)
    return nullptr;
  return FPDF_GetFieldAttr(pParent, name, nLevel + 1);
}

CFX_WideString FPDF_GetFullName(CPDF_Dictionary* pFieldDict) {
  CFX_WideString full_name;
  std::set<CPDF_Dictionary*> visited;
  CPDF_Dictionary* pLevel = pFieldDict;
  while (pLevel) {
    visited.insert(pLevel);
    CFX_WideString short_name = pLevel->GetUnicodeTextFor("T");
    if (!short_name.IsEmpty()) {
      if (full_name.IsEmpty())
        full_name = short_name;
      else
        full_name = short_name + L"." + full_name;
    }
    pLevel = pLevel->GetDictFor("Parent");
    if (pdfium::ContainsKey(visited, pLevel))
      break;
  }
  return full_name;
}

CPDF_FormField::CPDF_FormField(CPDF_InterForm* pForm, CPDF_Dictionary* pDict)
    : m_Type(Unknown),
      m_pForm(pForm),
      m_pDict(pDict),
      m_FontSize(0),
      m_pFont(nullptr) {
  SyncFieldFlags();
}

CPDF_FormField::~CPDF_FormField() {}

void CPDF_FormField::SyncFieldFlags() {
  CFX_ByteString type_name = FPDF_GetFieldAttr(m_pDict, "FT")
                                 ? FPDF_GetFieldAttr(m_pDict, "FT")->GetString()
                                 : CFX_ByteString();
  uint32_t flags = FPDF_GetFieldAttr(m_pDict, "Ff")
                       ? FPDF_GetFieldAttr(m_pDict, "Ff")->GetInteger()
                       : 0;
  m_Flags = 0;
  if (flags & FORMFLAG_READONLY)
    m_Flags |= FORMFLAG_READONLY;
  if (flags & FORMFLAG_REQUIRED)
    m_Flags |= FORMFLAG_REQUIRED;
  if (flags & FORMFLAG_NOEXPORT)
    m_Flags |= FORMFLAG_NOEXPORT;

  if (type_name == "Btn") {
    if (flags & 0x8000) {
      m_Type = RadioButton;
      if (flags & 0x4000)
        m_Flags |= kFormRadioNoToggleOff;
      if (flags & 0x2000000)
        m_Flags |= kFormRadioUnison;
    } else if (flags & 0x10000) {
      m_Type = PushButton;
    } else {
      m_Type = CheckBox;
    }
  } else if (type_name == "Tx") {
    if (flags & 0x100000) {
      m_Type = File;
    } else if (flags & 0x2000000) {
      m_Type = RichText;
    } else {
      m_Type = Text;
      if (flags & 0x1000)
        m_Flags |= kFormTextMultiLine;
      if (flags & 0x2000)
        m_Flags |= kFormTextPassword;
      if (flags & 0x800000)
        m_Flags |= kFormTextNoScroll;
      if (flags & 0x100000)
        m_Flags |= kFormTextComb;
    }
    LoadDA();
  } else if (type_name == "Ch") {
    if (flags & 0x20000) {
      m_Type = ComboBox;
      if (flags & 0x40000)
        m_Flags |= kFormComboEdit;
    } else {
      m_Type = ListBox;
      if (flags & 0x200000)
        m_Flags |= kFormListMultiSelect;
    }
    LoadDA();
  } else if (type_name == "Sig") {
    m_Type = Sign;
  }
}

CFX_WideString CPDF_FormField::GetFullName() const {
  return FPDF_GetFullName(m_pDict);
}

bool CPDF_FormField::ResetField(bool bNotify) {
  switch (m_Type) {
    case CPDF_FormField::CheckBox:
    case CPDF_FormField::RadioButton: {
      int iCount = CountControls();
      if (iCount) {
        // TODO(weili): Check whether anything special needs to be done for
        // unison field. Otherwise, merge these branches.
        if (IsUnison(this)) {
          for (int i = 0; i < iCount; i++)
            CheckControl(i, GetControl(i)->IsDefaultChecked(), false);
        } else {
          for (int i = 0; i < iCount; i++)
            CheckControl(i, GetControl(i)->IsDefaultChecked(), false);
        }
      }
      if (bNotify && m_pForm->m_pFormNotify)
        m_pForm->m_pFormNotify->AfterCheckedStatusChange(this);
      break;
    }
    case CPDF_FormField::ComboBox:
    case CPDF_FormField::ListBox: {
      CFX_WideString csValue;
      ClearSelection();
      int iIndex = GetDefaultSelectedItem();
      if (iIndex >= 0)
        csValue = GetOptionLabel(iIndex);

      if (bNotify && !NotifyListOrComboBoxBeforeChange(csValue))
        return false;

      SetItemSelection(iIndex, true);
      if (bNotify)
        NotifyListOrComboBoxAfterChange();
      break;
    }
    case CPDF_FormField::Text:
    case CPDF_FormField::RichText:
    case CPDF_FormField::File:
    default: {
      CPDF_Object* pDV = FPDF_GetFieldAttr(m_pDict, "DV");
      CFX_WideString csDValue;
      if (pDV)
        csDValue = pDV->GetUnicodeText();

      CPDF_Object* pV = FPDF_GetFieldAttr(m_pDict, "V");
      CFX_WideString csValue;
      if (pV)
        csValue = pV->GetUnicodeText();

      CPDF_Object* pRV = FPDF_GetFieldAttr(m_pDict, "RV");
      if (!pRV && (csDValue == csValue))
        return false;

      if (bNotify && !NotifyBeforeValueChange(csDValue))
        return false;

      if (pDV) {
        std::unique_ptr<CPDF_Object> pClone = pDV->Clone();
        if (!pClone)
          return false;

        m_pDict->SetFor("V", std::move(pClone));
        if (pRV)
          m_pDict->SetFor("RV", pDV->Clone());
      } else {
        m_pDict->RemoveFor("V");
        m_pDict->RemoveFor("RV");
      }
      if (bNotify)
        NotifyAfterValueChange();
      break;
    }
  }
  return true;
}

int CPDF_FormField::GetControlIndex(const CPDF_FormControl* pControl) const {
  if (!pControl)
    return -1;

  auto it = std::find(m_ControlList.begin(), m_ControlList.end(), pControl);
  return it != m_ControlList.end() ? it - m_ControlList.begin() : -1;
}

int CPDF_FormField::GetFieldType() const {
  switch (m_Type) {
    case PushButton:
      return FIELDTYPE_PUSHBUTTON;
    case CheckBox:
      return FIELDTYPE_CHECKBOX;
    case RadioButton:
      return FIELDTYPE_RADIOBUTTON;
    case ComboBox:
      return FIELDTYPE_COMBOBOX;
    case ListBox:
      return FIELDTYPE_LISTBOX;
    case Text:
    case RichText:
    case File:
      return FIELDTYPE_TEXTFIELD;
    case Sign:
      return FIELDTYPE_SIGNATURE;
    default:
      break;
  }
  return FIELDTYPE_UNKNOWN;
}

CPDF_AAction CPDF_FormField::GetAdditionalAction() const {
  CPDF_Object* pObj = FPDF_GetFieldAttr(m_pDict, "AA");
  return CPDF_AAction(pObj ? pObj->GetDict() : nullptr);
}

CFX_WideString CPDF_FormField::GetAlternateName() const {
  CPDF_Object* pObj = FPDF_GetFieldAttr(m_pDict, "TU");
  return pObj ? pObj->GetUnicodeText() : L"";
}

CFX_WideString CPDF_FormField::GetMappingName() const {
  CPDF_Object* pObj = FPDF_GetFieldAttr(m_pDict, "TM");
  return pObj ? pObj->GetUnicodeText() : L"";
}

uint32_t CPDF_FormField::GetFieldFlags() const {
  CPDF_Object* pObj = FPDF_GetFieldAttr(m_pDict, "Ff");
  return pObj ? pObj->GetInteger() : 0;
}

CFX_ByteString CPDF_FormField::GetDefaultStyle() const {
  CPDF_Object* pObj = FPDF_GetFieldAttr(m_pDict, "DS");
  return pObj ? pObj->GetString() : "";
}

CFX_WideString CPDF_FormField::GetRichTextString() const {
  CPDF_Object* pObj = FPDF_GetFieldAttr(m_pDict, "RV");
  return pObj ? pObj->GetUnicodeText() : L"";
}

CFX_WideString CPDF_FormField::GetValue(bool bDefault) const {
  if (GetType() == CheckBox || GetType() == RadioButton)
    return GetCheckValue(bDefault);

  CPDF_Object* pValue = FPDF_GetFieldAttr(m_pDict, bDefault ? "DV" : "V");
  if (!pValue) {
    if (!bDefault) {
      if (m_Type == RichText)
        pValue = FPDF_GetFieldAttr(m_pDict, "V");
      if (!pValue && m_Type != Text)
        pValue = FPDF_GetFieldAttr(m_pDict, "DV");
    }
    if (!pValue)
      return CFX_WideString();
  }

  switch (pValue->GetType()) {
    case CPDF_Object::STRING:
    case CPDF_Object::STREAM:
      return pValue->GetUnicodeText();
    case CPDF_Object::ARRAY:
      pValue = pValue->AsArray()->GetDirectObjectAt(0);
      if (pValue)
        return pValue->GetUnicodeText();
      break;
    default:
      break;
  }
  return CFX_WideString();
}

CFX_WideString CPDF_FormField::GetValue() const {
  return GetValue(false);
}

CFX_WideString CPDF_FormField::GetDefaultValue() const {
  return GetValue(true);
}

bool CPDF_FormField::SetValue(const CFX_WideString& value,
                              bool bDefault,
                              bool bNotify) {
  switch (m_Type) {
    case CheckBox:
    case RadioButton: {
      SetCheckValue(value, bDefault, bNotify);
      return true;
    }
    case File:
    case RichText:
    case Text:
    case ComboBox: {
      CFX_WideString csValue = value;
      if (bNotify && !NotifyBeforeValueChange(csValue))
        return false;

      CFX_ByteString key(bDefault ? "DV" : "V");
      int iIndex = FindOptionValue(csValue);
      if (iIndex < 0) {
        CFX_ByteString bsEncodeText = PDF_EncodeText(csValue);
        m_pDict->SetNewFor<CPDF_String>(key, bsEncodeText, false);
        if (m_Type == RichText && !bDefault)
          m_pDict->SetNewFor<CPDF_String>("RV", bsEncodeText, false);
        m_pDict->RemoveFor("I");
      } else {
        m_pDict->SetNewFor<CPDF_String>(key, PDF_EncodeText(csValue), false);
        if (!bDefault) {
          ClearSelection();
          SetItemSelection(iIndex, true);
        }
      }
      if (bNotify)
        NotifyAfterValueChange();
      break;
    }
    case ListBox: {
      int iIndex = FindOptionValue(value);
      if (iIndex < 0)
        return false;

      if (bDefault && iIndex == GetDefaultSelectedItem())
        return false;

      if (bNotify && !NotifyBeforeSelectionChange(value))
        return false;

      if (!bDefault) {
        ClearSelection();
        SetItemSelection(iIndex, true);
      }
      if (bNotify)
        NotifyAfterSelectionChange();
      break;
    }
    default:
      break;
  }
  return true;
}

bool CPDF_FormField::SetValue(const CFX_WideString& value, bool bNotify) {
  return SetValue(value, false, bNotify);
}

int CPDF_FormField::GetMaxLen() const {
  if (CPDF_Object* pObj = FPDF_GetFieldAttr(m_pDict, "MaxLen"))
    return pObj->GetInteger();

  for (const auto& pControl : m_ControlList) {
    if (!pControl)
      continue;
    CPDF_Dictionary* pWidgetDict = pControl->m_pWidgetDict;
    if (pWidgetDict->KeyExist("MaxLen"))
      return pWidgetDict->GetIntegerFor("MaxLen");
  }
  return 0;
}

int CPDF_FormField::CountSelectedItems() const {
  CPDF_Object* pValue = FPDF_GetFieldAttr(m_pDict, "V");
  if (!pValue) {
    pValue = FPDF_GetFieldAttr(m_pDict, "I");
    if (!pValue)
      return 0;
  }

  if (pValue->IsString() || pValue->IsNumber())
    return pValue->GetString().IsEmpty() ? 0 : 1;
  if (CPDF_Array* pArray = pValue->AsArray())
    return pArray->GetCount();
  return 0;
}

int CPDF_FormField::GetSelectedIndex(int index) const {
  CPDF_Object* pValue = FPDF_GetFieldAttr(m_pDict, "V");
  if (!pValue) {
    pValue = FPDF_GetFieldAttr(m_pDict, "I");
    if (!pValue)
      return -1;
  }
  if (pValue->IsNumber())
    return pValue->GetInteger();

  CFX_WideString sel_value;
  if (pValue->IsString()) {
    if (index != 0)
      return -1;
    sel_value = pValue->GetUnicodeText();
  } else {
    CPDF_Array* pArray = pValue->AsArray();
    if (!pArray || index < 0)
      return -1;

    CPDF_Object* elementValue = pArray->GetDirectObjectAt(index);
    sel_value =
        elementValue ? elementValue->GetUnicodeText() : CFX_WideString();
  }
  if (index < CountSelectedOptions()) {
    int iOptIndex = GetSelectedOptionIndex(index);
    CFX_WideString csOpt = GetOptionValue(iOptIndex);
    if (csOpt == sel_value)
      return iOptIndex;
  }
  for (int i = 0; i < CountOptions(); i++) {
    if (sel_value == GetOptionValue(i))
      return i;
  }
  return -1;
}

bool CPDF_FormField::ClearSelection(bool bNotify) {
  if (bNotify && m_pForm->m_pFormNotify) {
    CFX_WideString csValue;
    int iIndex = GetSelectedIndex(0);
    if (iIndex >= 0)
      csValue = GetOptionLabel(iIndex);

    if (!NotifyListOrComboBoxBeforeChange(csValue))
      return false;
  }
  m_pDict->RemoveFor("V");
  m_pDict->RemoveFor("I");
  if (bNotify)
    NotifyListOrComboBoxAfterChange();
  return true;
}

bool CPDF_FormField::IsItemSelected(int index) const {
  ASSERT(GetType() == ComboBox || GetType() == ListBox);
  if (index < 0 || index >= CountOptions())
    return false;
  if (IsOptionSelected(index))
    return true;

  CFX_WideString opt_value = GetOptionValue(index);
  CPDF_Object* pValue = FPDF_GetFieldAttr(m_pDict, "V");
  if (!pValue) {
    pValue = FPDF_GetFieldAttr(m_pDict, "I");
    if (!pValue)
      return false;
  }

  if (pValue->IsString())
    return pValue->GetUnicodeText() == opt_value;

  if (pValue->IsNumber()) {
    if (pValue->GetString().IsEmpty())
      return false;
    return (pValue->GetInteger() == index);
  }

  CPDF_Array* pArray = pValue->AsArray();
  if (!pArray)
    return false;

  int iPos = -1;
  for (int j = 0; j < CountSelectedOptions(); j++) {
    if (GetSelectedOptionIndex(j) == index) {
      iPos = j;
      break;
    }
  }
  for (int i = 0; i < static_cast<int>(pArray->GetCount()); i++)
    if (pArray->GetDirectObjectAt(i)->GetUnicodeText() == opt_value &&
        i == iPos) {
      return true;
    }
  return false;
}

bool CPDF_FormField::SetItemSelection(int index, bool bSelected, bool bNotify) {
  ASSERT(GetType() == ComboBox || GetType() == ListBox);
  if (index < 0 || index >= CountOptions())
    return false;

  CFX_WideString opt_value = GetOptionValue(index);
  if (bNotify && !NotifyListOrComboBoxBeforeChange(opt_value))
    return false;

  if (bSelected) {
    if (GetType() == ListBox) {
      SelectOption(index, true);
      if (!(m_Flags & kFormListMultiSelect)) {
        m_pDict->SetNewFor<CPDF_String>("V", PDF_EncodeText(opt_value), false);
      } else {
        CPDF_Array* pArray = m_pDict->SetNewFor<CPDF_Array>("V");
        for (int i = 0; i < CountOptions(); i++) {
          if (i == index || IsItemSelected(i)) {
            opt_value = GetOptionValue(i);
            pArray->AddNew<CPDF_String>(PDF_EncodeText(opt_value), false);
          }
        }
      }
    } else {
      m_pDict->SetNewFor<CPDF_String>("V", PDF_EncodeText(opt_value), false);
      CPDF_Array* pI = m_pDict->SetNewFor<CPDF_Array>("I");
      pI->AddNew<CPDF_Number>(index);
    }
  } else {
    CPDF_Object* pValue = FPDF_GetFieldAttr(m_pDict, "V");
    if (pValue) {
      if (GetType() == ListBox) {
        SelectOption(index, false);
        if (pValue->IsString()) {
          if (pValue->GetUnicodeText() == opt_value)
            m_pDict->RemoveFor("V");
        } else if (pValue->IsArray()) {
          std::unique_ptr<CPDF_Array> pArray(new CPDF_Array);
          for (int i = 0; i < CountOptions(); i++) {
            if (i != index && IsItemSelected(i)) {
              opt_value = GetOptionValue(i);
              pArray->AddNew<CPDF_String>(PDF_EncodeText(opt_value), false);
            }
          }
          if (pArray->GetCount() > 0)
            m_pDict->SetFor("V", std::move(pArray));
        }
      } else {
        m_pDict->RemoveFor("V");
        m_pDict->RemoveFor("I");
      }
    }
  }
  if (bNotify)
    NotifyListOrComboBoxAfterChange();
  return true;
}

bool CPDF_FormField::IsItemDefaultSelected(int index) const {
  ASSERT(GetType() == ComboBox || GetType() == ListBox);
  if (index < 0 || index >= CountOptions())
    return false;
  int iDVIndex = GetDefaultSelectedItem();
  return iDVIndex >= 0 && iDVIndex == index;
}

int CPDF_FormField::GetDefaultSelectedItem() const {
  ASSERT(GetType() == ComboBox || GetType() == ListBox);
  CPDF_Object* pValue = FPDF_GetFieldAttr(m_pDict, "DV");
  if (!pValue)
    return -1;
  CFX_WideString csDV = pValue->GetUnicodeText();
  if (csDV.IsEmpty())
    return -1;
  for (int i = 0; i < CountOptions(); i++) {
    if (csDV == GetOptionValue(i))
      return i;
  }
  return -1;
}

int CPDF_FormField::CountOptions() const {
  CPDF_Array* pArray = ToArray(FPDF_GetFieldAttr(m_pDict, "Opt"));
  return pArray ? pArray->GetCount() : 0;
}

CFX_WideString CPDF_FormField::GetOptionText(int index, int sub_index) const {
  CPDF_Array* pArray = ToArray(FPDF_GetFieldAttr(m_pDict, "Opt"));
  if (!pArray)
    return CFX_WideString();

  CPDF_Object* pOption = pArray->GetDirectObjectAt(index);
  if (!pOption)
    return CFX_WideString();
  if (CPDF_Array* pOptionArray = pOption->AsArray())
    pOption = pOptionArray->GetDirectObjectAt(sub_index);

  CPDF_String* pString = ToString(pOption);
  return pString ? pString->GetUnicodeText() : CFX_WideString();
}

CFX_WideString CPDF_FormField::GetOptionLabel(int index) const {
  return GetOptionText(index, 1);
}

CFX_WideString CPDF_FormField::GetOptionValue(int index) const {
  return GetOptionText(index, 0);
}

int CPDF_FormField::FindOption(CFX_WideString csOptLabel) const {
  for (int i = 0; i < CountOptions(); i++) {
    if (GetOptionValue(i) == csOptLabel)
      return i;
  }
  return -1;
}

int CPDF_FormField::FindOptionValue(const CFX_WideString& csOptValue) const {
  for (int i = 0; i < CountOptions(); i++) {
    if (GetOptionValue(i) == csOptValue)
      return i;
  }
  return -1;
}

#ifdef PDF_ENABLE_XFA
int CPDF_FormField::InsertOption(CFX_WideString csOptLabel,
                                 int index,
                                 bool bNotify) {
  if (csOptLabel.IsEmpty())
    return -1;

  if (bNotify && !NotifyListOrComboBoxBeforeChange(csOptLabel))
    return -1;

  CFX_ByteString csStr =
      PDF_EncodeText(csOptLabel.c_str(), csOptLabel.GetLength());
  CPDF_Array* pOpt = ToArray(FPDF_GetFieldAttr(m_pDict, "Opt"));
  if (!pOpt)
    pOpt = m_pDict->SetNewFor<CPDF_Array>("Opt");

  int iCount = pdfium::base::checked_cast<int>(pOpt->GetCount());
  if (index >= iCount) {
    pOpt->AddNew<CPDF_String>(csStr, false);
    index = iCount;
  } else {
    pOpt->InsertNewAt<CPDF_String>(index, csStr, false);
  }

  if (bNotify)
    NotifyListOrComboBoxAfterChange();
  return index;
}

bool CPDF_FormField::ClearOptions(bool bNotify) {
  if (bNotify && m_pForm->m_pFormNotify) {
    CFX_WideString csValue;
    int iIndex = GetSelectedIndex(0);
    if (iIndex >= 0)
      csValue = GetOptionLabel(iIndex);
    if (!NotifyListOrComboBoxBeforeChange(csValue))
      return false;
  }

  m_pDict->RemoveFor("Opt");
  m_pDict->RemoveFor("V");
  m_pDict->RemoveFor("DV");
  m_pDict->RemoveFor("I");
  m_pDict->RemoveFor("TI");

  if (bNotify)
    NotifyListOrComboBoxAfterChange();

  return true;
}
#endif  // PDF_ENABLE_XFA

bool CPDF_FormField::CheckControl(int iControlIndex,
                                  bool bChecked,
                                  bool bNotify) {
  ASSERT(GetType() == CheckBox || GetType() == RadioButton);
  CPDF_FormControl* pControl = GetControl(iControlIndex);
  if (!pControl)
    return false;
  if (!bChecked && pControl->IsChecked() == bChecked)
    return false;

  CFX_WideString csWExport = pControl->GetExportValue();
  CFX_ByteString csBExport = PDF_EncodeText(csWExport);
  int iCount = CountControls();
  bool bUnison = IsUnison(this);
  for (int i = 0; i < iCount; i++) {
    CPDF_FormControl* pCtrl = GetControl(i);
    if (bUnison) {
      CFX_WideString csEValue = pCtrl->GetExportValue();
      if (csEValue == csWExport) {
        if (pCtrl->GetOnStateName() == pControl->GetOnStateName())
          pCtrl->CheckControl(bChecked);
        else if (bChecked)
          pCtrl->CheckControl(false);
      } else if (bChecked) {
        pCtrl->CheckControl(false);
      }
    } else {
      if (i == iControlIndex)
        pCtrl->CheckControl(bChecked);
      else if (bChecked)
        pCtrl->CheckControl(false);
    }
  }

  CPDF_Object* pOpt = FPDF_GetFieldAttr(m_pDict, "Opt");
  if (!ToArray(pOpt)) {
    if (bChecked) {
      m_pDict->SetNewFor<CPDF_Name>("V", csBExport);
    } else {
      CFX_ByteString csV;
      CPDF_Object* pV = FPDF_GetFieldAttr(m_pDict, "V");
      if (pV)
        csV = pV->GetString();
      if (csV == csBExport)
        m_pDict->SetNewFor<CPDF_Name>("V", "Off");
    }
  } else if (bChecked) {
    CFX_ByteString csIndex;
    csIndex.Format("%d", iControlIndex);
    m_pDict->SetNewFor<CPDF_Name>("V", csIndex);
  }
  if (bNotify && m_pForm->m_pFormNotify)
    m_pForm->m_pFormNotify->AfterCheckedStatusChange(this);
  return true;
}

CFX_WideString CPDF_FormField::GetCheckValue(bool bDefault) const {
  ASSERT(GetType() == CheckBox || GetType() == RadioButton);
  CFX_WideString csExport = L"Off";
  int iCount = CountControls();
  for (int i = 0; i < iCount; i++) {
    CPDF_FormControl* pControl = GetControl(i);
    bool bChecked =
        bDefault ? pControl->IsDefaultChecked() : pControl->IsChecked();
    if (bChecked) {
      csExport = pControl->GetExportValue();
      break;
    }
  }
  return csExport;
}

bool CPDF_FormField::SetCheckValue(const CFX_WideString& value,
                                   bool bDefault,
                                   bool bNotify) {
  ASSERT(GetType() == CheckBox || GetType() == RadioButton);
  int iCount = CountControls();
  for (int i = 0; i < iCount; i++) {
    CPDF_FormControl* pControl = GetControl(i);
    CFX_WideString csExport = pControl->GetExportValue();
    bool val = csExport == value;
    if (!bDefault)
      CheckControl(GetControlIndex(pControl), val);
    if (val)
      break;
  }
  if (bNotify && m_pForm->m_pFormNotify)
    m_pForm->m_pFormNotify->AfterCheckedStatusChange(this);
  return true;
}

int CPDF_FormField::GetTopVisibleIndex() const {
  CPDF_Object* pObj = FPDF_GetFieldAttr(m_pDict, "TI");
  return pObj ? pObj->GetInteger() : 0;
}

int CPDF_FormField::CountSelectedOptions() const {
  CPDF_Array* pArray = ToArray(FPDF_GetFieldAttr(m_pDict, "I"));
  return pArray ? pArray->GetCount() : 0;
}

int CPDF_FormField::GetSelectedOptionIndex(int index) const {
  CPDF_Array* pArray = ToArray(FPDF_GetFieldAttr(m_pDict, "I"));
  if (!pArray)
    return -1;

  int iCount = pArray->GetCount();
  if (iCount < 0 || index >= iCount)
    return -1;
  return pArray->GetIntegerAt(index);
}

bool CPDF_FormField::IsOptionSelected(int iOptIndex) const {
  CPDF_Array* pArray = ToArray(FPDF_GetFieldAttr(m_pDict, "I"));
  if (!pArray)
    return false;

  for (const auto& pObj : *pArray) {
    if (pObj->GetInteger() == iOptIndex)
      return true;
  }
  return false;
}

bool CPDF_FormField::SelectOption(int iOptIndex, bool bSelected, bool bNotify) {
  CPDF_Array* pArray = m_pDict->GetArrayFor("I");
  if (!pArray) {
    if (!bSelected)
      return true;

    pArray = m_pDict->SetNewFor<CPDF_Array>("I");
  }

  bool bReturn = false;
  for (size_t i = 0; i < pArray->GetCount(); i++) {
    int iFind = pArray->GetIntegerAt(i);
    if (iFind == iOptIndex) {
      if (bSelected)
        return true;

      if (bNotify && m_pForm->m_pFormNotify) {
        CFX_WideString csValue = GetOptionLabel(iOptIndex);
        if (!NotifyListOrComboBoxBeforeChange(csValue))
          return false;
      }
      pArray->RemoveAt(i);
      bReturn = true;
      break;
    }

    if (iFind > iOptIndex) {
      if (!bSelected)
        continue;

      if (bNotify && m_pForm->m_pFormNotify) {
        CFX_WideString csValue = GetOptionLabel(iOptIndex);
        if (!NotifyListOrComboBoxBeforeChange(csValue))
          return false;
      }
      pArray->InsertNewAt<CPDF_Number>(i, iOptIndex);
      bReturn = true;
      break;
    }
  }
  if (!bReturn) {
    if (bSelected)
      pArray->AddNew<CPDF_Number>(iOptIndex);

    if (pArray->IsEmpty())
      m_pDict->RemoveFor("I");
  }
  if (bNotify)
    NotifyListOrComboBoxAfterChange();

  return true;
}

bool CPDF_FormField::ClearSelectedOptions(bool bNotify) {
  if (bNotify && m_pForm->m_pFormNotify) {
    CFX_WideString csValue;
    int iIndex = GetSelectedIndex(0);
    if (iIndex >= 0)
      csValue = GetOptionLabel(iIndex);

    if (!NotifyListOrComboBoxBeforeChange(csValue))
      return false;
  }
  m_pDict->RemoveFor("I");
  if (bNotify)
    NotifyListOrComboBoxAfterChange();

  return true;
}

void CPDF_FormField::LoadDA() {
  CPDF_Dictionary* pFormDict = m_pForm->m_pFormDict;
  if (!pFormDict)
    return;

  CFX_ByteString DA;
  if (CPDF_Object* pObj = FPDF_GetFieldAttr(m_pDict, "DA"))
    DA = pObj->GetString();

  if (DA.IsEmpty())
    DA = pFormDict->GetStringFor("DA");

  if (DA.IsEmpty())
    return;

  CPDF_Dictionary* pDR = pFormDict->GetDictFor("DR");
  if (!pDR)
    return;

  CPDF_Dictionary* pFont = pDR->GetDictFor("Font");
  if (!pFont)
    return;

  CPDF_SimpleParser syntax(DA.AsStringC());
  syntax.FindTagParamFromStart("Tf", 2);
  CFX_ByteString font_name(syntax.GetWord());
  CPDF_Dictionary* pFontDict = pFont->GetDictFor(font_name);
  if (!pFontDict)
    return;

  m_pFont = m_pForm->m_pDocument->LoadFont(pFontDict);
  m_FontSize = FX_atof(syntax.GetWord());
}

bool CPDF_FormField::NotifyBeforeSelectionChange(const CFX_WideString& value) {
  if (!m_pForm->m_pFormNotify)
    return true;
  return m_pForm->m_pFormNotify->BeforeSelectionChange(this, value) >= 0;
}

void CPDF_FormField::NotifyAfterSelectionChange() {
  if (!m_pForm->m_pFormNotify)
    return;
  m_pForm->m_pFormNotify->AfterSelectionChange(this);
}

bool CPDF_FormField::NotifyBeforeValueChange(const CFX_WideString& value) {
  if (!m_pForm->m_pFormNotify)
    return true;
  return m_pForm->m_pFormNotify->BeforeValueChange(this, value) >= 0;
}

void CPDF_FormField::NotifyAfterValueChange() {
  if (!m_pForm->m_pFormNotify)
    return;
  m_pForm->m_pFormNotify->AfterValueChange(this);
}

bool CPDF_FormField::NotifyListOrComboBoxBeforeChange(
    const CFX_WideString& value) {
  switch (GetType()) {
    case ListBox:
      return NotifyBeforeSelectionChange(value);
    case ComboBox:
      return NotifyBeforeValueChange(value);
    default:
      return true;
  }
}

void CPDF_FormField::NotifyListOrComboBoxAfterChange() {
  switch (GetType()) {
    case ListBox:
      NotifyAfterSelectionChange();
      break;
    case ComboBox:
      NotifyAfterValueChange();
      break;
    default:
      break;
  }
}
