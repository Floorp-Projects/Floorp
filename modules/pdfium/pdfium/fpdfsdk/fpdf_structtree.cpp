// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/fpdf_structtree.h"

#include <memory>

#include "core/fpdfapi/page/cpdf_page.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfdoc/fpdf_tagged.h"
#include "fpdfsdk/fsdk_define.h"

namespace {

IPDF_StructTree* ToStructTree(FPDF_STRUCTTREE struct_tree) {
  return reinterpret_cast<IPDF_StructTree*>(struct_tree);
}

IPDF_StructElement* ToStructTreeElement(FPDF_STRUCTELEMENT struct_element) {
  return reinterpret_cast<IPDF_StructElement*>(struct_element);
}

}  // namespace

DLLEXPORT FPDF_STRUCTTREE STDCALL FPDF_StructTree_GetForPage(FPDF_PAGE page) {
  CPDF_Page* pPage = CPDFPageFromFPDFPage(page);
  if (!pPage)
    return nullptr;
  return IPDF_StructTree::LoadPage(pPage->m_pDocument, pPage->m_pFormDict)
      .release();
}

DLLEXPORT void STDCALL FPDF_StructTree_Close(FPDF_STRUCTTREE struct_tree) {
  std::unique_ptr<IPDF_StructTree>(ToStructTree(struct_tree));
}

DLLEXPORT int STDCALL
FPDF_StructTree_CountChildren(FPDF_STRUCTTREE struct_tree) {
  IPDF_StructTree* tree = ToStructTree(struct_tree);
  return tree ? tree->CountTopElements() : -1;
}

DLLEXPORT FPDF_STRUCTELEMENT STDCALL
FPDF_StructTree_GetChildAtIndex(FPDF_STRUCTTREE struct_tree, int index) {
  IPDF_StructTree* tree = ToStructTree(struct_tree);
  if (!tree || index < 0 || index >= tree->CountTopElements())
    return nullptr;
  return tree->GetTopElement(index);
}

DLLEXPORT unsigned long STDCALL
FPDF_StructElement_GetAltText(FPDF_STRUCTELEMENT struct_element,
                              void* buffer,
                              unsigned long buflen) {
  IPDF_StructElement* elem = ToStructTreeElement(struct_element);
  if (!elem)
    return 0;

  CPDF_Dictionary* dict = elem->GetDict();
  if (!dict)
    return 0;

  CFX_WideString str = elem->GetDict()->GetUnicodeTextFor("Alt");
  if (str.IsEmpty())
    return 0;

  CFX_ByteString encodedStr = str.UTF16LE_Encode();
  const unsigned long len = encodedStr.GetLength();
  if (buffer && len <= buflen)
    FXSYS_memcpy(buffer, encodedStr.c_str(), len);
  return len;
}

DLLEXPORT int STDCALL
FPDF_StructElement_CountChildren(FPDF_STRUCTELEMENT struct_element) {
  IPDF_StructElement* elem = ToStructTreeElement(struct_element);
  return elem ? elem->CountKids() : -1;
}

DLLEXPORT FPDF_STRUCTELEMENT STDCALL
FPDF_StructElement_GetChildAtIndex(FPDF_STRUCTELEMENT struct_element,
                                   int index) {
  IPDF_StructElement* elem = ToStructTreeElement(struct_element);
  if (!elem || index < 0 || index >= elem->CountKids())
    return nullptr;

  return elem->GetKidIfElement(index);
}
