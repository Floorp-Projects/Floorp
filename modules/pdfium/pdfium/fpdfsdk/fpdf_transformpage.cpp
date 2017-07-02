// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "public/fpdf_transformpage.h"

#include <vector>

#include "core/fpdfapi/page/cpdf_clippath.h"
#include "core/fpdfapi/page/cpdf_page.h"
#include "core/fpdfapi/page/cpdf_pageobject.h"
#include "core/fpdfapi/page/cpdf_path.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/parser/cpdf_number.h"
#include "core/fpdfapi/parser/cpdf_reference.h"
#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fxge/cfx_pathdata.h"
#include "fpdfsdk/fsdk_define.h"

namespace {

void SetBoundingBox(CPDF_Page* page,
                    const CFX_ByteString& key,
                    float left,
                    float bottom,
                    float right,
                    float top) {
  CPDF_Array* pBoundingBoxArray = page->m_pFormDict->SetNewFor<CPDF_Array>(key);
  pBoundingBoxArray->AddNew<CPDF_Number>(left);
  pBoundingBoxArray->AddNew<CPDF_Number>(bottom);
  pBoundingBoxArray->AddNew<CPDF_Number>(right);
  pBoundingBoxArray->AddNew<CPDF_Number>(top);
}

bool GetBoundingBox(CPDF_Page* page,
                    const CFX_ByteString& key,
                    float* left,
                    float* bottom,
                    float* right,
                    float* top) {
  CPDF_Array* pArray = page->m_pFormDict->GetArrayFor(key);
  if (!pArray)
    return false;

  *left = pArray->GetFloatAt(0);
  *bottom = pArray->GetFloatAt(1);
  *right = pArray->GetFloatAt(2);
  *top = pArray->GetFloatAt(3);
  return true;
}

}  // namespace

DLLEXPORT void STDCALL FPDFPage_SetMediaBox(FPDF_PAGE page,
                                            float left,
                                            float bottom,
                                            float right,
                                            float top) {
  CPDF_Page* pPage = CPDFPageFromFPDFPage(page);
  if (!pPage)
    return;

  SetBoundingBox(pPage, "MediaBox", left, bottom, right, top);
}

DLLEXPORT void STDCALL FPDFPage_SetCropBox(FPDF_PAGE page,
                                           float left,
                                           float bottom,
                                           float right,
                                           float top) {
  CPDF_Page* pPage = CPDFPageFromFPDFPage(page);
  if (!pPage)
    return;

  SetBoundingBox(pPage, "CropBox", left, bottom, right, top);
}

DLLEXPORT FPDF_BOOL STDCALL FPDFPage_GetMediaBox(FPDF_PAGE page,
                                                 float* left,
                                                 float* bottom,
                                                 float* right,
                                                 float* top) {
  CPDF_Page* pPage = CPDFPageFromFPDFPage(page);
  return pPage && GetBoundingBox(pPage, "MediaBox", left, bottom, right, top);
}

DLLEXPORT FPDF_BOOL STDCALL FPDFPage_GetCropBox(FPDF_PAGE page,
                                                float* left,
                                                float* bottom,
                                                float* right,
                                                float* top) {
  CPDF_Page* pPage = CPDFPageFromFPDFPage(page);
  return pPage && GetBoundingBox(pPage, "CropBox", left, bottom, right, top);
}

DLLEXPORT FPDF_BOOL STDCALL FPDFPage_TransFormWithClip(FPDF_PAGE page,
                                                       FS_MATRIX* matrix,
                                                       FS_RECTF* clipRect) {
  CPDF_Page* pPage = CPDFPageFromFPDFPage(page);
  if (!pPage)
    return false;

  CFX_ByteTextBuf textBuf;
  textBuf << "q ";
  CFX_FloatRect rect(clipRect->left, clipRect->bottom, clipRect->right,
                     clipRect->top);
  rect.Normalize();
  CFX_ByteString bsClipping;
  bsClipping.Format("%f %f %f %f re W* n ", rect.left, rect.bottom,
                    rect.Width(), rect.Height());
  textBuf << bsClipping;

  CFX_ByteString bsMatix;
  bsMatix.Format("%f %f %f %f %f %f cm ", matrix->a, matrix->b, matrix->c,
                 matrix->d, matrix->e, matrix->f);
  textBuf << bsMatix;

  CPDF_Dictionary* pPageDic = pPage->m_pFormDict;
  CPDF_Object* pContentObj =
      pPageDic ? pPageDic->GetObjectFor("Contents") : nullptr;
  if (!pContentObj)
    pContentObj = pPageDic ? pPageDic->GetArrayFor("Contents") : nullptr;
  if (!pContentObj)
    return false;

  CPDF_Document* pDoc = pPage->m_pDocument;
  if (!pDoc)
    return false;

  CPDF_Stream* pStream = pDoc->NewIndirect<CPDF_Stream>(
      nullptr, 0,
      pdfium::MakeUnique<CPDF_Dictionary>(pDoc->GetByteStringPool()));
  pStream->SetData(textBuf.GetBuffer(), textBuf.GetSize());

  CPDF_Stream* pEndStream = pDoc->NewIndirect<CPDF_Stream>(
      nullptr, 0,
      pdfium::MakeUnique<CPDF_Dictionary>(pDoc->GetByteStringPool()));
  pEndStream->SetData((const uint8_t*)" Q", 2);

  CPDF_Array* pContentArray = nullptr;
  CPDF_Array* pArray = ToArray(pContentObj);
  if (pArray) {
    pContentArray = pArray;
    pContentArray->InsertNewAt<CPDF_Reference>(0, pDoc, pStream->GetObjNum());
    pContentArray->AddNew<CPDF_Reference>(pDoc, pEndStream->GetObjNum());
  } else if (CPDF_Reference* pReference = ToReference(pContentObj)) {
    CPDF_Object* pDirectObj = pReference->GetDirect();
    if (pDirectObj) {
      CPDF_Array* pObjArray = pDirectObj->AsArray();
      if (pObjArray) {
        pContentArray = pObjArray;
        pContentArray->InsertNewAt<CPDF_Reference>(0, pDoc,
                                                   pStream->GetObjNum());
        pContentArray->AddNew<CPDF_Reference>(pDoc, pEndStream->GetObjNum());
      } else if (pDirectObj->IsStream()) {
        pContentArray = pDoc->NewIndirect<CPDF_Array>();
        pContentArray->AddNew<CPDF_Reference>(pDoc, pStream->GetObjNum());
        pContentArray->AddNew<CPDF_Reference>(pDoc, pDirectObj->GetObjNum());
        pContentArray->AddNew<CPDF_Reference>(pDoc, pEndStream->GetObjNum());
        pPageDic->SetNewFor<CPDF_Reference>("Contents", pDoc,
                                            pContentArray->GetObjNum());
      }
    }
  }

  // Need to transform the patterns as well.
  CPDF_Dictionary* pRes = pPageDic->GetDictFor("Resources");
  if (pRes) {
    CPDF_Dictionary* pPattenDict = pRes->GetDictFor("Pattern");
    if (pPattenDict) {
      for (const auto& it : *pPattenDict) {
        CPDF_Object* pObj = it.second.get();
        if (pObj->IsReference())
          pObj = pObj->GetDirect();

        CPDF_Dictionary* pDict = nullptr;
        if (pObj->IsDictionary())
          pDict = pObj->AsDictionary();
        else if (CPDF_Stream* pObjStream = pObj->AsStream())
          pDict = pObjStream->GetDict();
        else
          continue;

        CFX_Matrix m = pDict->GetMatrixFor("Matrix");
        CFX_Matrix t = *(CFX_Matrix*)matrix;
        m.Concat(t);
        pDict->SetMatrixFor("Matrix", m);
      }
    }
  }

  return true;
}

DLLEXPORT void STDCALL
FPDFPageObj_TransformClipPath(FPDF_PAGEOBJECT page_object,
                              double a,
                              double b,
                              double c,
                              double d,
                              double e,
                              double f) {
  CPDF_PageObject* pPageObj = (CPDF_PageObject*)page_object;
  if (!pPageObj)
    return;
  CFX_Matrix matrix((FX_FLOAT)a, (FX_FLOAT)b, (FX_FLOAT)c, (FX_FLOAT)d,
                    (FX_FLOAT)e, (FX_FLOAT)f);

  // Special treatment to shading object, because the ClipPath for shading
  // object is already transformed.
  if (!pPageObj->IsShading())
    pPageObj->TransformClipPath(matrix);
  pPageObj->TransformGeneralState(matrix);
}

DLLEXPORT FPDF_CLIPPATH STDCALL FPDF_CreateClipPath(float left,
                                                    float bottom,
                                                    float right,
                                                    float top) {
  CPDF_Path Path;
  Path.AppendRect(left, bottom, right, top);

  CPDF_ClipPath* pNewClipPath = new CPDF_ClipPath();
  pNewClipPath->AppendPath(Path, FXFILL_ALTERNATE, false);
  return pNewClipPath;
}

DLLEXPORT void STDCALL FPDF_DestroyClipPath(FPDF_CLIPPATH clipPath) {
  delete (CPDF_ClipPath*)clipPath;
}

void OutputPath(CFX_ByteTextBuf& buf, CPDF_Path path) {
  const CFX_PathData* pPathData = path.GetObject();
  if (!pPathData)
    return;

  const std::vector<FX_PATHPOINT>& pPoints = pPathData->GetPoints();
  if (path.IsRect()) {
    CFX_PointF diff = pPoints[2].m_Point - pPoints[0].m_Point;
    buf << pPoints[0].m_Point.x << " " << pPoints[0].m_Point.y << " " << diff.x
        << " " << diff.y << " re\n";
    return;
  }

  CFX_ByteString temp;
  for (size_t i = 0; i < pPoints.size(); i++) {
    buf << pPoints[i].m_Point.x << " " << pPoints[i].m_Point.y;
    FXPT_TYPE point_type = pPoints[i].m_Type;
    if (point_type == FXPT_TYPE::MoveTo) {
      buf << " m\n";
    } else if (point_type == FXPT_TYPE::BezierTo) {
      buf << " " << pPoints[i + 1].m_Point.x << " " << pPoints[i + 1].m_Point.y
          << " " << pPoints[i + 2].m_Point.x << " " << pPoints[i + 2].m_Point.y;
      buf << " c";
      if (pPoints[i + 2].m_CloseFigure)
        buf << " h";
      buf << "\n";

      i += 2;
    } else if (point_type == FXPT_TYPE::LineTo) {
      buf << " l";
      if (pPoints[i].m_CloseFigure)
        buf << " h";
      buf << "\n";
    }
  }
}

DLLEXPORT void STDCALL FPDFPage_InsertClipPath(FPDF_PAGE page,
                                               FPDF_CLIPPATH clipPath) {
  CPDF_Page* pPage = CPDFPageFromFPDFPage(page);
  if (!pPage)
    return;

  CPDF_Dictionary* pPageDic = pPage->m_pFormDict;
  CPDF_Object* pContentObj =
      pPageDic ? pPageDic->GetObjectFor("Contents") : nullptr;
  if (!pContentObj)
    pContentObj = pPageDic ? pPageDic->GetArrayFor("Contents") : nullptr;
  if (!pContentObj)
    return;

  CFX_ByteTextBuf strClip;
  CPDF_ClipPath* pClipPath = (CPDF_ClipPath*)clipPath;
  uint32_t i;
  for (i = 0; i < pClipPath->GetPathCount(); i++) {
    CPDF_Path path = pClipPath->GetPath(i);
    int iClipType = pClipPath->GetClipType(i);
    if (path.GetPoints().empty()) {
      // Empty clipping (totally clipped out)
      strClip << "0 0 m W n ";
    } else {
      OutputPath(strClip, path);
      if (iClipType == FXFILL_WINDING)
        strClip << "W n\n";
      else
        strClip << "W* n\n";
    }
  }
  CPDF_Document* pDoc = pPage->m_pDocument;
  if (!pDoc)
    return;

  CPDF_Stream* pStream = pDoc->NewIndirect<CPDF_Stream>(
      nullptr, 0,
      pdfium::MakeUnique<CPDF_Dictionary>(pDoc->GetByteStringPool()));
  pStream->SetData(strClip.GetBuffer(), strClip.GetSize());

  CPDF_Array* pArray = ToArray(pContentObj);
  if (pArray) {
    pArray->InsertNewAt<CPDF_Reference>(0, pDoc, pStream->GetObjNum());
    return;
  }
  CPDF_Reference* pReference = ToReference(pContentObj);
  if (!pReference)
    return;

  CPDF_Object* pDirectObj = pReference->GetDirect();
  if (!pDirectObj)
    return;

  CPDF_Array* pObjArray = pDirectObj->AsArray();
  if (pObjArray) {
    pObjArray->InsertNewAt<CPDF_Reference>(0, pDoc, pStream->GetObjNum());
    return;
  }
  if (pDirectObj->IsStream()) {
    CPDF_Array* pContentArray = pDoc->NewIndirect<CPDF_Array>();
    pContentArray->AddNew<CPDF_Reference>(pDoc, pStream->GetObjNum());
    pContentArray->AddNew<CPDF_Reference>(pDoc, pDirectObj->GetObjNum());
    pPageDic->SetNewFor<CPDF_Reference>("Contents", pDoc,
                                        pContentArray->GetObjNum());
  }
}
