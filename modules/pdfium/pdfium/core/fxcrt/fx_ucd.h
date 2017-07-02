// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCRT_FX_UCD_H_
#define CORE_FXCRT_FX_UCD_H_

#include "core/fxcrt/cfx_retain_ptr.h"
#include "core/fxcrt/fx_basic.h"

#define FX_BIDICLASSBITS 6
#define FX_BIDICLASSBITSMASK (31 << FX_BIDICLASSBITS)

enum FX_BIDICLASS {
  FX_BIDICLASS_ON = 0,    // Other Neutral
  FX_BIDICLASS_L = 1,     // Left Letter
  FX_BIDICLASS_R = 2,     // Right Letter
  FX_BIDICLASS_AN = 3,    // Arabic Number
  FX_BIDICLASS_EN = 4,    // European Number
  FX_BIDICLASS_AL = 5,    // Arabic Letter
  FX_BIDICLASS_NSM = 6,   // Non-spacing Mark
  FX_BIDICLASS_CS = 7,    // Common Number Separator
  FX_BIDICLASS_ES = 8,    // European Separator
  FX_BIDICLASS_ET = 9,    // European Number Terminator
  FX_BIDICLASS_BN = 10,   // Boundary Neutral
  FX_BIDICLASS_S = 11,    // Segment Separator
  FX_BIDICLASS_WS = 12,   // Whitespace
  FX_BIDICLASS_B = 13,    // Paragraph Separator
  FX_BIDICLASS_RLO = 14,  // Right-to-Left Override
  FX_BIDICLASS_RLE = 15,  // Right-to-Left Embedding
  FX_BIDICLASS_LRO = 16,  // Left-to-Right Override
  FX_BIDICLASS_LRE = 17,  // Left-to-Right Embedding
  FX_BIDICLASS_PDF = 18,  // Pop Directional Format
  FX_BIDICLASS_N = FX_BIDICLASS_ON,
};

extern const uint32_t kTextLayoutCodeProperties[];
extern const size_t kTextLayoutCodePropertiesSize;

extern const uint16_t kFXTextLayoutVerticalMirror[];
extern const size_t kFXTextLayoutVerticalMirrorSize;

extern const uint16_t kFXTextLayoutBidiMirror[];
extern const size_t kFXTextLayoutBidiMirrorSize;

uint32_t FX_GetUnicodeProperties(FX_WCHAR wch);
FX_WCHAR FX_GetMirrorChar(FX_WCHAR wch, bool bRTL, bool bVertical);

#ifdef PDF_ENABLE_XFA
enum FX_CHARBREAKPROP {
  FX_CBP_OP = 0,
  FX_CBP_CL = 1,
  FX_CBP_QU = 2,
  FX_CBP_GL = 3,
  FX_CBP_NS = 4,
  FX_CBP_EX = 5,
  FX_CBP_SY = 6,
  FX_CBP_IS = 7,
  FX_CBP_PR = 8,
  FX_CBP_PO = 9,
  FX_CBP_NU = 10,
  FX_CBP_AL = 11,
  FX_CBP_ID = 12,
  FX_CBP_IN = 13,
  FX_CBP_HY = 14,
  FX_CBP_BA = 15,
  FX_CBP_BB = 16,
  FX_CBP_B2 = 17,
  FX_CBP_ZW = 18,
  FX_CBP_CM = 19,
  FX_CBP_WJ = 20,
  FX_CBP_H2 = 21,
  FX_CBP_H3 = 22,
  FX_CBP_JL = 23,
  FX_CBP_JV = 24,
  FX_CBP_JT = 25,

  FX_CBP_BK = 26,
  FX_CBP_CR = 27,
  FX_CBP_LF = 28,
  FX_CBP_NL = 29,
  FX_CBP_SA = 30,
  FX_CBP_SG = 31,
  FX_CBP_CB = 32,
  FX_CBP_XX = 33,
  FX_CBP_AI = 34,
  FX_CBP_SP = 35,
  FX_CBP_TB = 37,
  FX_CBP_NONE = 36,
};

#define FX_CHARTYPEBITS 11
#define FX_CHARTYPEBITSMASK (15 << FX_CHARTYPEBITS)
enum FX_CHARTYPE {
  FX_CHARTYPE_Unknown = 0,
  FX_CHARTYPE_Tab = (1 << FX_CHARTYPEBITS),
  FX_CHARTYPE_Space = (2 << FX_CHARTYPEBITS),
  FX_CHARTYPE_Control = (3 << FX_CHARTYPEBITS),
  FX_CHARTYPE_Combination = (4 << FX_CHARTYPEBITS),
  FX_CHARTYPE_Numeric = (5 << FX_CHARTYPEBITS),
  FX_CHARTYPE_Normal = (6 << FX_CHARTYPEBITS),
  FX_CHARTYPE_ArabicAlef = (7 << FX_CHARTYPEBITS),
  FX_CHARTYPE_ArabicSpecial = (8 << FX_CHARTYPEBITS),
  FX_CHARTYPE_ArabicDistortion = (9 << FX_CHARTYPEBITS),
  FX_CHARTYPE_ArabicNormal = (10 << FX_CHARTYPEBITS),
  FX_CHARTYPE_ArabicForm = (11 << FX_CHARTYPEBITS),
  FX_CHARTYPE_Arabic = (12 << FX_CHARTYPEBITS),
};
inline FX_CHARTYPE GetCharTypeFromProp(uint32_t prop) {
  return static_cast<FX_CHARTYPE>(prop & FX_CHARTYPEBITSMASK);
}

bool FX_IsCtrlCode(FX_WCHAR ch);
FX_WCHAR FX_GetMirrorChar(FX_WCHAR wch,
                          uint32_t dwProps,
                          bool bRTL,
                          bool bVertical);
class CFX_Char {
 public:
  CFX_Char()
      : m_wCharCode(0),
        m_nBreakType(0),
        m_dwCharProps(0),
        m_iCharWidth(0),
        m_iHorizontalScale(100),
        m_iVerticalScale(100) {}

  CFX_Char(uint16_t wCharCode, uint32_t dwCharProps)
      : m_wCharCode(wCharCode),
        m_nBreakType(0),
        m_dwCharProps(dwCharProps),
        m_iCharWidth(0),
        m_iHorizontalScale(100),
        m_iVerticalScale(100) {}

  FX_CHARTYPE GetCharType() const { return GetCharTypeFromProp(m_dwCharProps); }

  uint16_t m_wCharCode;
  uint8_t m_nBreakType;
  uint32_t m_dwCharProps;
  int32_t m_iCharWidth;
  int32_t m_iHorizontalScale;
  int32_t m_iVerticalScale;
};

class CFX_TxtChar : public CFX_Char {
 public:
  CFX_TxtChar()
      : m_nRotation(0),
        m_dwCharStyles(0),
        m_dwStatus(0),
        m_iBidiClass(0),
        m_iBidiLevel(0),
        m_iBidiPos(0),
        m_iBidiOrder(0),
        m_pUserData(nullptr) {}

  int8_t m_nRotation;
  uint32_t m_dwCharStyles;
  uint32_t m_dwStatus;
  int16_t m_iBidiClass;
  int16_t m_iBidiLevel;
  int16_t m_iBidiPos;
  int16_t m_iBidiOrder;
  void* m_pUserData;
};

enum class CFX_RTFBreakType { None = 0, Piece, Line, Paragraph, Page };

class CFX_RTFChar : public CFX_Char {
 public:
  CFX_RTFChar();
  CFX_RTFChar(const CFX_RTFChar& other);
  ~CFX_RTFChar();

  CFX_RTFBreakType m_dwStatus;
  int32_t m_iFontSize;
  int32_t m_iFontHeight;
  int16_t m_iBidiClass;
  int16_t m_iBidiLevel;
  int16_t m_iBidiPos;
  int16_t m_iBidiOrder;
  uint32_t m_dwIdentity;
  CFX_RetainPtr<CFX_Retainable> m_pUserData;
};

inline CFX_RTFChar::CFX_RTFChar()
    : m_dwStatus(CFX_RTFBreakType::None),
      m_iFontSize(0),
      m_iFontHeight(0),
      m_iBidiClass(0),
      m_iBidiLevel(0),
      m_iBidiPos(0),
      m_dwIdentity(0),
      m_pUserData(nullptr) {}

inline CFX_RTFChar::CFX_RTFChar(const CFX_RTFChar& other) = default;
inline CFX_RTFChar::~CFX_RTFChar() = default;

#endif  // PDF_ENABLE_XFA

#endif  // CORE_FXCRT_FX_UCD_H_
