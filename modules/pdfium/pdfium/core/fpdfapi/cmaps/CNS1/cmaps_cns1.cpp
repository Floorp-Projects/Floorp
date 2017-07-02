// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/cmaps/CNS1/cmaps_cns1.h"

#include "core/fpdfapi/cmaps/cmap_int.h"
#include "core/fpdfapi/cpdf_modulemgr.h"
#include "core/fpdfapi/font/font_int.h"
#include "core/fpdfapi/page/cpdf_pagemodule.h"
#include "core/fxcrt/fx_basic.h"

static const FXCMAP_CMap g_FXCMAP_CNS1_cmaps[] = {
    {"B5pc-H", FXCMAP_CMap::Range, g_FXCMAP_B5pc_H_0, 247, FXCMAP_CMap::None,
     nullptr, 0, 0},
    {"B5pc-V", FXCMAP_CMap::Range, g_FXCMAP_B5pc_V_0, 12, FXCMAP_CMap::None,
     nullptr, 0, -1},
    {"HKscs-B5-H", FXCMAP_CMap::Range, g_FXCMAP_HKscs_B5_H_5, 1210,
     FXCMAP_CMap::None, nullptr, 0, 0},
    {"HKscs-B5-V", FXCMAP_CMap::Range, g_FXCMAP_HKscs_B5_V_5, 13,
     FXCMAP_CMap::None, nullptr, 0, -1},
    {"ETen-B5-H", FXCMAP_CMap::Range, g_FXCMAP_ETen_B5_H_0, 254,
     FXCMAP_CMap::None, nullptr, 0, 0},
    {"ETen-B5-V", FXCMAP_CMap::Range, g_FXCMAP_ETen_B5_V_0, 13,
     FXCMAP_CMap::None, nullptr, 0, -1},
    {"ETenms-B5-H", FXCMAP_CMap::Range, g_FXCMAP_ETenms_B5_H_0, 1,
     FXCMAP_CMap::None, nullptr, 0, -2},
    {"ETenms-B5-V", FXCMAP_CMap::Range, g_FXCMAP_ETenms_B5_V_0, 18,
     FXCMAP_CMap::None, nullptr, 0, -1},
    {"CNS-EUC-H", FXCMAP_CMap::Range, g_FXCMAP_CNS_EUC_H_0, 157,
     FXCMAP_CMap::Range, g_FXCMAP_CNS_EUC_H_0_DWord, 238, 0},
    {"CNS-EUC-V", FXCMAP_CMap::Range, g_FXCMAP_CNS_EUC_V_0, 180,
     FXCMAP_CMap::Range, g_FXCMAP_CNS_EUC_V_0_DWord, 261, 0},
    {"UniCNS-UCS2-H", FXCMAP_CMap::Range, g_FXCMAP_UniCNS_UCS2_H_3, 16418,
     FXCMAP_CMap::None, nullptr, 0, 0},
    {"UniCNS-UCS2-V", FXCMAP_CMap::Range, g_FXCMAP_UniCNS_UCS2_V_3, 13,
     FXCMAP_CMap::None, nullptr, 0, -1},
    {"UniCNS-UTF16-H", FXCMAP_CMap::Single, g_FXCMAP_UniCNS_UTF16_H_0, 14557,
     FXCMAP_CMap::None, nullptr, 0, 0},
    {"UniCNS-UTF16-V", FXCMAP_CMap::Range, g_FXCMAP_UniCNS_UCS2_V_3, 13,
     FXCMAP_CMap::None, nullptr, 0, -1},
};

void CPDF_ModuleMgr::LoadEmbeddedCNS1CMaps() {
  CPDF_FontGlobals* pFontGlobals =
      CPDF_ModuleMgr::Get()->GetPageModule()->GetFontGlobals();
  pFontGlobals->m_EmbeddedCharsets[CIDSET_CNS1].m_pMapList =
      g_FXCMAP_CNS1_cmaps;
  pFontGlobals->m_EmbeddedCharsets[CIDSET_CNS1].m_Count =
      FX_ArraySize(g_FXCMAP_CNS1_cmaps);
  pFontGlobals->m_EmbeddedToUnicodes[CIDSET_CNS1].m_pMap =
      g_FXCMAP_CNS1CID2Unicode_5;
  pFontGlobals->m_EmbeddedToUnicodes[CIDSET_CNS1].m_Count = 19088;
}
