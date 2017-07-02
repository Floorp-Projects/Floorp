// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_PAGE_CPDF_STREAMCONTENTPARSER_H_
#define CORE_FPDFAPI_PAGE_CPDF_STREAMCONTENTPARSER_H_

#include <memory>
#include <unordered_map>
#include <vector>

#include "core/fpdfapi/page/cpdf_contentmark.h"
#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fxcrt/fx_string.h"
#include "core/fxge/cfx_pathdata.h"

class CPDF_AllStates;
class CPDF_Dictionary;
class CPDF_Document;
class CPDF_Font;
class CPDF_Image;
class CPDF_ImageObject;
class CPDF_PageObject;
class CPDF_PageObjectHolder;
class CPDF_StreamParser;
class CPDF_TextObject;
class CPDF_ColorSpace;
class CPDF_Pattern;

class CPDF_StreamContentParser {
 public:
  CPDF_StreamContentParser(CPDF_Document* pDoc,
                           CPDF_Dictionary* pPageResources,
                           CPDF_Dictionary* pParentResources,
                           const CFX_Matrix* pmtContentToUser,
                           CPDF_PageObjectHolder* pObjectHolder,
                           CPDF_Dictionary* pResources,
                           CFX_FloatRect* pBBox,
                           CPDF_AllStates* pAllStates,
                           int level);
  ~CPDF_StreamContentParser();

  uint32_t Parse(const uint8_t* pData, uint32_t dwSize, uint32_t max_cost);
  CPDF_PageObjectHolder* GetPageObjectHolder() const { return m_pObjectHolder; }
  CPDF_AllStates* GetCurStates() const { return m_pCurStates.get(); }
  bool IsColored() const { return m_bColored; }
  const FX_FLOAT* GetType3Data() const { return m_Type3Data; }
  CPDF_Font* FindFont(const CFX_ByteString& name);

 private:
  struct ContentParam {
    enum Type { OBJECT = 0, NUMBER, NAME };

    ContentParam();
    ~ContentParam();

    Type m_Type;
    std::unique_ptr<CPDF_Object> m_pObject;
    struct {
      bool m_bInteger;
      union {
        int m_Integer;
        FX_FLOAT m_Float;
      };
    } m_Number;
    struct {
      int m_Len;
      char m_Buffer[32];
    } m_Name;
  };

  static const int kParamBufSize = 16;

  using OpCodes =
      std::unordered_map<uint32_t, void (CPDF_StreamContentParser::*)()>;
  static OpCodes InitializeOpCodes();

  void AddNameParam(const CFX_ByteStringC& str);
  void AddNumberParam(const CFX_ByteStringC& str);
  void AddObjectParam(std::unique_ptr<CPDF_Object> pObj);
  int GetNextParamPos();
  void ClearAllParams();
  CPDF_Object* GetObject(uint32_t index);
  CFX_ByteString GetString(uint32_t index);
  FX_FLOAT GetNumber(uint32_t index);
  int GetInteger(uint32_t index) { return (int32_t)(GetNumber(index)); }
  void OnOperator(const CFX_ByteStringC& op);
  void AddTextObject(CFX_ByteString* pText,
                     FX_FLOAT fInitKerning,
                     FX_FLOAT* pKerning,
                     int count);

  void OnChangeTextMatrix();
  void ParsePathObject();
  void AddPathPoint(FX_FLOAT x, FX_FLOAT y, FXPT_TYPE type, bool close);
  void AddPathRect(FX_FLOAT x, FX_FLOAT y, FX_FLOAT w, FX_FLOAT h);
  void AddPathObject(int FillType, bool bStroke);
  CPDF_ImageObject* AddImage(std::unique_ptr<CPDF_Stream> pStream);
  CPDF_ImageObject* AddImage(uint32_t streamObjNum);
  CPDF_ImageObject* AddImage(CPDF_Image* pImage);

  void AddForm(CPDF_Stream* pStream);
  void SetGraphicStates(CPDF_PageObject* pObj,
                        bool bColor,
                        bool bText,
                        bool bGraph);
  CPDF_ColorSpace* FindColorSpace(const CFX_ByteString& name);
  CPDF_Pattern* FindPattern(const CFX_ByteString& name, bool bShading);
  CPDF_Object* FindResourceObj(const CFX_ByteString& type,
                               const CFX_ByteString& name);

  // Takes ownership of |pImageObj|, returns unowned pointer to it.
  CPDF_ImageObject* AddImageObject(std::unique_ptr<CPDF_ImageObject> pImageObj);

  void Handle_CloseFillStrokePath();
  void Handle_FillStrokePath();
  void Handle_CloseEOFillStrokePath();
  void Handle_EOFillStrokePath();
  void Handle_BeginMarkedContent_Dictionary();
  void Handle_BeginImage();
  void Handle_BeginMarkedContent();
  void Handle_BeginText();
  void Handle_CurveTo_123();
  void Handle_ConcatMatrix();
  void Handle_SetColorSpace_Fill();
  void Handle_SetColorSpace_Stroke();
  void Handle_SetDash();
  void Handle_SetCharWidth();
  void Handle_SetCachedDevice();
  void Handle_ExecuteXObject();
  void Handle_MarkPlace_Dictionary();
  void Handle_EndImage();
  void Handle_EndMarkedContent();
  void Handle_EndText();
  void Handle_FillPath();
  void Handle_FillPathOld();
  void Handle_EOFillPath();
  void Handle_SetGray_Fill();
  void Handle_SetGray_Stroke();
  void Handle_SetExtendGraphState();
  void Handle_ClosePath();
  void Handle_SetFlat();
  void Handle_BeginImageData();
  void Handle_SetLineJoin();
  void Handle_SetLineCap();
  void Handle_SetCMYKColor_Fill();
  void Handle_SetCMYKColor_Stroke();
  void Handle_LineTo();
  void Handle_MoveTo();
  void Handle_SetMiterLimit();
  void Handle_MarkPlace();
  void Handle_EndPath();
  void Handle_SaveGraphState();
  void Handle_RestoreGraphState();
  void Handle_Rectangle();
  void Handle_SetRGBColor_Fill();
  void Handle_SetRGBColor_Stroke();
  void Handle_SetRenderIntent();
  void Handle_CloseStrokePath();
  void Handle_StrokePath();
  void Handle_SetColor_Fill();
  void Handle_SetColor_Stroke();
  void Handle_SetColorPS_Fill();
  void Handle_SetColorPS_Stroke();
  void Handle_ShadeFill();
  void Handle_SetCharSpace();
  void Handle_MoveTextPoint();
  void Handle_MoveTextPoint_SetLeading();
  void Handle_SetFont();
  void Handle_ShowText();
  void Handle_ShowText_Positioning();
  void Handle_SetTextLeading();
  void Handle_SetTextMatrix();
  void Handle_SetTextRenderMode();
  void Handle_SetTextRise();
  void Handle_SetWordSpace();
  void Handle_SetHorzScale();
  void Handle_MoveToNextLine();
  void Handle_CurveTo_23();
  void Handle_SetLineWidth();
  void Handle_Clip();
  void Handle_EOClip();
  void Handle_CurveTo_13();
  void Handle_NextLineShowText();
  void Handle_NextLineShowText_Space();
  void Handle_Invalid();

  CPDF_Document* const m_pDocument;
  CPDF_Dictionary* m_pPageResources;
  CPDF_Dictionary* m_pParentResources;
  CPDF_Dictionary* m_pResources;
  CPDF_PageObjectHolder* m_pObjectHolder;
  int m_Level;
  CFX_Matrix m_mtContentToUser;
  CFX_FloatRect m_BBox;
  ContentParam m_ParamBuf[kParamBufSize];
  uint32_t m_ParamStartPos;
  uint32_t m_ParamCount;
  CPDF_StreamParser* m_pSyntax;
  std::unique_ptr<CPDF_AllStates> m_pCurStates;
  CPDF_ContentMark m_CurContentMark;
  std::vector<std::unique_ptr<CPDF_TextObject>> m_ClipTextList;
  CPDF_TextObject* m_pLastTextObject;
  FX_FLOAT m_DefFontSize;
  FX_PATHPOINT* m_pPathPoints;
  int m_PathPointCount;
  int m_PathAllocSize;
  FX_FLOAT m_PathStartX;
  FX_FLOAT m_PathStartY;
  FX_FLOAT m_PathCurrentX;
  FX_FLOAT m_PathCurrentY;
  uint8_t m_PathClipType;
  CFX_ByteString m_LastImageName;
  CPDF_Image* m_pLastImage;
  bool m_bColored;
  FX_FLOAT m_Type3Data[6];
  bool m_bResourceMissing;
  std::vector<std::unique_ptr<CPDF_AllStates>> m_StateStack;
};

#endif  // CORE_FPDFAPI_PAGE_CPDF_STREAMCONTENTPARSER_H_
