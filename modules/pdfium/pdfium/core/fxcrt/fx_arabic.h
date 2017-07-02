// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCRT_FX_ARABIC_H_
#define CORE_FXCRT_FX_ARABIC_H_

#include "core/fxcrt/fx_arb.h"

#define FX_BIDIMAXLEVEL 61
#define FX_BidiDirection(a) (FX_IsOdd(a) ? FX_BIDICLASS_R : FX_BIDICLASS_L)
#define FX_BidiGetDeferredType(a) (((a) >> 4) & 0x0F)
#define FX_BidiGetResolvedType(a) ((a)&0x0F)

namespace pdfium {
namespace arabic {

bool IsArabicChar(FX_WCHAR wch);
bool IsArabicFormChar(FX_WCHAR wch);
FX_WCHAR GetFormChar(FX_WCHAR wch, FX_WCHAR prev = 0, FX_WCHAR next = 0);
FX_WCHAR GetFormChar(const CFX_Char* cur,
                     const CFX_Char* prev,
                     const CFX_Char* next);

}  // namespace arabic
}  // namespace pdfium

void FX_BidiReverseString(CFX_WideString& wsText,
                          int32_t iStart,
                          int32_t iCount);
void FX_BidiSetDeferredRun(CFX_ArrayTemplate<int32_t>& values,
                           int32_t iStart,
                           int32_t iCount,
                           int32_t iValue);
void FX_BidiClassify(const CFX_WideString& wsText,
                     CFX_ArrayTemplate<int32_t>& classes,
                     bool bWS = false);
int32_t FX_BidiResolveExplicit(int32_t iBaseLevel,
                               int32_t iDirection,
                               CFX_ArrayTemplate<int32_t>& classes,
                               CFX_ArrayTemplate<int32_t>& levels,
                               int32_t iStart,
                               int32_t iCount,
                               int32_t iNest = 0);

enum FX_BIDIWEAKSTATE {
  FX_BIDIWEAKSTATE_xa = 0,
  FX_BIDIWEAKSTATE_xr,
  FX_BIDIWEAKSTATE_xl,
  FX_BIDIWEAKSTATE_ao,
  FX_BIDIWEAKSTATE_ro,
  FX_BIDIWEAKSTATE_lo,
  FX_BIDIWEAKSTATE_rt,
  FX_BIDIWEAKSTATE_lt,
  FX_BIDIWEAKSTATE_cn,
  FX_BIDIWEAKSTATE_ra,
  FX_BIDIWEAKSTATE_re,
  FX_BIDIWEAKSTATE_la,
  FX_BIDIWEAKSTATE_le,
  FX_BIDIWEAKSTATE_ac,
  FX_BIDIWEAKSTATE_rc,
  FX_BIDIWEAKSTATE_rs,
  FX_BIDIWEAKSTATE_lc,
  FX_BIDIWEAKSTATE_ls,
  FX_BIDIWEAKSTATE_ret,
  FX_BIDIWEAKSTATE_let,
};
#define FX_BWSxa FX_BIDIWEAKSTATE_xa
#define FX_BWSxr FX_BIDIWEAKSTATE_xr
#define FX_BWSxl FX_BIDIWEAKSTATE_xl
#define FX_BWSao FX_BIDIWEAKSTATE_ao
#define FX_BWSro FX_BIDIWEAKSTATE_ro
#define FX_BWSlo FX_BIDIWEAKSTATE_lo
#define FX_BWSrt FX_BIDIWEAKSTATE_rt
#define FX_BWSlt FX_BIDIWEAKSTATE_lt
#define FX_BWScn FX_BIDIWEAKSTATE_cn
#define FX_BWSra FX_BIDIWEAKSTATE_ra
#define FX_BWSre FX_BIDIWEAKSTATE_re
#define FX_BWSla FX_BIDIWEAKSTATE_la
#define FX_BWSle FX_BIDIWEAKSTATE_le
#define FX_BWSac FX_BIDIWEAKSTATE_ac
#define FX_BWSrc FX_BIDIWEAKSTATE_rc
#define FX_BWSrs FX_BIDIWEAKSTATE_rs
#define FX_BWSlc FX_BIDIWEAKSTATE_lc
#define FX_BWSls FX_BIDIWEAKSTATE_ls
#define FX_BWSret FX_BIDIWEAKSTATE_ret
#define FX_BWSlet FX_BIDIWEAKSTATE_let

enum FX_BIDIWEAKACTION {
  FX_BIDIWEAKACTION_IX = 0x100,
  FX_BIDIWEAKACTION_XX = 0x0F,
  FX_BIDIWEAKACTION_xxx = (0x0F << 4) + 0x0F,
  FX_BIDIWEAKACTION_xIx = 0x100 + FX_BIDIWEAKACTION_xxx,
  FX_BIDIWEAKACTION_xxN = (0x0F << 4) + FX_BIDICLASS_ON,
  FX_BIDIWEAKACTION_xxE = (0x0F << 4) + FX_BIDICLASS_EN,
  FX_BIDIWEAKACTION_xxA = (0x0F << 4) + FX_BIDICLASS_AN,
  FX_BIDIWEAKACTION_xxR = (0x0F << 4) + FX_BIDICLASS_R,
  FX_BIDIWEAKACTION_xxL = (0x0F << 4) + FX_BIDICLASS_L,
  FX_BIDIWEAKACTION_Nxx = (FX_BIDICLASS_ON << 4) + 0x0F,
  FX_BIDIWEAKACTION_Axx = (FX_BIDICLASS_AN << 4) + 0x0F,
  FX_BIDIWEAKACTION_ExE = (FX_BIDICLASS_EN << 4) + FX_BIDICLASS_EN,
  FX_BIDIWEAKACTION_NIx = (FX_BIDICLASS_ON << 4) + 0x0F + 0x100,
  FX_BIDIWEAKACTION_NxN = (FX_BIDICLASS_ON << 4) + FX_BIDICLASS_ON,
  FX_BIDIWEAKACTION_NxR = (FX_BIDICLASS_ON << 4) + FX_BIDICLASS_R,
  FX_BIDIWEAKACTION_NxE = (FX_BIDICLASS_ON << 4) + FX_BIDICLASS_EN,
  FX_BIDIWEAKACTION_AxA = (FX_BIDICLASS_AN << 4) + FX_BIDICLASS_AN,
  FX_BIDIWEAKACTION_NxL = (FX_BIDICLASS_ON << 4) + FX_BIDICLASS_L,
  FX_BIDIWEAKACTION_LxL = (FX_BIDICLASS_L << 4) + FX_BIDICLASS_L,
  FX_BIDIWEAKACTION_xIL = (0x0F << 4) + FX_BIDICLASS_L + 0x100,
  FX_BIDIWEAKACTION_AxR = (FX_BIDICLASS_AN << 4) + FX_BIDICLASS_R,
  FX_BIDIWEAKACTION_Lxx = (FX_BIDICLASS_L << 4) + 0x0F,
};
#define FX_BWAIX FX_BIDIWEAKACTION_IX
#define FX_BWAXX FX_BIDIWEAKACTION_XX
#define FX_BWAxxx FX_BIDIWEAKACTION_xxx
#define FX_BWAxIx FX_BIDIWEAKACTION_xIx
#define FX_BWAxxN FX_BIDIWEAKACTION_xxN
#define FX_BWAxxE FX_BIDIWEAKACTION_xxE
#define FX_BWAxxA FX_BIDIWEAKACTION_xxA
#define FX_BWAxxR FX_BIDIWEAKACTION_xxR
#define FX_BWAxxL FX_BIDIWEAKACTION_xxL
#define FX_BWANxx FX_BIDIWEAKACTION_Nxx
#define FX_BWAAxx FX_BIDIWEAKACTION_Axx
#define FX_BWAExE FX_BIDIWEAKACTION_ExE
#define FX_BWANIx FX_BIDIWEAKACTION_NIx
#define FX_BWANxN FX_BIDIWEAKACTION_NxN
#define FX_BWANxR FX_BIDIWEAKACTION_NxR
#define FX_BWANxE FX_BIDIWEAKACTION_NxE
#define FX_BWAAxA FX_BIDIWEAKACTION_AxA
#define FX_BWANxL FX_BIDIWEAKACTION_NxL
#define FX_BWALxL FX_BIDIWEAKACTION_LxL
#define FX_BWAxIL FX_BIDIWEAKACTION_xIL
#define FX_BWAAxR FX_BIDIWEAKACTION_AxR
#define FX_BWALxx FX_BIDIWEAKACTION_Lxx

void FX_BidiResolveWeak(int32_t iBaseLevel,
                        CFX_ArrayTemplate<int32_t>& classes,
                        CFX_ArrayTemplate<int32_t>& levels);
enum FX_BIDINEUTRALSTATE {
  FX_BIDINEUTRALSTATE_r = 0,
  FX_BIDINEUTRALSTATE_l,
  FX_BIDINEUTRALSTATE_rn,
  FX_BIDINEUTRALSTATE_ln,
  FX_BIDINEUTRALSTATE_a,
  FX_BIDINEUTRALSTATE_na,
};
#define FX_BNSr FX_BIDINEUTRALSTATE_r
#define FX_BNSl FX_BIDINEUTRALSTATE_l
#define FX_BNSrn FX_BIDINEUTRALSTATE_rn
#define FX_BNSln FX_BIDINEUTRALSTATE_ln
#define FX_BNSa FX_BIDINEUTRALSTATE_a
#define FX_BNSna FX_BIDINEUTRALSTATE_na
enum FX_BIDINEUTRALACTION {
  FX_BIDINEUTRALACTION_nL = FX_BIDICLASS_L,
  FX_BIDINEUTRALACTION_En = (FX_BIDICLASS_AN << 4),
  FX_BIDINEUTRALACTION_Rn = (FX_BIDICLASS_R << 4),
  FX_BIDINEUTRALACTION_Ln = (FX_BIDICLASS_L << 4),
  FX_BIDINEUTRALACTION_In = FX_BIDIWEAKACTION_IX,
  FX_BIDINEUTRALACTION_LnL = (FX_BIDICLASS_L << 4) + FX_BIDICLASS_L,
};
#define FX_BNAnL FX_BIDINEUTRALACTION_nL
#define FX_BNAEn FX_BIDINEUTRALACTION_En
#define FX_BNARn FX_BIDINEUTRALACTION_Rn
#define FX_BNALn FX_BIDINEUTRALACTION_Ln
#define FX_BNAIn FX_BIDINEUTRALACTION_In
#define FX_BNALnL FX_BIDINEUTRALACTION_LnL
int32_t FX_BidiGetDeferredNeutrals(int32_t iAction, int32_t iLevel);
int32_t FX_BidiGetResolvedNeutrals(int32_t iAction);
void FX_BidiResolveNeutrals(int32_t iBaseLevel,
                            CFX_ArrayTemplate<int32_t>& classes,
                            const CFX_ArrayTemplate<int32_t>& levels);
void FX_BidiResolveImplicit(const CFX_ArrayTemplate<int32_t>& classes,
                            CFX_ArrayTemplate<int32_t>& levels);
void FX_BidiResolveWhitespace(int32_t iBaseLevel,
                              const CFX_ArrayTemplate<int32_t>& classes,
                              CFX_ArrayTemplate<int32_t>& levels);
int32_t FX_BidiReorderLevel(int32_t iBaseLevel,
                            CFX_WideString& wsText,
                            const CFX_ArrayTemplate<int32_t>& levels,
                            int32_t iStart,
                            bool bReverse = false);
void FX_BidiReorder(int32_t iBaseLevel,
                    CFX_WideString& wsText,
                    const CFX_ArrayTemplate<int32_t>& levels);

#endif  // CORE_FXCRT_FX_ARABIC_H_
