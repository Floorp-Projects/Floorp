// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxge/fx_font.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "core/fpdfapi/font/cpdf_font.h"
#include "core/fxge/cfx_facecache.h"
#include "core/fxge/cfx_fontcache.h"
#include "core/fxge/cfx_fontmgr.h"
#include "core/fxge/cfx_gemodule.h"
#include "core/fxge/cfx_pathdata.h"
#include "core/fxge/cfx_substfont.h"
#include "core/fxge/fx_freetype.h"
#include "core/fxge/ge/fx_text_int.h"
#include "third_party/base/ptr_util.h"

#define EM_ADJUST(em, a) (em == 0 ? (a) : (a)*1000 / em)

namespace {

typedef struct {
  CFX_PathData* m_pPath;
  int m_CurX;
  int m_CurY;
  FX_FLOAT m_CoordUnit;
} OUTLINE_PARAMS;

#ifdef PDF_ENABLE_XFA

unsigned long FTStreamRead(FXFT_Stream stream,
                           unsigned long offset,
                           unsigned char* buffer,
                           unsigned long count) {
  if (count == 0)
    return 0;

  IFX_SeekableReadStream* pFile =
      static_cast<IFX_SeekableReadStream*>(stream->descriptor.pointer);
  if (!pFile)
    return 0;

  if (!pFile->ReadBlock(buffer, offset, count))
    return 0;

  return count;
}

void FTStreamClose(FXFT_Stream stream) {}

bool LoadFileImp(FXFT_Library library,
                 FXFT_Face* Face,
                 const CFX_RetainPtr<IFX_SeekableReadStream>& pFile,
                 int32_t faceIndex,
                 std::unique_ptr<FXFT_StreamRec>* stream) {
  auto stream1 = pdfium::MakeUnique<FXFT_StreamRec>();
  stream1->base = nullptr;
  stream1->size = static_cast<unsigned long>(pFile->GetSize());
  stream1->pos = 0;
  stream1->descriptor.pointer = static_cast<void*>(pFile.Get());
  stream1->close = FTStreamClose;
  stream1->read = FTStreamRead;
  FXFT_Open_Args args;
  args.flags = FT_OPEN_STREAM;
  args.stream = stream1.get();
  if (FXFT_Open_Face(library, &args, faceIndex, Face))
    return false;
  if (stream)
    *stream = std::move(stream1);
  return true;
}
#endif  // PDF_ENABLE_XFA

FXFT_Face FT_LoadFont(const uint8_t* pData, int size) {
  return CFX_GEModule::Get()->GetFontMgr()->GetFixedFace(pData, size, 0);
}

void Outline_CheckEmptyContour(OUTLINE_PARAMS* param) {
  std::vector<FX_PATHPOINT>& points = param->m_pPath->GetPoints();
  size_t size = points.size();

  if (size >= 2 && points[size - 2].IsTypeAndOpen(FXPT_TYPE::MoveTo) &&
      points[size - 2].m_Point == points[size - 1].m_Point) {
    size -= 2;
  }
  if (size >= 4 && points[size - 4].IsTypeAndOpen(FXPT_TYPE::MoveTo) &&
      points[size - 3].IsTypeAndOpen(FXPT_TYPE::BezierTo) &&
      points[size - 3].m_Point == points[size - 4].m_Point &&
      points[size - 2].m_Point == points[size - 4].m_Point &&
      points[size - 1].m_Point == points[size - 4].m_Point) {
    size -= 4;
  }
  points.resize(size);
}

int Outline_MoveTo(const FXFT_Vector* to, void* user) {
  OUTLINE_PARAMS* param = (OUTLINE_PARAMS*)user;

  Outline_CheckEmptyContour(param);

  param->m_pPath->ClosePath();
  param->m_pPath->AppendPoint(
      CFX_PointF(to->x / param->m_CoordUnit, to->y / param->m_CoordUnit),
      FXPT_TYPE::MoveTo, false);

  param->m_CurX = to->x;
  param->m_CurY = to->y;
  return 0;
}

int Outline_LineTo(const FXFT_Vector* to, void* user) {
  OUTLINE_PARAMS* param = (OUTLINE_PARAMS*)user;

  param->m_pPath->AppendPoint(
      CFX_PointF(to->x / param->m_CoordUnit, to->y / param->m_CoordUnit),
      FXPT_TYPE::LineTo, false);

  param->m_CurX = to->x;
  param->m_CurY = to->y;
  return 0;
}

int Outline_ConicTo(const FXFT_Vector* control,
                    const FXFT_Vector* to,
                    void* user) {
  OUTLINE_PARAMS* param = (OUTLINE_PARAMS*)user;

  param->m_pPath->AppendPoint(
      CFX_PointF((param->m_CurX + (control->x - param->m_CurX) * 2 / 3) /
                     param->m_CoordUnit,
                 (param->m_CurY + (control->y - param->m_CurY) * 2 / 3) /
                     param->m_CoordUnit),
      FXPT_TYPE::BezierTo, false);

  param->m_pPath->AppendPoint(
      CFX_PointF((control->x + (to->x - control->x) / 3) / param->m_CoordUnit,
                 (control->y + (to->y - control->y) / 3) / param->m_CoordUnit),
      FXPT_TYPE::BezierTo, false);

  param->m_pPath->AppendPoint(
      CFX_PointF(to->x / param->m_CoordUnit, to->y / param->m_CoordUnit),
      FXPT_TYPE::BezierTo, false);

  param->m_CurX = to->x;
  param->m_CurY = to->y;
  return 0;
}

int Outline_CubicTo(const FXFT_Vector* control1,
                    const FXFT_Vector* control2,
                    const FXFT_Vector* to,
                    void* user) {
  OUTLINE_PARAMS* param = (OUTLINE_PARAMS*)user;

  param->m_pPath->AppendPoint(CFX_PointF(control1->x / param->m_CoordUnit,
                                         control1->y / param->m_CoordUnit),
                              FXPT_TYPE::BezierTo, false);

  param->m_pPath->AppendPoint(CFX_PointF(control2->x / param->m_CoordUnit,
                                         control2->y / param->m_CoordUnit),
                              FXPT_TYPE::BezierTo, false);

  param->m_pPath->AppendPoint(
      CFX_PointF(to->x / param->m_CoordUnit, to->y / param->m_CoordUnit),
      FXPT_TYPE::BezierTo, false);

  param->m_CurX = to->x;
  param->m_CurY = to->y;
  return 0;
}

}  // namespace

const char CFX_Font::s_AngleSkew[] = {
    0,  2,  3,  5,  7,  9,  11, 12, 14, 16, 18, 19, 21, 23, 25,
    27, 29, 31, 32, 34, 36, 38, 40, 42, 45, 47, 49, 51, 53, 55,
};

const uint8_t CFX_Font::s_WeightPow[] = {
    0,  3,  6,  7,  8,  9,  11, 12, 14, 15, 16, 17, 18, 19, 20, 21, 22,
    23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 35, 36, 36, 37,
    37, 37, 38, 38, 38, 39, 39, 39, 40, 40, 40, 41, 41, 41, 42, 42, 42,
    42, 43, 43, 43, 44, 44, 44, 44, 45, 45, 45, 45, 46, 46, 46, 46, 47,
    47, 47, 47, 48, 48, 48, 48, 48, 49, 49, 49, 49, 50, 50, 50, 50, 50,
    51, 51, 51, 51, 51, 52, 52, 52, 52, 52, 53, 53, 53, 53, 53,
};

const uint8_t CFX_Font::s_WeightPow_11[] = {
    0,  4,  7,  8,  9,  10, 12, 13, 15, 17, 18, 19, 20, 21, 22, 23, 24,
    25, 26, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 39, 39, 40, 40, 41,
    41, 41, 42, 42, 42, 43, 43, 43, 44, 44, 44, 45, 45, 45, 46, 46, 46,
    46, 43, 47, 47, 48, 48, 48, 48, 45, 50, 50, 50, 46, 51, 51, 51, 52,
    52, 52, 52, 53, 53, 53, 53, 53, 54, 54, 54, 54, 55, 55, 55, 55, 55,
    56, 56, 56, 56, 56, 57, 57, 57, 57, 57, 58, 58, 58, 58, 58,
};

const uint8_t CFX_Font::s_WeightPow_SHIFTJIS[] = {
    0,  0,  1,  2,  3,  4,  5,  7,  8,  10, 11, 13, 14, 16, 17, 19, 21,
    22, 24, 26, 28, 30, 32, 33, 35, 37, 39, 41, 43, 45, 48, 48, 48, 48,
    49, 49, 49, 50, 50, 50, 50, 51, 51, 51, 51, 52, 52, 52, 52, 52, 53,
    53, 53, 53, 53, 54, 54, 54, 54, 54, 55, 55, 55, 55, 55, 56, 56, 56,
    56, 56, 56, 57, 57, 57, 57, 57, 57, 57, 58, 58, 58, 58, 58, 58, 58,
    59, 59, 59, 59, 59, 59, 59, 60, 60, 60, 60, 60, 60, 60, 60,
};

CFX_Font::CFX_Font()
    :
#ifdef PDF_ENABLE_XFA
      m_bShallowCopy(false),
      m_pOwnedStream(nullptr),
#endif  // PDF_ENABLE_XFA
      m_Face(nullptr),
      m_FaceCache(nullptr),
      m_pFontData(nullptr),
      m_pGsubData(nullptr),
      m_dwSize(0),
#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
      m_pPlatformFont(nullptr),
#endif
      m_bEmbedded(false),
      m_bVertical(false) {
}

#ifdef PDF_ENABLE_XFA
bool CFX_Font::LoadClone(const CFX_Font* pFont) {
  if (!pFont)
    return false;

  m_bShallowCopy = true;
  if (pFont->m_pSubstFont) {
    m_pSubstFont = pdfium::MakeUnique<CFX_SubstFont>();
    m_pSubstFont->m_Charset = pFont->m_pSubstFont->m_Charset;
    m_pSubstFont->m_SubstFlags = pFont->m_pSubstFont->m_SubstFlags;
    m_pSubstFont->m_Weight = pFont->m_pSubstFont->m_Weight;
    m_pSubstFont->m_Family = pFont->m_pSubstFont->m_Family;
    m_pSubstFont->m_ItalicAngle = pFont->m_pSubstFont->m_ItalicAngle;
  }
  m_Face = pFont->m_Face;
  m_bEmbedded = pFont->m_bEmbedded;
  m_bVertical = pFont->m_bVertical;
  m_dwSize = pFont->m_dwSize;
  m_pFontData = pFont->m_pFontData;
  m_pGsubData = pFont->m_pGsubData;
#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
  m_pPlatformFont = pFont->m_pPlatformFont;
#endif
  m_pOwnedStream = pFont->m_pOwnedStream;
  m_FaceCache = pFont->GetFaceCache();
  return true;
}

void CFX_Font::SetFace(FXFT_Face face) {
  ClearFaceCache();
  m_Face = face;
}

#endif  // PDF_ENABLE_XFA

CFX_Font::~CFX_Font() {
#ifdef PDF_ENABLE_XFA
  if (m_bShallowCopy)
    return;
#endif  // PDF_ENABLE_XFA
  if (m_Face) {
#ifndef PDF_ENABLE_XFA
    if (FXFT_Get_Face_External_Stream(m_Face)) {
      FXFT_Clear_Face_External_Stream(m_Face);
    }
#endif  // PDF_ENABLE_XFA
    DeleteFace();
  }
#ifdef PDF_ENABLE_XFA
  delete m_pOwnedStream;
#endif  // PDF_ENABLE_XFA
  FX_Free(m_pGsubData);
#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_ && !defined _SKIA_SUPPORT_
  ReleasePlatformResource();
#endif
}

void CFX_Font::DeleteFace() {
  ClearFaceCache();
  if (m_bEmbedded) {
    FXFT_Done_Face(m_Face);
  } else {
    CFX_GEModule::Get()->GetFontMgr()->ReleaseFace(m_Face);
  }
  m_Face = nullptr;
}

void CFX_Font::LoadSubst(const CFX_ByteString& face_name,
                         bool bTrueType,
                         uint32_t flags,
                         int weight,
                         int italic_angle,
                         int CharsetCP,
                         bool bVertical) {
  m_bEmbedded = false;
  m_bVertical = bVertical;
  m_pSubstFont = pdfium::MakeUnique<CFX_SubstFont>();
  m_Face = CFX_GEModule::Get()->GetFontMgr()->FindSubstFont(
      face_name, bTrueType, flags, weight, italic_angle, CharsetCP,
      m_pSubstFont.get());
  if (m_Face) {
    m_pFontData = FXFT_Get_Face_Stream_Base(m_Face);
    m_dwSize = FXFT_Get_Face_Stream_Size(m_Face);
  }
}

#ifdef PDF_ENABLE_XFA
bool CFX_Font::LoadFile(const CFX_RetainPtr<IFX_SeekableReadStream>& pFile,
                        int nFaceIndex,
                        int* pFaceCount) {
  m_bEmbedded = false;

  CFX_FontMgr* pFontMgr = CFX_GEModule::Get()->GetFontMgr();
  pFontMgr->InitFTLibrary();

  FXFT_Library library = pFontMgr->GetFTLibrary();
  std::unique_ptr<FXFT_StreamRec> stream;
  if (!LoadFileImp(library, &m_Face, pFile, nFaceIndex, &stream))
    return false;

  if (pFaceCount)
    *pFaceCount = (int)m_Face->num_faces;
  m_pOwnedStream = stream.release();
  FXFT_Set_Pixel_Sizes(m_Face, 0, 64);
  return true;
}
#endif  // PDF_ENABLE_XFA

int CFX_Font::GetGlyphWidth(uint32_t glyph_index) {
  if (!m_Face)
    return 0;
  if (m_pSubstFont && (m_pSubstFont->m_SubstFlags & FXFONT_SUBST_MM))
    AdjustMMParams(glyph_index, 0, 0);
  int err = FXFT_Load_Glyph(
      m_Face, glyph_index,
      FXFT_LOAD_NO_SCALE | FXFT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH);
  if (err)
    return 0;
  int width = EM_ADJUST(FXFT_Get_Face_UnitsPerEM(m_Face),
                        FXFT_Get_Glyph_HoriAdvance(m_Face));
  return width;
}

bool CFX_Font::LoadEmbedded(const uint8_t* data, uint32_t size) {
  std::vector<uint8_t> temp(data, data + size);
  m_pFontDataAllocation.swap(temp);
  m_Face = FT_LoadFont(m_pFontDataAllocation.data(), size);
  m_pFontData = m_pFontDataAllocation.data();
  m_bEmbedded = true;
  m_dwSize = size;
  return !!m_Face;
}

bool CFX_Font::IsTTFont() const {
  if (!m_Face)
    return false;
  return FXFT_Is_Face_TT_OT(m_Face) == FXFT_FACE_FLAG_SFNT;
}

int CFX_Font::GetAscent() const {
  if (!m_Face)
    return 0;
  return EM_ADJUST(FXFT_Get_Face_UnitsPerEM(m_Face),
                   FXFT_Get_Face_Ascender(m_Face));
}

int CFX_Font::GetDescent() const {
  if (!m_Face)
    return 0;
  return EM_ADJUST(FXFT_Get_Face_UnitsPerEM(m_Face),
                   FXFT_Get_Face_Descender(m_Face));
}

bool CFX_Font::GetGlyphBBox(uint32_t glyph_index, FX_RECT& bbox) {
  if (!m_Face)
    return false;

  if (FXFT_Is_Face_Tricky(m_Face)) {
    int error = FXFT_Set_Char_Size(m_Face, 0, 1000 * 64, 72, 72);
    if (error)
      return false;
    error = FXFT_Load_Glyph(m_Face, glyph_index,
                            FXFT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH);
    if (error)
      return false;
    FXFT_BBox cbox;
    FT_Glyph glyph;
    error = FXFT_Get_Glyph(((FXFT_Face)m_Face)->glyph, &glyph);
    if (error)
      return false;
    FXFT_Glyph_Get_CBox(glyph, FXFT_GLYPH_BBOX_PIXELS, &cbox);
    int pixel_size_x = ((FXFT_Face)m_Face)->size->metrics.x_ppem,
        pixel_size_y = ((FXFT_Face)m_Face)->size->metrics.y_ppem;
    if (pixel_size_x == 0 || pixel_size_y == 0) {
      bbox.left = cbox.xMin;
      bbox.right = cbox.xMax;
      bbox.top = cbox.yMax;
      bbox.bottom = cbox.yMin;
    } else {
      bbox.left = cbox.xMin * 1000 / pixel_size_x;
      bbox.right = cbox.xMax * 1000 / pixel_size_x;
      bbox.top = cbox.yMax * 1000 / pixel_size_y;
      bbox.bottom = cbox.yMin * 1000 / pixel_size_y;
    }
    if (bbox.top > FXFT_Get_Face_Ascender(m_Face))
      bbox.top = FXFT_Get_Face_Ascender(m_Face);
    if (bbox.bottom < FXFT_Get_Face_Descender(m_Face))
      bbox.bottom = FXFT_Get_Face_Descender(m_Face);
    FT_Done_Glyph(glyph);
    return FXFT_Set_Pixel_Sizes(m_Face, 0, 64) == 0;
  }
  if (FXFT_Load_Glyph(
          m_Face, glyph_index,
          FXFT_LOAD_NO_SCALE | FXFT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH)) {
    return false;
  }
  int em = FXFT_Get_Face_UnitsPerEM(m_Face);
  if (em == 0) {
    bbox.left = FXFT_Get_Glyph_HoriBearingX(m_Face);
    bbox.bottom = FXFT_Get_Glyph_HoriBearingY(m_Face);
    bbox.top = bbox.bottom - FXFT_Get_Glyph_Height(m_Face);
    bbox.right = bbox.left + FXFT_Get_Glyph_Width(m_Face);
  } else {
    bbox.left = FXFT_Get_Glyph_HoriBearingX(m_Face) * 1000 / em;
    bbox.top =
        (FXFT_Get_Glyph_HoriBearingY(m_Face) - FXFT_Get_Glyph_Height(m_Face)) *
        1000 / em;
    bbox.right =
        (FXFT_Get_Glyph_HoriBearingX(m_Face) + FXFT_Get_Glyph_Width(m_Face)) *
        1000 / em;
    bbox.bottom = (FXFT_Get_Glyph_HoriBearingY(m_Face)) * 1000 / em;
  }
  return true;
}

bool CFX_Font::IsItalic() const {
  if (!m_Face)
    return false;

  if (FXFT_Is_Face_Italic(m_Face) == FXFT_STYLE_FLAG_ITALIC)
    return true;
  CFX_ByteString str(FXFT_Get_Face_Style_Name(m_Face));
  str.MakeLower();
  return str.Find("italic") != -1;
}

bool CFX_Font::IsBold() const {
  if (!m_Face)
    return false;
  return FXFT_Is_Face_Bold(m_Face) == FXFT_STYLE_FLAG_BOLD;
}

bool CFX_Font::IsFixedWidth() const {
  if (!m_Face)
    return false;
  return FXFT_Is_Face_fixedwidth(m_Face) != 0;
}

CFX_ByteString CFX_Font::GetPsName() const {
  if (!m_Face)
    return CFX_ByteString();

  CFX_ByteString psName = FXFT_Get_Postscript_Name(m_Face);
  if (psName.IsEmpty())
    psName = "Untitled";
  return psName;
}

CFX_ByteString CFX_Font::GetFamilyName() const {
  if (!m_Face && !m_pSubstFont)
    return CFX_ByteString();
  if (m_Face)
    return CFX_ByteString(FXFT_Get_Face_Family_Name(m_Face));
  return m_pSubstFont->m_Family;
}

CFX_ByteString CFX_Font::GetFaceName() const {
  if (!m_Face && !m_pSubstFont)
    return CFX_ByteString();
  if (m_Face) {
    CFX_ByteString facename;
    CFX_ByteString style = CFX_ByteString(FXFT_Get_Face_Style_Name(m_Face));
    facename = GetFamilyName();
    if (facename.IsEmpty())
      facename = "Untitled";
    if (!style.IsEmpty() && style != "Regular")
      facename += " " + style;
    return facename;
  }
  return m_pSubstFont->m_Family;
}

bool CFX_Font::GetBBox(FX_RECT& bbox) {
  if (!m_Face)
    return false;
  int em = FXFT_Get_Face_UnitsPerEM(m_Face);
  if (em == 0) {
    bbox.left = FXFT_Get_Face_xMin(m_Face);
    bbox.bottom = FXFT_Get_Face_yMax(m_Face);
    bbox.top = FXFT_Get_Face_yMin(m_Face);
    bbox.right = FXFT_Get_Face_xMax(m_Face);
  } else {
    bbox.left = FXFT_Get_Face_xMin(m_Face) * 1000 / em;
    bbox.top = FXFT_Get_Face_yMin(m_Face) * 1000 / em;
    bbox.right = FXFT_Get_Face_xMax(m_Face) * 1000 / em;
    bbox.bottom = FXFT_Get_Face_yMax(m_Face) * 1000 / em;
  }
  return true;
}

int CFX_Font::GetHeight() const {
  if (!m_Face)
    return 0;

  return EM_ADJUST(FXFT_Get_Face_UnitsPerEM(m_Face),
                   FXFT_Get_Face_Height(m_Face));
}

int CFX_Font::GetMaxAdvanceWidth() const {
  if (!m_Face)
    return 0;

  return EM_ADJUST(FXFT_Get_Face_UnitsPerEM(m_Face),
                   FXFT_Get_Face_MaxAdvanceWidth(m_Face));
}

CFX_FaceCache* CFX_Font::GetFaceCache() const {
  if (!m_FaceCache) {
    m_FaceCache = CFX_GEModule::Get()->GetFontCache()->GetCachedFace(this);
  }
  return m_FaceCache;
}

void CFX_Font::ClearFaceCache() {
  if (!m_FaceCache)
    return;
  CFX_GEModule::Get()->GetFontCache()->ReleaseCachedFace(this);
  m_FaceCache = nullptr;
}

int CFX_Font::GetULPos() const {
  if (!m_Face)
    return 0;

  return EM_ADJUST(FXFT_Get_Face_UnitsPerEM(m_Face),
                   FXFT_Get_Face_UnderLinePosition(m_Face));
}

int CFX_Font::GetULthickness() const {
  if (!m_Face)
    return 0;

  return EM_ADJUST(FXFT_Get_Face_UnitsPerEM(m_Face),
                   FXFT_Get_Face_UnderLineThickness(m_Face));
}

void CFX_Font::AdjustMMParams(int glyph_index,
                              int dest_width,
                              int weight) const {
  FXFT_MM_Var pMasters = nullptr;
  FXFT_Get_MM_Var(m_Face, &pMasters);
  if (!pMasters)
    return;
  long coords[2];
  if (weight == 0)
    coords[0] = FXFT_Get_MM_Axis_Def(FXFT_Get_MM_Axis(pMasters, 0)) / 65536;
  else
    coords[0] = weight;
  if (dest_width == 0) {
    coords[1] = FXFT_Get_MM_Axis_Def(FXFT_Get_MM_Axis(pMasters, 1)) / 65536;
  } else {
    int min_param = FXFT_Get_MM_Axis_Min(FXFT_Get_MM_Axis(pMasters, 1)) / 65536;
    int max_param = FXFT_Get_MM_Axis_Max(FXFT_Get_MM_Axis(pMasters, 1)) / 65536;
    coords[1] = min_param;
    FXFT_Set_MM_Design_Coordinates(m_Face, 2, coords);
    FXFT_Load_Glyph(m_Face, glyph_index,
                    FXFT_LOAD_NO_SCALE | FXFT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH);
    int min_width = FXFT_Get_Glyph_HoriAdvance(m_Face) * 1000 /
                    FXFT_Get_Face_UnitsPerEM(m_Face);
    coords[1] = max_param;
    FXFT_Set_MM_Design_Coordinates(m_Face, 2, coords);
    FXFT_Load_Glyph(m_Face, glyph_index,
                    FXFT_LOAD_NO_SCALE | FXFT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH);
    int max_width = FXFT_Get_Glyph_HoriAdvance(m_Face) * 1000 /
                    FXFT_Get_Face_UnitsPerEM(m_Face);
    if (max_width == min_width) {
      FXFT_Free(m_Face, pMasters);
      return;
    }
    int param = min_param +
                (max_param - min_param) * (dest_width - min_width) /
                    (max_width - min_width);
    coords[1] = param;
  }
  FXFT_Free(m_Face, pMasters);
  FXFT_Set_MM_Design_Coordinates(m_Face, 2, coords);
}

CFX_PathData* CFX_Font::LoadGlyphPathImpl(uint32_t glyph_index,
                                          int dest_width) const {
  if (!m_Face)
    return nullptr;
  FXFT_Set_Pixel_Sizes(m_Face, 0, 64);
  FXFT_Matrix ft_matrix = {65536, 0, 0, 65536};
  if (m_pSubstFont) {
    if (m_pSubstFont->m_ItalicAngle) {
      int skew = m_pSubstFont->m_ItalicAngle;
      // |skew| is nonpositive so |-skew| is used as the index. We need to make
      // sure |skew| != INT_MIN since -INT_MIN is undefined.
      if (skew <= 0 && skew != std::numeric_limits<int>::min() &&
          static_cast<size_t>(-skew) < kAngleSkewArraySize) {
        skew = -s_AngleSkew[-skew];
      } else {
        skew = -58;
      }
      if (m_bVertical)
        ft_matrix.yx += ft_matrix.yy * skew / 100;
      else
        ft_matrix.xy -= ft_matrix.xx * skew / 100;
    }
    if (m_pSubstFont->m_SubstFlags & FXFONT_SUBST_MM) {
      AdjustMMParams(glyph_index, dest_width, m_pSubstFont->m_Weight);
    }
  }
  ScopedFontTransform scoped_transform(m_Face, &ft_matrix);
  int load_flags = FXFT_LOAD_NO_BITMAP;
  if (!(m_Face->face_flags & FT_FACE_FLAG_SFNT) || !FT_IS_TRICKY(m_Face))
    load_flags |= FT_LOAD_NO_HINTING;
  if (FXFT_Load_Glyph(m_Face, glyph_index, load_flags))
    return nullptr;
  if (m_pSubstFont && !(m_pSubstFont->m_SubstFlags & FXFONT_SUBST_MM) &&
      m_pSubstFont->m_Weight > 400) {
    uint32_t index = (m_pSubstFont->m_Weight - 400) / 10;
    index = std::min(index, static_cast<uint32_t>(kWeightPowArraySize - 1));
    int level = 0;
    if (m_pSubstFont->m_Charset == FXFONT_SHIFTJIS_CHARSET)
      level = s_WeightPow_SHIFTJIS[index] * 2 * 65536 / 36655;
    else
      level = s_WeightPow[index] * 2;
    FXFT_Outline_Embolden(FXFT_Get_Glyph_Outline(m_Face), level);
  }

  FXFT_Outline_Funcs funcs;
  funcs.move_to = Outline_MoveTo;
  funcs.line_to = Outline_LineTo;
  funcs.conic_to = Outline_ConicTo;
  funcs.cubic_to = Outline_CubicTo;
  funcs.shift = 0;
  funcs.delta = 0;

  OUTLINE_PARAMS params;
  auto pPath = pdfium::MakeUnique<CFX_PathData>();
  params.m_pPath = pPath.get();
  params.m_CurX = params.m_CurY = 0;
  params.m_CoordUnit = 64 * 64.0;

  FXFT_Outline_Decompose(FXFT_Get_Glyph_Outline(m_Face), &funcs, &params);
  if (pPath->GetPoints().empty())
    return nullptr;

  Outline_CheckEmptyContour(&params);
  pPath->ClosePath();

  return pPath.release();
}

const CFX_GlyphBitmap* CFX_Font::LoadGlyphBitmap(uint32_t glyph_index,
                                                 bool bFontStyle,
                                                 const CFX_Matrix* pMatrix,
                                                 int dest_width,
                                                 int anti_alias,
                                                 int& text_flags) const {
  return GetFaceCache()->LoadGlyphBitmap(this, glyph_index, bFontStyle, pMatrix,
                                         dest_width, anti_alias, text_flags);
}

const CFX_PathData* CFX_Font::LoadGlyphPath(uint32_t glyph_index,
                                            int dest_width) const {
  return GetFaceCache()->LoadGlyphPath(this, glyph_index, dest_width);
}

#if defined _SKIA_SUPPORT_ || _SKIA_SUPPORT_PATHS_
CFX_TypeFace* CFX_Font::GetDeviceCache() const {
  return GetFaceCache()->GetDeviceCache(this);
}
#endif
