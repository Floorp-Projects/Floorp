// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/fpdf_edit.h"

#include "core/fpdfapi/page/cpdf_path.h"
#include "core/fpdfapi/page/cpdf_pathobject.h"
#include "core/fxcrt/fx_system.h"

DLLEXPORT FPDF_PAGEOBJECT STDCALL FPDFPageObj_CreateNewPath(float x, float y) {
  CPDF_PathObject* pPathObj = new CPDF_PathObject;
  pPathObj->m_Path.AppendPoint(CFX_PointF(x, y), FXPT_TYPE::MoveTo, false);
  pPathObj->DefaultStates();
  return pPathObj;
}

DLLEXPORT FPDF_PAGEOBJECT STDCALL FPDFPageObj_CreateNewRect(float x,
                                                            float y,
                                                            float w,
                                                            float h) {
  CPDF_PathObject* pPathObj = new CPDF_PathObject;
  pPathObj->m_Path.AppendRect(x, y, x + w, y + h);
  pPathObj->DefaultStates();
  return pPathObj;
}

DLLEXPORT FPDF_BOOL FPDFPath_SetStrokeColor(FPDF_PAGEOBJECT path,
                                            unsigned int R,
                                            unsigned int G,
                                            unsigned int B,
                                            unsigned int A) {
  if (!path || R > 255 || G > 255 || B > 255 || A > 255)
    return false;

  auto pPathObj = reinterpret_cast<CPDF_PathObject*>(path);
  pPathObj->m_GeneralState.SetStrokeAlpha(A / 255.f);
  FX_FLOAT rgb[3] = {R / 255.f, G / 255.f, B / 255.f};
  pPathObj->m_ColorState.SetStrokeColor(
      CPDF_ColorSpace::GetStockCS(PDFCS_DEVICERGB), rgb, 3);
  return true;
}

DLLEXPORT FPDF_BOOL FPDFPath_SetStrokeWidth(FPDF_PAGEOBJECT path, float width) {
  if (!path || width < 0.0f)
    return false;

  auto pPathObj = reinterpret_cast<CPDF_PathObject*>(path);
  pPathObj->m_GraphState.SetLineWidth(width);
  return true;
}

DLLEXPORT FPDF_BOOL FPDFPath_SetFillColor(FPDF_PAGEOBJECT path,
                                          unsigned int R,
                                          unsigned int G,
                                          unsigned int B,
                                          unsigned int A) {
  if (!path || R > 255 || G > 255 || B > 255 || A > 255)
    return false;

  auto pPathObj = reinterpret_cast<CPDF_PathObject*>(path);
  pPathObj->m_GeneralState.SetFillAlpha(A / 255.f);
  FX_FLOAT rgb[3] = {R / 255.f, G / 255.f, B / 255.f};
  pPathObj->m_ColorState.SetFillColor(
      CPDF_ColorSpace::GetStockCS(PDFCS_DEVICERGB), rgb, 3);
  return true;
}

DLLEXPORT FPDF_BOOL FPDFPath_MoveTo(FPDF_PAGEOBJECT path, float x, float y) {
  if (!path)
    return false;

  auto pPathObj = reinterpret_cast<CPDF_PathObject*>(path);
  pPathObj->m_Path.AppendPoint(CFX_PointF(x, y), FXPT_TYPE::MoveTo, false);
  return true;
}

DLLEXPORT FPDF_BOOL FPDFPath_LineTo(FPDF_PAGEOBJECT path, float x, float y) {
  if (!path)
    return false;

  auto pPathObj = reinterpret_cast<CPDF_PathObject*>(path);
  pPathObj->m_Path.AppendPoint(CFX_PointF(x, y), FXPT_TYPE::LineTo, false);
  return true;
}

DLLEXPORT FPDF_BOOL FPDFPath_BezierTo(FPDF_PAGEOBJECT path,
                                      float x1,
                                      float y1,
                                      float x2,
                                      float y2,
                                      float x3,
                                      float y3) {
  if (!path)
    return false;

  auto pPathObj = reinterpret_cast<CPDF_PathObject*>(path);
  pPathObj->m_Path.AppendPoint(CFX_PointF(x1, y1), FXPT_TYPE::BezierTo, false);
  pPathObj->m_Path.AppendPoint(CFX_PointF(x2, y2), FXPT_TYPE::BezierTo, false);
  pPathObj->m_Path.AppendPoint(CFX_PointF(x3, y3), FXPT_TYPE::BezierTo, false);
  return true;
}

DLLEXPORT FPDF_BOOL FPDFPath_Close(FPDF_PAGEOBJECT path) {
  if (!path)
    return false;

  auto pPathObj = reinterpret_cast<CPDF_PathObject*>(path);
  if (pPathObj->m_Path.GetPoints().empty())
    return false;

  pPathObj->m_Path.ClosePath();
  return true;
}

DLLEXPORT FPDF_BOOL FPDFPath_SetDrawMode(FPDF_PAGEOBJECT path,
                                         int fillmode,
                                         FPDF_BOOL stroke) {
  if (!path)
    return false;

  auto pPathObj = reinterpret_cast<CPDF_PathObject*>(path);

  if (fillmode == FPDF_FILLMODE_ALTERNATE)
    pPathObj->m_FillType = FXFILL_ALTERNATE;
  else if (fillmode == FPDF_FILLMODE_WINDING)
    pPathObj->m_FillType = FXFILL_WINDING;
  else
    pPathObj->m_FillType = 0;
  pPathObj->m_bStroke = stroke != 0;
  return true;
}
