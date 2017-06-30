// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/render/cpdf_renderstatus.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "core/fpdfapi/font/cpdf_font.h"
#include "core/fpdfapi/font/cpdf_type3char.h"
#include "core/fpdfapi/font/cpdf_type3font.h"
#include "core/fpdfapi/page/cpdf_docpagedata.h"
#include "core/fpdfapi/page/cpdf_form.h"
#include "core/fpdfapi/page/cpdf_formobject.h"
#include "core/fpdfapi/page/cpdf_graphicstates.h"
#include "core/fpdfapi/page/cpdf_image.h"
#include "core/fpdfapi/page/cpdf_imageobject.h"
#include "core/fpdfapi/page/cpdf_meshstream.h"
#include "core/fpdfapi/page/cpdf_page.h"
#include "core/fpdfapi/page/cpdf_pageobject.h"
#include "core/fpdfapi/page/cpdf_pathobject.h"
#include "core/fpdfapi/page/cpdf_shadingobject.h"
#include "core/fpdfapi/page/cpdf_shadingpattern.h"
#include "core/fpdfapi/page/cpdf_textobject.h"
#include "core/fpdfapi/page/cpdf_tilingpattern.h"
#include "core/fpdfapi/page/pageint.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/render/cpdf_charposlist.h"
#include "core/fpdfapi/render/cpdf_devicebuffer.h"
#include "core/fpdfapi/render/cpdf_dibsource.h"
#include "core/fpdfapi/render/cpdf_docrenderdata.h"
#include "core/fpdfapi/render/cpdf_imagerenderer.h"
#include "core/fpdfapi/render/cpdf_pagerendercache.h"
#include "core/fpdfapi/render/cpdf_rendercontext.h"
#include "core/fpdfapi/render/cpdf_renderoptions.h"
#include "core/fpdfapi/render/cpdf_scaledrenderbuffer.h"
#include "core/fpdfapi/render/cpdf_textrenderer.h"
#include "core/fpdfapi/render/cpdf_transferfunc.h"
#include "core/fpdfapi/render/cpdf_type3cache.h"
#include "core/fpdfdoc/cpdf_occontext.h"
#include "core/fxcodec/fx_codec.h"
#include "core/fxcrt/cfx_maybe_owned.h"
#include "core/fxcrt/fx_safe_types.h"
#include "core/fxge/cfx_fxgedevice.h"
#include "core/fxge/cfx_graphstatedata.h"
#include "core/fxge/cfx_pathdata.h"
#include "core/fxge/cfx_renderdevice.h"
#include "core/fxge/ifx_renderdevicedriver.h"
#include "third_party/base/numerics/safe_math.h"
#include "third_party/base/ptr_util.h"
#include "third_party/base/stl_util.h"

#ifdef _SKIA_SUPPORT_
#include "core/fxge/skia/fx_skia_device.h"
#endif

#define SHADING_STEPS 256

namespace {

void ReleaseCachedType3(CPDF_Type3Font* pFont) {
  if (!pFont->m_pDocument)
    return;

  pFont->m_pDocument->GetRenderData()->ReleaseCachedType3(pFont);
  pFont->m_pDocument->GetPageData()->ReleaseFont(pFont->GetFontDict());
}

class CPDF_RefType3Cache {
 public:
  explicit CPDF_RefType3Cache(CPDF_Type3Font* pType3Font)
      : m_dwCount(0), m_pType3Font(pType3Font) {}

  ~CPDF_RefType3Cache() {
    while (m_dwCount--)
      ReleaseCachedType3(m_pType3Font);
  }

  uint32_t m_dwCount;
  CPDF_Type3Font* const m_pType3Font;
};

uint32_t CountOutputs(
    const std::vector<std::unique_ptr<CPDF_Function>>& funcs) {
  uint32_t total = 0;
  for (const auto& func : funcs) {
    if (func)
      total += func->CountOutputs();
  }
  return total;
}

void DrawAxialShading(CFX_DIBitmap* pBitmap,
                      CFX_Matrix* pObject2Bitmap,
                      CPDF_Dictionary* pDict,
                      const std::vector<std::unique_ptr<CPDF_Function>>& funcs,
                      CPDF_ColorSpace* pCS,
                      int alpha) {
  ASSERT(pBitmap->GetFormat() == FXDIB_Argb);
  CPDF_Array* pCoords = pDict->GetArrayFor("Coords");
  if (!pCoords)
    return;

  FX_FLOAT start_x = pCoords->GetNumberAt(0);
  FX_FLOAT start_y = pCoords->GetNumberAt(1);
  FX_FLOAT end_x = pCoords->GetNumberAt(2);
  FX_FLOAT end_y = pCoords->GetNumberAt(3);
  FX_FLOAT t_min = 0;
  FX_FLOAT t_max = 1.0f;
  CPDF_Array* pArray = pDict->GetArrayFor("Domain");
  if (pArray) {
    t_min = pArray->GetNumberAt(0);
    t_max = pArray->GetNumberAt(1);
  }
  bool bStartExtend = false;
  bool bEndExtend = false;
  pArray = pDict->GetArrayFor("Extend");
  if (pArray) {
    bStartExtend = !!pArray->GetIntegerAt(0);
    bEndExtend = !!pArray->GetIntegerAt(1);
  }
  int width = pBitmap->GetWidth();
  int height = pBitmap->GetHeight();
  FX_FLOAT x_span = end_x - start_x;
  FX_FLOAT y_span = end_y - start_y;
  FX_FLOAT axis_len_square = (x_span * x_span) + (y_span * y_span);
  CFX_Matrix matrix;
  matrix.SetReverse(*pObject2Bitmap);
  uint32_t total_results =
      std::max(CountOutputs(funcs), pCS->CountComponents());
  CFX_FixedBufGrow<FX_FLOAT, 16> result_array(total_results);
  FX_FLOAT* pResults = result_array;
  FXSYS_memset(pResults, 0, total_results * sizeof(FX_FLOAT));
  uint32_t rgb_array[SHADING_STEPS];
  for (int i = 0; i < SHADING_STEPS; i++) {
    FX_FLOAT input = (t_max - t_min) * i / SHADING_STEPS + t_min;
    int offset = 0;
    for (const auto& func : funcs) {
      if (func) {
        int nresults = 0;
        if (func->Call(&input, 1, pResults + offset, nresults))
          offset += nresults;
      }
    }
    FX_FLOAT R = 0.0f, G = 0.0f, B = 0.0f;
    pCS->GetRGB(pResults, R, G, B);
    rgb_array[i] =
        FXARGB_TODIB(FXARGB_MAKE(alpha, FXSYS_round(R * 255),
                                 FXSYS_round(G * 255), FXSYS_round(B * 255)));
  }
  int pitch = pBitmap->GetPitch();
  for (int row = 0; row < height; row++) {
    uint32_t* dib_buf = (uint32_t*)(pBitmap->GetBuffer() + row * pitch);
    for (int column = 0; column < width; column++) {
      CFX_PointF pos = matrix.Transform(CFX_PointF(
          static_cast<FX_FLOAT>(column), static_cast<FX_FLOAT>(row)));
      FX_FLOAT scale =
          (((pos.x - start_x) * x_span) + ((pos.y - start_y) * y_span)) /
          axis_len_square;
      int index = (int32_t)(scale * (SHADING_STEPS - 1));
      if (index < 0) {
        if (!bStartExtend)
          continue;

        index = 0;
      } else if (index >= SHADING_STEPS) {
        if (!bEndExtend)
          continue;

        index = SHADING_STEPS - 1;
      }
      dib_buf[column] = rgb_array[index];
    }
  }
}

void DrawRadialShading(CFX_DIBitmap* pBitmap,
                       CFX_Matrix* pObject2Bitmap,
                       CPDF_Dictionary* pDict,
                       const std::vector<std::unique_ptr<CPDF_Function>>& funcs,
                       CPDF_ColorSpace* pCS,
                       int alpha) {
  ASSERT(pBitmap->GetFormat() == FXDIB_Argb);
  CPDF_Array* pCoords = pDict->GetArrayFor("Coords");
  if (!pCoords)
    return;

  FX_FLOAT start_x = pCoords->GetNumberAt(0);
  FX_FLOAT start_y = pCoords->GetNumberAt(1);
  FX_FLOAT start_r = pCoords->GetNumberAt(2);
  FX_FLOAT end_x = pCoords->GetNumberAt(3);
  FX_FLOAT end_y = pCoords->GetNumberAt(4);
  FX_FLOAT end_r = pCoords->GetNumberAt(5);
  CFX_Matrix matrix;
  matrix.SetReverse(*pObject2Bitmap);
  FX_FLOAT t_min = 0;
  FX_FLOAT t_max = 1.0f;
  CPDF_Array* pArray = pDict->GetArrayFor("Domain");
  if (pArray) {
    t_min = pArray->GetNumberAt(0);
    t_max = pArray->GetNumberAt(1);
  }
  bool bStartExtend = false;
  bool bEndExtend = false;
  pArray = pDict->GetArrayFor("Extend");
  if (pArray) {
    bStartExtend = !!pArray->GetIntegerAt(0);
    bEndExtend = !!pArray->GetIntegerAt(1);
  }
  uint32_t total_results =
      std::max(CountOutputs(funcs), pCS->CountComponents());
  CFX_FixedBufGrow<FX_FLOAT, 16> result_array(total_results);
  FX_FLOAT* pResults = result_array;
  FXSYS_memset(pResults, 0, total_results * sizeof(FX_FLOAT));
  uint32_t rgb_array[SHADING_STEPS];
  for (int i = 0; i < SHADING_STEPS; i++) {
    FX_FLOAT input = (t_max - t_min) * i / SHADING_STEPS + t_min;
    int offset = 0;
    for (const auto& func : funcs) {
      if (func) {
        int nresults;
        if (func->Call(&input, 1, pResults + offset, nresults))
          offset += nresults;
      }
    }
    FX_FLOAT R = 0.0f, G = 0.0f, B = 0.0f;
    pCS->GetRGB(pResults, R, G, B);
    rgb_array[i] =
        FXARGB_TODIB(FXARGB_MAKE(alpha, FXSYS_round(R * 255),
                                 FXSYS_round(G * 255), FXSYS_round(B * 255)));
  }
  FX_FLOAT a = ((start_x - end_x) * (start_x - end_x)) +
               ((start_y - end_y) * (start_y - end_y)) -
               ((start_r - end_r) * (start_r - end_r));
  int width = pBitmap->GetWidth();
  int height = pBitmap->GetHeight();
  int pitch = pBitmap->GetPitch();
  bool bDecreasing = false;
  if (start_r > end_r) {
    int length = (int)FXSYS_sqrt((((start_x - end_x) * (start_x - end_x)) +
                                  ((start_y - end_y) * (start_y - end_y))));
    if (length < start_r - end_r) {
      bDecreasing = true;
    }
  }
  for (int row = 0; row < height; row++) {
    uint32_t* dib_buf = (uint32_t*)(pBitmap->GetBuffer() + row * pitch);
    for (int column = 0; column < width; column++) {
      CFX_PointF pos = matrix.Transform(CFX_PointF(
          static_cast<FX_FLOAT>(column), static_cast<FX_FLOAT>(row)));
      FX_FLOAT b = -2 * (((pos.x - start_x) * (end_x - start_x)) +
                         ((pos.y - start_y) * (end_y - start_y)) +
                         (start_r * (end_r - start_r)));
      FX_FLOAT c = ((pos.x - start_x) * (pos.x - start_x)) +
                   ((pos.y - start_y) * (pos.y - start_y)) -
                   (start_r * start_r);
      FX_FLOAT s;
      if (a == 0) {
        s = -c / b;
      } else {
        FX_FLOAT b2_4ac = (b * b) - 4 * (a * c);
        if (b2_4ac < 0) {
          continue;
        }
        FX_FLOAT root = FXSYS_sqrt(b2_4ac);
        FX_FLOAT s1, s2;
        if (a > 0) {
          s1 = (-b - root) / (2 * a);
          s2 = (-b + root) / (2 * a);
        } else {
          s2 = (-b - root) / (2 * a);
          s1 = (-b + root) / (2 * a);
        }
        if (bDecreasing) {
          if (s1 >= 0 || bStartExtend) {
            s = s1;
          } else {
            s = s2;
          }
        } else {
          if (s2 <= 1.0f || bEndExtend) {
            s = s2;
          } else {
            s = s1;
          }
        }
        if ((start_r + s * (end_r - start_r)) < 0) {
          continue;
        }
      }
      int index = (int32_t)(s * (SHADING_STEPS - 1));
      if (index < 0) {
        if (!bStartExtend) {
          continue;
        }
        index = 0;
      }
      if (index >= SHADING_STEPS) {
        if (!bEndExtend) {
          continue;
        }
        index = SHADING_STEPS - 1;
      }
      dib_buf[column] = rgb_array[index];
    }
  }
}

void DrawFuncShading(CFX_DIBitmap* pBitmap,
                     CFX_Matrix* pObject2Bitmap,
                     CPDF_Dictionary* pDict,
                     const std::vector<std::unique_ptr<CPDF_Function>>& funcs,
                     CPDF_ColorSpace* pCS,
                     int alpha) {
  ASSERT(pBitmap->GetFormat() == FXDIB_Argb);
  CPDF_Array* pDomain = pDict->GetArrayFor("Domain");
  FX_FLOAT xmin = 0, ymin = 0, xmax = 1.0f, ymax = 1.0f;
  if (pDomain) {
    xmin = pDomain->GetNumberAt(0);
    xmax = pDomain->GetNumberAt(1);
    ymin = pDomain->GetNumberAt(2);
    ymax = pDomain->GetNumberAt(3);
  }
  CFX_Matrix mtDomain2Target = pDict->GetMatrixFor("Matrix");
  CFX_Matrix matrix;
  matrix.SetReverse(*pObject2Bitmap);

  CFX_Matrix reverse_matrix;
  reverse_matrix.SetReverse(mtDomain2Target);
  matrix.Concat(reverse_matrix);
  int width = pBitmap->GetWidth();
  int height = pBitmap->GetHeight();
  int pitch = pBitmap->GetPitch();
  uint32_t total_results =
      std::max(CountOutputs(funcs), pCS->CountComponents());
  CFX_FixedBufGrow<FX_FLOAT, 16> result_array(total_results);
  FX_FLOAT* pResults = result_array;
  FXSYS_memset(pResults, 0, total_results * sizeof(FX_FLOAT));
  for (int row = 0; row < height; row++) {
    uint32_t* dib_buf = (uint32_t*)(pBitmap->GetBuffer() + row * pitch);
    for (int column = 0; column < width; column++) {
      CFX_PointF pos = matrix.Transform(CFX_PointF(
          static_cast<FX_FLOAT>(column), static_cast<FX_FLOAT>(row)));
      if (pos.x < xmin || pos.x > xmax || pos.y < ymin || pos.y > ymax)
        continue;

      FX_FLOAT input[] = {pos.x, pos.y};
      int offset = 0;
      for (const auto& func : funcs) {
        if (func) {
          int nresults;
          if (func->Call(input, 2, pResults + offset, nresults))
            offset += nresults;
        }
      }

      FX_FLOAT R = 0.0f;
      FX_FLOAT G = 0.0f;
      FX_FLOAT B = 0.0f;
      pCS->GetRGB(pResults, R, G, B);
      dib_buf[column] = FXARGB_TODIB(FXARGB_MAKE(
          alpha, (int32_t)(R * 255), (int32_t)(G * 255), (int32_t)(B * 255)));
    }
  }
}

bool GetScanlineIntersect(int y,
                          const CFX_PointF& first,
                          const CFX_PointF& second,
                          FX_FLOAT* x) {
  if (first.y == second.y)
    return false;

  if (first.y < second.y) {
    if (y < first.y || y > second.y)
      return false;
  } else if (y < second.y || y > first.y) {
    return false;
  }
  *x = first.x + ((second.x - first.x) * (y - first.y) / (second.y - first.y));
  return true;
}

void DrawGouraud(CFX_DIBitmap* pBitmap,
                 int alpha,
                 CPDF_MeshVertex triangle[3]) {
  FX_FLOAT min_y = triangle[0].position.y;
  FX_FLOAT max_y = triangle[0].position.y;
  for (int i = 1; i < 3; i++) {
    min_y = std::min(min_y, triangle[i].position.y);
    max_y = std::max(max_y, triangle[i].position.y);
  }
  if (min_y == max_y)
    return;

  int min_yi = std::max(static_cast<int>(FXSYS_floor(min_y)), 0);
  int max_yi = static_cast<int>(FXSYS_ceil(max_y));

  if (max_yi >= pBitmap->GetHeight())
    max_yi = pBitmap->GetHeight() - 1;

  for (int y = min_yi; y <= max_yi; y++) {
    int nIntersects = 0;
    FX_FLOAT inter_x[3];
    FX_FLOAT r[3];
    FX_FLOAT g[3];
    FX_FLOAT b[3];
    for (int i = 0; i < 3; i++) {
      CPDF_MeshVertex& vertex1 = triangle[i];
      CPDF_MeshVertex& vertex2 = triangle[(i + 1) % 3];
      CFX_PointF& position1 = vertex1.position;
      CFX_PointF& position2 = vertex2.position;
      bool bIntersect =
          GetScanlineIntersect(y, position1, position2, &inter_x[nIntersects]);
      if (!bIntersect)
        continue;

      FX_FLOAT y_dist = (y - position1.y) / (position2.y - position1.y);
      r[nIntersects] = vertex1.r + ((vertex2.r - vertex1.r) * y_dist);
      g[nIntersects] = vertex1.g + ((vertex2.g - vertex1.g) * y_dist);
      b[nIntersects] = vertex1.b + ((vertex2.b - vertex1.b) * y_dist);
      nIntersects++;
    }
    if (nIntersects != 2)
      continue;

    int min_x, max_x, start_index, end_index;
    if (inter_x[0] < inter_x[1]) {
      min_x = (int)FXSYS_floor(inter_x[0]);
      max_x = (int)FXSYS_ceil(inter_x[1]);
      start_index = 0;
      end_index = 1;
    } else {
      min_x = (int)FXSYS_floor(inter_x[1]);
      max_x = (int)FXSYS_ceil(inter_x[0]);
      start_index = 1;
      end_index = 0;
    }

    int start_x = std::max(min_x, 0);
    int end_x = max_x;
    if (end_x > pBitmap->GetWidth())
      end_x = pBitmap->GetWidth();

    uint8_t* dib_buf =
        pBitmap->GetBuffer() + y * pBitmap->GetPitch() + start_x * 4;
    FX_FLOAT r_unit = (r[end_index] - r[start_index]) / (max_x - min_x);
    FX_FLOAT g_unit = (g[end_index] - g[start_index]) / (max_x - min_x);
    FX_FLOAT b_unit = (b[end_index] - b[start_index]) / (max_x - min_x);
    FX_FLOAT R = r[start_index] + (start_x - min_x) * r_unit;
    FX_FLOAT G = g[start_index] + (start_x - min_x) * g_unit;
    FX_FLOAT B = b[start_index] + (start_x - min_x) * b_unit;
    for (int x = start_x; x < end_x; x++) {
      R += r_unit;
      G += g_unit;
      B += b_unit;
      FXARGB_SETDIB(dib_buf,
                    FXARGB_MAKE(alpha, (int32_t)(R * 255), (int32_t)(G * 255),
                                (int32_t)(B * 255)));
      dib_buf += 4;
    }
  }
}

void DrawFreeGouraudShading(
    CFX_DIBitmap* pBitmap,
    CFX_Matrix* pObject2Bitmap,
    CPDF_Stream* pShadingStream,
    const std::vector<std::unique_ptr<CPDF_Function>>& funcs,
    CPDF_ColorSpace* pCS,
    int alpha) {
  ASSERT(pBitmap->GetFormat() == FXDIB_Argb);

  CPDF_MeshStream stream(kFreeFormGouraudTriangleMeshShading, funcs,
                         pShadingStream, pCS);
  if (!stream.Load())
    return;

  CPDF_MeshVertex triangle[3];
  FXSYS_memset(triangle, 0, sizeof(triangle));

  while (!stream.BitStream()->IsEOF()) {
    CPDF_MeshVertex vertex;
    uint32_t flag;
    if (!stream.ReadVertex(*pObject2Bitmap, &vertex, &flag))
      return;

    if (flag == 0) {
      triangle[0] = vertex;
      for (int j = 1; j < 3; j++) {
        uint32_t tflag;
        if (!stream.ReadVertex(*pObject2Bitmap, &triangle[j], &tflag))
          return;
      }
    } else {
      if (flag == 1)
        triangle[0] = triangle[1];

      triangle[1] = triangle[2];
      triangle[2] = vertex;
    }
    DrawGouraud(pBitmap, alpha, triangle);
  }
}

void DrawLatticeGouraudShading(
    CFX_DIBitmap* pBitmap,
    CFX_Matrix* pObject2Bitmap,
    CPDF_Stream* pShadingStream,
    const std::vector<std::unique_ptr<CPDF_Function>>& funcs,
    CPDF_ColorSpace* pCS,
    int alpha) {
  ASSERT(pBitmap->GetFormat() == FXDIB_Argb);

  int row_verts = pShadingStream->GetDict()->GetIntegerFor("VerticesPerRow");
  if (row_verts < 2)
    return;

  CPDF_MeshStream stream(kLatticeFormGouraudTriangleMeshShading, funcs,
                         pShadingStream, pCS);
  if (!stream.Load())
    return;

  std::unique_ptr<CPDF_MeshVertex, FxFreeDeleter> vertex(
      FX_Alloc2D(CPDF_MeshVertex, row_verts, 2));
  if (!stream.ReadVertexRow(*pObject2Bitmap, row_verts, vertex.get()))
    return;

  int last_index = 0;
  while (1) {
    CPDF_MeshVertex* last_row = vertex.get() + last_index * row_verts;
    CPDF_MeshVertex* this_row = vertex.get() + (1 - last_index) * row_verts;
    if (!stream.ReadVertexRow(*pObject2Bitmap, row_verts, this_row))
      return;

    CPDF_MeshVertex triangle[3];
    for (int i = 1; i < row_verts; i++) {
      triangle[0] = last_row[i];
      triangle[1] = this_row[i - 1];
      triangle[2] = last_row[i - 1];
      DrawGouraud(pBitmap, alpha, triangle);
      triangle[2] = this_row[i];
      DrawGouraud(pBitmap, alpha, triangle);
    }
    last_index = 1 - last_index;
  }
}

struct Coon_BezierCoeff {
  float a, b, c, d;
  void FromPoints(float p0, float p1, float p2, float p3) {
    a = -p0 + 3 * p1 - 3 * p2 + p3;
    b = 3 * p0 - 6 * p1 + 3 * p2;
    c = -3 * p0 + 3 * p1;
    d = p0;
  }
  Coon_BezierCoeff first_half() {
    Coon_BezierCoeff result;
    result.a = a / 8;
    result.b = b / 4;
    result.c = c / 2;
    result.d = d;
    return result;
  }
  Coon_BezierCoeff second_half() {
    Coon_BezierCoeff result;
    result.a = a / 8;
    result.b = 3 * a / 8 + b / 4;
    result.c = 3 * a / 8 + b / 2 + c / 2;
    result.d = a / 8 + b / 4 + c / 2 + d;
    return result;
  }
  void GetPoints(float p[4]) {
    p[0] = d;
    p[1] = c / 3 + p[0];
    p[2] = b / 3 - p[0] + 2 * p[1];
    p[3] = a + p[0] - 3 * p[1] + 3 * p[2];
  }
  void GetPointsReverse(float p[4]) {
    p[3] = d;
    p[2] = c / 3 + p[3];
    p[1] = b / 3 - p[3] + 2 * p[2];
    p[0] = a + p[3] - 3 * p[2] + 3 * p[1];
  }
  void BezierInterpol(Coon_BezierCoeff& C1,
                      Coon_BezierCoeff& C2,
                      Coon_BezierCoeff& D1,
                      Coon_BezierCoeff& D2) {
    a = (D1.a + D2.a) / 2;
    b = (D1.b + D2.b) / 2;
    c = (D1.c + D2.c) / 2 - (C1.a / 8 + C1.b / 4 + C1.c / 2) +
        (C2.a / 8 + C2.b / 4) + (-C1.d + D2.d) / 2 - (C2.a + C2.b) / 2;
    d = C1.a / 8 + C1.b / 4 + C1.c / 2 + C1.d;
  }
  float Distance() {
    float dis = a + b + c;
    return dis < 0 ? -dis : dis;
  }
};

struct Coon_Bezier {
  Coon_BezierCoeff x, y;
  void FromPoints(float x0,
                  float y0,
                  float x1,
                  float y1,
                  float x2,
                  float y2,
                  float x3,
                  float y3) {
    x.FromPoints(x0, x1, x2, x3);
    y.FromPoints(y0, y1, y2, y3);
  }

  Coon_Bezier first_half() {
    Coon_Bezier result;
    result.x = x.first_half();
    result.y = y.first_half();
    return result;
  }

  Coon_Bezier second_half() {
    Coon_Bezier result;
    result.x = x.second_half();
    result.y = y.second_half();
    return result;
  }

  void BezierInterpol(Coon_Bezier& C1,
                      Coon_Bezier& C2,
                      Coon_Bezier& D1,
                      Coon_Bezier& D2) {
    x.BezierInterpol(C1.x, C2.x, D1.x, D2.x);
    y.BezierInterpol(C1.y, C2.y, D1.y, D2.y);
  }

  void GetPoints(std::vector<FX_PATHPOINT>& pPoints, size_t start_idx) {
    float p[4];
    int i;
    x.GetPoints(p);
    for (i = 0; i < 4; i++)
      pPoints[start_idx + i].m_Point.x = p[i];

    y.GetPoints(p);
    for (i = 0; i < 4; i++)
      pPoints[start_idx + i].m_Point.y = p[i];
  }

  void GetPointsReverse(std::vector<FX_PATHPOINT>& pPoints, size_t start_idx) {
    float p[4];
    int i;
    x.GetPointsReverse(p);
    for (i = 0; i < 4; i++)
      pPoints[i + start_idx].m_Point.x = p[i];

    y.GetPointsReverse(p);
    for (i = 0; i < 4; i++)
      pPoints[i + start_idx].m_Point.y = p[i];
  }

  float Distance() { return x.Distance() + y.Distance(); }
};

int BiInterpolImpl(int c0,
                   int c1,
                   int c2,
                   int c3,
                   int x,
                   int y,
                   int x_scale,
                   int y_scale) {
  int x1 = c0 + (c3 - c0) * x / x_scale;
  int x2 = c1 + (c2 - c1) * x / x_scale;
  return x1 + (x2 - x1) * y / y_scale;
}

struct Coon_Color {
  Coon_Color() { FXSYS_memset(comp, 0, sizeof(int) * 3); }
  int comp[3];

  void BiInterpol(Coon_Color colors[4],
                  int x,
                  int y,
                  int x_scale,
                  int y_scale) {
    for (int i = 0; i < 3; i++) {
      comp[i] = BiInterpolImpl(colors[0].comp[i], colors[1].comp[i],
                               colors[2].comp[i], colors[3].comp[i], x, y,
                               x_scale, y_scale);
    }
  }

  int Distance(Coon_Color& o) {
    return std::max({FXSYS_abs(comp[0] - o.comp[0]),
                     FXSYS_abs(comp[1] - o.comp[1]),
                     FXSYS_abs(comp[2] - o.comp[2])});
  }
};

#define COONCOLOR_THRESHOLD 4
struct CPDF_PatchDrawer {
  Coon_Color patch_colors[4];
  int max_delta;
  CFX_PathData path;
  CFX_RenderDevice* pDevice;
  int fill_mode;
  int alpha;
  void Draw(int x_scale,
            int y_scale,
            int left,
            int bottom,
            Coon_Bezier C1,
            Coon_Bezier C2,
            Coon_Bezier D1,
            Coon_Bezier D2) {
    bool bSmall = C1.Distance() < 2 && C2.Distance() < 2 && D1.Distance() < 2 &&
                  D2.Distance() < 2;
    Coon_Color div_colors[4];
    int d_bottom = 0;
    int d_left = 0;
    int d_top = 0;
    int d_right = 0;
    div_colors[0].BiInterpol(patch_colors, left, bottom, x_scale, y_scale);
    if (!bSmall) {
      div_colors[1].BiInterpol(patch_colors, left, bottom + 1, x_scale,
                               y_scale);
      div_colors[2].BiInterpol(patch_colors, left + 1, bottom + 1, x_scale,
                               y_scale);
      div_colors[3].BiInterpol(patch_colors, left + 1, bottom, x_scale,
                               y_scale);
      d_bottom = div_colors[3].Distance(div_colors[0]);
      d_left = div_colors[1].Distance(div_colors[0]);
      d_top = div_colors[1].Distance(div_colors[2]);
      d_right = div_colors[2].Distance(div_colors[3]);
    }

    if (bSmall ||
        (d_bottom < COONCOLOR_THRESHOLD && d_left < COONCOLOR_THRESHOLD &&
         d_top < COONCOLOR_THRESHOLD && d_right < COONCOLOR_THRESHOLD)) {
      std::vector<FX_PATHPOINT>& pPoints = path.GetPoints();
      C1.GetPoints(pPoints, 0);
      D2.GetPoints(pPoints, 3);
      C2.GetPointsReverse(pPoints, 6);
      D1.GetPointsReverse(pPoints, 9);
      int fillFlags = FXFILL_WINDING | FXFILL_FULLCOVER;
      if (fill_mode & RENDER_NOPATHSMOOTH) {
        fillFlags |= FXFILL_NOPATHSMOOTH;
      }
      pDevice->DrawPath(
          &path, nullptr, nullptr,
          FXARGB_MAKE(alpha, div_colors[0].comp[0], div_colors[0].comp[1],
                      div_colors[0].comp[2]),
          0, fillFlags);
    } else {
      if (d_bottom < COONCOLOR_THRESHOLD && d_top < COONCOLOR_THRESHOLD) {
        Coon_Bezier m1;
        m1.BezierInterpol(D1, D2, C1, C2);
        y_scale *= 2;
        bottom *= 2;
        Draw(x_scale, y_scale, left, bottom, C1, m1, D1.first_half(),
             D2.first_half());
        Draw(x_scale, y_scale, left, bottom + 1, m1, C2, D1.second_half(),
             D2.second_half());
      } else if (d_left < COONCOLOR_THRESHOLD &&
                 d_right < COONCOLOR_THRESHOLD) {
        Coon_Bezier m2;
        m2.BezierInterpol(C1, C2, D1, D2);
        x_scale *= 2;
        left *= 2;
        Draw(x_scale, y_scale, left, bottom, C1.first_half(), C2.first_half(),
             D1, m2);
        Draw(x_scale, y_scale, left + 1, bottom, C1.second_half(),
             C2.second_half(), m2, D2);
      } else {
        Coon_Bezier m1, m2;
        m1.BezierInterpol(D1, D2, C1, C2);
        m2.BezierInterpol(C1, C2, D1, D2);
        Coon_Bezier m1f = m1.first_half();
        Coon_Bezier m1s = m1.second_half();
        Coon_Bezier m2f = m2.first_half();
        Coon_Bezier m2s = m2.second_half();
        x_scale *= 2;
        y_scale *= 2;
        left *= 2;
        bottom *= 2;
        Draw(x_scale, y_scale, left, bottom, C1.first_half(), m1f,
             D1.first_half(), m2f);
        Draw(x_scale, y_scale, left, bottom + 1, m1f, C2.first_half(),
             D1.second_half(), m2s);
        Draw(x_scale, y_scale, left + 1, bottom, C1.second_half(), m1s, m2f,
             D2.first_half());
        Draw(x_scale, y_scale, left + 1, bottom + 1, m1s, C2.second_half(), m2s,
             D2.second_half());
      }
    }
  }
};

void DrawCoonPatchMeshes(
    ShadingType type,
    CFX_DIBitmap* pBitmap,
    CFX_Matrix* pObject2Bitmap,
    CPDF_Stream* pShadingStream,
    const std::vector<std::unique_ptr<CPDF_Function>>& funcs,
    CPDF_ColorSpace* pCS,
    int fill_mode,
    int alpha) {
  ASSERT(pBitmap->GetFormat() == FXDIB_Argb);
  ASSERT(type == kCoonsPatchMeshShading ||
         type == kTensorProductPatchMeshShading);

  CFX_FxgeDevice device;
  device.Attach(pBitmap, false, nullptr, false);
  CPDF_MeshStream stream(type, funcs, pShadingStream, pCS);
  if (!stream.Load())
    return;

  CPDF_PatchDrawer patch;
  patch.alpha = alpha;
  patch.pDevice = &device;
  patch.fill_mode = fill_mode;

  for (int i = 0; i < 13; i++) {
    patch.path.AppendPoint(
        CFX_PointF(), i == 0 ? FXPT_TYPE::MoveTo : FXPT_TYPE::BezierTo, false);
  }

  CFX_PointF coords[16];
  int point_count = type == kTensorProductPatchMeshShading ? 16 : 12;
  while (!stream.BitStream()->IsEOF()) {
    if (!stream.CanReadFlag())
      break;
    uint32_t flag = stream.ReadFlag();
    int iStartPoint = 0, iStartColor = 0, i = 0;
    if (flag) {
      iStartPoint = 4;
      iStartColor = 2;
      CFX_PointF tempCoords[4];
      for (i = 0; i < 4; i++) {
        tempCoords[i] = coords[(flag * 3 + i) % 12];
      }
      FXSYS_memcpy(coords, tempCoords, sizeof(tempCoords));
      Coon_Color tempColors[2];
      tempColors[0] = patch.patch_colors[flag];
      tempColors[1] = patch.patch_colors[(flag + 1) % 4];
      FXSYS_memcpy(patch.patch_colors, tempColors, sizeof(Coon_Color) * 2);
    }
    for (i = iStartPoint; i < point_count; i++) {
      if (!stream.CanReadCoords())
        break;
      coords[i] = pObject2Bitmap->Transform(stream.ReadCoords());
    }

    for (i = iStartColor; i < 4; i++) {
      if (!stream.CanReadColor())
        break;

      FX_FLOAT r;
      FX_FLOAT g;
      FX_FLOAT b;
      std::tie(r, g, b) = stream.ReadColor();

      patch.patch_colors[i].comp[0] = (int32_t)(r * 255);
      patch.patch_colors[i].comp[1] = (int32_t)(g * 255);
      patch.patch_colors[i].comp[2] = (int32_t)(b * 255);
    }
    CFX_FloatRect bbox = CFX_FloatRect::GetBBox(coords, point_count);
    if (bbox.right <= 0 || bbox.left >= (FX_FLOAT)pBitmap->GetWidth() ||
        bbox.top <= 0 || bbox.bottom >= (FX_FLOAT)pBitmap->GetHeight()) {
      continue;
    }
    Coon_Bezier C1, C2, D1, D2;
    C1.FromPoints(coords[0].x, coords[0].y, coords[11].x, coords[11].y,
                  coords[10].x, coords[10].y, coords[9].x, coords[9].y);
    C2.FromPoints(coords[3].x, coords[3].y, coords[4].x, coords[4].y,
                  coords[5].x, coords[5].y, coords[6].x, coords[6].y);
    D1.FromPoints(coords[0].x, coords[0].y, coords[1].x, coords[1].y,
                  coords[2].x, coords[2].y, coords[3].x, coords[3].y);
    D2.FromPoints(coords[9].x, coords[9].y, coords[8].x, coords[8].y,
                  coords[7].x, coords[7].y, coords[6].x, coords[6].y);
    patch.Draw(1, 1, 0, 0, C1, C2, D1, D2);
  }
}

std::unique_ptr<CFX_DIBitmap> DrawPatternBitmap(
    CPDF_Document* pDoc,
    CPDF_PageRenderCache* pCache,
    CPDF_TilingPattern* pPattern,
    const CFX_Matrix* pObject2Device,
    int width,
    int height,
    int flags) {
  std::unique_ptr<CFX_DIBitmap> pBitmap(new CFX_DIBitmap);
  if (!pBitmap->Create(width, height,
                       pPattern->colored() ? FXDIB_Argb : FXDIB_8bppMask)) {
    return nullptr;
  }
  CFX_FxgeDevice bitmap_device;
  bitmap_device.Attach(pBitmap.get(), false, nullptr, false);
  pBitmap->Clear(0);
  CFX_FloatRect cell_bbox = pPattern->bbox();
  pPattern->pattern_to_form()->TransformRect(cell_bbox);
  pObject2Device->TransformRect(cell_bbox);
  CFX_FloatRect bitmap_rect(0.0f, 0.0f, (FX_FLOAT)width, (FX_FLOAT)height);
  CFX_Matrix mtAdjust;
  mtAdjust.MatchRect(bitmap_rect, cell_bbox);

  CFX_Matrix mtPattern2Bitmap = *pObject2Device;
  mtPattern2Bitmap.Concat(mtAdjust);
  CPDF_RenderOptions options;
  if (!pPattern->colored())
    options.m_ColorMode = RENDER_COLOR_ALPHA;

  flags |= RENDER_FORCE_HALFTONE;
  options.m_Flags = flags;
  CPDF_RenderContext context(pDoc, pCache);
  context.AppendLayer(pPattern->form(), &mtPattern2Bitmap);
  context.Render(&bitmap_device, &options, nullptr);
#if defined _SKIA_SUPPORT_PATHS_
  bitmap_device.Flush();
  pBitmap->UnPreMultiply();
#endif
  return pBitmap;
}

bool IsAvailableMatrix(const CFX_Matrix& matrix) {
  if (matrix.a == 0 || matrix.d == 0)
    return matrix.b != 0 && matrix.c != 0;

  if (matrix.b == 0 || matrix.c == 0)
    return matrix.a != 0 && matrix.d != 0;

  return true;
}

}  // namespace

// static
int CPDF_RenderStatus::s_CurrentRecursionDepth = 0;

CPDF_RenderStatus::CPDF_RenderStatus()
    : m_pFormResource(nullptr),
      m_pPageResource(nullptr),
      m_pContext(nullptr),
      m_bStopped(false),
      m_pDevice(nullptr),
      m_pCurObj(nullptr),
      m_pStopObj(nullptr),
      m_bPrint(false),
      m_Transparency(0),
      m_bDropObjects(false),
      m_bStdCS(false),
      m_GroupFamily(0),
      m_bLoadMask(false),
      m_pType3Char(nullptr),
      m_T3FillColor(0),
      m_curBlend(FXDIB_BLEND_NORMAL) {}

CPDF_RenderStatus::~CPDF_RenderStatus() {}

bool CPDF_RenderStatus::Initialize(CPDF_RenderContext* pContext,
                                   CFX_RenderDevice* pDevice,
                                   const CFX_Matrix* pDeviceMatrix,
                                   const CPDF_PageObject* pStopObj,
                                   const CPDF_RenderStatus* pParentState,
                                   const CPDF_GraphicStates* pInitialStates,
                                   const CPDF_RenderOptions* pOptions,
                                   int transparency,
                                   bool bDropObjects,
                                   CPDF_Dictionary* pFormResource,
                                   bool bStdCS,
                                   CPDF_Type3Char* pType3Char,
                                   FX_ARGB fill_color,
                                   uint32_t GroupFamily,
                                   bool bLoadMask) {
  m_pContext = pContext;
  m_pDevice = pDevice;
  m_bPrint = m_pDevice->GetDeviceClass() != FXDC_DISPLAY;
  if (pDeviceMatrix) {
    m_DeviceMatrix = *pDeviceMatrix;
  }
  m_pStopObj = pStopObj;
  if (pOptions) {
    m_Options = *pOptions;
  }
  m_bDropObjects = bDropObjects;
  m_bStdCS = bStdCS;
  m_T3FillColor = fill_color;
  m_pType3Char = pType3Char;
  m_GroupFamily = GroupFamily;
  m_bLoadMask = bLoadMask;
  m_pFormResource = pFormResource;
  m_pPageResource = m_pContext->GetPageResources();
  if (pInitialStates && !m_pType3Char) {
    m_InitialStates.CopyStates(*pInitialStates);
    if (pParentState) {
      if (!m_InitialStates.m_ColorState.HasFillColor()) {
        m_InitialStates.m_ColorState.SetFillRGB(
            pParentState->m_InitialStates.m_ColorState.GetFillRGB());
        m_InitialStates.m_ColorState.GetMutableFillColor()->Copy(
            pParentState->m_InitialStates.m_ColorState.GetFillColor());
      }
      if (!m_InitialStates.m_ColorState.HasStrokeColor()) {
        m_InitialStates.m_ColorState.SetStrokeRGB(
            pParentState->m_InitialStates.m_ColorState.GetFillRGB());
        m_InitialStates.m_ColorState.GetMutableStrokeColor()->Copy(
            pParentState->m_InitialStates.m_ColorState.GetStrokeColor());
      }
    }
  } else {
    m_InitialStates.DefaultStates();
  }
  m_pImageRenderer.reset();
  m_Transparency = transparency;
  return true;
}

void CPDF_RenderStatus::RenderObjectList(
    const CPDF_PageObjectHolder* pObjectHolder,
    const CFX_Matrix* pObj2Device) {
#if defined _SKIA_SUPPORT_
  DebugVerifyDeviceIsPreMultiplied();
#endif
  CFX_FloatRect clip_rect(m_pDevice->GetClipBox());
  CFX_Matrix device2object;
  device2object.SetReverse(*pObj2Device);
  device2object.TransformRect(clip_rect);

  for (const auto& pCurObj : *pObjectHolder->GetPageObjectList()) {
    if (pCurObj.get() == m_pStopObj) {
      m_bStopped = true;
      return;
    }
    if (!pCurObj)
      continue;

    if (pCurObj->m_Left > clip_rect.right ||
        pCurObj->m_Right < clip_rect.left ||
        pCurObj->m_Bottom > clip_rect.top ||
        pCurObj->m_Top < clip_rect.bottom) {
      continue;
    }
    RenderSingleObject(pCurObj.get(), pObj2Device);
    if (m_bStopped)
      return;
  }
#if defined _SKIA_SUPPORT_
  DebugVerifyDeviceIsPreMultiplied();
#endif
}

void CPDF_RenderStatus::RenderSingleObject(CPDF_PageObject* pObj,
                                           const CFX_Matrix* pObj2Device) {
#if defined _SKIA_SUPPORT_
  DebugVerifyDeviceIsPreMultiplied();
#endif
  CFX_AutoRestorer<int> restorer(&s_CurrentRecursionDepth);
  if (++s_CurrentRecursionDepth > kRenderMaxRecursionDepth) {
    return;
  }
  m_pCurObj = pObj;
  if (m_Options.m_pOCContext && pObj->m_ContentMark) {
    if (!m_Options.m_pOCContext->CheckObjectVisible(pObj)) {
      return;
    }
  }
  ProcessClipPath(pObj->m_ClipPath, pObj2Device);
  if (ProcessTransparency(pObj, pObj2Device)) {
    return;
  }
  ProcessObjectNoClip(pObj, pObj2Device);
#if defined _SKIA_SUPPORT_
  DebugVerifyDeviceIsPreMultiplied();
#endif
}

bool CPDF_RenderStatus::ContinueSingleObject(CPDF_PageObject* pObj,
                                             const CFX_Matrix* pObj2Device,
                                             IFX_Pause* pPause) {
  if (m_pImageRenderer) {
    if (m_pImageRenderer->Continue(pPause))
      return true;

    if (!m_pImageRenderer->GetResult())
      DrawObjWithBackground(pObj, pObj2Device);
    m_pImageRenderer.reset();
    return false;
  }

  m_pCurObj = pObj;
  if (m_Options.m_pOCContext && pObj->m_ContentMark &&
      !m_Options.m_pOCContext->CheckObjectVisible(pObj)) {
    return false;
  }

  ProcessClipPath(pObj->m_ClipPath, pObj2Device);
  if (ProcessTransparency(pObj, pObj2Device))
    return false;

  if (pObj->IsImage()) {
    m_pImageRenderer = pdfium::MakeUnique<CPDF_ImageRenderer>();
    if (!m_pImageRenderer->Start(this, pObj, pObj2Device, false,
                                 FXDIB_BLEND_NORMAL)) {
      if (!m_pImageRenderer->GetResult())
        DrawObjWithBackground(pObj, pObj2Device);
      m_pImageRenderer.reset();
      return false;
    }
    return ContinueSingleObject(pObj, pObj2Device, pPause);
  }

  ProcessObjectNoClip(pObj, pObj2Device);
  return false;
}

bool CPDF_RenderStatus::GetObjectClippedRect(const CPDF_PageObject* pObj,
                                             const CFX_Matrix* pObj2Device,
                                             bool bLogical,
                                             FX_RECT& rect) const {
  rect = pObj->GetBBox(pObj2Device);
  FX_RECT rtClip = m_pDevice->GetClipBox();
  if (!bLogical) {
    CFX_Matrix dCTM = m_pDevice->GetCTM();
    FX_FLOAT a = FXSYS_fabs(dCTM.a);
    FX_FLOAT d = FXSYS_fabs(dCTM.d);
    if (a != 1.0f || d != 1.0f) {
      rect.right = rect.left + (int32_t)FXSYS_ceil((FX_FLOAT)rect.Width() * a);
      rect.bottom = rect.top + (int32_t)FXSYS_ceil((FX_FLOAT)rect.Height() * d);
      rtClip.right =
          rtClip.left + (int32_t)FXSYS_ceil((FX_FLOAT)rtClip.Width() * a);
      rtClip.bottom =
          rtClip.top + (int32_t)FXSYS_ceil((FX_FLOAT)rtClip.Height() * d);
    }
  }
  rect.Intersect(rtClip);
  return rect.IsEmpty();
}

void CPDF_RenderStatus::ProcessObjectNoClip(CPDF_PageObject* pObj,
                                            const CFX_Matrix* pObj2Device) {
#if defined _SKIA_SUPPORT_
  DebugVerifyDeviceIsPreMultiplied();
#endif
  bool bRet = false;
  switch (pObj->GetType()) {
    case CPDF_PageObject::TEXT:
      bRet = ProcessText(pObj->AsText(), pObj2Device, nullptr);
      break;
    case CPDF_PageObject::PATH:
      bRet = ProcessPath(pObj->AsPath(), pObj2Device);
      break;
    case CPDF_PageObject::IMAGE:
      bRet = ProcessImage(pObj->AsImage(), pObj2Device);
      break;
    case CPDF_PageObject::SHADING:
      ProcessShading(pObj->AsShading(), pObj2Device);
      return;
    case CPDF_PageObject::FORM:
      bRet = ProcessForm(pObj->AsForm(), pObj2Device);
      break;
  }
  if (!bRet)
    DrawObjWithBackground(pObj, pObj2Device);
#if defined _SKIA_SUPPORT_
  DebugVerifyDeviceIsPreMultiplied();
#endif
}

bool CPDF_RenderStatus::DrawObjWithBlend(CPDF_PageObject* pObj,
                                         const CFX_Matrix* pObj2Device) {
  bool bRet = false;
  switch (pObj->GetType()) {
    case CPDF_PageObject::PATH:
      bRet = ProcessPath(pObj->AsPath(), pObj2Device);
      break;
    case CPDF_PageObject::IMAGE:
      bRet = ProcessImage(pObj->AsImage(), pObj2Device);
      break;
    case CPDF_PageObject::FORM:
      bRet = ProcessForm(pObj->AsForm(), pObj2Device);
      break;
    default:
      break;
  }
  return bRet;
}

void CPDF_RenderStatus::GetScaledMatrix(CFX_Matrix& matrix) const {
  CFX_Matrix dCTM = m_pDevice->GetCTM();
  matrix.a *= FXSYS_fabs(dCTM.a);
  matrix.d *= FXSYS_fabs(dCTM.d);
}

void CPDF_RenderStatus::DrawObjWithBackground(CPDF_PageObject* pObj,
                                              const CFX_Matrix* pObj2Device) {
  FX_RECT rect;
  if (GetObjectClippedRect(pObj, pObj2Device, false, rect)) {
    return;
  }
  int res = 300;
  if (pObj->IsImage() &&
      m_pDevice->GetDeviceCaps(FXDC_DEVICE_CLASS) == FXDC_PRINTER) {
    res = 0;
  }
  CPDF_ScaledRenderBuffer buffer;
  if (!buffer.Initialize(m_pContext, m_pDevice, rect, pObj, &m_Options, res)) {
    return;
  }
  CFX_Matrix matrix = *pObj2Device;
  matrix.Concat(*buffer.GetMatrix());
  GetScaledMatrix(matrix);
  CPDF_Dictionary* pFormResource = nullptr;
  if (pObj->IsForm()) {
    const CPDF_FormObject* pFormObj = pObj->AsForm();
    if (pFormObj->m_pForm && pFormObj->m_pForm->m_pFormDict) {
      pFormResource = pFormObj->m_pForm->m_pFormDict->GetDictFor("Resources");
    }
  }
  CPDF_RenderStatus status;
  status.Initialize(m_pContext, buffer.GetDevice(), buffer.GetMatrix(), nullptr,
                    nullptr, nullptr, &m_Options, m_Transparency,
                    m_bDropObjects, pFormResource);
  status.RenderSingleObject(pObj, &matrix);
  buffer.OutputToDevice();
}

bool CPDF_RenderStatus::ProcessForm(const CPDF_FormObject* pFormObj,
                                    const CFX_Matrix* pObj2Device) {
#if defined _SKIA_SUPPORT_
  DebugVerifyDeviceIsPreMultiplied();
#endif
  CPDF_Dictionary* pOC = pFormObj->m_pForm->m_pFormDict->GetDictFor("OC");
  if (pOC && m_Options.m_pOCContext &&
      !m_Options.m_pOCContext->CheckOCGVisible(pOC)) {
    return true;
  }
  CFX_Matrix matrix = pFormObj->m_FormMatrix;
  matrix.Concat(*pObj2Device);
  CPDF_Dictionary* pResources = nullptr;
  if (pFormObj->m_pForm && pFormObj->m_pForm->m_pFormDict) {
    pResources = pFormObj->m_pForm->m_pFormDict->GetDictFor("Resources");
  }
  CPDF_RenderStatus status;
  status.Initialize(m_pContext, m_pDevice, nullptr, m_pStopObj, this, pFormObj,
                    &m_Options, m_Transparency, m_bDropObjects, pResources,
                    false);
  status.m_curBlend = m_curBlend;
  m_pDevice->SaveState();
  status.RenderObjectList(pFormObj->m_pForm.get(), &matrix);
  m_bStopped = status.m_bStopped;
  m_pDevice->RestoreState(false);
#if defined _SKIA_SUPPORT_
  DebugVerifyDeviceIsPreMultiplied();
#endif
  return true;
}

bool CPDF_RenderStatus::ProcessPath(CPDF_PathObject* pPathObj,
                                    const CFX_Matrix* pObj2Device) {
  int FillType = pPathObj->m_FillType;
  bool bStroke = pPathObj->m_bStroke;
  ProcessPathPattern(pPathObj, pObj2Device, FillType, bStroke);
  if (FillType == 0 && !bStroke)
    return true;

  uint32_t fill_argb = FillType ? GetFillArgb(pPathObj) : 0;
  uint32_t stroke_argb = bStroke ? GetStrokeArgb(pPathObj) : 0;
  CFX_Matrix path_matrix = pPathObj->m_Matrix;
  path_matrix.Concat(*pObj2Device);
  if (!IsAvailableMatrix(path_matrix))
    return true;

  if (FillType && (m_Options.m_Flags & RENDER_RECT_AA))
    FillType |= FXFILL_RECT_AA;
  if (m_Options.m_Flags & RENDER_FILL_FULLCOVER)
    FillType |= FXFILL_FULLCOVER;
  if (m_Options.m_Flags & RENDER_NOPATHSMOOTH)
    FillType |= FXFILL_NOPATHSMOOTH;
  if (bStroke)
    FillType |= FX_FILL_STROKE;

  const CPDF_PageObject* pPageObj =
      static_cast<const CPDF_PageObject*>(pPathObj);
  if (pPageObj->m_GeneralState.GetStrokeAdjust())
    FillType |= FX_STROKE_ADJUST;
  if (m_pType3Char)
    FillType |= FX_FILL_TEXT_MODE;

  CFX_GraphState graphState = pPathObj->m_GraphState;
  if (m_Options.m_Flags & RENDER_THINLINE)
    graphState.SetLineWidth(0);
  return m_pDevice->DrawPathWithBlend(
      pPathObj->m_Path.GetObject(), &path_matrix, graphState.GetObject(),
      fill_argb, stroke_argb, FillType, m_curBlend);
}

CPDF_TransferFunc* CPDF_RenderStatus::GetTransferFunc(CPDF_Object* pObj) const {
  ASSERT(pObj);
  CPDF_DocRenderData* pDocCache = m_pContext->GetDocument()->GetRenderData();
  return pDocCache ? pDocCache->GetTransferFunc(pObj) : nullptr;
}

FX_ARGB CPDF_RenderStatus::GetFillArgb(CPDF_PageObject* pObj,
                                       bool bType3) const {
  const CPDF_ColorState* pColorState = &pObj->m_ColorState;
  if (m_pType3Char && !bType3 &&
      (!m_pType3Char->m_bColored ||
       (m_pType3Char->m_bColored &&
        (!*pColorState || pColorState->GetFillColor()->IsNull())))) {
    return m_T3FillColor;
  }
  if (!*pColorState || pColorState->GetFillColor()->IsNull())
    pColorState = &m_InitialStates.m_ColorState;

  FX_COLORREF rgb = pColorState->GetFillRGB();
  if (rgb == (uint32_t)-1)
    return 0;

  int32_t alpha =
      static_cast<int32_t>((pObj->m_GeneralState.GetFillAlpha() * 255));
  if (pObj->m_GeneralState.GetTR()) {
    if (!pObj->m_GeneralState.GetTransferFunc()) {
      pObj->m_GeneralState.SetTransferFunc(
          GetTransferFunc(pObj->m_GeneralState.GetTR()));
    }
    if (pObj->m_GeneralState.GetTransferFunc())
      rgb = pObj->m_GeneralState.GetTransferFunc()->TranslateColor(rgb);
  }
  return m_Options.TranslateColor(ArgbEncode(alpha, rgb));
}

FX_ARGB CPDF_RenderStatus::GetStrokeArgb(CPDF_PageObject* pObj) const {
  const CPDF_ColorState* pColorState = &pObj->m_ColorState;
  if (m_pType3Char &&
      (!m_pType3Char->m_bColored ||
       (m_pType3Char->m_bColored &&
        (!*pColorState || pColorState->GetStrokeColor()->IsNull())))) {
    return m_T3FillColor;
  }
  if (!*pColorState || pColorState->GetStrokeColor()->IsNull())
    pColorState = &m_InitialStates.m_ColorState;

  FX_COLORREF rgb = pColorState->GetStrokeRGB();
  if (rgb == (uint32_t)-1)
    return 0;

  int32_t alpha = static_cast<int32_t>(pObj->m_GeneralState.GetStrokeAlpha() *
                                       255);  // not rounded.
  if (pObj->m_GeneralState.GetTR()) {
    if (!pObj->m_GeneralState.GetTransferFunc()) {
      pObj->m_GeneralState.SetTransferFunc(
          GetTransferFunc(pObj->m_GeneralState.GetTR()));
    }
    if (pObj->m_GeneralState.GetTransferFunc())
      rgb = pObj->m_GeneralState.GetTransferFunc()->TranslateColor(rgb);
  }
  return m_Options.TranslateColor(ArgbEncode(alpha, rgb));
}

void CPDF_RenderStatus::ProcessClipPath(CPDF_ClipPath ClipPath,
                                        const CFX_Matrix* pObj2Device) {
  if (!ClipPath) {
    if (m_LastClipPath) {
      m_pDevice->RestoreState(true);
      m_LastClipPath.SetNull();
    }
    return;
  }
  if (m_LastClipPath == ClipPath)
    return;

  m_LastClipPath = ClipPath;
  m_pDevice->RestoreState(true);
  int nClipPath = ClipPath.GetPathCount();
  for (int i = 0; i < nClipPath; ++i) {
    const CFX_PathData* pPathData = ClipPath.GetPath(i).GetObject();
    if (!pPathData)
      continue;

    if (pPathData->GetPoints().empty()) {
      CFX_PathData EmptyPath;
      EmptyPath.AppendRect(-1, -1, 0, 0);
      int fill_mode = FXFILL_WINDING;
      m_pDevice->SetClip_PathFill(&EmptyPath, nullptr, fill_mode);
    } else {
      int ClipType = ClipPath.GetClipType(i);
      m_pDevice->SetClip_PathFill(pPathData, pObj2Device, ClipType);
    }
  }
  int textcount = ClipPath.GetTextCount();
  if (textcount == 0)
    return;

  if (m_pDevice->GetDeviceClass() == FXDC_DISPLAY &&
      !(m_pDevice->GetDeviceCaps(FXDC_RENDER_CAPS) & FXRC_SOFT_CLIP)) {
    return;
  }

  std::unique_ptr<CFX_PathData> pTextClippingPath;
  for (int i = 0; i < textcount; ++i) {
    CPDF_TextObject* pText = ClipPath.GetText(i);
    if (pText) {
      if (!pTextClippingPath)
        pTextClippingPath = pdfium::MakeUnique<CFX_PathData>();
      ProcessText(pText, pObj2Device, pTextClippingPath.get());
      continue;
    }

    if (!pTextClippingPath)
      continue;

    int fill_mode = FXFILL_WINDING;
    if (m_Options.m_Flags & RENDER_NOTEXTSMOOTH)
      fill_mode |= FXFILL_NOPATHSMOOTH;
    m_pDevice->SetClip_PathFill(pTextClippingPath.get(), nullptr, fill_mode);
    pTextClippingPath.reset();
  }
}

bool CPDF_RenderStatus::SelectClipPath(const CPDF_PathObject* pPathObj,
                                       const CFX_Matrix* pObj2Device,
                                       bool bStroke) {
  CFX_Matrix path_matrix = pPathObj->m_Matrix;
  path_matrix.Concat(*pObj2Device);
  if (bStroke) {
    CFX_GraphState graphState = pPathObj->m_GraphState;
    if (m_Options.m_Flags & RENDER_THINLINE)
      graphState.SetLineWidth(0);
    return m_pDevice->SetClip_PathStroke(pPathObj->m_Path.GetObject(),
                                         &path_matrix, graphState.GetObject());
  }
  int fill_mode = pPathObj->m_FillType;
  if (m_Options.m_Flags & RENDER_NOPATHSMOOTH) {
    fill_mode |= FXFILL_NOPATHSMOOTH;
  }
  return m_pDevice->SetClip_PathFill(pPathObj->m_Path.GetObject(), &path_matrix,
                                     fill_mode);
}

bool CPDF_RenderStatus::ProcessTransparency(CPDF_PageObject* pPageObj,
                                            const CFX_Matrix* pObj2Device) {
#if defined _SKIA_SUPPORT_
  DebugVerifyDeviceIsPreMultiplied();
#endif
  int blend_type = pPageObj->m_GeneralState.GetBlendType();
  if (blend_type == FXDIB_BLEND_UNSUPPORTED)
    return true;

  CPDF_Dictionary* pSMaskDict =
      ToDictionary(pPageObj->m_GeneralState.GetSoftMask());
  if (pSMaskDict) {
    if (pPageObj->IsImage() &&
        pPageObj->AsImage()->GetImage()->GetDict()->KeyExist("SMask")) {
      pSMaskDict = nullptr;
    }
  }
  CPDF_Dictionary* pFormResource = nullptr;
  FX_FLOAT group_alpha = 1.0f;
  int Transparency = m_Transparency;
  bool bGroupTransparent = false;
  if (pPageObj->IsForm()) {
    const CPDF_FormObject* pFormObj = pPageObj->AsForm();
    group_alpha = pFormObj->m_GeneralState.GetFillAlpha();
    Transparency = pFormObj->m_pForm->m_Transparency;
    bGroupTransparent = !!(Transparency & PDFTRANS_ISOLATED);
    if (pFormObj->m_pForm->m_pFormDict) {
      pFormResource = pFormObj->m_pForm->m_pFormDict->GetDictFor("Resources");
    }
  }
  bool bTextClip =
      (pPageObj->m_ClipPath && pPageObj->m_ClipPath.GetTextCount() &&
       m_pDevice->GetDeviceClass() == FXDC_DISPLAY &&
       !(m_pDevice->GetDeviceCaps(FXDC_RENDER_CAPS) & FXRC_SOFT_CLIP));
  if ((m_Options.m_Flags & RENDER_OVERPRINT) && pPageObj->IsImage() &&
      pPageObj->m_GeneralState.GetFillOP() &&
      pPageObj->m_GeneralState.GetStrokeOP()) {
    CPDF_Document* pDocument = nullptr;
    CPDF_Page* pPage = nullptr;
    if (m_pContext->GetPageCache()) {
      pPage = m_pContext->GetPageCache()->GetPage();
      pDocument = pPage->m_pDocument;
    } else {
      pDocument = pPageObj->AsImage()->GetImage()->GetDocument();
    }
    CPDF_Dictionary* pPageResources = pPage ? pPage->m_pPageResources : nullptr;
    CPDF_Object* pCSObj = pPageObj->AsImage()
                              ->GetImage()
                              ->GetStream()
                              ->GetDict()
                              ->GetDirectObjectFor("ColorSpace");
    CPDF_ColorSpace* pColorSpace =
        pDocument->LoadColorSpace(pCSObj, pPageResources);
    if (pColorSpace) {
      int format = pColorSpace->GetFamily();
      if (format == PDFCS_DEVICECMYK || format == PDFCS_SEPARATION ||
          format == PDFCS_DEVICEN) {
        blend_type = FXDIB_BLEND_DARKEN;
      }
      pDocument->GetPageData()->ReleaseColorSpace(pCSObj);
    }
  }
  if (!pSMaskDict && group_alpha == 1.0f && blend_type == FXDIB_BLEND_NORMAL &&
      !bTextClip && !bGroupTransparent) {
    return false;
  }
  bool isolated = !!(Transparency & PDFTRANS_ISOLATED);
  if (m_bPrint) {
    bool bRet = false;
    int rendCaps = m_pDevice->GetRenderCaps();
    if (!((Transparency & PDFTRANS_ISOLATED) || pSMaskDict || bTextClip) &&
        (rendCaps & FXRC_BLEND_MODE)) {
      int oldBlend = m_curBlend;
      m_curBlend = blend_type;
      bRet = DrawObjWithBlend(pPageObj, pObj2Device);
      m_curBlend = oldBlend;
    }
    if (!bRet) {
      DrawObjWithBackground(pPageObj, pObj2Device);
    }
    return true;
  }
  FX_RECT rect = pPageObj->GetBBox(pObj2Device);
  rect.Intersect(m_pDevice->GetClipBox());
  if (rect.IsEmpty()) {
    return true;
  }
  CFX_Matrix deviceCTM = m_pDevice->GetCTM();
  FX_FLOAT scaleX = FXSYS_fabs(deviceCTM.a);
  FX_FLOAT scaleY = FXSYS_fabs(deviceCTM.d);
  int width = FXSYS_round((FX_FLOAT)rect.Width() * scaleX);
  int height = FXSYS_round((FX_FLOAT)rect.Height() * scaleY);
  CFX_FxgeDevice bitmap_device;
  std::unique_ptr<CFX_DIBitmap> oriDevice;
  if (!isolated && (m_pDevice->GetRenderCaps() & FXRC_GET_BITS)) {
    oriDevice = pdfium::MakeUnique<CFX_DIBitmap>();
    if (!m_pDevice->CreateCompatibleBitmap(oriDevice.get(), width, height))
      return true;
    m_pDevice->GetDIBits(oriDevice.get(), rect.left, rect.top);
  }
  if (!bitmap_device.Create(width, height, FXDIB_Argb, oriDevice.get()))
    return true;
  CFX_DIBitmap* bitmap = bitmap_device.GetBitmap();
  bitmap->Clear(0);
  CFX_Matrix new_matrix = *pObj2Device;
  new_matrix.Translate(-rect.left, -rect.top);
  new_matrix.Scale(scaleX, scaleY);
  std::unique_ptr<CFX_DIBitmap> pTextMask;
  if (bTextClip) {
    pTextMask = pdfium::MakeUnique<CFX_DIBitmap>();
    if (!pTextMask->Create(width, height, FXDIB_8bppMask))
      return true;

    pTextMask->Clear(0);
    CFX_FxgeDevice text_device;
    text_device.Attach(pTextMask.get(), false, nullptr, false);
    for (uint32_t i = 0; i < pPageObj->m_ClipPath.GetTextCount(); i++) {
      CPDF_TextObject* textobj = pPageObj->m_ClipPath.GetText(i);
      if (!textobj)
        break;

      CFX_Matrix text_matrix = textobj->GetTextMatrix();
      CPDF_TextRenderer::DrawTextPath(
          &text_device, textobj->m_CharCodes, textobj->m_CharPos,
          textobj->m_TextState.GetFont(), textobj->m_TextState.GetFontSize(),
          &text_matrix, &new_matrix, textobj->m_GraphState.GetObject(),
          (FX_ARGB)-1, 0, nullptr, 0);
    }
  }
  CPDF_RenderStatus bitmap_render;
  bitmap_render.Initialize(m_pContext, &bitmap_device, nullptr, m_pStopObj,
                           nullptr, nullptr, &m_Options, 0, m_bDropObjects,
                           pFormResource, true);
  bitmap_render.ProcessObjectNoClip(pPageObj, &new_matrix);
#if defined _SKIA_SUPPORT_PATHS_
  bitmap_device.Flush();
  bitmap->UnPreMultiply();
#endif
  m_bStopped = bitmap_render.m_bStopped;
  if (pSMaskDict) {
    CFX_Matrix smask_matrix = *pPageObj->m_GeneralState.GetSMaskMatrix();
    smask_matrix.Concat(*pObj2Device);
    std::unique_ptr<CFX_DIBSource> pSMaskSource =
        LoadSMask(pSMaskDict, &rect, &smask_matrix);
    if (pSMaskSource)
      bitmap->MultiplyAlpha(pSMaskSource.get());
  }
  if (pTextMask) {
    bitmap->MultiplyAlpha(pTextMask.get());
    pTextMask.reset();
  }
  int32_t blitAlpha = 255;
  if (Transparency & PDFTRANS_GROUP && group_alpha != 1.0f) {
    blitAlpha = (int32_t)(group_alpha * 255);
#ifndef _SKIA_SUPPORT_
    bitmap->MultiplyAlpha(blitAlpha);
    blitAlpha = 255;
#endif
  }
  Transparency = m_Transparency;
  if (pPageObj->IsForm()) {
    Transparency |= PDFTRANS_GROUP;
  }
  CompositeDIBitmap(bitmap, rect.left, rect.top, 0, blitAlpha, blend_type,
                    Transparency);
#if defined _SKIA_SUPPORT_
  DebugVerifyDeviceIsPreMultiplied();
#endif
  return true;
}

std::unique_ptr<CFX_DIBitmap> CPDF_RenderStatus::GetBackdrop(
    const CPDF_PageObject* pObj,
    const FX_RECT& rect,
    int& left,
    int& top,
    bool bBackAlphaRequired) {
  FX_RECT bbox = rect;
  bbox.Intersect(m_pDevice->GetClipBox());
  left = bbox.left;
  top = bbox.top;
  CFX_Matrix deviceCTM = m_pDevice->GetCTM();
  FX_FLOAT scaleX = FXSYS_fabs(deviceCTM.a);
  FX_FLOAT scaleY = FXSYS_fabs(deviceCTM.d);
  int width = FXSYS_round(bbox.Width() * scaleX);
  int height = FXSYS_round(bbox.Height() * scaleY);
  auto pBackdrop = pdfium::MakeUnique<CFX_DIBitmap>();
  if (bBackAlphaRequired && !m_bDropObjects)
    pBackdrop->Create(width, height, FXDIB_Argb);
  else
    m_pDevice->CreateCompatibleBitmap(pBackdrop.get(), width, height);

  if (!pBackdrop->GetBuffer())
    return nullptr;

  bool bNeedDraw;
  if (pBackdrop->HasAlpha())
    bNeedDraw = !(m_pDevice->GetRenderCaps() & FXRC_ALPHA_OUTPUT);
  else
    bNeedDraw = !(m_pDevice->GetRenderCaps() & FXRC_GET_BITS);

  if (!bNeedDraw) {
    m_pDevice->GetDIBits(pBackdrop.get(), left, top);
    return pBackdrop;
  }

  CFX_Matrix FinalMatrix = m_DeviceMatrix;
  FinalMatrix.Translate(-left, -top);
  FinalMatrix.Scale(scaleX, scaleY);
  pBackdrop->Clear(pBackdrop->HasAlpha() ? 0 : 0xffffffff);
  CFX_FxgeDevice device;
  device.Attach(pBackdrop.get(), false, nullptr, false);
  m_pContext->Render(&device, pObj, &m_Options, &FinalMatrix);
  return pBackdrop;
}

CPDF_GraphicStates* CPDF_RenderStatus::CloneObjStates(
    const CPDF_GraphicStates* pSrcStates,
    bool bStroke) {
  if (!pSrcStates)
    return nullptr;

  CPDF_GraphicStates* pStates = new CPDF_GraphicStates;
  pStates->CopyStates(*pSrcStates);
  const CPDF_Color* pObjColor = bStroke
                                    ? pSrcStates->m_ColorState.GetStrokeColor()
                                    : pSrcStates->m_ColorState.GetFillColor();
  if (!pObjColor->IsNull()) {
    pStates->m_ColorState.SetFillRGB(
        bStroke ? pSrcStates->m_ColorState.GetStrokeRGB()
                : pSrcStates->m_ColorState.GetFillRGB());
    pStates->m_ColorState.SetStrokeRGB(pStates->m_ColorState.GetFillRGB());
  }
  return pStates;
}

#if defined _SKIA_SUPPORT_
void CPDF_RenderStatus::DebugVerifyDeviceIsPreMultiplied() const {
  m_pDevice->DebugVerifyBitmapIsPreMultiplied();
}
#endif

bool CPDF_RenderStatus::ProcessText(CPDF_TextObject* textobj,
                                    const CFX_Matrix* pObj2Device,
                                    CFX_PathData* pClippingPath) {
  if (textobj->m_CharCodes.empty())
    return true;

  const TextRenderingMode text_render_mode = textobj->m_TextState.GetTextMode();
  if (text_render_mode == TextRenderingMode::MODE_INVISIBLE)
    return true;

  CPDF_Font* pFont = textobj->m_TextState.GetFont();
  if (pFont->IsType3Font())
    return ProcessType3Text(textobj, pObj2Device);

  bool bFill = false;
  bool bStroke = false;
  bool bClip = false;
  if (pClippingPath) {
    bClip = true;
  } else {
    switch (text_render_mode) {
      case TextRenderingMode::MODE_FILL:
      case TextRenderingMode::MODE_FILL_CLIP:
        bFill = true;
        break;
      case TextRenderingMode::MODE_STROKE:
      case TextRenderingMode::MODE_STROKE_CLIP:
        if (pFont->GetFace())
          bStroke = true;
        else
          bFill = true;
        break;
      case TextRenderingMode::MODE_FILL_STROKE:
      case TextRenderingMode::MODE_FILL_STROKE_CLIP:
        bFill = true;
        if (pFont->GetFace())
          bStroke = true;
        break;
      case TextRenderingMode::MODE_INVISIBLE:
        // Already handled above, but the compiler is not smart enough to
        // realize it. Fall through.
        ASSERT(false);
      case TextRenderingMode::MODE_CLIP:
        return true;
    }
  }
  FX_ARGB stroke_argb = 0;
  FX_ARGB fill_argb = 0;
  bool bPattern = false;
  if (bStroke) {
    if (textobj->m_ColorState.GetStrokeColor()->IsPattern()) {
      bPattern = true;
    } else {
      stroke_argb = GetStrokeArgb(textobj);
    }
  }
  if (bFill) {
    if (textobj->m_ColorState.GetFillColor()->IsPattern()) {
      bPattern = true;
    } else {
      fill_argb = GetFillArgb(textobj);
    }
  }
  CFX_Matrix text_matrix = textobj->GetTextMatrix();
  if (!IsAvailableMatrix(text_matrix))
    return true;

  FX_FLOAT font_size = textobj->m_TextState.GetFontSize();
  if (bPattern) {
    DrawTextPathWithPattern(textobj, pObj2Device, pFont, font_size,
                            &text_matrix, bFill, bStroke);
    return true;
  }
  if (bClip || bStroke) {
    const CFX_Matrix* pDeviceMatrix = pObj2Device;
    CFX_Matrix device_matrix;
    if (bStroke) {
      const FX_FLOAT* pCTM = textobj->m_TextState.GetCTM();
      if (pCTM[0] != 1.0f || pCTM[3] != 1.0f) {
        CFX_Matrix ctm(pCTM[0], pCTM[1], pCTM[2], pCTM[3], 0, 0);
        text_matrix.ConcatInverse(ctm);
        device_matrix = ctm;
        device_matrix.Concat(*pObj2Device);
        pDeviceMatrix = &device_matrix;
      }
    }
    int flag = 0;
    if (bStroke && bFill) {
      flag |= FX_FILL_STROKE;
      flag |= FX_STROKE_TEXT_MODE;
    }
    if (textobj->m_GeneralState.GetStrokeAdjust())
      flag |= FX_STROKE_ADJUST;
    if (m_Options.m_Flags & RENDER_NOTEXTSMOOTH)
      flag |= FXFILL_NOPATHSMOOTH;
    return CPDF_TextRenderer::DrawTextPath(
        m_pDevice, textobj->m_CharCodes, textobj->m_CharPos, pFont, font_size,
        &text_matrix, pDeviceMatrix, textobj->m_GraphState.GetObject(),
        fill_argb, stroke_argb, pClippingPath, flag);
  }
  text_matrix.Concat(*pObj2Device);
  return CPDF_TextRenderer::DrawNormalText(m_pDevice, textobj->m_CharCodes,
                                           textobj->m_CharPos, pFont, font_size,
                                           &text_matrix, fill_argb, &m_Options);
}

CPDF_Type3Cache* CPDF_RenderStatus::GetCachedType3(CPDF_Type3Font* pFont) {
  if (!pFont->m_pDocument) {
    return nullptr;
  }
  pFont->m_pDocument->GetPageData()->GetFont(pFont->GetFontDict());
  return pFont->m_pDocument->GetRenderData()->GetCachedType3(pFont);
}

// TODO(npm): Font fallback for type 3 fonts? (Completely separate code!!)
bool CPDF_RenderStatus::ProcessType3Text(CPDF_TextObject* textobj,
                                         const CFX_Matrix* pObj2Device) {
  CPDF_Type3Font* pType3Font = textobj->m_TextState.GetFont()->AsType3Font();
  if (pdfium::ContainsValue(m_Type3FontCache, pType3Font))
    return true;

  CFX_Matrix dCTM = m_pDevice->GetCTM();
  FX_FLOAT sa = FXSYS_fabs(dCTM.a);
  FX_FLOAT sd = FXSYS_fabs(dCTM.d);
  CFX_Matrix text_matrix = textobj->GetTextMatrix();
  CFX_Matrix char_matrix = pType3Font->GetFontMatrix();
  FX_FLOAT font_size = textobj->m_TextState.GetFontSize();
  char_matrix.Scale(font_size, font_size);
  FX_ARGB fill_argb = GetFillArgb(textobj, true);
  int fill_alpha = FXARGB_A(fill_argb);
  int device_class = m_pDevice->GetDeviceClass();
  std::vector<FXTEXT_GLYPHPOS> glyphs;
  if (device_class == FXDC_DISPLAY)
    glyphs.resize(textobj->m_CharCodes.size());
  else if (fill_alpha < 255)
    return false;

  CPDF_RefType3Cache refTypeCache(pType3Font);
  for (int iChar = 0; iChar < pdfium::CollectionSize<int>(textobj->m_CharCodes);
       iChar++) {
    uint32_t charcode = textobj->m_CharCodes[iChar];
    if (charcode == static_cast<uint32_t>(-1))
      continue;

    CPDF_Type3Char* pType3Char = pType3Font->LoadChar(charcode);
    if (!pType3Char)
      continue;

    CFX_Matrix matrix = char_matrix;
    matrix.e += iChar ? textobj->m_CharPos[iChar - 1] : 0;
    matrix.Concat(text_matrix);
    matrix.Concat(*pObj2Device);
    if (!pType3Char->LoadBitmap(m_pContext)) {
      if (!glyphs.empty()) {
        for (int i = 0; i < iChar; i++) {
          const FXTEXT_GLYPHPOS& glyph = glyphs[i];
          if (!glyph.m_pGlyph)
            continue;

          m_pDevice->SetBitMask(&glyph.m_pGlyph->m_Bitmap,
                                glyph.m_Origin.x + glyph.m_pGlyph->m_Left,
                                glyph.m_Origin.y - glyph.m_pGlyph->m_Top,
                                fill_argb);
        }
        glyphs.clear();
      }
      CPDF_GraphicStates* pStates = CloneObjStates(textobj, false);
      CPDF_RenderOptions Options = m_Options;
      Options.m_Flags |= RENDER_FORCE_HALFTONE | RENDER_RECT_AA;
      Options.m_Flags &= ~RENDER_FORCE_DOWNSAMPLE;
      CPDF_Dictionary* pFormResource = nullptr;
      if (pType3Char->m_pForm && pType3Char->m_pForm->m_pFormDict) {
        pFormResource =
            pType3Char->m_pForm->m_pFormDict->GetDictFor("Resources");
      }
      if (fill_alpha == 255) {
        CPDF_RenderStatus status;
        status.Initialize(m_pContext, m_pDevice, nullptr, nullptr, this,
                          pStates, &Options,
                          pType3Char->m_pForm->m_Transparency, m_bDropObjects,
                          pFormResource, false, pType3Char, fill_argb);
        status.m_Type3FontCache = m_Type3FontCache;
        status.m_Type3FontCache.push_back(pType3Font);
        m_pDevice->SaveState();
        status.RenderObjectList(pType3Char->m_pForm.get(), &matrix);
        m_pDevice->RestoreState(false);
      } else {
        CFX_FloatRect rect_f = pType3Char->m_pForm->CalcBoundingBox();
        matrix.TransformRect(rect_f);

        FX_RECT rect = rect_f.GetOuterRect();
        CFX_FxgeDevice bitmap_device;
        if (!bitmap_device.Create((int)(rect.Width() * sa),
                                  (int)(rect.Height() * sd), FXDIB_Argb,
                                  nullptr)) {
          return true;
        }
        bitmap_device.GetBitmap()->Clear(0);
        CPDF_RenderStatus status;
        status.Initialize(m_pContext, &bitmap_device, nullptr, nullptr, this,
                          pStates, &Options,
                          pType3Char->m_pForm->m_Transparency, m_bDropObjects,
                          pFormResource, false, pType3Char, fill_argb);
        status.m_Type3FontCache = m_Type3FontCache;
        status.m_Type3FontCache.push_back(pType3Font);
        matrix.Translate(-rect.left, -rect.top);
        matrix.Scale(sa, sd);
        status.RenderObjectList(pType3Char->m_pForm.get(), &matrix);
        m_pDevice->SetDIBits(bitmap_device.GetBitmap(), rect.left, rect.top);
      }
      delete pStates;
    } else if (pType3Char->m_pBitmap) {
      if (device_class == FXDC_DISPLAY) {
        CPDF_Type3Cache* pCache = GetCachedType3(pType3Font);
        refTypeCache.m_dwCount++;
        CFX_GlyphBitmap* pBitmap = pCache->LoadGlyph(charcode, &matrix, sa, sd);
        if (!pBitmap)
          continue;

        CFX_Point origin(FXSYS_round(matrix.e), FXSYS_round(matrix.f));
        if (glyphs.empty()) {
          m_pDevice->SetBitMask(&pBitmap->m_Bitmap, origin.x + pBitmap->m_Left,
                                origin.y - pBitmap->m_Top, fill_argb);
        } else {
          glyphs[iChar].m_pGlyph = pBitmap;
          glyphs[iChar].m_Origin = origin;
        }
      } else {
        CFX_Matrix image_matrix = pType3Char->m_ImageMatrix;
        image_matrix.Concat(matrix);
        CPDF_ImageRenderer renderer;
        if (renderer.Start(this, pType3Char->m_pBitmap.get(), fill_argb, 255,
                           &image_matrix, 0, false, FXDIB_BLEND_NORMAL)) {
          renderer.Continue(nullptr);
        }
        if (!renderer.GetResult())
          return false;
      }
    }
  }

  if (glyphs.empty())
    return true;

  FX_RECT rect = FXGE_GetGlyphsBBox(glyphs, 0, sa, sd);
  CFX_DIBitmap bitmap;
  if (!bitmap.Create(static_cast<int>(rect.Width() * sa),
                     static_cast<int>(rect.Height() * sd), FXDIB_8bppMask)) {
    return true;
  }
  bitmap.Clear(0);
  for (const FXTEXT_GLYPHPOS& glyph : glyphs) {
    if (!glyph.m_pGlyph)
      continue;

    pdfium::base::CheckedNumeric<int> left = glyph.m_Origin.x;
    left += glyph.m_pGlyph->m_Left;
    left -= rect.left;
    left *= sa;
    if (!left.IsValid())
      continue;

    pdfium::base::CheckedNumeric<int> top = glyph.m_Origin.y;
    top -= glyph.m_pGlyph->m_Top;
    top -= rect.top;
    top *= sd;
    if (!top.IsValid())
      continue;

    bitmap.CompositeMask(left.ValueOrDie(), top.ValueOrDie(),
                         glyph.m_pGlyph->m_Bitmap.GetWidth(),
                         glyph.m_pGlyph->m_Bitmap.GetHeight(),
                         &glyph.m_pGlyph->m_Bitmap, fill_argb, 0, 0,
                         FXDIB_BLEND_NORMAL, nullptr, false, 0, nullptr);
  }
  m_pDevice->SetBitMask(&bitmap, rect.left, rect.top, fill_argb);
  return true;
}

void CPDF_RenderStatus::DrawTextPathWithPattern(const CPDF_TextObject* textobj,
                                                const CFX_Matrix* pObj2Device,
                                                CPDF_Font* pFont,
                                                FX_FLOAT font_size,
                                                const CFX_Matrix* pTextMatrix,
                                                bool bFill,
                                                bool bStroke) {
  if (!bStroke) {
    CPDF_PathObject path;
    std::vector<std::unique_ptr<CPDF_TextObject>> pCopy;
    pCopy.push_back(std::unique_ptr<CPDF_TextObject>(textobj->Clone()));
    path.m_bStroke = false;
    path.m_FillType = FXFILL_WINDING;
    path.m_ClipPath.AppendTexts(&pCopy);
    path.m_ColorState = textobj->m_ColorState;
    path.m_Path.AppendRect(textobj->m_Left, textobj->m_Bottom, textobj->m_Right,
                           textobj->m_Top);
    path.m_Left = textobj->m_Left;
    path.m_Bottom = textobj->m_Bottom;
    path.m_Right = textobj->m_Right;
    path.m_Top = textobj->m_Top;
    RenderSingleObject(&path, pObj2Device);
    return;
  }
  CPDF_CharPosList CharPosList;
  CharPosList.Load(textobj->m_CharCodes, textobj->m_CharPos, pFont, font_size);
  for (uint32_t i = 0; i < CharPosList.m_nChars; i++) {
    FXTEXT_CHARPOS& charpos = CharPosList.m_pCharPos[i];
    auto font =
        charpos.m_FallbackFontPosition == -1
            ? &pFont->m_Font
            : pFont->m_FontFallbacks[charpos.m_FallbackFontPosition].get();
    const CFX_PathData* pPath =
        font->LoadGlyphPath(charpos.m_GlyphIndex, charpos.m_FontCharWidth);
    if (!pPath)
      continue;

    CPDF_PathObject path;
    path.m_GraphState = textobj->m_GraphState;
    path.m_ColorState = textobj->m_ColorState;

    CFX_Matrix matrix;
    if (charpos.m_bGlyphAdjust) {
      matrix = CFX_Matrix(charpos.m_AdjustMatrix[0], charpos.m_AdjustMatrix[1],
                          charpos.m_AdjustMatrix[2], charpos.m_AdjustMatrix[3],
                          0, 0);
    }
    matrix.Concat(CFX_Matrix(font_size, 0, 0, font_size, charpos.m_Origin.x,
                             charpos.m_Origin.y));
    path.m_Path.Append(pPath, &matrix);
    path.m_Matrix = *pTextMatrix;
    path.m_bStroke = bStroke;
    path.m_FillType = bFill ? FXFILL_WINDING : 0;
    path.CalcBoundingBox();
    ProcessPath(&path, pObj2Device);
  }
}

void CPDF_RenderStatus::DrawShading(CPDF_ShadingPattern* pPattern,
                                    CFX_Matrix* pMatrix,
                                    FX_RECT& clip_rect,
                                    int alpha,
                                    bool bAlphaMode) {
  const auto& funcs = pPattern->GetFuncs();
  CPDF_Dictionary* pDict = pPattern->GetShadingObject()->GetDict();
  CPDF_ColorSpace* pColorSpace = pPattern->GetCS();
  if (!pColorSpace)
    return;

  FX_ARGB background = 0;
  if (!pPattern->IsShadingObject() && pDict->KeyExist("Background")) {
    CPDF_Array* pBackColor = pDict->GetArrayFor("Background");
    if (pBackColor &&
        pBackColor->GetCount() >= pColorSpace->CountComponents()) {
      CFX_FixedBufGrow<FX_FLOAT, 16> comps(pColorSpace->CountComponents());
      for (uint32_t i = 0; i < pColorSpace->CountComponents(); i++)
        comps[i] = pBackColor->GetNumberAt(i);
      FX_FLOAT R = 0.0f, G = 0.0f, B = 0.0f;
      pColorSpace->GetRGB(comps, R, G, B);
      background = ArgbEncode(255, (int32_t)(R * 255), (int32_t)(G * 255),
                              (int32_t)(B * 255));
    }
  }
  if (pDict->KeyExist("BBox")) {
    CFX_FloatRect rect = pDict->GetRectFor("BBox");
    pMatrix->TransformRect(rect);
    clip_rect.Intersect(rect.GetOuterRect());
  }
  if (m_pDevice->GetDeviceCaps(FXDC_RENDER_CAPS) & FXRC_SHADING &&
      m_pDevice->GetDeviceDriver()->DrawShading(pPattern, pMatrix, clip_rect,
                                                alpha, bAlphaMode)) {
    return;
  }
  CPDF_DeviceBuffer buffer;
  buffer.Initialize(m_pContext, m_pDevice, &clip_rect, m_pCurObj, 150);
  CFX_Matrix FinalMatrix = *pMatrix;
  FinalMatrix.Concat(*buffer.GetMatrix());
  CFX_DIBitmap* pBitmap = buffer.GetBitmap();
  if (!pBitmap->GetBuffer())
    return;

  pBitmap->Clear(background);
  int fill_mode = m_Options.m_Flags;
  switch (pPattern->GetShadingType()) {
    case kInvalidShading:
    case kMaxShading:
      return;
    case kFunctionBasedShading:
      DrawFuncShading(pBitmap, &FinalMatrix, pDict, funcs, pColorSpace, alpha);
      break;
    case kAxialShading:
      DrawAxialShading(pBitmap, &FinalMatrix, pDict, funcs, pColorSpace, alpha);
      break;
    case kRadialShading:
      DrawRadialShading(pBitmap, &FinalMatrix, pDict, funcs, pColorSpace,
                        alpha);
      break;
    case kFreeFormGouraudTriangleMeshShading: {
      // The shading object can be a stream or a dictionary. We do not handle
      // the case of dictionary at the moment.
      if (CPDF_Stream* pStream = ToStream(pPattern->GetShadingObject())) {
        DrawFreeGouraudShading(pBitmap, &FinalMatrix, pStream, funcs,
                               pColorSpace, alpha);
      }
    } break;
    case kLatticeFormGouraudTriangleMeshShading: {
      // The shading object can be a stream or a dictionary. We do not handle
      // the case of dictionary at the moment.
      if (CPDF_Stream* pStream = ToStream(pPattern->GetShadingObject())) {
        DrawLatticeGouraudShading(pBitmap, &FinalMatrix, pStream, funcs,
                                  pColorSpace, alpha);
      }
    } break;
    case kCoonsPatchMeshShading:
    case kTensorProductPatchMeshShading: {
      // The shading object can be a stream or a dictionary. We do not handle
      // the case of dictionary at the moment.
      if (CPDF_Stream* pStream = ToStream(pPattern->GetShadingObject())) {
        DrawCoonPatchMeshes(pPattern->GetShadingType(), pBitmap, &FinalMatrix,
                            pStream, funcs, pColorSpace, fill_mode, alpha);
      }
    } break;
  }
  if (bAlphaMode)
    pBitmap->LoadChannel(FXDIB_Red, pBitmap, FXDIB_Alpha);

  if (m_Options.m_ColorMode == RENDER_COLOR_GRAY)
    pBitmap->ConvertColorScale(m_Options.m_ForeColor, m_Options.m_BackColor);
  buffer.OutputToDevice();
}

void CPDF_RenderStatus::DrawShadingPattern(CPDF_ShadingPattern* pattern,
                                           const CPDF_PageObject* pPageObj,
                                           const CFX_Matrix* pObj2Device,
                                           bool bStroke) {
  if (!pattern->Load())
    return;

  m_pDevice->SaveState();
  if (pPageObj->IsPath()) {
    if (!SelectClipPath(pPageObj->AsPath(), pObj2Device, bStroke)) {
      m_pDevice->RestoreState(false);
      return;
    }
  } else if (pPageObj->IsImage()) {
    m_pDevice->SetClip_Rect(pPageObj->GetBBox(pObj2Device));
  } else {
    return;
  }
  FX_RECT rect;
  if (GetObjectClippedRect(pPageObj, pObj2Device, false, rect)) {
    m_pDevice->RestoreState(false);
    return;
  }
  CFX_Matrix matrix = *pattern->pattern_to_form();
  matrix.Concat(*pObj2Device);
  GetScaledMatrix(matrix);
  int alpha =
      FXSYS_round(255 * (bStroke ? pPageObj->m_GeneralState.GetStrokeAlpha()
                                 : pPageObj->m_GeneralState.GetFillAlpha()));
  DrawShading(pattern, &matrix, rect, alpha,
              m_Options.m_ColorMode == RENDER_COLOR_ALPHA);
  m_pDevice->RestoreState(false);
}

void CPDF_RenderStatus::ProcessShading(const CPDF_ShadingObject* pShadingObj,
                                       const CFX_Matrix* pObj2Device) {
  FX_RECT rect = pShadingObj->GetBBox(pObj2Device);
  FX_RECT clip_box = m_pDevice->GetClipBox();
  rect.Intersect(clip_box);
  if (rect.IsEmpty())
    return;

  CFX_Matrix matrix = pShadingObj->m_Matrix;
  matrix.Concat(*pObj2Device);
  DrawShading(pShadingObj->m_pShading, &matrix, rect,
              FXSYS_round(255 * pShadingObj->m_GeneralState.GetFillAlpha()),
              m_Options.m_ColorMode == RENDER_COLOR_ALPHA);
}

void CPDF_RenderStatus::DrawTilingPattern(CPDF_TilingPattern* pPattern,
                                          CPDF_PageObject* pPageObj,
                                          const CFX_Matrix* pObj2Device,
                                          bool bStroke) {
  if (!pPattern->Load()) {
    return;
  }
  m_pDevice->SaveState();
  if (pPageObj->IsPath()) {
    if (!SelectClipPath(pPageObj->AsPath(), pObj2Device, bStroke)) {
      m_pDevice->RestoreState(false);
      return;
    }
  } else if (pPageObj->IsImage()) {
    m_pDevice->SetClip_Rect(pPageObj->GetBBox(pObj2Device));
  } else {
    return;
  }
  FX_RECT clip_box = m_pDevice->GetClipBox();
  if (clip_box.IsEmpty()) {
    m_pDevice->RestoreState(false);
    return;
  }
  CFX_Matrix dCTM = m_pDevice->GetCTM();
  FX_FLOAT sa = FXSYS_fabs(dCTM.a);
  FX_FLOAT sd = FXSYS_fabs(dCTM.d);
  clip_box.right = clip_box.left + (int32_t)FXSYS_ceil(clip_box.Width() * sa);
  clip_box.bottom = clip_box.top + (int32_t)FXSYS_ceil(clip_box.Height() * sd);
  CFX_Matrix mtPattern2Device = *pPattern->pattern_to_form();
  mtPattern2Device.Concat(*pObj2Device);
  GetScaledMatrix(mtPattern2Device);
  bool bAligned = false;
  if (pPattern->bbox().left == 0 && pPattern->bbox().bottom == 0 &&
      pPattern->bbox().right == pPattern->x_step() &&
      pPattern->bbox().top == pPattern->y_step() &&
      (mtPattern2Device.IsScaled() || mtPattern2Device.Is90Rotated())) {
    bAligned = true;
  }
  CFX_FloatRect cell_bbox = pPattern->bbox();
  mtPattern2Device.TransformRect(cell_bbox);
  int width = (int)FXSYS_ceil(cell_bbox.Width());
  int height = (int)FXSYS_ceil(cell_bbox.Height());
  if (width == 0) {
    width = 1;
  }
  if (height == 0) {
    height = 1;
  }
  int min_col, max_col, min_row, max_row;
  CFX_Matrix mtDevice2Pattern;
  mtDevice2Pattern.SetReverse(mtPattern2Device);

  CFX_FloatRect clip_box_p(clip_box);
  mtDevice2Pattern.TransformRect(clip_box_p);

  min_col = (int)FXSYS_ceil((clip_box_p.left - pPattern->bbox().right) /
                            pPattern->x_step());
  max_col = (int)FXSYS_floor((clip_box_p.right - pPattern->bbox().left) /
                             pPattern->x_step());
  min_row = (int)FXSYS_ceil((clip_box_p.bottom - pPattern->bbox().top) /
                            pPattern->y_step());
  max_row = (int)FXSYS_floor((clip_box_p.top - pPattern->bbox().bottom) /
                             pPattern->y_step());

  if (width > clip_box.Width() || height > clip_box.Height() ||
      width * height > clip_box.Width() * clip_box.Height()) {
    CPDF_GraphicStates* pStates = nullptr;
    if (!pPattern->colored())
      pStates = CloneObjStates(pPageObj, bStroke);

    CPDF_Dictionary* pFormResource = nullptr;
    if (pPattern->form()->m_pFormDict)
      pFormResource = pPattern->form()->m_pFormDict->GetDictFor("Resources");

    for (int col = min_col; col <= max_col; col++)
      for (int row = min_row; row <= max_row; row++) {
        CFX_PointF original = mtPattern2Device.Transform(
            CFX_PointF(col * pPattern->x_step(), row * pPattern->y_step()));
        CFX_Matrix matrix = *pObj2Device;
        matrix.Translate(original.x - mtPattern2Device.e,
                         original.y - mtPattern2Device.f);
        m_pDevice->SaveState();
        CPDF_RenderStatus status;
        status.Initialize(m_pContext, m_pDevice, nullptr, nullptr, this,
                          pStates, &m_Options, pPattern->form()->m_Transparency,
                          m_bDropObjects, pFormResource);
        status.RenderObjectList(pPattern->form(), &matrix);
        m_pDevice->RestoreState(false);
      }
    m_pDevice->RestoreState(false);
    delete pStates;
    return;
  }
  if (bAligned) {
    int orig_x = FXSYS_round(mtPattern2Device.e);
    int orig_y = FXSYS_round(mtPattern2Device.f);
    min_col = (clip_box.left - orig_x) / width;
    if (clip_box.left < orig_x) {
      min_col--;
    }
    max_col = (clip_box.right - orig_x) / width;
    if (clip_box.right <= orig_x) {
      max_col--;
    }
    min_row = (clip_box.top - orig_y) / height;
    if (clip_box.top < orig_y) {
      min_row--;
    }
    max_row = (clip_box.bottom - orig_y) / height;
    if (clip_box.bottom <= orig_y) {
      max_row--;
    }
  }
  FX_FLOAT left_offset = cell_bbox.left - mtPattern2Device.e;
  FX_FLOAT top_offset = cell_bbox.bottom - mtPattern2Device.f;
  std::unique_ptr<CFX_DIBitmap> pPatternBitmap;
  if (width * height < 16) {
    std::unique_ptr<CFX_DIBitmap> pEnlargedBitmap =
        DrawPatternBitmap(m_pContext->GetDocument(), m_pContext->GetPageCache(),
                          pPattern, pObj2Device, 8, 8, m_Options.m_Flags);
    pPatternBitmap = pEnlargedBitmap->StretchTo(width, height);
  } else {
    pPatternBitmap = DrawPatternBitmap(
        m_pContext->GetDocument(), m_pContext->GetPageCache(), pPattern,
        pObj2Device, width, height, m_Options.m_Flags);
  }
  if (!pPatternBitmap) {
    m_pDevice->RestoreState(false);
    return;
  }
  if (m_Options.m_ColorMode == RENDER_COLOR_GRAY) {
    pPatternBitmap->ConvertColorScale(m_Options.m_ForeColor,
                                      m_Options.m_BackColor);
  }
  FX_ARGB fill_argb = GetFillArgb(pPageObj);
  int clip_width = clip_box.right - clip_box.left;
  int clip_height = clip_box.bottom - clip_box.top;
  CFX_DIBitmap screen;
  if (!screen.Create(clip_width, clip_height, FXDIB_Argb)) {
    return;
  }
  screen.Clear(0);
  uint32_t* src_buf = (uint32_t*)pPatternBitmap->GetBuffer();
  for (int col = min_col; col <= max_col; col++) {
    for (int row = min_row; row <= max_row; row++) {
      int start_x, start_y;
      if (bAligned) {
        start_x = FXSYS_round(mtPattern2Device.e) + col * width - clip_box.left;
        start_y = FXSYS_round(mtPattern2Device.f) + row * height - clip_box.top;
      } else {
        CFX_PointF original = mtPattern2Device.Transform(
            CFX_PointF(col * pPattern->x_step(), row * pPattern->y_step()));

        pdfium::base::CheckedNumeric<int> safeStartX =
            FXSYS_round(original.x + left_offset);
        pdfium::base::CheckedNumeric<int> safeStartY =
            FXSYS_round(original.y + top_offset);

        safeStartX -= clip_box.left;
        safeStartY -= clip_box.top;
        if (!safeStartX.IsValid() || !safeStartY.IsValid())
          return;

        start_x = safeStartX.ValueOrDie();
        start_y = safeStartY.ValueOrDie();
      }
      if (width == 1 && height == 1) {
        if (start_x < 0 || start_x >= clip_box.Width() || start_y < 0 ||
            start_y >= clip_box.Height()) {
          continue;
        }
        uint32_t* dest_buf =
            (uint32_t*)(screen.GetBuffer() + screen.GetPitch() * start_y +
                        start_x * 4);
        if (pPattern->colored())
          *dest_buf = *src_buf;
        else
          *dest_buf = (*(uint8_t*)src_buf << 24) | (fill_argb & 0xffffff);
      } else {
        if (pPattern->colored()) {
          screen.CompositeBitmap(start_x, start_y, width, height,
                                 pPatternBitmap.get(), 0, 0);
        } else {
          screen.CompositeMask(start_x, start_y, width, height,
                               pPatternBitmap.get(), fill_argb, 0, 0);
        }
      }
    }
  }
  CompositeDIBitmap(&screen, clip_box.left, clip_box.top, 0, 255,
                    FXDIB_BLEND_NORMAL, false);
  m_pDevice->RestoreState(false);
}

void CPDF_RenderStatus::DrawPathWithPattern(CPDF_PathObject* pPathObj,
                                            const CFX_Matrix* pObj2Device,
                                            const CPDF_Color* pColor,
                                            bool bStroke) {
  CPDF_Pattern* pattern = pColor->GetPattern();
  if (!pattern)
    return;

  if (CPDF_TilingPattern* pTilingPattern = pattern->AsTilingPattern())
    DrawTilingPattern(pTilingPattern, pPathObj, pObj2Device, bStroke);
  else if (CPDF_ShadingPattern* pShadingPattern = pattern->AsShadingPattern())
    DrawShadingPattern(pShadingPattern, pPathObj, pObj2Device, bStroke);
}

void CPDF_RenderStatus::ProcessPathPattern(CPDF_PathObject* pPathObj,
                                           const CFX_Matrix* pObj2Device,
                                           int& filltype,
                                           bool& bStroke) {
  if (filltype) {
    const CPDF_Color& FillColor = *pPathObj->m_ColorState.GetFillColor();
    if (FillColor.IsPattern()) {
      DrawPathWithPattern(pPathObj, pObj2Device, &FillColor, false);
      filltype = 0;
    }
  }
  if (bStroke) {
    const CPDF_Color& StrokeColor = *pPathObj->m_ColorState.GetStrokeColor();
    if (StrokeColor.IsPattern()) {
      DrawPathWithPattern(pPathObj, pObj2Device, &StrokeColor, true);
      bStroke = false;
    }
  }
}

bool CPDF_RenderStatus::ProcessImage(CPDF_ImageObject* pImageObj,
                                     const CFX_Matrix* pObj2Device) {
  CPDF_ImageRenderer render;
  if (render.Start(this, pImageObj, pObj2Device, m_bStdCS, m_curBlend))
    render.Continue(nullptr);
  return render.GetResult();
}

void CPDF_RenderStatus::CompositeDIBitmap(CFX_DIBitmap* pDIBitmap,
                                          int left,
                                          int top,
                                          FX_ARGB mask_argb,
                                          int bitmap_alpha,
                                          int blend_mode,
                                          int Transparency) {
  if (!pDIBitmap) {
    return;
  }
  if (blend_mode == FXDIB_BLEND_NORMAL) {
    if (!pDIBitmap->IsAlphaMask()) {
      if (bitmap_alpha < 255) {
#ifdef _SKIA_SUPPORT_
        void* dummy;
        CFX_Matrix m(pDIBitmap->GetWidth(), 0, 0, -pDIBitmap->GetHeight(), left,
                     top + pDIBitmap->GetHeight());
        m_pDevice->StartDIBits(pDIBitmap, bitmap_alpha, 0, &m, 0, dummy);
        return;
#else
        pDIBitmap->MultiplyAlpha(bitmap_alpha);
#endif
      }
#ifdef _SKIA_SUPPORT_
      CFX_SkiaDeviceDriver::PreMultiply(pDIBitmap);
#endif
      if (m_pDevice->SetDIBits(pDIBitmap, left, top)) {
        return;
      }
    } else {
      uint32_t fill_argb = m_Options.TranslateColor(mask_argb);
      if (bitmap_alpha < 255) {
        ((uint8_t*)&fill_argb)[3] =
            ((uint8_t*)&fill_argb)[3] * bitmap_alpha / 255;
      }
      if (m_pDevice->SetBitMask(pDIBitmap, left, top, fill_argb)) {
        return;
      }
    }
  }
  bool bIsolated = !!(Transparency & PDFTRANS_ISOLATED);
  bool bGroup = !!(Transparency & PDFTRANS_GROUP);
  bool bBackAlphaRequired = blend_mode && bIsolated && !m_bDropObjects;
  bool bGetBackGround =
      ((m_pDevice->GetRenderCaps() & FXRC_ALPHA_OUTPUT)) ||
      (!(m_pDevice->GetRenderCaps() & FXRC_ALPHA_OUTPUT) &&
       (m_pDevice->GetRenderCaps() & FXRC_GET_BITS) && !bBackAlphaRequired);
  if (bGetBackGround) {
    if (bIsolated || !bGroup) {
      if (pDIBitmap->IsAlphaMask()) {
        return;
      }
      m_pDevice->SetDIBitsWithBlend(pDIBitmap, left, top, blend_mode);
    } else {
      FX_RECT rect(left, top, left + pDIBitmap->GetWidth(),
                   top + pDIBitmap->GetHeight());
      rect.Intersect(m_pDevice->GetClipBox());
      CFX_MaybeOwned<CFX_DIBitmap> pClone;
      if (m_pDevice->GetBackDrop() && m_pDevice->GetBitmap()) {
        pClone = m_pDevice->GetBackDrop()->Clone(&rect);
        CFX_DIBitmap* pForeBitmap = m_pDevice->GetBitmap();
        pClone->CompositeBitmap(0, 0, pClone->GetWidth(), pClone->GetHeight(),
                                pForeBitmap, rect.left, rect.top);
        left = left >= 0 ? 0 : left;
        top = top >= 0 ? 0 : top;
        if (!pDIBitmap->IsAlphaMask())
          pClone->CompositeBitmap(0, 0, pClone->GetWidth(), pClone->GetHeight(),
                                  pDIBitmap, left, top, blend_mode);
        else
          pClone->CompositeMask(0, 0, pClone->GetWidth(), pClone->GetHeight(),
                                pDIBitmap, mask_argb, left, top, blend_mode);
      } else {
        pClone = pDIBitmap;
      }
      if (m_pDevice->GetBackDrop()) {
        m_pDevice->SetDIBits(pClone.Get(), rect.left, rect.top);
      } else {
        if (pDIBitmap->IsAlphaMask())
          return;
        m_pDevice->SetDIBitsWithBlend(pDIBitmap, rect.left, rect.top,
                                      blend_mode);
      }
    }
    return;
  }
  int back_left, back_top;
  FX_RECT rect(left, top, left + pDIBitmap->GetWidth(),
               top + pDIBitmap->GetHeight());
  std::unique_ptr<CFX_DIBitmap> pBackdrop =
      GetBackdrop(m_pCurObj, rect, back_left, back_top,
                  blend_mode > FXDIB_BLEND_NORMAL && bIsolated);
  if (!pBackdrop)
    return;

  if (!pDIBitmap->IsAlphaMask()) {
    pBackdrop->CompositeBitmap(left - back_left, top - back_top,
                               pDIBitmap->GetWidth(), pDIBitmap->GetHeight(),
                               pDIBitmap, 0, 0, blend_mode);
  } else {
    pBackdrop->CompositeMask(left - back_left, top - back_top,
                             pDIBitmap->GetWidth(), pDIBitmap->GetHeight(),
                             pDIBitmap, mask_argb, 0, 0, blend_mode);
  }

  auto pBackdrop1 = pdfium::MakeUnique<CFX_DIBitmap>();
  pBackdrop1->Create(pBackdrop->GetWidth(), pBackdrop->GetHeight(),
                     FXDIB_Rgb32);
  pBackdrop1->Clear((uint32_t)-1);
  pBackdrop1->CompositeBitmap(0, 0, pBackdrop->GetWidth(),
                              pBackdrop->GetHeight(), pBackdrop.get(), 0, 0);
  pBackdrop = std::move(pBackdrop1);
  m_pDevice->SetDIBits(pBackdrop.get(), back_left, back_top);
}

std::unique_ptr<CFX_DIBitmap> CPDF_RenderStatus::LoadSMask(
    CPDF_Dictionary* pSMaskDict,
    FX_RECT* pClipRect,
    const CFX_Matrix* pMatrix) {
  if (!pSMaskDict)
    return nullptr;

  CPDF_Stream* pGroup = pSMaskDict->GetStreamFor("G");
  if (!pGroup)
    return nullptr;

  std::unique_ptr<CPDF_Function> pFunc;
  CPDF_Object* pFuncObj = pSMaskDict->GetDirectObjectFor("TR");
  if (pFuncObj && (pFuncObj->IsDictionary() || pFuncObj->IsStream()))
    pFunc = CPDF_Function::Load(pFuncObj);

  CFX_Matrix matrix = *pMatrix;
  matrix.Translate(-pClipRect->left, -pClipRect->top);

  CPDF_Form form(m_pContext->GetDocument(), m_pContext->GetPageResources(),
                 pGroup);
  form.ParseContent(nullptr, nullptr, nullptr);

  CFX_FxgeDevice bitmap_device;
  bool bLuminosity = pSMaskDict->GetStringFor("S") != "Alpha";
  int width = pClipRect->right - pClipRect->left;
  int height = pClipRect->bottom - pClipRect->top;
  FXDIB_Format format;
#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_ || defined _SKIA_SUPPORT_ || \
    defined _SKIA_SUPPORT_PATHS_
  format = bLuminosity ? FXDIB_Rgb32 : FXDIB_8bppMask;
#else
  format = bLuminosity ? FXDIB_Rgb : FXDIB_8bppMask;
#endif
  if (!bitmap_device.Create(width, height, format, nullptr))
    return nullptr;

  CFX_DIBitmap& bitmap = *bitmap_device.GetBitmap();
  int color_space_family = 0;
  if (bLuminosity) {
    CPDF_Array* pBC = pSMaskDict->GetArrayFor("BC");
    FX_ARGB back_color = 0xff000000;
    if (pBC) {
      CPDF_Object* pCSObj = nullptr;
      CPDF_Dictionary* pDict = pGroup->GetDict();
      if (pDict && pDict->GetDictFor("Group")) {
        pCSObj = pDict->GetDictFor("Group")->GetDirectObjectFor("CS");
      }
      const CPDF_ColorSpace* pCS =
          m_pContext->GetDocument()->LoadColorSpace(pCSObj);
      if (pCS) {
        // Store Color Space Family to use in CPDF_RenderStatus::Initialize.
        color_space_family = pCS->GetFamily();

        FX_FLOAT R, G, B;
        uint32_t comps = 8;
        if (pCS->CountComponents() > comps) {
          comps = pCS->CountComponents();
        }
        CFX_FixedBufGrow<FX_FLOAT, 8> float_array(comps);
        FX_FLOAT* pFloats = float_array;
        FX_SAFE_UINT32 num_floats = comps;
        num_floats *= sizeof(FX_FLOAT);
        if (!num_floats.IsValid()) {
          return nullptr;
        }
        FXSYS_memset(pFloats, 0, num_floats.ValueOrDie());
        size_t count = pBC->GetCount() > 8 ? 8 : pBC->GetCount();
        for (size_t i = 0; i < count; i++) {
          pFloats[i] = pBC->GetNumberAt(i);
        }
        pCS->GetRGB(pFloats, R, G, B);
        back_color = 0xff000000 | ((int32_t)(R * 255) << 16) |
                     ((int32_t)(G * 255) << 8) | (int32_t)(B * 255);
        m_pContext->GetDocument()->GetPageData()->ReleaseColorSpace(pCSObj);
      }
    }
    bitmap.Clear(back_color);
  } else {
    bitmap.Clear(0);
  }
  CPDF_Dictionary* pFormResource = nullptr;
  if (form.m_pFormDict) {
    pFormResource = form.m_pFormDict->GetDictFor("Resources");
  }
  CPDF_RenderOptions options;
  options.m_ColorMode = bLuminosity ? RENDER_COLOR_NORMAL : RENDER_COLOR_ALPHA;
  CPDF_RenderStatus status;
  status.Initialize(m_pContext, &bitmap_device, nullptr, nullptr, nullptr,
                    nullptr, &options, 0, m_bDropObjects, pFormResource, true,
                    nullptr, 0, color_space_family, bLuminosity);
  status.RenderObjectList(&form, &matrix);

  auto pMask = pdfium::MakeUnique<CFX_DIBitmap>();
  if (!pMask->Create(width, height, FXDIB_8bppMask))
    return nullptr;

  uint8_t* dest_buf = pMask->GetBuffer();
  int dest_pitch = pMask->GetPitch();
  uint8_t* src_buf = bitmap.GetBuffer();
  int src_pitch = bitmap.GetPitch();
  std::vector<uint8_t> transfers(256);
  if (pFunc) {
    CFX_FixedBufGrow<FX_FLOAT, 16> results(pFunc->CountOutputs());
    for (int i = 0; i < 256; i++) {
      FX_FLOAT input = (FX_FLOAT)i / 255.0f;
      int nresult;
      pFunc->Call(&input, 1, results, nresult);
      transfers[i] = FXSYS_round(results[0] * 255);
    }
  } else {
    for (int i = 0; i < 256; i++) {
      transfers[i] = i;
    }
  }
  if (bLuminosity) {
    int Bpp = bitmap.GetBPP() / 8;
    for (int row = 0; row < height; row++) {
      uint8_t* dest_pos = dest_buf + row * dest_pitch;
      uint8_t* src_pos = src_buf + row * src_pitch;
      for (int col = 0; col < width; col++) {
        *dest_pos++ = transfers[FXRGB2GRAY(src_pos[2], src_pos[1], *src_pos)];
        src_pos += Bpp;
      }
    }
  } else if (pFunc) {
    int size = dest_pitch * height;
    for (int i = 0; i < size; i++) {
      dest_buf[i] = transfers[src_buf[i]];
    }
  } else {
    FXSYS_memcpy(dest_buf, src_buf, dest_pitch * height);
  }
  return pMask;
}
