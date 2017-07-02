// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/page/cpdf_streamcontentparser.h"

#include <memory>
#include <utility>
#include <vector>

#include "core/fpdfapi/font/cpdf_font.h"
#include "core/fpdfapi/font/cpdf_type3font.h"
#include "core/fpdfapi/page/cpdf_allstates.h"
#include "core/fpdfapi/page/cpdf_docpagedata.h"
#include "core/fpdfapi/page/cpdf_form.h"
#include "core/fpdfapi/page/cpdf_formobject.h"
#include "core/fpdfapi/page/cpdf_image.h"
#include "core/fpdfapi/page/cpdf_imageobject.h"
#include "core/fpdfapi/page/cpdf_meshstream.h"
#include "core/fpdfapi/page/cpdf_pageobject.h"
#include "core/fpdfapi/page/cpdf_pathobject.h"
#include "core/fpdfapi/page/cpdf_shadingobject.h"
#include "core/fpdfapi/page/cpdf_shadingpattern.h"
#include "core/fpdfapi/page/cpdf_streamparser.h"
#include "core/fpdfapi/page/cpdf_textobject.h"
#include "core/fpdfapi/page/pageint.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/parser/cpdf_name.h"
#include "core/fpdfapi/parser/cpdf_number.h"
#include "core/fpdfapi/parser/cpdf_reference.h"
#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fpdfapi/parser/fpdf_parser_decode.h"
#include "core/fxcrt/fx_safe_types.h"
#include "core/fxge/cfx_graphstatedata.h"
#include "third_party/base/ptr_util.h"

namespace {

const int kMaxFormLevel = 30;

const int kSingleCoordinatePair = 1;
const int kTensorCoordinatePairs = 16;
const int kCoonsCoordinatePairs = 12;
const int kSingleColorPerPatch = 1;
const int kQuadColorsPerPatch = 4;

const char kPathOperatorSubpath = 'm';
const char kPathOperatorLine = 'l';
const char kPathOperatorCubicBezier1 = 'c';
const char kPathOperatorCubicBezier2 = 'v';
const char kPathOperatorCubicBezier3 = 'y';
const char kPathOperatorClosePath = 'h';
const char kPathOperatorRectangle[] = "re";

class CPDF_StreamParserAutoClearer {
 public:
  CPDF_StreamParserAutoClearer(CPDF_StreamParser** scoped_variable,
                               CPDF_StreamParser* new_parser)
      : scoped_variable_(scoped_variable) {
    *scoped_variable_ = new_parser;
  }
  ~CPDF_StreamParserAutoClearer() { *scoped_variable_ = nullptr; }

 private:
  CPDF_StreamParser** scoped_variable_;
};

CFX_FloatRect GetShadingBBox(CPDF_ShadingPattern* pShading,
                             const CFX_Matrix& matrix) {
  ShadingType type = pShading->GetShadingType();
  CPDF_Stream* pStream = ToStream(pShading->GetShadingObject());
  CPDF_ColorSpace* pCS = pShading->GetCS();
  if (!pStream || !pCS)
    return CFX_FloatRect(0, 0, 0, 0);

  CPDF_MeshStream stream(type, pShading->GetFuncs(), pStream, pCS);
  if (!stream.Load())
    return CFX_FloatRect(0, 0, 0, 0);

  CFX_FloatRect rect;
  bool bStarted = false;
  bool bGouraud = type == kFreeFormGouraudTriangleMeshShading ||
                  type == kLatticeFormGouraudTriangleMeshShading;

  int point_count = kSingleCoordinatePair;
  if (type == kTensorProductPatchMeshShading)
    point_count = kTensorCoordinatePairs;
  else if (type == kCoonsPatchMeshShading)
    point_count = kCoonsCoordinatePairs;

  int color_count = kSingleColorPerPatch;
  if (type == kCoonsPatchMeshShading || type == kTensorProductPatchMeshShading)
    color_count = kQuadColorsPerPatch;

  while (!stream.BitStream()->IsEOF()) {
    uint32_t flag = 0;
    if (type != kLatticeFormGouraudTriangleMeshShading) {
      if (!stream.CanReadFlag())
        break;
      flag = stream.ReadFlag();
    }

    if (!bGouraud && flag) {
      point_count -= 4;
      color_count -= 2;
    }

    for (int i = 0; i < point_count; i++) {
      if (!stream.CanReadCoords())
        break;
      CFX_PointF origin = stream.ReadCoords();
      if (bStarted) {
        rect.UpdateRect(origin.x, origin.y);
      } else {
        rect.InitRect(origin.x, origin.y);
        bStarted = true;
      }
    }
    FX_SAFE_UINT32 nBits = stream.Components();
    nBits *= stream.ComponentBits();
    nBits *= color_count;
    if (!nBits.IsValid())
      break;

    stream.BitStream()->SkipBits(nBits.ValueOrDie());
    if (bGouraud)
      stream.BitStream()->ByteAlign();
  }
  matrix.TransformRect(rect);
  return rect;
}

struct AbbrPair {
  const FX_CHAR* abbr;
  const FX_CHAR* full_name;
};

const AbbrPair InlineKeyAbbr[] = {
    {"BPC", "BitsPerComponent"}, {"CS", "ColorSpace"}, {"D", "Decode"},
    {"DP", "DecodeParms"},       {"F", "Filter"},      {"H", "Height"},
    {"IM", "ImageMask"},         {"I", "Interpolate"}, {"W", "Width"},
};

const AbbrPair InlineValueAbbr[] = {
    {"G", "DeviceGray"},       {"RGB", "DeviceRGB"},
    {"CMYK", "DeviceCMYK"},    {"I", "Indexed"},
    {"AHx", "ASCIIHexDecode"}, {"A85", "ASCII85Decode"},
    {"LZW", "LZWDecode"},      {"Fl", "FlateDecode"},
    {"RL", "RunLengthDecode"}, {"CCF", "CCITTFaxDecode"},
    {"DCT", "DCTDecode"},
};

struct AbbrReplacementOp {
  bool is_replace_key;
  CFX_ByteString key;
  CFX_ByteStringC replacement;
};

CFX_ByteStringC FindFullName(const AbbrPair* table,
                             size_t count,
                             const CFX_ByteStringC& abbr) {
  auto it = std::find_if(table, table + count, [abbr](const AbbrPair& pair) {
    return pair.abbr == abbr;
  });
  return it != table + count ? CFX_ByteStringC(it->full_name)
                             : CFX_ByteStringC();
}

void ReplaceAbbr(CPDF_Object* pObj) {
  switch (pObj->GetType()) {
    case CPDF_Object::DICTIONARY: {
      CPDF_Dictionary* pDict = pObj->AsDictionary();
      std::vector<AbbrReplacementOp> replacements;
      for (const auto& it : *pDict) {
        CFX_ByteString key = it.first;
        CPDF_Object* value = it.second.get();
        CFX_ByteStringC fullname = FindFullName(
            InlineKeyAbbr, FX_ArraySize(InlineKeyAbbr), key.AsStringC());
        if (!fullname.IsEmpty()) {
          AbbrReplacementOp op;
          op.is_replace_key = true;
          op.key = key;
          op.replacement = fullname;
          replacements.push_back(op);
          key = fullname;
        }

        if (value->IsName()) {
          CFX_ByteString name = value->GetString();
          fullname = FindFullName(
              InlineValueAbbr, FX_ArraySize(InlineValueAbbr), name.AsStringC());
          if (!fullname.IsEmpty()) {
            AbbrReplacementOp op;
            op.is_replace_key = false;
            op.key = key;
            op.replacement = fullname;
            replacements.push_back(op);
          }
        } else {
          ReplaceAbbr(value);
        }
      }
      for (const auto& op : replacements) {
        if (op.is_replace_key)
          pDict->ReplaceKey(op.key, CFX_ByteString(op.replacement));
        else
          pDict->SetNewFor<CPDF_Name>(op.key, CFX_ByteString(op.replacement));
      }
      break;
    }
    case CPDF_Object::ARRAY: {
      CPDF_Array* pArray = pObj->AsArray();
      for (size_t i = 0; i < pArray->GetCount(); i++) {
        CPDF_Object* pElement = pArray->GetObjectAt(i);
        if (pElement->IsName()) {
          CFX_ByteString name = pElement->GetString();
          CFX_ByteStringC fullname = FindFullName(
              InlineValueAbbr, FX_ArraySize(InlineValueAbbr), name.AsStringC());
          if (!fullname.IsEmpty())
            pArray->SetNewAt<CPDF_Name>(i, CFX_ByteString(fullname));
        } else {
          ReplaceAbbr(pElement);
        }
      }
      break;
    }
    default:
      break;
  }
}

}  // namespace

CFX_ByteStringC PDF_FindKeyAbbreviationForTesting(const CFX_ByteStringC& abbr) {
  return FindFullName(InlineKeyAbbr, FX_ArraySize(InlineKeyAbbr), abbr);
}

CFX_ByteStringC PDF_FindValueAbbreviationForTesting(
    const CFX_ByteStringC& abbr) {
  return FindFullName(InlineValueAbbr, FX_ArraySize(InlineValueAbbr), abbr);
}

CPDF_StreamContentParser::CPDF_StreamContentParser(
    CPDF_Document* pDocument,
    CPDF_Dictionary* pPageResources,
    CPDF_Dictionary* pParentResources,
    const CFX_Matrix* pmtContentToUser,
    CPDF_PageObjectHolder* pObjHolder,
    CPDF_Dictionary* pResources,
    CFX_FloatRect* pBBox,
    CPDF_AllStates* pStates,
    int level)
    : m_pDocument(pDocument),
      m_pPageResources(pPageResources),
      m_pParentResources(pParentResources),
      m_pResources(pResources),
      m_pObjectHolder(pObjHolder),
      m_Level(level),
      m_ParamStartPos(0),
      m_ParamCount(0),
      m_pCurStates(new CPDF_AllStates),
      m_pLastTextObject(nullptr),
      m_DefFontSize(0),
      m_pPathPoints(nullptr),
      m_PathPointCount(0),
      m_PathAllocSize(0),
      m_PathStartX(0.0f),
      m_PathStartY(0.0f),
      m_PathCurrentX(0.0f),
      m_PathCurrentY(0.0f),
      m_PathClipType(0),
      m_pLastImage(nullptr),
      m_bColored(false),
      m_bResourceMissing(false) {
  if (pmtContentToUser)
    m_mtContentToUser = *pmtContentToUser;
  if (!m_pResources)
    m_pResources = m_pParentResources;
  if (!m_pResources)
    m_pResources = m_pPageResources;
  if (pBBox)
    m_BBox = *pBBox;
  if (pStates) {
    m_pCurStates->Copy(*pStates);
  } else {
    m_pCurStates->m_GeneralState.Emplace();
    m_pCurStates->m_GraphState.Emplace();
    m_pCurStates->m_TextState.Emplace();
    m_pCurStates->m_ColorState.Emplace();
  }
  for (size_t i = 0; i < FX_ArraySize(m_Type3Data); ++i) {
    m_Type3Data[i] = 0.0;
  }
}

CPDF_StreamContentParser::~CPDF_StreamContentParser() {
  ClearAllParams();
  FX_Free(m_pPathPoints);
}

int CPDF_StreamContentParser::GetNextParamPos() {
  if (m_ParamCount == kParamBufSize) {
    m_ParamStartPos++;
    if (m_ParamStartPos == kParamBufSize) {
      m_ParamStartPos = 0;
    }
    if (m_ParamBuf[m_ParamStartPos].m_Type == ContentParam::OBJECT)
      m_ParamBuf[m_ParamStartPos].m_pObject.reset();

    return m_ParamStartPos;
  }
  int index = m_ParamStartPos + m_ParamCount;
  if (index >= kParamBufSize) {
    index -= kParamBufSize;
  }
  m_ParamCount++;
  return index;
}

void CPDF_StreamContentParser::AddNameParam(const CFX_ByteStringC& bsName) {
  ContentParam& param = m_ParamBuf[GetNextParamPos()];
  if (bsName.GetLength() > 32) {
    param.m_Type = ContentParam::OBJECT;
    param.m_pObject = pdfium::MakeUnique<CPDF_Name>(
        m_pDocument->GetByteStringPool(), PDF_NameDecode(bsName));
  } else {
    param.m_Type = ContentParam::NAME;
    if (bsName.Find('#') == -1) {
      FXSYS_memcpy(param.m_Name.m_Buffer, bsName.raw_str(), bsName.GetLength());
      param.m_Name.m_Len = bsName.GetLength();
    } else {
      CFX_ByteString str = PDF_NameDecode(bsName);
      FXSYS_memcpy(param.m_Name.m_Buffer, str.c_str(), str.GetLength());
      param.m_Name.m_Len = str.GetLength();
    }
  }
}

void CPDF_StreamContentParser::AddNumberParam(const CFX_ByteStringC& str) {
  ContentParam& param = m_ParamBuf[GetNextParamPos()];
  param.m_Type = ContentParam::NUMBER;
  param.m_Number.m_bInteger = FX_atonum(str, &param.m_Number.m_Integer);
}

void CPDF_StreamContentParser::AddObjectParam(
    std::unique_ptr<CPDF_Object> pObj) {
  ContentParam& param = m_ParamBuf[GetNextParamPos()];
  param.m_Type = ContentParam::OBJECT;
  param.m_pObject = std::move(pObj);
}

void CPDF_StreamContentParser::ClearAllParams() {
  uint32_t index = m_ParamStartPos;
  for (uint32_t i = 0; i < m_ParamCount; i++) {
    if (m_ParamBuf[index].m_Type == ContentParam::OBJECT)
      m_ParamBuf[index].m_pObject.reset();
    index++;
    if (index == kParamBufSize)
      index = 0;
  }
  m_ParamStartPos = 0;
  m_ParamCount = 0;
}

CPDF_Object* CPDF_StreamContentParser::GetObject(uint32_t index) {
  if (index >= m_ParamCount) {
    return nullptr;
  }
  int real_index = m_ParamStartPos + m_ParamCount - index - 1;
  if (real_index >= kParamBufSize) {
    real_index -= kParamBufSize;
  }
  ContentParam& param = m_ParamBuf[real_index];
  if (param.m_Type == ContentParam::NUMBER) {
    param.m_Type = ContentParam::OBJECT;
    param.m_pObject =
        param.m_Number.m_bInteger
            ? pdfium::MakeUnique<CPDF_Number>(param.m_Number.m_Integer)
            : pdfium::MakeUnique<CPDF_Number>(param.m_Number.m_Float);
    return param.m_pObject.get();
  }
  if (param.m_Type == ContentParam::NAME) {
    param.m_Type = ContentParam::OBJECT;
    param.m_pObject = pdfium::MakeUnique<CPDF_Name>(
        m_pDocument->GetByteStringPool(),
        CFX_ByteString(param.m_Name.m_Buffer, param.m_Name.m_Len));
    return param.m_pObject.get();
  }
  if (param.m_Type == ContentParam::OBJECT)
    return param.m_pObject.get();

  ASSERT(false);
  return nullptr;
}

CFX_ByteString CPDF_StreamContentParser::GetString(uint32_t index) {
  if (index >= m_ParamCount) {
    return CFX_ByteString();
  }
  int real_index = m_ParamStartPos + m_ParamCount - index - 1;
  if (real_index >= kParamBufSize) {
    real_index -= kParamBufSize;
  }
  ContentParam& param = m_ParamBuf[real_index];
  if (param.m_Type == ContentParam::NAME) {
    return CFX_ByteString(param.m_Name.m_Buffer, param.m_Name.m_Len);
  }
  if (param.m_Type == 0 && param.m_pObject) {
    return param.m_pObject->GetString();
  }
  return CFX_ByteString();
}

FX_FLOAT CPDF_StreamContentParser::GetNumber(uint32_t index) {
  if (index >= m_ParamCount) {
    return 0;
  }
  int real_index = m_ParamStartPos + m_ParamCount - index - 1;
  if (real_index >= kParamBufSize) {
    real_index -= kParamBufSize;
  }
  ContentParam& param = m_ParamBuf[real_index];
  if (param.m_Type == ContentParam::NUMBER) {
    return param.m_Number.m_bInteger ? (FX_FLOAT)param.m_Number.m_Integer
                                     : param.m_Number.m_Float;
  }
  if (param.m_Type == 0 && param.m_pObject) {
    return param.m_pObject->GetNumber();
  }
  return 0;
}

void CPDF_StreamContentParser::SetGraphicStates(CPDF_PageObject* pObj,
                                                bool bColor,
                                                bool bText,
                                                bool bGraph) {
  pObj->m_GeneralState = m_pCurStates->m_GeneralState;
  pObj->m_ClipPath = m_pCurStates->m_ClipPath;
  pObj->m_ContentMark = m_CurContentMark;
  if (bColor) {
    pObj->m_ColorState = m_pCurStates->m_ColorState;
  }
  if (bGraph) {
    pObj->m_GraphState = m_pCurStates->m_GraphState;
  }
  if (bText) {
    pObj->m_TextState = m_pCurStates->m_TextState;
  }
}

// static
CPDF_StreamContentParser::OpCodes
CPDF_StreamContentParser::InitializeOpCodes() {
  return OpCodes({
      {FXBSTR_ID('"', 0, 0, 0),
       &CPDF_StreamContentParser::Handle_NextLineShowText_Space},
      {FXBSTR_ID('\'', 0, 0, 0),
       &CPDF_StreamContentParser::Handle_NextLineShowText},
      {FXBSTR_ID('B', 0, 0, 0),
       &CPDF_StreamContentParser::Handle_FillStrokePath},
      {FXBSTR_ID('B', '*', 0, 0),
       &CPDF_StreamContentParser::Handle_EOFillStrokePath},
      {FXBSTR_ID('B', 'D', 'C', 0),
       &CPDF_StreamContentParser::Handle_BeginMarkedContent_Dictionary},
      {FXBSTR_ID('B', 'I', 0, 0), &CPDF_StreamContentParser::Handle_BeginImage},
      {FXBSTR_ID('B', 'M', 'C', 0),
       &CPDF_StreamContentParser::Handle_BeginMarkedContent},
      {FXBSTR_ID('B', 'T', 0, 0), &CPDF_StreamContentParser::Handle_BeginText},
      {FXBSTR_ID('C', 'S', 0, 0),
       &CPDF_StreamContentParser::Handle_SetColorSpace_Stroke},
      {FXBSTR_ID('D', 'P', 0, 0),
       &CPDF_StreamContentParser::Handle_MarkPlace_Dictionary},
      {FXBSTR_ID('D', 'o', 0, 0),
       &CPDF_StreamContentParser::Handle_ExecuteXObject},
      {FXBSTR_ID('E', 'I', 0, 0), &CPDF_StreamContentParser::Handle_EndImage},
      {FXBSTR_ID('E', 'M', 'C', 0),
       &CPDF_StreamContentParser::Handle_EndMarkedContent},
      {FXBSTR_ID('E', 'T', 0, 0), &CPDF_StreamContentParser::Handle_EndText},
      {FXBSTR_ID('F', 0, 0, 0), &CPDF_StreamContentParser::Handle_FillPathOld},
      {FXBSTR_ID('G', 0, 0, 0),
       &CPDF_StreamContentParser::Handle_SetGray_Stroke},
      {FXBSTR_ID('I', 'D', 0, 0),
       &CPDF_StreamContentParser::Handle_BeginImageData},
      {FXBSTR_ID('J', 0, 0, 0), &CPDF_StreamContentParser::Handle_SetLineCap},
      {FXBSTR_ID('K', 0, 0, 0),
       &CPDF_StreamContentParser::Handle_SetCMYKColor_Stroke},
      {FXBSTR_ID('M', 0, 0, 0),
       &CPDF_StreamContentParser::Handle_SetMiterLimit},
      {FXBSTR_ID('M', 'P', 0, 0), &CPDF_StreamContentParser::Handle_MarkPlace},
      {FXBSTR_ID('Q', 0, 0, 0),
       &CPDF_StreamContentParser::Handle_RestoreGraphState},
      {FXBSTR_ID('R', 'G', 0, 0),
       &CPDF_StreamContentParser::Handle_SetRGBColor_Stroke},
      {FXBSTR_ID('S', 0, 0, 0), &CPDF_StreamContentParser::Handle_StrokePath},
      {FXBSTR_ID('S', 'C', 0, 0),
       &CPDF_StreamContentParser::Handle_SetColor_Stroke},
      {FXBSTR_ID('S', 'C', 'N', 0),
       &CPDF_StreamContentParser::Handle_SetColorPS_Stroke},
      {FXBSTR_ID('T', '*', 0, 0),
       &CPDF_StreamContentParser::Handle_MoveToNextLine},
      {FXBSTR_ID('T', 'D', 0, 0),
       &CPDF_StreamContentParser::Handle_MoveTextPoint_SetLeading},
      {FXBSTR_ID('T', 'J', 0, 0),
       &CPDF_StreamContentParser::Handle_ShowText_Positioning},
      {FXBSTR_ID('T', 'L', 0, 0),
       &CPDF_StreamContentParser::Handle_SetTextLeading},
      {FXBSTR_ID('T', 'c', 0, 0),
       &CPDF_StreamContentParser::Handle_SetCharSpace},
      {FXBSTR_ID('T', 'd', 0, 0),
       &CPDF_StreamContentParser::Handle_MoveTextPoint},
      {FXBSTR_ID('T', 'f', 0, 0), &CPDF_StreamContentParser::Handle_SetFont},
      {FXBSTR_ID('T', 'j', 0, 0), &CPDF_StreamContentParser::Handle_ShowText},
      {FXBSTR_ID('T', 'm', 0, 0),
       &CPDF_StreamContentParser::Handle_SetTextMatrix},
      {FXBSTR_ID('T', 'r', 0, 0),
       &CPDF_StreamContentParser::Handle_SetTextRenderMode},
      {FXBSTR_ID('T', 's', 0, 0),
       &CPDF_StreamContentParser::Handle_SetTextRise},
      {FXBSTR_ID('T', 'w', 0, 0),
       &CPDF_StreamContentParser::Handle_SetWordSpace},
      {FXBSTR_ID('T', 'z', 0, 0),
       &CPDF_StreamContentParser::Handle_SetHorzScale},
      {FXBSTR_ID('W', 0, 0, 0), &CPDF_StreamContentParser::Handle_Clip},
      {FXBSTR_ID('W', '*', 0, 0), &CPDF_StreamContentParser::Handle_EOClip},
      {FXBSTR_ID('b', 0, 0, 0),
       &CPDF_StreamContentParser::Handle_CloseFillStrokePath},
      {FXBSTR_ID('b', '*', 0, 0),
       &CPDF_StreamContentParser::Handle_CloseEOFillStrokePath},
      {FXBSTR_ID('c', 0, 0, 0), &CPDF_StreamContentParser::Handle_CurveTo_123},
      {FXBSTR_ID('c', 'm', 0, 0),
       &CPDF_StreamContentParser::Handle_ConcatMatrix},
      {FXBSTR_ID('c', 's', 0, 0),
       &CPDF_StreamContentParser::Handle_SetColorSpace_Fill},
      {FXBSTR_ID('d', 0, 0, 0), &CPDF_StreamContentParser::Handle_SetDash},
      {FXBSTR_ID('d', '0', 0, 0),
       &CPDF_StreamContentParser::Handle_SetCharWidth},
      {FXBSTR_ID('d', '1', 0, 0),
       &CPDF_StreamContentParser::Handle_SetCachedDevice},
      {FXBSTR_ID('f', 0, 0, 0), &CPDF_StreamContentParser::Handle_FillPath},
      {FXBSTR_ID('f', '*', 0, 0), &CPDF_StreamContentParser::Handle_EOFillPath},
      {FXBSTR_ID('g', 0, 0, 0), &CPDF_StreamContentParser::Handle_SetGray_Fill},
      {FXBSTR_ID('g', 's', 0, 0),
       &CPDF_StreamContentParser::Handle_SetExtendGraphState},
      {FXBSTR_ID('h', 0, 0, 0), &CPDF_StreamContentParser::Handle_ClosePath},
      {FXBSTR_ID('i', 0, 0, 0), &CPDF_StreamContentParser::Handle_SetFlat},
      {FXBSTR_ID('j', 0, 0, 0), &CPDF_StreamContentParser::Handle_SetLineJoin},
      {FXBSTR_ID('k', 0, 0, 0),
       &CPDF_StreamContentParser::Handle_SetCMYKColor_Fill},
      {FXBSTR_ID('l', 0, 0, 0), &CPDF_StreamContentParser::Handle_LineTo},
      {FXBSTR_ID('m', 0, 0, 0), &CPDF_StreamContentParser::Handle_MoveTo},
      {FXBSTR_ID('n', 0, 0, 0), &CPDF_StreamContentParser::Handle_EndPath},
      {FXBSTR_ID('q', 0, 0, 0),
       &CPDF_StreamContentParser::Handle_SaveGraphState},
      {FXBSTR_ID('r', 'e', 0, 0), &CPDF_StreamContentParser::Handle_Rectangle},
      {FXBSTR_ID('r', 'g', 0, 0),
       &CPDF_StreamContentParser::Handle_SetRGBColor_Fill},
      {FXBSTR_ID('r', 'i', 0, 0),
       &CPDF_StreamContentParser::Handle_SetRenderIntent},
      {FXBSTR_ID('s', 0, 0, 0),
       &CPDF_StreamContentParser::Handle_CloseStrokePath},
      {FXBSTR_ID('s', 'c', 0, 0),
       &CPDF_StreamContentParser::Handle_SetColor_Fill},
      {FXBSTR_ID('s', 'c', 'n', 0),
       &CPDF_StreamContentParser::Handle_SetColorPS_Fill},
      {FXBSTR_ID('s', 'h', 0, 0), &CPDF_StreamContentParser::Handle_ShadeFill},
      {FXBSTR_ID('v', 0, 0, 0), &CPDF_StreamContentParser::Handle_CurveTo_23},
      {FXBSTR_ID('w', 0, 0, 0), &CPDF_StreamContentParser::Handle_SetLineWidth},
      {FXBSTR_ID('y', 0, 0, 0), &CPDF_StreamContentParser::Handle_CurveTo_13},
  });
}

void CPDF_StreamContentParser::OnOperator(const CFX_ByteStringC& op) {
  static const OpCodes s_OpCodes = InitializeOpCodes();

  auto it = s_OpCodes.find(op.GetID());
  if (it != s_OpCodes.end())
    (this->*it->second)();
}

void CPDF_StreamContentParser::Handle_CloseFillStrokePath() {
  Handle_ClosePath();
  AddPathObject(FXFILL_WINDING, true);
}

void CPDF_StreamContentParser::Handle_FillStrokePath() {
  AddPathObject(FXFILL_WINDING, true);
}

void CPDF_StreamContentParser::Handle_CloseEOFillStrokePath() {
  AddPathPoint(m_PathStartX, m_PathStartY, FXPT_TYPE::LineTo, true);
  AddPathObject(FXFILL_ALTERNATE, true);
}

void CPDF_StreamContentParser::Handle_EOFillStrokePath() {
  AddPathObject(FXFILL_ALTERNATE, true);
}

void CPDF_StreamContentParser::Handle_BeginMarkedContent_Dictionary() {
  CFX_ByteString tag = GetString(1);
  CPDF_Object* pProperty = GetObject(0);
  if (!pProperty) {
    return;
  }
  bool bDirect = true;
  if (pProperty->IsName()) {
    pProperty = FindResourceObj("Properties", pProperty->GetString());
    if (!pProperty)
      return;
    bDirect = false;
  }
  if (CPDF_Dictionary* pDict = pProperty->AsDictionary()) {
    m_CurContentMark.AddMark(tag, pDict, bDirect);
  }
}

void CPDF_StreamContentParser::Handle_BeginImage() {
  FX_FILESIZE savePos = m_pSyntax->GetPos();
  auto pDict =
      pdfium::MakeUnique<CPDF_Dictionary>(m_pDocument->GetByteStringPool());
  while (1) {
    CPDF_StreamParser::SyntaxType type = m_pSyntax->ParseNextElement();
    if (type == CPDF_StreamParser::Keyword) {
      if (m_pSyntax->GetWord() != "ID") {
        m_pSyntax->SetPos(savePos);
        return;
      }
    }
    if (type != CPDF_StreamParser::Name) {
      break;
    }
    CFX_ByteString key(m_pSyntax->GetWord().Mid(1));
    auto pObj = m_pSyntax->ReadNextObject(false, false, 0);
    if (!key.IsEmpty()) {
      uint32_t dwObjNum = pObj ? pObj->GetObjNum() : 0;
      if (dwObjNum)
        pDict->SetNewFor<CPDF_Reference>(key, m_pDocument, dwObjNum);
      else
        pDict->SetFor(key, std::move(pObj));
    }
  }
  ReplaceAbbr(pDict.get());
  CPDF_Object* pCSObj = nullptr;
  if (pDict->KeyExist("ColorSpace")) {
    pCSObj = pDict->GetDirectObjectFor("ColorSpace");
    if (pCSObj->IsName()) {
      CFX_ByteString name = pCSObj->GetString();
      if (name != "DeviceRGB" && name != "DeviceGray" && name != "DeviceCMYK") {
        pCSObj = FindResourceObj("ColorSpace", name);
        if (pCSObj && pCSObj->IsInline())
          pDict->SetFor("ColorSpace", pCSObj->Clone());
      }
    }
  }
  pDict->SetNewFor<CPDF_Name>("Subtype", "Image");
  std::unique_ptr<CPDF_Stream> pStream =
      m_pSyntax->ReadInlineStream(m_pDocument, std::move(pDict), pCSObj);
  while (1) {
    CPDF_StreamParser::SyntaxType type = m_pSyntax->ParseNextElement();
    if (type == CPDF_StreamParser::EndOfData) {
      break;
    }
    if (type != CPDF_StreamParser::Keyword) {
      continue;
    }
    if (m_pSyntax->GetWord() == "EI") {
      break;
    }
  }
  AddImage(std::move(pStream));
}

void CPDF_StreamContentParser::Handle_BeginMarkedContent() {
  m_CurContentMark.AddMark(GetString(0), nullptr, false);
}

void CPDF_StreamContentParser::Handle_BeginText() {
  m_pCurStates->m_TextMatrix = CFX_Matrix();
  OnChangeTextMatrix();
  m_pCurStates->m_TextPos = CFX_PointF();
  m_pCurStates->m_TextLinePos = CFX_PointF();
}

void CPDF_StreamContentParser::Handle_CurveTo_123() {
  AddPathPoint(GetNumber(5), GetNumber(4), FXPT_TYPE::BezierTo, false);
  AddPathPoint(GetNumber(3), GetNumber(2), FXPT_TYPE::BezierTo, false);
  AddPathPoint(GetNumber(1), GetNumber(0), FXPT_TYPE::BezierTo, false);
}

void CPDF_StreamContentParser::Handle_ConcatMatrix() {
  CFX_Matrix new_matrix(GetNumber(5), GetNumber(4), GetNumber(3), GetNumber(2),
                        GetNumber(1), GetNumber(0));
  new_matrix.Concat(m_pCurStates->m_CTM);
  m_pCurStates->m_CTM = new_matrix;
  OnChangeTextMatrix();
}

void CPDF_StreamContentParser::Handle_SetColorSpace_Fill() {
  CPDF_ColorSpace* pCS = FindColorSpace(GetString(0));
  if (!pCS)
    return;

  m_pCurStates->m_ColorState.GetMutableFillColor()->SetColorSpace(pCS);
}

void CPDF_StreamContentParser::Handle_SetColorSpace_Stroke() {
  CPDF_ColorSpace* pCS = FindColorSpace(GetString(0));
  if (!pCS)
    return;

  m_pCurStates->m_ColorState.GetMutableStrokeColor()->SetColorSpace(pCS);
}

void CPDF_StreamContentParser::Handle_SetDash() {
  CPDF_Array* pArray = ToArray(GetObject(1));
  if (!pArray)
    return;

  m_pCurStates->SetLineDash(pArray, GetNumber(0), 1.0f);
}

void CPDF_StreamContentParser::Handle_SetCharWidth() {
  m_Type3Data[0] = GetNumber(1);
  m_Type3Data[1] = GetNumber(0);
  m_bColored = true;
}

void CPDF_StreamContentParser::Handle_SetCachedDevice() {
  for (int i = 0; i < 6; i++) {
    m_Type3Data[i] = GetNumber(5 - i);
  }
  m_bColored = false;
}

void CPDF_StreamContentParser::Handle_ExecuteXObject() {
  CFX_ByteString name = GetString(0);
  if (name == m_LastImageName && m_pLastImage && m_pLastImage->GetStream() &&
      m_pLastImage->GetStream()->GetObjNum()) {
    AddImage(m_pLastImage);
    return;
  }

  CPDF_Stream* pXObject = ToStream(FindResourceObj("XObject", name));
  if (!pXObject) {
    m_bResourceMissing = true;
    return;
  }

  CFX_ByteString type;
  if (pXObject->GetDict())
    type = pXObject->GetDict()->GetStringFor("Subtype");

  if (type == "Image") {
    CPDF_ImageObject* pObj = pXObject->IsInline()
                                 ? AddImage(std::unique_ptr<CPDF_Stream>(
                                       ToStream(pXObject->Clone())))
                                 : AddImage(pXObject->GetObjNum());

    m_LastImageName = name;
    m_pLastImage = pObj->GetImage();
    if (!m_pObjectHolder->HasImageMask())
      m_pObjectHolder->SetHasImageMask(m_pLastImage->IsMask());
  } else if (type == "Form") {
    AddForm(pXObject);
  }
}

void CPDF_StreamContentParser::AddForm(CPDF_Stream* pStream) {
  std::unique_ptr<CPDF_FormObject> pFormObj(new CPDF_FormObject);
  pFormObj->m_pForm.reset(
      new CPDF_Form(m_pDocument, m_pPageResources, pStream, m_pResources));
  pFormObj->m_FormMatrix = m_pCurStates->m_CTM;
  pFormObj->m_FormMatrix.Concat(m_mtContentToUser);
  CPDF_AllStates status;
  status.m_GeneralState = m_pCurStates->m_GeneralState;
  status.m_GraphState = m_pCurStates->m_GraphState;
  status.m_ColorState = m_pCurStates->m_ColorState;
  status.m_TextState = m_pCurStates->m_TextState;
  pFormObj->m_pForm->ParseContent(&status, nullptr, nullptr, m_Level + 1);
  if (!m_pObjectHolder->BackgroundAlphaNeeded() &&
      pFormObj->m_pForm->BackgroundAlphaNeeded()) {
    m_pObjectHolder->SetBackgroundAlphaNeeded(true);
  }
  pFormObj->CalcBoundingBox();
  SetGraphicStates(pFormObj.get(), true, true, true);
  m_pObjectHolder->GetPageObjectList()->push_back(std::move(pFormObj));
}

CPDF_ImageObject* CPDF_StreamContentParser::AddImage(
    std::unique_ptr<CPDF_Stream> pStream) {
  if (!pStream)
    return nullptr;

  auto pImageObj = pdfium::MakeUnique<CPDF_ImageObject>();
  pImageObj->SetOwnedImage(
      pdfium::MakeUnique<CPDF_Image>(m_pDocument, std::move(pStream)));
  return AddImageObject(std::move(pImageObj));
}

CPDF_ImageObject* CPDF_StreamContentParser::AddImage(uint32_t streamObjNum) {
  auto pImageObj = pdfium::MakeUnique<CPDF_ImageObject>();
  pImageObj->SetUnownedImage(m_pDocument->LoadImageFromPageData(streamObjNum));
  return AddImageObject(std::move(pImageObj));
}

CPDF_ImageObject* CPDF_StreamContentParser::AddImage(CPDF_Image* pImage) {
  if (!pImage)
    return nullptr;

  auto pImageObj = pdfium::MakeUnique<CPDF_ImageObject>();
  pImageObj->SetUnownedImage(
      m_pDocument->GetPageData()->GetImage(pImage->GetStream()->GetObjNum()));

  return AddImageObject(std::move(pImageObj));
}

CPDF_ImageObject* CPDF_StreamContentParser::AddImageObject(
    std::unique_ptr<CPDF_ImageObject> pImageObj) {
  SetGraphicStates(pImageObj.get(), pImageObj->GetImage()->IsMask(), false,
                   false);

  CFX_Matrix ImageMatrix = m_pCurStates->m_CTM;
  ImageMatrix.Concat(m_mtContentToUser);
  pImageObj->set_matrix(ImageMatrix);
  pImageObj->CalcBoundingBox();

  CPDF_ImageObject* pRet = pImageObj.get();
  m_pObjectHolder->GetPageObjectList()->push_back(std::move(pImageObj));
  return pRet;
}

void CPDF_StreamContentParser::Handle_MarkPlace_Dictionary() {}

void CPDF_StreamContentParser::Handle_EndImage() {}

void CPDF_StreamContentParser::Handle_EndMarkedContent() {
  if (m_CurContentMark)
    m_CurContentMark.DeleteLastMark();
}

void CPDF_StreamContentParser::Handle_EndText() {
  if (m_ClipTextList.empty())
    return;

  if (TextRenderingModeIsClipMode(m_pCurStates->m_TextState.GetTextMode()))
    m_pCurStates->m_ClipPath.AppendTexts(&m_ClipTextList);

  m_ClipTextList.clear();
}

void CPDF_StreamContentParser::Handle_FillPath() {
  AddPathObject(FXFILL_WINDING, false);
}

void CPDF_StreamContentParser::Handle_FillPathOld() {
  AddPathObject(FXFILL_WINDING, false);
}

void CPDF_StreamContentParser::Handle_EOFillPath() {
  AddPathObject(FXFILL_ALTERNATE, false);
}

void CPDF_StreamContentParser::Handle_SetGray_Fill() {
  FX_FLOAT value = GetNumber(0);
  CPDF_ColorSpace* pCS = CPDF_ColorSpace::GetStockCS(PDFCS_DEVICEGRAY);
  m_pCurStates->m_ColorState.SetFillColor(pCS, &value, 1);
}

void CPDF_StreamContentParser::Handle_SetGray_Stroke() {
  FX_FLOAT value = GetNumber(0);
  CPDF_ColorSpace* pCS = CPDF_ColorSpace::GetStockCS(PDFCS_DEVICEGRAY);
  m_pCurStates->m_ColorState.SetStrokeColor(pCS, &value, 1);
}

void CPDF_StreamContentParser::Handle_SetExtendGraphState() {
  CFX_ByteString name = GetString(0);
  CPDF_Dictionary* pGS = ToDictionary(FindResourceObj("ExtGState", name));
  if (!pGS) {
    m_bResourceMissing = true;
    return;
  }
  m_pCurStates->ProcessExtGS(pGS, this);
}

void CPDF_StreamContentParser::Handle_ClosePath() {
  if (m_PathPointCount == 0) {
    return;
  }
  if (m_PathStartX != m_PathCurrentX || m_PathStartY != m_PathCurrentY) {
    AddPathPoint(m_PathStartX, m_PathStartY, FXPT_TYPE::LineTo, true);
  } else if (m_pPathPoints[m_PathPointCount - 1].m_Type != FXPT_TYPE::MoveTo) {
    m_pPathPoints[m_PathPointCount - 1].m_CloseFigure = true;
  }
}

void CPDF_StreamContentParser::Handle_SetFlat() {
  m_pCurStates->m_GeneralState.SetFlatness(GetNumber(0));
}

void CPDF_StreamContentParser::Handle_BeginImageData() {}

void CPDF_StreamContentParser::Handle_SetLineJoin() {
  m_pCurStates->m_GraphState.SetLineJoin(
      static_cast<CFX_GraphStateData::LineJoin>(GetInteger(0)));
}

void CPDF_StreamContentParser::Handle_SetLineCap() {
  m_pCurStates->m_GraphState.SetLineCap(
      static_cast<CFX_GraphStateData::LineCap>(GetInteger(0)));
}

void CPDF_StreamContentParser::Handle_SetCMYKColor_Fill() {
  if (m_ParamCount != 4)
    return;

  FX_FLOAT values[4];
  for (int i = 0; i < 4; i++) {
    values[i] = GetNumber(3 - i);
  }
  CPDF_ColorSpace* pCS = CPDF_ColorSpace::GetStockCS(PDFCS_DEVICECMYK);
  m_pCurStates->m_ColorState.SetFillColor(pCS, values, 4);
}

void CPDF_StreamContentParser::Handle_SetCMYKColor_Stroke() {
  if (m_ParamCount != 4)
    return;

  FX_FLOAT values[4];
  for (int i = 0; i < 4; i++) {
    values[i] = GetNumber(3 - i);
  }
  CPDF_ColorSpace* pCS = CPDF_ColorSpace::GetStockCS(PDFCS_DEVICECMYK);
  m_pCurStates->m_ColorState.SetStrokeColor(pCS, values, 4);
}

void CPDF_StreamContentParser::Handle_LineTo() {
  if (m_ParamCount != 2)
    return;

  AddPathPoint(GetNumber(1), GetNumber(0), FXPT_TYPE::LineTo, false);
}

void CPDF_StreamContentParser::Handle_MoveTo() {
  if (m_ParamCount != 2)
    return;

  AddPathPoint(GetNumber(1), GetNumber(0), FXPT_TYPE::MoveTo, false);
  ParsePathObject();
}

void CPDF_StreamContentParser::Handle_SetMiterLimit() {
  m_pCurStates->m_GraphState.SetMiterLimit(GetNumber(0));
}

void CPDF_StreamContentParser::Handle_MarkPlace() {}

void CPDF_StreamContentParser::Handle_EndPath() {
  AddPathObject(0, false);
}

void CPDF_StreamContentParser::Handle_SaveGraphState() {
  std::unique_ptr<CPDF_AllStates> pStates(new CPDF_AllStates);
  pStates->Copy(*m_pCurStates);
  m_StateStack.push_back(std::move(pStates));
}

void CPDF_StreamContentParser::Handle_RestoreGraphState() {
  if (m_StateStack.empty())
    return;
  std::unique_ptr<CPDF_AllStates> pStates = std::move(m_StateStack.back());
  m_StateStack.pop_back();
  m_pCurStates->Copy(*pStates);
}

void CPDF_StreamContentParser::Handle_Rectangle() {
  FX_FLOAT x = GetNumber(3), y = GetNumber(2);
  FX_FLOAT w = GetNumber(1), h = GetNumber(0);
  AddPathRect(x, y, w, h);
}

void CPDF_StreamContentParser::AddPathRect(FX_FLOAT x,
                                           FX_FLOAT y,
                                           FX_FLOAT w,
                                           FX_FLOAT h) {
  AddPathPoint(x, y, FXPT_TYPE::MoveTo, false);
  AddPathPoint(x + w, y, FXPT_TYPE::LineTo, false);
  AddPathPoint(x + w, y + h, FXPT_TYPE::LineTo, false);
  AddPathPoint(x, y + h, FXPT_TYPE::LineTo, false);
  AddPathPoint(x, y, FXPT_TYPE::LineTo, true);
}

void CPDF_StreamContentParser::Handle_SetRGBColor_Fill() {
  if (m_ParamCount != 3)
    return;

  FX_FLOAT values[3];
  for (int i = 0; i < 3; i++) {
    values[i] = GetNumber(2 - i);
  }
  CPDF_ColorSpace* pCS = CPDF_ColorSpace::GetStockCS(PDFCS_DEVICERGB);
  m_pCurStates->m_ColorState.SetFillColor(pCS, values, 3);
}

void CPDF_StreamContentParser::Handle_SetRGBColor_Stroke() {
  if (m_ParamCount != 3)
    return;

  FX_FLOAT values[3];
  for (int i = 0; i < 3; i++) {
    values[i] = GetNumber(2 - i);
  }
  CPDF_ColorSpace* pCS = CPDF_ColorSpace::GetStockCS(PDFCS_DEVICERGB);
  m_pCurStates->m_ColorState.SetStrokeColor(pCS, values, 3);
}

void CPDF_StreamContentParser::Handle_SetRenderIntent() {}

void CPDF_StreamContentParser::Handle_CloseStrokePath() {
  Handle_ClosePath();
  AddPathObject(0, true);
}

void CPDF_StreamContentParser::Handle_StrokePath() {
  AddPathObject(0, true);
}

void CPDF_StreamContentParser::Handle_SetColor_Fill() {
  FX_FLOAT values[4];
  int nargs = m_ParamCount;
  if (nargs > 4) {
    nargs = 4;
  }
  for (int i = 0; i < nargs; i++) {
    values[i] = GetNumber(nargs - i - 1);
  }
  m_pCurStates->m_ColorState.SetFillColor(nullptr, values, nargs);
}

void CPDF_StreamContentParser::Handle_SetColor_Stroke() {
  FX_FLOAT values[4];
  int nargs = m_ParamCount;
  if (nargs > 4) {
    nargs = 4;
  }
  for (int i = 0; i < nargs; i++) {
    values[i] = GetNumber(nargs - i - 1);
  }
  m_pCurStates->m_ColorState.SetStrokeColor(nullptr, values, nargs);
}

void CPDF_StreamContentParser::Handle_SetColorPS_Fill() {
  CPDF_Object* pLastParam = GetObject(0);
  if (!pLastParam) {
    return;
  }
  uint32_t nargs = m_ParamCount;
  uint32_t nvalues = nargs;
  if (pLastParam->IsName())
    nvalues--;
  FX_FLOAT* values = nullptr;
  if (nvalues) {
    values = FX_Alloc(FX_FLOAT, nvalues);
    for (uint32_t i = 0; i < nvalues; i++) {
      values[i] = GetNumber(nargs - i - 1);
    }
  }
  if (nvalues != nargs) {
    CPDF_Pattern* pPattern = FindPattern(GetString(0), false);
    if (pPattern) {
      m_pCurStates->m_ColorState.SetFillPattern(pPattern, values, nvalues);
    }
  } else {
    m_pCurStates->m_ColorState.SetFillColor(nullptr, values, nvalues);
  }
  FX_Free(values);
}

void CPDF_StreamContentParser::Handle_SetColorPS_Stroke() {
  CPDF_Object* pLastParam = GetObject(0);
  if (!pLastParam) {
    return;
  }
  int nargs = m_ParamCount;
  int nvalues = nargs;
  if (pLastParam->IsName())
    nvalues--;

  FX_FLOAT* values = nullptr;
  if (nvalues) {
    values = FX_Alloc(FX_FLOAT, nvalues);
    for (int i = 0; i < nvalues; i++) {
      values[i] = GetNumber(nargs - i - 1);
    }
  }
  if (nvalues != nargs) {
    CPDF_Pattern* pPattern = FindPattern(GetString(0), false);
    if (pPattern) {
      m_pCurStates->m_ColorState.SetStrokePattern(pPattern, values, nvalues);
    }
  } else {
    m_pCurStates->m_ColorState.SetStrokeColor(nullptr, values, nvalues);
  }
  FX_Free(values);
}

void CPDF_StreamContentParser::Handle_ShadeFill() {
  CPDF_Pattern* pPattern = FindPattern(GetString(0), true);
  if (!pPattern)
    return;

  CPDF_ShadingPattern* pShading = pPattern->AsShadingPattern();
  if (!pShading)
    return;

  if (!pShading->IsShadingObject() || !pShading->Load())
    return;

  std::unique_ptr<CPDF_ShadingObject> pObj(new CPDF_ShadingObject);
  pObj->m_pShading = pShading;
  SetGraphicStates(pObj.get(), false, false, false);
  pObj->m_Matrix = m_pCurStates->m_CTM;
  pObj->m_Matrix.Concat(m_mtContentToUser);
  CFX_FloatRect bbox =
      pObj->m_ClipPath ? pObj->m_ClipPath.GetClipBox() : m_BBox;
  if (pShading->IsMeshShading())
    bbox.Intersect(GetShadingBBox(pShading, pObj->m_Matrix));
  pObj->m_Left = bbox.left;
  pObj->m_Right = bbox.right;
  pObj->m_Top = bbox.top;
  pObj->m_Bottom = bbox.bottom;
  m_pObjectHolder->GetPageObjectList()->push_back(std::move(pObj));
}

void CPDF_StreamContentParser::Handle_SetCharSpace() {
  m_pCurStates->m_TextState.SetCharSpace(GetNumber(0));
}

void CPDF_StreamContentParser::Handle_MoveTextPoint() {
  m_pCurStates->m_TextLinePos += CFX_PointF(GetNumber(1), GetNumber(0));
  m_pCurStates->m_TextPos = m_pCurStates->m_TextLinePos;
}

void CPDF_StreamContentParser::Handle_MoveTextPoint_SetLeading() {
  Handle_MoveTextPoint();
  m_pCurStates->m_TextLeading = -GetNumber(0);
}

void CPDF_StreamContentParser::Handle_SetFont() {
  FX_FLOAT fs = GetNumber(0);
  if (fs == 0) {
    fs = m_DefFontSize;
  }
  m_pCurStates->m_TextState.SetFontSize(fs);
  CPDF_Font* pFont = FindFont(GetString(1));
  if (pFont) {
    m_pCurStates->m_TextState.SetFont(pFont);
  }
}

CPDF_Object* CPDF_StreamContentParser::FindResourceObj(
    const CFX_ByteString& type,
    const CFX_ByteString& name) {
  if (!m_pResources)
    return nullptr;
  CPDF_Dictionary* pDict = m_pResources->GetDictFor(type);
  if (pDict)
    return pDict->GetDirectObjectFor(name);
  if (m_pResources == m_pPageResources || !m_pPageResources)
    return nullptr;

  CPDF_Dictionary* pPageDict = m_pPageResources->GetDictFor(type);
  return pPageDict ? pPageDict->GetDirectObjectFor(name) : nullptr;
}

CPDF_Font* CPDF_StreamContentParser::FindFont(const CFX_ByteString& name) {
  CPDF_Dictionary* pFontDict = ToDictionary(FindResourceObj("Font", name));
  if (!pFontDict) {
    m_bResourceMissing = true;
    return CPDF_Font::GetStockFont(m_pDocument, "Helvetica");
  }

  CPDF_Font* pFont = m_pDocument->LoadFont(pFontDict);
  if (pFont && pFont->IsType3Font()) {
    pFont->AsType3Font()->SetPageResources(m_pResources);
    pFont->AsType3Font()->CheckType3FontMetrics();
  }
  return pFont;
}

CPDF_ColorSpace* CPDF_StreamContentParser::FindColorSpace(
    const CFX_ByteString& name) {
  if (name == "Pattern") {
    return CPDF_ColorSpace::GetStockCS(PDFCS_PATTERN);
  }
  if (name == "DeviceGray" || name == "DeviceCMYK" || name == "DeviceRGB") {
    CFX_ByteString defname = "Default";
    defname += name.Mid(7);
    CPDF_Object* pDefObj = FindResourceObj("ColorSpace", defname);
    if (!pDefObj) {
      if (name == "DeviceGray") {
        return CPDF_ColorSpace::GetStockCS(PDFCS_DEVICEGRAY);
      }
      if (name == "DeviceRGB") {
        return CPDF_ColorSpace::GetStockCS(PDFCS_DEVICERGB);
      }
      return CPDF_ColorSpace::GetStockCS(PDFCS_DEVICECMYK);
    }
    return m_pDocument->LoadColorSpace(pDefObj);
  }
  CPDF_Object* pCSObj = FindResourceObj("ColorSpace", name);
  if (!pCSObj) {
    m_bResourceMissing = true;
    return nullptr;
  }
  return m_pDocument->LoadColorSpace(pCSObj);
}

CPDF_Pattern* CPDF_StreamContentParser::FindPattern(const CFX_ByteString& name,
                                                    bool bShading) {
  CPDF_Object* pPattern =
      FindResourceObj(bShading ? "Shading" : "Pattern", name);
  if (!pPattern || (!pPattern->IsDictionary() && !pPattern->IsStream())) {
    m_bResourceMissing = true;
    return nullptr;
  }
  return m_pDocument->LoadPattern(pPattern, bShading,
                                  m_pCurStates->m_ParentMatrix);
}

void CPDF_StreamContentParser::AddTextObject(CFX_ByteString* pStrs,
                                             FX_FLOAT fInitKerning,
                                             FX_FLOAT* pKerning,
                                             int nsegs) {
  CPDF_Font* pFont = m_pCurStates->m_TextState.GetFont();
  if (!pFont) {
    return;
  }
  if (fInitKerning != 0) {
    if (!pFont->IsVertWriting()) {
      m_pCurStates->m_TextPos.x -=
          (fInitKerning * m_pCurStates->m_TextState.GetFontSize() *
           m_pCurStates->m_TextHorzScale) /
          1000;
    } else {
      m_pCurStates->m_TextPos.y -=
          (fInitKerning * m_pCurStates->m_TextState.GetFontSize()) / 1000;
    }
  }
  if (nsegs == 0) {
    return;
  }
  const TextRenderingMode text_mode =
      pFont->IsType3Font() ? TextRenderingMode::MODE_FILL
                           : m_pCurStates->m_TextState.GetTextMode();
  {
    std::unique_ptr<CPDF_TextObject> pText(new CPDF_TextObject);
    m_pLastTextObject = pText.get();
    SetGraphicStates(m_pLastTextObject, true, true, true);
    if (TextRenderingModeIsStrokeMode(text_mode)) {
      FX_FLOAT* pCTM = pText->m_TextState.GetMutableCTM();
      pCTM[0] = m_pCurStates->m_CTM.a;
      pCTM[1] = m_pCurStates->m_CTM.c;
      pCTM[2] = m_pCurStates->m_CTM.b;
      pCTM[3] = m_pCurStates->m_CTM.d;
    }
    pText->SetSegments(pStrs, pKerning, nsegs);
    pText->m_Pos = m_mtContentToUser.Transform(
        m_pCurStates->m_CTM.Transform(m_pCurStates->m_TextMatrix.Transform(
            CFX_PointF(m_pCurStates->m_TextPos.x,
                       m_pCurStates->m_TextPos.y + m_pCurStates->m_TextRise))));

    m_pCurStates->m_TextPos +=
        pText->CalcPositionData(m_pCurStates->m_TextHorzScale);
    if (TextRenderingModeIsClipMode(text_mode)) {
      m_ClipTextList.push_back(
          std::unique_ptr<CPDF_TextObject>(pText->Clone()));
    }
    m_pObjectHolder->GetPageObjectList()->push_back(std::move(pText));
  }
  if (pKerning && pKerning[nsegs - 1] != 0) {
    if (!pFont->IsVertWriting()) {
      m_pCurStates->m_TextPos.x -=
          (pKerning[nsegs - 1] * m_pCurStates->m_TextState.GetFontSize() *
           m_pCurStates->m_TextHorzScale) /
          1000;
    } else {
      m_pCurStates->m_TextPos.y -=
          (pKerning[nsegs - 1] * m_pCurStates->m_TextState.GetFontSize()) /
          1000;
    }
  }
}

void CPDF_StreamContentParser::Handle_ShowText() {
  CFX_ByteString str = GetString(0);
  if (str.IsEmpty()) {
    return;
  }
  AddTextObject(&str, 0, nullptr, 1);
}

void CPDF_StreamContentParser::Handle_ShowText_Positioning() {
  CPDF_Array* pArray = ToArray(GetObject(0));
  if (!pArray)
    return;

  size_t n = pArray->GetCount();
  size_t nsegs = 0;
  for (size_t i = 0; i < n; i++) {
    if (pArray->GetDirectObjectAt(i)->IsString())
      nsegs++;
  }
  if (nsegs == 0) {
    for (size_t i = 0; i < n; i++) {
      m_pCurStates->m_TextPos.x -=
          (pArray->GetNumberAt(i) * m_pCurStates->m_TextState.GetFontSize() *
           m_pCurStates->m_TextHorzScale) /
          1000;
    }
    return;
  }
  CFX_ByteString* pStrs = new CFX_ByteString[nsegs];
  FX_FLOAT* pKerning = FX_Alloc(FX_FLOAT, nsegs);
  size_t iSegment = 0;
  FX_FLOAT fInitKerning = 0;
  for (size_t i = 0; i < n; i++) {
    CPDF_Object* pObj = pArray->GetDirectObjectAt(i);
    if (pObj->IsString()) {
      CFX_ByteString str = pObj->GetString();
      if (str.IsEmpty()) {
        continue;
      }
      pStrs[iSegment] = str;
      pKerning[iSegment++] = 0;
    } else {
      FX_FLOAT num = pObj ? pObj->GetNumber() : 0;
      if (iSegment == 0) {
        fInitKerning += num;
      } else {
        pKerning[iSegment - 1] += num;
      }
    }
  }
  AddTextObject(pStrs, fInitKerning, pKerning, iSegment);
  delete[] pStrs;
  FX_Free(pKerning);
}

void CPDF_StreamContentParser::Handle_SetTextLeading() {
  m_pCurStates->m_TextLeading = GetNumber(0);
}

void CPDF_StreamContentParser::Handle_SetTextMatrix() {
  m_pCurStates->m_TextMatrix =
      CFX_Matrix(GetNumber(5), GetNumber(4), GetNumber(3), GetNumber(2),
                 GetNumber(1), GetNumber(0));
  OnChangeTextMatrix();
  m_pCurStates->m_TextPos = CFX_PointF();
  m_pCurStates->m_TextLinePos = CFX_PointF();
}

void CPDF_StreamContentParser::OnChangeTextMatrix() {
  CFX_Matrix text_matrix(m_pCurStates->m_TextHorzScale, 0.0f, 0.0f, 1.0f, 0.0f,
                         0.0f);
  text_matrix.Concat(m_pCurStates->m_TextMatrix);
  text_matrix.Concat(m_pCurStates->m_CTM);
  text_matrix.Concat(m_mtContentToUser);
  FX_FLOAT* pTextMatrix = m_pCurStates->m_TextState.GetMutableMatrix();
  pTextMatrix[0] = text_matrix.a;
  pTextMatrix[1] = text_matrix.c;
  pTextMatrix[2] = text_matrix.b;
  pTextMatrix[3] = text_matrix.d;
}

void CPDF_StreamContentParser::Handle_SetTextRenderMode() {
  TextRenderingMode mode;
  if (SetTextRenderingModeFromInt(GetInteger(0), &mode))
    m_pCurStates->m_TextState.SetTextMode(mode);
}

void CPDF_StreamContentParser::Handle_SetTextRise() {
  m_pCurStates->m_TextRise = GetNumber(0);
}

void CPDF_StreamContentParser::Handle_SetWordSpace() {
  m_pCurStates->m_TextState.SetWordSpace(GetNumber(0));
}

void CPDF_StreamContentParser::Handle_SetHorzScale() {
  if (m_ParamCount != 1) {
    return;
  }
  m_pCurStates->m_TextHorzScale = GetNumber(0) / 100;
  OnChangeTextMatrix();
}

void CPDF_StreamContentParser::Handle_MoveToNextLine() {
  m_pCurStates->m_TextLinePos.y -= m_pCurStates->m_TextLeading;
  m_pCurStates->m_TextPos = m_pCurStates->m_TextLinePos;
}

void CPDF_StreamContentParser::Handle_CurveTo_23() {
  AddPathPoint(m_PathCurrentX, m_PathCurrentY, FXPT_TYPE::BezierTo, false);
  AddPathPoint(GetNumber(3), GetNumber(2), FXPT_TYPE::BezierTo, false);
  AddPathPoint(GetNumber(1), GetNumber(0), FXPT_TYPE::BezierTo, false);
}

void CPDF_StreamContentParser::Handle_SetLineWidth() {
  m_pCurStates->m_GraphState.SetLineWidth(GetNumber(0));
}

void CPDF_StreamContentParser::Handle_Clip() {
  m_PathClipType = FXFILL_WINDING;
}

void CPDF_StreamContentParser::Handle_EOClip() {
  m_PathClipType = FXFILL_ALTERNATE;
}

void CPDF_StreamContentParser::Handle_CurveTo_13() {
  AddPathPoint(GetNumber(3), GetNumber(2), FXPT_TYPE::BezierTo, false);
  AddPathPoint(GetNumber(1), GetNumber(0), FXPT_TYPE::BezierTo, false);
  AddPathPoint(GetNumber(1), GetNumber(0), FXPT_TYPE::BezierTo, false);
}

void CPDF_StreamContentParser::Handle_NextLineShowText() {
  Handle_MoveToNextLine();
  Handle_ShowText();
}

void CPDF_StreamContentParser::Handle_NextLineShowText_Space() {
  m_pCurStates->m_TextState.SetWordSpace(GetNumber(2));
  m_pCurStates->m_TextState.SetCharSpace(GetNumber(1));
  Handle_NextLineShowText();
}

void CPDF_StreamContentParser::Handle_Invalid() {}

void CPDF_StreamContentParser::AddPathPoint(FX_FLOAT x,
                                            FX_FLOAT y,
                                            FXPT_TYPE type,
                                            bool close) {
  m_PathCurrentX = x;
  m_PathCurrentY = y;
  if (type == FXPT_TYPE::MoveTo && !close) {
    m_PathStartX = x;
    m_PathStartY = y;
    if (m_PathPointCount &&
        m_pPathPoints[m_PathPointCount - 1].IsTypeAndOpen(FXPT_TYPE::MoveTo)) {
      m_pPathPoints[m_PathPointCount - 1].m_Point = CFX_PointF(x, y);
      return;
    }
  } else if (m_PathPointCount == 0) {
    return;
  }
  m_PathPointCount++;
  if (m_PathPointCount > m_PathAllocSize) {
    int newsize = m_PathPointCount + 256;
    FX_PATHPOINT* pNewPoints = FX_Alloc(FX_PATHPOINT, newsize);
    if (m_PathAllocSize) {
      FXSYS_memcpy(pNewPoints, m_pPathPoints,
                   m_PathAllocSize * sizeof(FX_PATHPOINT));
      FX_Free(m_pPathPoints);
    }
    m_pPathPoints = pNewPoints;
    m_PathAllocSize = newsize;
  }
  m_pPathPoints[m_PathPointCount - 1].m_Type = type;
  m_pPathPoints[m_PathPointCount - 1].m_CloseFigure = close;
  m_pPathPoints[m_PathPointCount - 1].m_Point = CFX_PointF(x, y);
}

void CPDF_StreamContentParser::AddPathObject(int FillType, bool bStroke) {
  int PathPointCount = m_PathPointCount;
  uint8_t PathClipType = m_PathClipType;
  m_PathPointCount = 0;
  m_PathClipType = 0;
  if (PathPointCount <= 1) {
    if (PathPointCount && PathClipType) {
      CPDF_Path path;
      path.AppendRect(0, 0, 0, 0);
      m_pCurStates->m_ClipPath.AppendPath(path, FXFILL_WINDING, true);
    }
    return;
  }
  if (PathPointCount &&
      m_pPathPoints[PathPointCount - 1].IsTypeAndOpen(FXPT_TYPE::MoveTo)) {
    PathPointCount--;
  }

  CPDF_Path Path;
  for (int i = 0; i < PathPointCount; i++) {
    FX_PATHPOINT& point = m_pPathPoints[i];
    Path.AppendPoint(point.m_Point, point.m_Type, point.m_CloseFigure);
  }

  CFX_Matrix matrix = m_pCurStates->m_CTM;
  matrix.Concat(m_mtContentToUser);
  if (bStroke || FillType) {
    std::unique_ptr<CPDF_PathObject> pPathObj(new CPDF_PathObject);
    pPathObj->m_bStroke = bStroke;
    pPathObj->m_FillType = FillType;
    pPathObj->m_Path = Path;
    pPathObj->m_Matrix = matrix;
    SetGraphicStates(pPathObj.get(), true, false, true);
    pPathObj->CalcBoundingBox();
    m_pObjectHolder->GetPageObjectList()->push_back(std::move(pPathObj));
  }
  if (PathClipType) {
    if (!matrix.IsIdentity()) {
      Path.Transform(&matrix);
      matrix.SetIdentity();
    }
    m_pCurStates->m_ClipPath.AppendPath(Path, PathClipType, true);
  }
}

uint32_t CPDF_StreamContentParser::Parse(const uint8_t* pData,
                                         uint32_t dwSize,
                                         uint32_t max_cost) {
  if (m_Level > kMaxFormLevel)
    return dwSize;

  uint32_t InitObjCount = m_pObjectHolder->GetPageObjectList()->size();
  CPDF_StreamParser syntax(pData, dwSize, m_pDocument->GetByteStringPool());
  CPDF_StreamParserAutoClearer auto_clearer(&m_pSyntax, &syntax);
  while (1) {
    uint32_t cost = m_pObjectHolder->GetPageObjectList()->size() - InitObjCount;
    if (max_cost && cost >= max_cost) {
      break;
    }
    switch (syntax.ParseNextElement()) {
      case CPDF_StreamParser::EndOfData:
        return m_pSyntax->GetPos();
      case CPDF_StreamParser::Keyword:
        OnOperator(syntax.GetWord());
        ClearAllParams();
        break;
      case CPDF_StreamParser::Number:
        AddNumberParam(syntax.GetWord());
        break;
      case CPDF_StreamParser::Name:
        AddNameParam(syntax.GetWord().Mid(1));
        break;
      default:
        AddObjectParam(syntax.GetObject());
    }
  }
  return m_pSyntax->GetPos();
}

void CPDF_StreamContentParser::ParsePathObject() {
  FX_FLOAT params[6] = {};
  int nParams = 0;
  int last_pos = m_pSyntax->GetPos();
  while (1) {
    CPDF_StreamParser::SyntaxType type = m_pSyntax->ParseNextElement();
    bool bProcessed = true;
    switch (type) {
      case CPDF_StreamParser::EndOfData:
        return;
      case CPDF_StreamParser::Keyword: {
        CFX_ByteStringC strc = m_pSyntax->GetWord();
        int len = strc.GetLength();
        if (len == 1) {
          switch (strc[0]) {
            case kPathOperatorSubpath:
              AddPathPoint(params[0], params[1], FXPT_TYPE::MoveTo, false);
              nParams = 0;
              break;
            case kPathOperatorLine:
              AddPathPoint(params[0], params[1], FXPT_TYPE::LineTo, false);
              nParams = 0;
              break;
            case kPathOperatorCubicBezier1:
              AddPathPoint(params[0], params[1], FXPT_TYPE::BezierTo, false);
              AddPathPoint(params[2], params[3], FXPT_TYPE::BezierTo, false);
              AddPathPoint(params[4], params[5], FXPT_TYPE::BezierTo, false);
              nParams = 0;
              break;
            case kPathOperatorCubicBezier2:
              AddPathPoint(m_PathCurrentX, m_PathCurrentY, FXPT_TYPE::BezierTo,
                           false);
              AddPathPoint(params[0], params[1], FXPT_TYPE::BezierTo, false);
              AddPathPoint(params[2], params[3], FXPT_TYPE::BezierTo, false);
              nParams = 0;
              break;
            case kPathOperatorCubicBezier3:
              AddPathPoint(params[0], params[1], FXPT_TYPE::BezierTo, false);
              AddPathPoint(params[2], params[3], FXPT_TYPE::BezierTo, false);
              AddPathPoint(params[2], params[3], FXPT_TYPE::BezierTo, false);
              nParams = 0;
              break;
            case kPathOperatorClosePath:
              Handle_ClosePath();
              nParams = 0;
              break;
            default:
              bProcessed = false;
              break;
          }
        } else if (len == 2) {
          if (strc[0] == kPathOperatorRectangle[0] &&
              strc[1] == kPathOperatorRectangle[1]) {
            AddPathRect(params[0], params[1], params[2], params[3]);
            nParams = 0;
          } else {
            bProcessed = false;
          }
        } else {
          bProcessed = false;
        }
        if (bProcessed) {
          last_pos = m_pSyntax->GetPos();
        }
        break;
      }
      case CPDF_StreamParser::Number: {
        if (nParams == 6)
          break;

        int value;
        bool bInteger = FX_atonum(m_pSyntax->GetWord(), &value);
        params[nParams++] = bInteger ? (FX_FLOAT)value : *(FX_FLOAT*)&value;
        break;
      }
      default:
        bProcessed = false;
    }
    if (!bProcessed) {
      m_pSyntax->SetPos(last_pos);
      return;
    }
  }
}

CPDF_StreamContentParser::ContentParam::ContentParam() {}

CPDF_StreamContentParser::ContentParam::~ContentParam() {}
