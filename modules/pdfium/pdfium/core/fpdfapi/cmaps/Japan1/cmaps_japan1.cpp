// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/cmaps/Japan1/cmaps_japan1.h"

#include "core/fpdfapi/cmaps/cmap_int.h"
#include "core/fpdfapi/cpdf_modulemgr.h"
#include "core/fpdfapi/font/font_int.h"
#include "core/fpdfapi/page/cpdf_pagemodule.h"
#include "core/fxcrt/fx_basic.h"

static const FXCMAP_CMap g_FXCMAP_Japan1_cmaps[] = {
    {"83pv-RKSJ-H", FXCMAP_CMap::Range, g_FXCMAP_83pv_RKSJ_H_1, 222,
     FXCMAP_CMap::None, nullptr, 0, 0},
    {"90ms-RKSJ-H", FXCMAP_CMap::Range, g_FXCMAP_90ms_RKSJ_H_2, 171,
     FXCMAP_CMap::None, nullptr, 0, 0},
    {"90ms-RKSJ-V", FXCMAP_CMap::Range, g_FXCMAP_90ms_RKSJ_V_2, 78,
     FXCMAP_CMap::None, nullptr, 0, -1},
    {"90msp-RKSJ-H", FXCMAP_CMap::Range, g_FXCMAP_90msp_RKSJ_H_2, 170,
     FXCMAP_CMap::None, nullptr, 0, -2},
    {"90msp-RKSJ-V", FXCMAP_CMap::Range, g_FXCMAP_90msp_RKSJ_V_2, 78,
     FXCMAP_CMap::None, nullptr, 0, -1},
    {"90pv-RKSJ-H", FXCMAP_CMap::Range, g_FXCMAP_90pv_RKSJ_H_1, 263,
     FXCMAP_CMap::None, nullptr, 0, 0},
    {"Add-RKSJ-H", FXCMAP_CMap::Range, g_FXCMAP_Add_RKSJ_H_1, 635,
     FXCMAP_CMap::None, nullptr, 0, 0},
    {"Add-RKSJ-V", FXCMAP_CMap::Range, g_FXCMAP_Add_RKSJ_V_1, 57,
     FXCMAP_CMap::None, nullptr, 0, -1},
    {"EUC-H", FXCMAP_CMap::Range, g_FXCMAP_EUC_H_1, 120, FXCMAP_CMap::None,
     nullptr, 0, 0},
    {"EUC-V", FXCMAP_CMap::Range, g_FXCMAP_EUC_V_1, 27, FXCMAP_CMap::None,
     nullptr, 0, -1},
    {"Ext-RKSJ-H", FXCMAP_CMap::Range, g_FXCMAP_Ext_RKSJ_H_2, 665,
     FXCMAP_CMap::None, nullptr, 0, -4},
    {"Ext-RKSJ-V", FXCMAP_CMap::Range, g_FXCMAP_Ext_RKSJ_V_2, 39,
     FXCMAP_CMap::None, nullptr, 0, -1},
    {"H", FXCMAP_CMap::Range, g_FXCMAP_H_1, 118, FXCMAP_CMap::None, nullptr, 0,
     0},
    {"V", FXCMAP_CMap::Range, g_FXCMAP_V_1, 27, FXCMAP_CMap::None, nullptr, 0,
     -1},
    {"UniJIS-UCS2-H", FXCMAP_CMap::Single, g_FXCMAP_UniJIS_UCS2_H_4, 9772,
     FXCMAP_CMap::None, nullptr, 0, 0},
    {"UniJIS-UCS2-V", FXCMAP_CMap::Single, g_FXCMAP_UniJIS_UCS2_V_4, 251,
     FXCMAP_CMap::None, nullptr, 0, -1},
    {"UniJIS-UCS2-HW-H", FXCMAP_CMap::Range, g_FXCMAP_UniJIS_UCS2_HW_H_4, 4,
     FXCMAP_CMap::None, nullptr, 0, -2},
    {"UniJIS-UCS2-HW-V", FXCMAP_CMap::Range, g_FXCMAP_UniJIS_UCS2_HW_V_4, 199,
     FXCMAP_CMap::None, nullptr, 0, -1},
    {"UniJIS-UTF16-H", FXCMAP_CMap::Single, g_FXCMAP_UniJIS_UCS2_H_4, 9772,
     FXCMAP_CMap::None, nullptr, 0, 0},
    {"UniJIS-UTF16-V", FXCMAP_CMap::Single, g_FXCMAP_UniJIS_UCS2_V_4, 251,
     FXCMAP_CMap::None, nullptr, 0, -1},
};

void CPDF_ModuleMgr::LoadEmbeddedJapan1CMaps() {
  CPDF_FontGlobals* pFontGlobals =
      CPDF_ModuleMgr::Get()->GetPageModule()->GetFontGlobals();
  pFontGlobals->m_EmbeddedCharsets[CIDSET_JAPAN1].m_pMapList =
      g_FXCMAP_Japan1_cmaps;
  pFontGlobals->m_EmbeddedCharsets[CIDSET_JAPAN1].m_Count =
      FX_ArraySize(g_FXCMAP_Japan1_cmaps);
  pFontGlobals->m_EmbeddedToUnicodes[CIDSET_JAPAN1].m_pMap =
      g_FXCMAP_Japan1CID2Unicode_4;
  pFontGlobals->m_EmbeddedToUnicodes[CIDSET_JAPAN1].m_Count = 15444;
}
