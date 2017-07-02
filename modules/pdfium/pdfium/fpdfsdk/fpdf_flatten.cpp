// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "public/fpdf_flatten.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "core/fpdfapi/page/cpdf_page.h"
#include "core/fpdfapi/page/cpdf_pageobject.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/parser/cpdf_name.h"
#include "core/fpdfapi/parser/cpdf_number.h"
#include "core/fpdfapi/parser/cpdf_reference.h"
#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fpdfapi/parser/cpdf_stream_acc.h"
#include "core/fpdfdoc/cpdf_annot.h"
#include "fpdfsdk/fsdk_define.h"
#include "third_party/base/stl_util.h"

enum FPDF_TYPE { MAX, MIN };
enum FPDF_VALUE { TOP, LEFT, RIGHT, BOTTOM };

namespace {

bool IsValiableRect(CFX_FloatRect rect, CFX_FloatRect rcPage) {
  if (rect.left - rect.right > 0.000001f || rect.bottom - rect.top > 0.000001f)
    return false;

  if (rect.left == 0.0f && rect.top == 0.0f && rect.right == 0.0f &&
      rect.bottom == 0.0f)
    return false;

  if (!rcPage.IsEmpty()) {
    if (rect.left - rcPage.left < -10.000001f ||
        rect.right - rcPage.right > 10.000001f ||
        rect.top - rcPage.top > 10.000001f ||
        rect.bottom - rcPage.bottom < -10.000001f)
      return false;
  }

  return true;
}

void GetContentsRect(CPDF_Document* pDoc,
                     CPDF_Dictionary* pDict,
                     std::vector<CFX_FloatRect>* pRectArray) {
  std::unique_ptr<CPDF_Page> pPDFPage(new CPDF_Page(pDoc, pDict, false));
  pPDFPage->ParseContent();

  for (const auto& pPageObject : *pPDFPage->GetPageObjectList()) {
    CFX_FloatRect rc;
    rc.left = pPageObject->m_Left;
    rc.right = pPageObject->m_Right;
    rc.bottom = pPageObject->m_Bottom;
    rc.top = pPageObject->m_Top;
    if (IsValiableRect(rc, pDict->GetRectFor("MediaBox")))
      pRectArray->push_back(rc);
  }
}

void ParserStream(CPDF_Dictionary* pPageDic,
                  CPDF_Dictionary* pStream,
                  std::vector<CFX_FloatRect>* pRectArray,
                  std::vector<CPDF_Dictionary*>* pObjectArray) {
  if (!pStream)
    return;
  CFX_FloatRect rect;
  if (pStream->KeyExist("Rect"))
    rect = pStream->GetRectFor("Rect");
  else if (pStream->KeyExist("BBox"))
    rect = pStream->GetRectFor("BBox");

  if (IsValiableRect(rect, pPageDic->GetRectFor("MediaBox")))
    pRectArray->push_back(rect);

  pObjectArray->push_back(pStream);
}

int ParserAnnots(CPDF_Document* pSourceDoc,
                 CPDF_Dictionary* pPageDic,
                 std::vector<CFX_FloatRect>* pRectArray,
                 std::vector<CPDF_Dictionary*>* pObjectArray,
                 int nUsage) {
  if (!pSourceDoc || !pPageDic)
    return FLATTEN_FAIL;

  GetContentsRect(pSourceDoc, pPageDic, pRectArray);
  CPDF_Array* pAnnots = pPageDic->GetArrayFor("Annots");
  if (!pAnnots)
    return FLATTEN_NOTHINGTODO;

  uint32_t dwSize = pAnnots->GetCount();
  for (int i = 0; i < (int)dwSize; i++) {
    CPDF_Dictionary* pAnnotDic = ToDictionary(pAnnots->GetDirectObjectAt(i));
    if (!pAnnotDic)
      continue;

    CFX_ByteString sSubtype = pAnnotDic->GetStringFor("Subtype");
    if (sSubtype == "Popup")
      continue;

    int nAnnotFlag = pAnnotDic->GetIntegerFor("F");
    if (nAnnotFlag & ANNOTFLAG_HIDDEN)
      continue;

    if (nUsage == FLAT_NORMALDISPLAY) {
      if (nAnnotFlag & ANNOTFLAG_INVISIBLE)
        continue;

      ParserStream(pPageDic, pAnnotDic, pRectArray, pObjectArray);
    } else {
      if (nAnnotFlag & ANNOTFLAG_PRINT)
        ParserStream(pPageDic, pAnnotDic, pRectArray, pObjectArray);
    }
  }
  return FLATTEN_SUCCESS;
}

FX_FLOAT GetMinMaxValue(const std::vector<CFX_FloatRect>& array,
                        FPDF_TYPE type,
                        FPDF_VALUE value) {
  size_t nRects = array.size();
  if (nRects <= 0)
    return 0.0f;

  std::vector<FX_FLOAT> pArray(nRects);
  switch (value) {
    case LEFT:
      for (size_t i = 0; i < nRects; i++)
        pArray[i] = array[i].left;
      break;
    case TOP:
      for (size_t i = 0; i < nRects; i++)
        pArray[i] = array[i].top;
      break;
    case RIGHT:
      for (size_t i = 0; i < nRects; i++)
        pArray[i] = array[i].right;
      break;
    case BOTTOM:
      for (size_t i = 0; i < nRects; i++)
        pArray[i] = array[i].bottom;
      break;
    default:
      // Not reachable.
      return 0.0f;
  }

  FX_FLOAT fRet = pArray[0];
  if (type == MAX) {
    for (size_t i = 1; i < nRects; i++)
      fRet = std::max(fRet, pArray[i]);
  } else {
    for (size_t i = 1; i < nRects; i++)
      fRet = std::min(fRet, pArray[i]);
  }
  return fRet;
}

CFX_FloatRect CalculateRect(std::vector<CFX_FloatRect>* pRectArray) {
  CFX_FloatRect rcRet;

  rcRet.left = GetMinMaxValue(*pRectArray, MIN, LEFT);
  rcRet.top = GetMinMaxValue(*pRectArray, MAX, TOP);
  rcRet.right = GetMinMaxValue(*pRectArray, MAX, RIGHT);
  rcRet.bottom = GetMinMaxValue(*pRectArray, MIN, BOTTOM);

  return rcRet;
}

uint32_t NewIndirectContentsStream(const CFX_ByteString& key,
                                   CPDF_Document* pDocument) {
  CPDF_Stream* pNewContents = pDocument->NewIndirect<CPDF_Stream>(
      nullptr, 0,
      pdfium::MakeUnique<CPDF_Dictionary>(pDocument->GetByteStringPool()));
  CFX_ByteString sStream;
  sStream.Format("q 1 0 0 1 0 0 cm /%s Do Q", key.c_str());
  pNewContents->SetData(sStream.raw_str(), sStream.GetLength());
  return pNewContents->GetObjNum();
}

void SetPageContents(const CFX_ByteString& key,
                     CPDF_Dictionary* pPage,
                     CPDF_Document* pDocument) {
  CPDF_Array* pContentsArray = nullptr;
  CPDF_Stream* pContentsStream = pPage->GetStreamFor("Contents");
  if (!pContentsStream) {
    pContentsArray = pPage->GetArrayFor("Contents");
    if (!pContentsArray) {
      if (!key.IsEmpty()) {
        pPage->SetNewFor<CPDF_Reference>(
            "Contents", pDocument, NewIndirectContentsStream(key, pDocument));
      }
      return;
    }
  }
  pPage->ConvertToIndirectObjectFor("Contents", pDocument);
  if (!pContentsArray) {
    pContentsArray = pDocument->NewIndirect<CPDF_Array>();
    CPDF_StreamAcc acc;
    acc.LoadAllData(pContentsStream);
    CFX_ByteString sStream = "q\n";
    CFX_ByteString sBody =
        CFX_ByteString((const FX_CHAR*)acc.GetData(), acc.GetSize());
    sStream = sStream + sBody + "\nQ";
    pContentsStream->SetData(sStream.raw_str(), sStream.GetLength());
    pContentsArray->AddNew<CPDF_Reference>(pDocument,
                                           pContentsStream->GetObjNum());
    pPage->SetNewFor<CPDF_Reference>("Contents", pDocument,
                                     pContentsArray->GetObjNum());
  }
  if (!key.IsEmpty()) {
    pContentsArray->AddNew<CPDF_Reference>(
        pDocument, NewIndirectContentsStream(key, pDocument));
  }
}

CFX_Matrix GetMatrix(CFX_FloatRect rcAnnot,
                     CFX_FloatRect rcStream,
                     const CFX_Matrix& matrix) {
  if (rcStream.IsEmpty())
    return CFX_Matrix();

  matrix.TransformRect(rcStream);
  rcStream.Normalize();

  FX_FLOAT a = rcAnnot.Width() / rcStream.Width();
  FX_FLOAT d = rcAnnot.Height() / rcStream.Height();

  FX_FLOAT e = rcAnnot.left - rcStream.left * a;
  FX_FLOAT f = rcAnnot.bottom - rcStream.bottom * d;
  return CFX_Matrix(a, 0, 0, d, e, f);
}

}  // namespace

DLLEXPORT int STDCALL FPDFPage_Flatten(FPDF_PAGE page, int nFlag) {
  CPDF_Page* pPage = CPDFPageFromFPDFPage(page);
  if (!page)
    return FLATTEN_FAIL;

  CPDF_Document* pDocument = pPage->m_pDocument;
  CPDF_Dictionary* pPageDict = pPage->m_pFormDict;
  if (!pDocument || !pPageDict)
    return FLATTEN_FAIL;

  std::vector<CPDF_Dictionary*> ObjectArray;
  std::vector<CFX_FloatRect> RectArray;
  int iRet =
      ParserAnnots(pDocument, pPageDict, &RectArray, &ObjectArray, nFlag);
  if (iRet == FLATTEN_NOTHINGTODO || iRet == FLATTEN_FAIL)
    return iRet;

  CFX_FloatRect rcOriginalCB;
  CFX_FloatRect rcMerger = CalculateRect(&RectArray);
  CFX_FloatRect rcOriginalMB = pPageDict->GetRectFor("MediaBox");
  if (pPageDict->KeyExist("CropBox"))
    rcOriginalMB = pPageDict->GetRectFor("CropBox");

  if (rcOriginalMB.IsEmpty())
    rcOriginalMB = CFX_FloatRect(0.0f, 0.0f, 612.0f, 792.0f);

  rcMerger.left = std::max(rcMerger.left, rcOriginalMB.left);
  rcMerger.right = std::min(rcMerger.right, rcOriginalMB.right);
  rcMerger.bottom = std::max(rcMerger.bottom, rcOriginalMB.bottom);
  rcMerger.top = std::min(rcMerger.top, rcOriginalMB.top);
  if (pPageDict->KeyExist("ArtBox"))
    rcOriginalCB = pPageDict->GetRectFor("ArtBox");
  else
    rcOriginalCB = rcOriginalMB;

  if (!rcOriginalMB.IsEmpty()) {
    CPDF_Array* pMediaBox = pPageDict->SetNewFor<CPDF_Array>("MediaBox");
    pMediaBox->AddNew<CPDF_Number>(rcOriginalMB.left);
    pMediaBox->AddNew<CPDF_Number>(rcOriginalMB.bottom);
    pMediaBox->AddNew<CPDF_Number>(rcOriginalMB.right);
    pMediaBox->AddNew<CPDF_Number>(rcOriginalMB.top);
  }

  if (!rcOriginalCB.IsEmpty()) {
    CPDF_Array* pCropBox = pPageDict->SetNewFor<CPDF_Array>("ArtBox");
    pCropBox->AddNew<CPDF_Number>(rcOriginalCB.left);
    pCropBox->AddNew<CPDF_Number>(rcOriginalCB.bottom);
    pCropBox->AddNew<CPDF_Number>(rcOriginalCB.right);
    pCropBox->AddNew<CPDF_Number>(rcOriginalCB.top);
  }

  CPDF_Dictionary* pRes = pPageDict->GetDictFor("Resources");
  if (!pRes)
    pRes = pPageDict->SetNewFor<CPDF_Dictionary>("Resources");

  CPDF_Stream* pNewXObject = pDocument->NewIndirect<CPDF_Stream>(
      nullptr, 0,
      pdfium::MakeUnique<CPDF_Dictionary>(pDocument->GetByteStringPool()));

  uint32_t dwObjNum = pNewXObject->GetObjNum();
  CPDF_Dictionary* pPageXObject = pRes->GetDictFor("XObject");
  if (!pPageXObject)
    pPageXObject = pRes->SetNewFor<CPDF_Dictionary>("XObject");

  CFX_ByteString key = "";
  int nStreams = pdfium::CollectionSize<int>(ObjectArray);
  if (nStreams > 0) {
    for (int iKey = 0; /*iKey < 100*/; iKey++) {
      char sExtend[5] = {};
      FXSYS_itoa(iKey, sExtend, 10);
      key = CFX_ByteString("FFT") + CFX_ByteString(sExtend);
      if (!pPageXObject->KeyExist(key))
        break;
    }
  }

  SetPageContents(key, pPageDict, pDocument);

  CPDF_Dictionary* pNewXORes = nullptr;
  if (!key.IsEmpty()) {
    pPageXObject->SetNewFor<CPDF_Reference>(key, pDocument, dwObjNum);
    CPDF_Dictionary* pNewOXbjectDic = pNewXObject->GetDict();
    pNewXORes = pNewOXbjectDic->SetNewFor<CPDF_Dictionary>("Resources");
    pNewOXbjectDic->SetNewFor<CPDF_Name>("Type", "XObject");
    pNewOXbjectDic->SetNewFor<CPDF_Name>("Subtype", "Form");
    pNewOXbjectDic->SetNewFor<CPDF_Number>("FormType", 1);
    pNewOXbjectDic->SetNewFor<CPDF_Name>("Name", "FRM");
    CFX_FloatRect rcBBox = pPageDict->GetRectFor("ArtBox");
    pNewOXbjectDic->SetRectFor("BBox", rcBBox);
  }

  for (int i = 0; i < nStreams; i++) {
    CPDF_Dictionary* pAnnotDic = ObjectArray[i];
    if (!pAnnotDic)
      continue;

    CFX_FloatRect rcAnnot = pAnnotDic->GetRectFor("Rect");
    rcAnnot.Normalize();

    CFX_ByteString sAnnotState = pAnnotDic->GetStringFor("AS");
    CPDF_Dictionary* pAnnotAP = pAnnotDic->GetDictFor("AP");
    if (!pAnnotAP)
      continue;

    CPDF_Stream* pAPStream = pAnnotAP->GetStreamFor("N");
    if (!pAPStream) {
      CPDF_Dictionary* pAPDic = pAnnotAP->GetDictFor("N");
      if (!pAPDic)
        continue;

      if (!sAnnotState.IsEmpty()) {
        pAPStream = pAPDic->GetStreamFor(sAnnotState);
      } else {
        auto it = pAPDic->begin();
        if (it != pAPDic->end()) {
          CPDF_Object* pFirstObj = it->second.get();
          if (pFirstObj) {
            if (pFirstObj->IsReference())
              pFirstObj = pFirstObj->GetDirect();
            if (!pFirstObj->IsStream())
              continue;
            pAPStream = pFirstObj->AsStream();
          }
        }
      }
    }
    if (!pAPStream)
      continue;

    CPDF_Dictionary* pAPDic = pAPStream->GetDict();
    CFX_FloatRect rcStream;
    if (pAPDic->KeyExist("Rect"))
      rcStream = pAPDic->GetRectFor("Rect");
    else if (pAPDic->KeyExist("BBox"))
      rcStream = pAPDic->GetRectFor("BBox");

    if (rcStream.IsEmpty())
      continue;

    CPDF_Object* pObj = pAPStream;
    if (pObj->IsInline()) {
      std::unique_ptr<CPDF_Object> pNew = pObj->Clone();
      pObj = pNew.get();
      pDocument->AddIndirectObject(std::move(pNew));
    }

    CPDF_Dictionary* pObjDic = pObj->GetDict();
    if (pObjDic) {
      pObjDic->SetNewFor<CPDF_Name>("Type", "XObject");
      pObjDic->SetNewFor<CPDF_Name>("Subtype", "Form");
    }

    CPDF_Dictionary* pXObject = pNewXORes->GetDictFor("XObject");
    if (!pXObject)
      pXObject = pNewXORes->SetNewFor<CPDF_Dictionary>("XObject");

    CFX_ByteString sFormName;
    sFormName.Format("F%d", i);
    pXObject->SetNewFor<CPDF_Reference>(sFormName, pDocument,
                                        pObj->GetObjNum());

    CPDF_StreamAcc acc;
    acc.LoadAllData(pNewXObject);

    const uint8_t* pData = acc.GetData();
    CFX_ByteString sStream(pData, acc.GetSize());
    CFX_Matrix matrix = pAPDic->GetMatrixFor("Matrix");
    if (matrix.IsIdentity()) {
      matrix.a = 1.0f;
      matrix.b = 0.0f;
      matrix.c = 0.0f;
      matrix.d = 1.0f;
      matrix.e = 0.0f;
      matrix.f = 0.0f;
    }

    CFX_ByteString sTemp;
    CFX_Matrix m = GetMatrix(rcAnnot, rcStream, matrix);
    sTemp.Format("q %f 0 0 %f %f %f cm /%s Do Q\n", m.a, m.d, m.e, m.f,
                 sFormName.c_str());
    sStream += sTemp;
    pNewXObject->SetData(sStream.raw_str(), sStream.GetLength());
  }
  pPageDict->RemoveFor("Annots");
  return FLATTEN_SUCCESS;
}
