// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxge/win32/cfx_psrenderer.h"

#include <memory>

#include "core/fxcodec/fx_codec.h"
#include "core/fxcrt/cfx_maybe_owned.h"
#include "core/fxge/cfx_facecache.h"
#include "core/fxge/cfx_fontcache.h"
#include "core/fxge/cfx_pathdata.h"
#include "core/fxge/cfx_renderdevice.h"
#include "core/fxge/ge/fx_text_int.h"
#include "core/fxge/win32/cpsoutput.h"
#include "third_party/base/ptr_util.h"

struct PSGlyph {
  CFX_Font* m_pFont;
  uint32_t m_GlyphIndex;
  bool m_bGlyphAdjust;
  FX_FLOAT m_AdjustMatrix[4];
};

class CPSFont {
 public:
  PSGlyph m_Glyphs[256];
  int m_nGlyphs;
};

CFX_PSRenderer::CFX_PSRenderer() {
  m_pOutput = nullptr;
  m_bColorSet = m_bGraphStateSet = false;
  m_bInited = false;
}

CFX_PSRenderer::~CFX_PSRenderer() {}

#define OUTPUT_PS(str) m_pOutput->OutputPS(str, sizeof(str) - 1)

void CFX_PSRenderer::Init(CPSOutput* pOutput,
                          int pslevel,
                          int width,
                          int height,
                          bool bCmykOutput) {
  m_PSLevel = pslevel;
  m_pOutput = pOutput;
  m_ClipBox.left = m_ClipBox.top = 0;
  m_ClipBox.right = width;
  m_ClipBox.bottom = height;
  m_bCmykOutput = bCmykOutput;
}

bool CFX_PSRenderer::StartRendering() {
  if (m_bInited) {
    return true;
  }
  static const char init_str[] =
      "\nsave\n/im/initmatrix load def\n"
      "/n/newpath load def/m/moveto load def/l/lineto load def/c/curveto load "
      "def/h/closepath load def\n"
      "/f/fill load def/F/eofill load def/s/stroke load def/W/clip load "
      "def/W*/eoclip load def\n"
      "/rg/setrgbcolor load def/k/setcmykcolor load def\n"
      "/J/setlinecap load def/j/setlinejoin load def/w/setlinewidth load "
      "def/M/setmiterlimit load def/d/setdash load def\n"
      "/q/gsave load def/Q/grestore load def/iM/imagemask load def\n"
      "/Tj/show load def/Ff/findfont load def/Fs/scalefont load def/Sf/setfont "
      "load def\n"
      "/cm/concat load def/Cm/currentmatrix load def/mx/matrix load "
      "def/sm/setmatrix load def\n";
  OUTPUT_PS(init_str);
  m_bInited = true;
  return true;
}

void CFX_PSRenderer::EndRendering() {
  if (m_bInited) {
    OUTPUT_PS("\nrestore\n");
    m_bInited = false;
  }
}

void CFX_PSRenderer::SaveState() {
  StartRendering();
  OUTPUT_PS("q\n");
  m_ClipBoxStack.push_back(m_ClipBox);
}

void CFX_PSRenderer::RestoreState(bool bKeepSaved) {
  StartRendering();
  if (bKeepSaved)
    OUTPUT_PS("Q\nq\n");
  else
    OUTPUT_PS("Q\n");

  m_bColorSet = false;
  m_bGraphStateSet = false;
  if (m_ClipBoxStack.empty())
    return;

  m_ClipBox = m_ClipBoxStack.back();
  if (!bKeepSaved)
    m_ClipBoxStack.pop_back();
}

void CFX_PSRenderer::OutputPath(const CFX_PathData* pPathData,
                                const CFX_Matrix* pObject2Device) {
  CFX_ByteTextBuf buf;
  size_t size = pPathData->GetPoints().size();
  buf.EstimateSize(size * 10);

  for (size_t i = 0; i < size; i++) {
    FXPT_TYPE type = pPathData->GetType(i);
    bool closing = pPathData->IsClosingFigure(i);
    CFX_PointF pos = pPathData->GetPoint(i);
    if (pObject2Device)
      pos = pObject2Device->Transform(pos);

    buf << pos.x << " " << pos.y;
    switch (type) {
      case FXPT_TYPE::MoveTo:
        buf << " m ";
        break;
      case FXPT_TYPE::LineTo:
        buf << " l ";
        if (closing)
          buf << "h ";
        break;
      case FXPT_TYPE::BezierTo: {
        CFX_PointF pos1 = pPathData->GetPoint(i + 1);
        CFX_PointF pos2 = pPathData->GetPoint(i + 2);
        if (pObject2Device) {
          pos1 = pObject2Device->Transform(pos1);
          pos2 = pObject2Device->Transform(pos2);
        }
        buf << " " << pos1.x << " " << pos1.y << " " << pos2.x << " " << pos2.y
            << " c";
        if (closing)
          buf << " h";
        buf << "\n";
        i += 2;
        break;
      }
    }
  }
  m_pOutput->OutputPS((const FX_CHAR*)buf.GetBuffer(), buf.GetSize());
}

void CFX_PSRenderer::SetClip_PathFill(const CFX_PathData* pPathData,
                                      const CFX_Matrix* pObject2Device,
                                      int fill_mode) {
  StartRendering();
  OutputPath(pPathData, pObject2Device);
  CFX_FloatRect rect = pPathData->GetBoundingBox();
  if (pObject2Device)
    pObject2Device->TransformRect(rect);

  m_ClipBox.left = static_cast<int>(rect.left);
  m_ClipBox.right = static_cast<int>(rect.left + rect.right);
  m_ClipBox.top = static_cast<int>(rect.top + rect.bottom);
  m_ClipBox.bottom = static_cast<int>(rect.bottom);
  if ((fill_mode & 3) == FXFILL_WINDING) {
    OUTPUT_PS("W n\n");
  } else {
    OUTPUT_PS("W* n\n");
  }
}

void CFX_PSRenderer::SetClip_PathStroke(const CFX_PathData* pPathData,
                                        const CFX_Matrix* pObject2Device,
                                        const CFX_GraphStateData* pGraphState) {
  StartRendering();
  SetGraphState(pGraphState);
  if (pObject2Device) {
    CFX_ByteTextBuf buf;
    buf << "mx Cm [" << pObject2Device->a << " " << pObject2Device->b << " "
        << pObject2Device->c << " " << pObject2Device->d << " "
        << pObject2Device->e << " " << pObject2Device->f << "]cm ";
    m_pOutput->OutputPS((const FX_CHAR*)buf.GetBuffer(), buf.GetSize());
  }
  OutputPath(pPathData, nullptr);
  CFX_FloatRect rect = pPathData->GetBoundingBox(pGraphState->m_LineWidth,
                                                 pGraphState->m_MiterLimit);
  pObject2Device->TransformRect(rect);
  m_ClipBox.Intersect(rect.GetOuterRect());
  if (pObject2Device) {
    OUTPUT_PS("strokepath W n sm\n");
  } else {
    OUTPUT_PS("strokepath W n\n");
  }
}

bool CFX_PSRenderer::DrawPath(const CFX_PathData* pPathData,
                              const CFX_Matrix* pObject2Device,
                              const CFX_GraphStateData* pGraphState,
                              uint32_t fill_color,
                              uint32_t stroke_color,
                              int fill_mode) {
  StartRendering();
  int fill_alpha = FXARGB_A(fill_color);
  int stroke_alpha = FXARGB_A(stroke_color);
  if (fill_alpha && fill_alpha < 255) {
    return false;
  }
  if (stroke_alpha && stroke_alpha < 255) {
    return false;
  }
  if (fill_alpha == 0 && stroke_alpha == 0) {
    return false;
  }
  if (stroke_alpha) {
    SetGraphState(pGraphState);
    if (pObject2Device) {
      CFX_ByteTextBuf buf;
      buf << "mx Cm [" << pObject2Device->a << " " << pObject2Device->b << " "
          << pObject2Device->c << " " << pObject2Device->d << " "
          << pObject2Device->e << " " << pObject2Device->f << "]cm ";
      m_pOutput->OutputPS((const FX_CHAR*)buf.GetBuffer(), buf.GetSize());
    }
  }
  OutputPath(pPathData, stroke_alpha ? nullptr : pObject2Device);
  if (fill_mode && fill_alpha) {
    SetColor(fill_color);
    if ((fill_mode & 3) == FXFILL_WINDING) {
      if (stroke_alpha) {
        OUTPUT_PS("q f Q ");
      } else {
        OUTPUT_PS("f");
      }
    } else if ((fill_mode & 3) == FXFILL_ALTERNATE) {
      if (stroke_alpha) {
        OUTPUT_PS("q F Q ");
      } else {
        OUTPUT_PS("F");
      }
    }
  }
  if (stroke_alpha) {
    SetColor(stroke_color);
    if (pObject2Device) {
      OUTPUT_PS("s sm");
    } else {
      OUTPUT_PS("s");
    }
  }
  OUTPUT_PS("\n");
  return true;
}

void CFX_PSRenderer::SetGraphState(const CFX_GraphStateData* pGraphState) {
  CFX_ByteTextBuf buf;
  if (!m_bGraphStateSet ||
      m_CurGraphState.m_LineCap != pGraphState->m_LineCap) {
    buf << pGraphState->m_LineCap << " J\n";
  }
  if (!m_bGraphStateSet ||
      m_CurGraphState.m_DashCount != pGraphState->m_DashCount ||
      FXSYS_memcmp(m_CurGraphState.m_DashArray, pGraphState->m_DashArray,
                   sizeof(FX_FLOAT) * m_CurGraphState.m_DashCount)) {
    buf << "[";
    for (int i = 0; i < pGraphState->m_DashCount; ++i) {
      buf << pGraphState->m_DashArray[i] << " ";
    }
    buf << "]" << pGraphState->m_DashPhase << " d\n";
  }
  if (!m_bGraphStateSet ||
      m_CurGraphState.m_LineJoin != pGraphState->m_LineJoin) {
    buf << pGraphState->m_LineJoin << " j\n";
  }
  if (!m_bGraphStateSet ||
      m_CurGraphState.m_LineWidth != pGraphState->m_LineWidth) {
    buf << pGraphState->m_LineWidth << " w\n";
  }
  if (!m_bGraphStateSet ||
      m_CurGraphState.m_MiterLimit != pGraphState->m_MiterLimit) {
    buf << pGraphState->m_MiterLimit << " M\n";
  }
  m_CurGraphState.Copy(*pGraphState);
  m_bGraphStateSet = true;
  if (buf.GetSize()) {
    m_pOutput->OutputPS((const FX_CHAR*)buf.GetBuffer(), buf.GetSize());
  }
}

static void FaxCompressData(uint8_t* src_buf,
                            int width,
                            int height,
                            std::unique_ptr<uint8_t, FxFreeDeleter>* dest_buf,
                            uint32_t* dest_size) {
  if (width * height > 128) {
    CCodec_FaxModule::FaxEncode(src_buf, width, height, (width + 7) / 8,
                                dest_buf, dest_size);
    FX_Free(src_buf);
  } else {
    dest_buf->reset(src_buf);
    *dest_size = (width + 7) / 8 * height;
  }
}

static void PSCompressData(int PSLevel,
                           uint8_t* src_buf,
                           uint32_t src_size,
                           uint8_t** output_buf,
                           uint32_t* output_size,
                           const FX_CHAR** filter) {
  *output_buf = src_buf;
  *output_size = src_size;
  *filter = "";
  if (src_size < 1024) {
    return;
  }
  CCodec_ModuleMgr* pEncoders = CFX_GEModule::Get()->GetCodecModule();
  uint8_t* dest_buf = nullptr;
  uint32_t dest_size = src_size;
  if (PSLevel >= 3) {
    if (pEncoders &&
        pEncoders->GetFlateModule()->Encode(src_buf, src_size, &dest_buf,
                                            &dest_size)) {
      *filter = "/FlateDecode filter ";
    }
  } else {
    if (pEncoders &&
        pEncoders->GetBasicModule()->RunLengthEncode(src_buf, src_size,
                                                     &dest_buf, &dest_size)) {
      *filter = "/RunLengthDecode filter ";
    }
  }
  if (dest_size < src_size) {
    *output_buf = dest_buf;
    *output_size = dest_size;
  } else {
    *filter = nullptr;
    FX_Free(dest_buf);
  }
}

bool CFX_PSRenderer::SetDIBits(const CFX_DIBSource* pSource,
                               uint32_t color,
                               int left,
                               int top) {
  StartRendering();
  CFX_Matrix matrix((FX_FLOAT)(pSource->GetWidth()), 0.0f, 0.0f,
                    -(FX_FLOAT)(pSource->GetHeight()), (FX_FLOAT)(left),
                    (FX_FLOAT)(top + pSource->GetHeight()));
  return DrawDIBits(pSource, color, &matrix, 0);
}

bool CFX_PSRenderer::StretchDIBits(const CFX_DIBSource* pSource,
                                   uint32_t color,
                                   int dest_left,
                                   int dest_top,
                                   int dest_width,
                                   int dest_height,
                                   uint32_t flags) {
  StartRendering();
  CFX_Matrix matrix((FX_FLOAT)(dest_width), 0.0f, 0.0f,
                    (FX_FLOAT)(-dest_height), (FX_FLOAT)(dest_left),
                    (FX_FLOAT)(dest_top + dest_height));
  return DrawDIBits(pSource, color, &matrix, flags);
}

bool CFX_PSRenderer::DrawDIBits(const CFX_DIBSource* pSource,
                                uint32_t color,
                                const CFX_Matrix* pMatrix,
                                uint32_t flags) {
  StartRendering();
  if ((pMatrix->a == 0 && pMatrix->b == 0) ||
      (pMatrix->c == 0 && pMatrix->d == 0)) {
    return true;
  }
  if (pSource->HasAlpha()) {
    return false;
  }
  int alpha = FXARGB_A(color);
  if (pSource->IsAlphaMask() && (alpha < 255 || pSource->GetBPP() != 1))
    return false;

  OUTPUT_PS("q\n");
  CFX_ByteTextBuf buf;
  buf << "[" << pMatrix->a << " " << pMatrix->b << " " << pMatrix->c << " "
      << pMatrix->d << " " << pMatrix->e << " " << pMatrix->f << "]cm ";
  int width = pSource->GetWidth();
  int height = pSource->GetHeight();
  buf << width << " " << height;
  if (pSource->GetBPP() == 1 && !pSource->GetPalette()) {
    int pitch = (width + 7) / 8;
    uint32_t src_size = height * pitch;
    uint8_t* src_buf = FX_Alloc(uint8_t, src_size);
    for (int row = 0; row < height; row++) {
      const uint8_t* src_scan = pSource->GetScanline(row);
      FXSYS_memcpy(src_buf + row * pitch, src_scan, pitch);
    }
    std::unique_ptr<uint8_t, FxFreeDeleter> output_buf;
    uint32_t output_size;
    FaxCompressData(src_buf, width, height, &output_buf, &output_size);
    if (pSource->IsAlphaMask()) {
      SetColor(color);
      m_bColorSet = false;
      buf << " true[";
    } else {
      buf << " 1[";
    }
    buf << width << " 0 0 -" << height << " 0 " << height
        << "]currentfile/ASCII85Decode filter ";
    if (output_buf.get() != src_buf) {
      buf << "<</K -1/EndOfBlock false/Columns " << width << "/Rows " << height
          << ">>/CCITTFaxDecode filter ";
    }
    if (pSource->IsAlphaMask()) {
      buf << "iM\n";
    } else {
      buf << "false 1 colorimage\n";
    }
    m_pOutput->OutputPS((const FX_CHAR*)buf.GetBuffer(), buf.GetSize());
    WritePSBinary(output_buf.get(), output_size);
    output_buf.release();
  } else {
    CFX_DIBExtractor source_extractor(pSource);
    CFX_MaybeOwned<CFX_DIBSource> pConverted(source_extractor.GetBitmap());
    if (!pConverted.Get())
      return false;
    switch (pSource->GetFormat()) {
      case FXDIB_1bppRgb:
      case FXDIB_Rgb32:
        pConverted = pConverted->CloneConvert(FXDIB_Rgb).release();
        break;
      case FXDIB_8bppRgb:
        if (pSource->GetPalette()) {
          pConverted = pConverted->CloneConvert(FXDIB_Rgb).release();
        }
        break;
      case FXDIB_1bppCmyk:
        pConverted = pConverted->CloneConvert(FXDIB_Cmyk).release();
        break;
      case FXDIB_8bppCmyk:
        if (pSource->GetPalette()) {
          pConverted = pConverted->CloneConvert(FXDIB_Cmyk).release();
        }
        break;
      default:
        break;
    }
    if (!pConverted) {
      OUTPUT_PS("\nQ\n");
      return false;
    }
    int bpp = pConverted->GetBPP() / 8;
    uint8_t* output_buf = nullptr;
    FX_STRSIZE output_size = 0;
    const FX_CHAR* filter = nullptr;
    if ((m_PSLevel == 2 || flags & FXRENDER_IMAGE_LOSSY) &&
        CCodec_JpegModule::JpegEncode(pConverted.Get(), &output_buf,
                                      &output_size)) {
      filter = "/DCTDecode filter ";
    }
    if (!filter) {
      int src_pitch = width * bpp;
      output_size = height * src_pitch;
      output_buf = FX_Alloc(uint8_t, output_size);
      for (int row = 0; row < height; row++) {
        const uint8_t* src_scan = pConverted->GetScanline(row);
        uint8_t* dest_scan = output_buf + row * src_pitch;
        if (bpp == 3) {
          for (int col = 0; col < width; col++) {
            *dest_scan++ = src_scan[2];
            *dest_scan++ = src_scan[1];
            *dest_scan++ = *src_scan;
            src_scan += 3;
          }
        } else {
          FXSYS_memcpy(dest_scan, src_scan, src_pitch);
        }
      }
      uint8_t* compressed_buf;
      uint32_t compressed_size;
      PSCompressData(m_PSLevel, output_buf, output_size, &compressed_buf,
                     &compressed_size, &filter);
      if (output_buf != compressed_buf) {
        FX_Free(output_buf);
      }
      output_buf = compressed_buf;
      output_size = compressed_size;
    }
    buf << " 8[";
    buf << width << " 0 0 -" << height << " 0 " << height << "]";
    buf << "currentfile/ASCII85Decode filter ";
    if (filter) {
      buf << filter;
    }
    buf << "false " << bpp;
    buf << " colorimage\n";
    m_pOutput->OutputPS((const FX_CHAR*)buf.GetBuffer(), buf.GetSize());
    WritePSBinary(output_buf, output_size);
    FX_Free(output_buf);
  }
  OUTPUT_PS("\nQ\n");
  return true;
}

void CFX_PSRenderer::SetColor(uint32_t color) {
  bool bCMYK = false;
  if (bCMYK != m_bCmykOutput || !m_bColorSet || m_LastColor != color) {
    CFX_ByteTextBuf buf;
    if (bCMYK) {
      buf << FXSYS_GetCValue(color) / 255.0 << " "
          << FXSYS_GetMValue(color) / 255.0 << " "
          << FXSYS_GetYValue(color) / 255.0 << " "
          << FXSYS_GetKValue(color) / 255.0 << " k\n";
    } else {
      buf << FXARGB_R(color) / 255.0 << " " << FXARGB_G(color) / 255.0 << " "
          << FXARGB_B(color) / 255.0 << " rg\n";
    }
    if (bCMYK == m_bCmykOutput) {
      m_bColorSet = true;
      m_LastColor = color;
    }
    m_pOutput->OutputPS((const FX_CHAR*)buf.GetBuffer(), buf.GetSize());
  }
}

void CFX_PSRenderer::FindPSFontGlyph(CFX_FaceCache* pFaceCache,
                                     CFX_Font* pFont,
                                     const FXTEXT_CHARPOS& charpos,
                                     int* ps_fontnum,
                                     int* ps_glyphindex) {
  int i = 0;
  for (const auto& pPSFont : m_PSFontList) {
    for (int j = 0; j < pPSFont->m_nGlyphs; j++) {
      if (pPSFont->m_Glyphs[j].m_pFont == pFont &&
          pPSFont->m_Glyphs[j].m_GlyphIndex == charpos.m_GlyphIndex &&
          ((!pPSFont->m_Glyphs[j].m_bGlyphAdjust && !charpos.m_bGlyphAdjust) ||
           (pPSFont->m_Glyphs[j].m_bGlyphAdjust && charpos.m_bGlyphAdjust &&
            (FXSYS_fabs(pPSFont->m_Glyphs[j].m_AdjustMatrix[0] -
                        charpos.m_AdjustMatrix[0]) < 0.01 &&
             FXSYS_fabs(pPSFont->m_Glyphs[j].m_AdjustMatrix[1] -
                        charpos.m_AdjustMatrix[1]) < 0.01 &&
             FXSYS_fabs(pPSFont->m_Glyphs[j].m_AdjustMatrix[2] -
                        charpos.m_AdjustMatrix[2]) < 0.01 &&
             FXSYS_fabs(pPSFont->m_Glyphs[j].m_AdjustMatrix[3] -
                        charpos.m_AdjustMatrix[3]) < 0.01)))) {
        *ps_fontnum = i;
        *ps_glyphindex = j;
        return;
      }
    }
    ++i;
  }
  if (m_PSFontList.empty() || m_PSFontList.back()->m_nGlyphs == 256) {
    m_PSFontList.push_back(pdfium::MakeUnique<CPSFont>());
    m_PSFontList.back()->m_nGlyphs = 0;
    CFX_ByteTextBuf buf;
    buf << "8 dict begin/FontType 3 def/FontMatrix[1 0 0 1 0 0]def\n"
           "/FontBBox[0 0 0 0]def/Encoding 256 array def 0 1 255{Encoding "
           "exch/.notdef put}for\n"
           "/CharProcs 1 dict def CharProcs begin/.notdef {} def end\n"
           "/BuildGlyph{1 0 -10 -10 10 10 setcachedevice exch/CharProcs get "
           "exch 2 copy known not{pop/.notdef}if get exec}bind def\n"
           "/BuildChar{1 index/Encoding get exch get 1 index/BuildGlyph get "
           "exec}bind def\n"
           "currentdict end\n";
    buf << "/X" << static_cast<uint32_t>(m_PSFontList.size() - 1)
        << " exch definefont pop\n";
    m_pOutput->OutputPS((const FX_CHAR*)buf.GetBuffer(), buf.GetSize());
    buf.Clear();
  }
  *ps_fontnum = m_PSFontList.size() - 1;
  CPSFont* pPSFont = m_PSFontList[*ps_fontnum].get();
  int glyphindex = pPSFont->m_nGlyphs;
  *ps_glyphindex = glyphindex;
  pPSFont->m_Glyphs[glyphindex].m_GlyphIndex = charpos.m_GlyphIndex;
  pPSFont->m_Glyphs[glyphindex].m_pFont = pFont;
  pPSFont->m_Glyphs[glyphindex].m_bGlyphAdjust = charpos.m_bGlyphAdjust;
  if (charpos.m_bGlyphAdjust) {
    pPSFont->m_Glyphs[glyphindex].m_AdjustMatrix[0] = charpos.m_AdjustMatrix[0];
    pPSFont->m_Glyphs[glyphindex].m_AdjustMatrix[1] = charpos.m_AdjustMatrix[1];
    pPSFont->m_Glyphs[glyphindex].m_AdjustMatrix[2] = charpos.m_AdjustMatrix[2];
    pPSFont->m_Glyphs[glyphindex].m_AdjustMatrix[3] = charpos.m_AdjustMatrix[3];
  }
  pPSFont->m_nGlyphs++;

  CFX_Matrix matrix;
  if (charpos.m_bGlyphAdjust) {
    matrix =
        CFX_Matrix(charpos.m_AdjustMatrix[0], charpos.m_AdjustMatrix[1],
                   charpos.m_AdjustMatrix[2], charpos.m_AdjustMatrix[3], 0, 0);
  }
  matrix.Concat(CFX_Matrix(1.0f, 0, 0, 1.0f, 0, 0));
  const CFX_PathData* pPathData = pFaceCache->LoadGlyphPath(
      pFont, charpos.m_GlyphIndex, charpos.m_FontCharWidth);
  if (!pPathData)
    return;

  CFX_PathData TransformedPath(*pPathData);
  if (charpos.m_bGlyphAdjust)
    TransformedPath.Transform(&matrix);

  CFX_ByteTextBuf buf;
  buf << "/X" << *ps_fontnum << " Ff/CharProcs get begin/" << glyphindex
      << "{n ";
  for (size_t p = 0; p < TransformedPath.GetPoints().size(); p++) {
    CFX_PointF point = TransformedPath.GetPoint(p);
    switch (TransformedPath.GetType(p)) {
      case FXPT_TYPE::MoveTo: {
        buf << point.x << " " << point.y << " m\n";
        break;
      }
      case FXPT_TYPE::LineTo: {
        buf << point.x << " " << point.y << " l\n";
        break;
      }
      case FXPT_TYPE::BezierTo: {
        CFX_PointF point1 = TransformedPath.GetPoint(p + 1);
        CFX_PointF point2 = TransformedPath.GetPoint(p + 2);
        buf << point.x << " " << point.y << " " << point1.x << " " << point1.y
            << " " << point2.x << " " << point2.y << " c\n";
        p += 2;
        break;
      }
    }
  }
  buf << "f}bind def end\n";
  buf << "/X" << *ps_fontnum << " Ff/Encoding get " << glyphindex << "/"
      << glyphindex << " put\n";
  m_pOutput->OutputPS((const FX_CHAR*)buf.GetBuffer(), buf.GetSize());
}

bool CFX_PSRenderer::DrawText(int nChars,
                              const FXTEXT_CHARPOS* pCharPos,
                              CFX_Font* pFont,
                              const CFX_Matrix* pObject2Device,
                              FX_FLOAT font_size,
                              uint32_t color) {
  StartRendering();
  int alpha = FXARGB_A(color);
  if (alpha < 255)
    return false;

  if ((pObject2Device->a == 0 && pObject2Device->b == 0) ||
      (pObject2Device->c == 0 && pObject2Device->d == 0)) {
    return true;
  }
  SetColor(color);
  CFX_ByteTextBuf buf;
  buf << "q[" << pObject2Device->a << " " << pObject2Device->b << " "
      << pObject2Device->c << " " << pObject2Device->d << " "
      << pObject2Device->e << " " << pObject2Device->f << "]cm\n";

  CFX_FontCache* pCache = CFX_GEModule::Get()->GetFontCache();
  CFX_FaceCache* pFaceCache = pCache->GetCachedFace(pFont);
  int last_fontnum = -1;
  for (int i = 0; i < nChars; i++) {
    int ps_fontnum, ps_glyphindex;
    FindPSFontGlyph(pFaceCache, pFont, pCharPos[i], &ps_fontnum,
                    &ps_glyphindex);
    if (last_fontnum != ps_fontnum) {
      buf << "/X" << ps_fontnum << " Ff " << font_size << " Fs Sf ";
      last_fontnum = ps_fontnum;
    }
    buf << pCharPos[i].m_Origin.x << " " << pCharPos[i].m_Origin.y << " m";
    CFX_ByteString hex;
    hex.Format("<%02X>", ps_glyphindex);
    buf << hex.AsStringC() << "Tj\n";
  }
  buf << "Q\n";
  m_pOutput->OutputPS((const FX_CHAR*)buf.GetBuffer(), buf.GetSize());
  pCache->ReleaseCachedFace(pFont);
  return true;
}

void CFX_PSRenderer::WritePSBinary(const uint8_t* data, int len) {
  uint8_t* dest_buf;
  uint32_t dest_size;
  CCodec_ModuleMgr* pEncoders = CFX_GEModule::Get()->GetCodecModule();
  if (pEncoders &&
      pEncoders->GetBasicModule()->A85Encode(data, len, &dest_buf,
                                             &dest_size)) {
    m_pOutput->OutputPS((const FX_CHAR*)dest_buf, dest_size);
    FX_Free(dest_buf);
  } else {
    m_pOutput->OutputPS((const FX_CHAR*)data, len);
  }
}
