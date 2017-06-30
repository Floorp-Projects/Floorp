// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fxge/cfx_fxgedevice.h"
#include "core/fxge/cfx_graphstatedata.h"
#include "core/fxge/cfx_pathdata.h"
#include "core/fxge/cfx_renderdevice.h"
#include "core/fxge/skia/fx_skia_device.h"
#include "fpdfsdk/fsdk_define.h"
#include "public/fpdfview.h"
#include "testing/fx_string_testhelpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"

namespace {

struct State {
  enum class Change { kNo, kYes };
  enum class Save { kNo, kYes };
  enum class Clip { kNo, kSame, kDifferentPath, kDifferentMatrix };
  enum class Graphic { kNone, kPath, kText };

  Change m_change;
  Save m_save;
  Clip m_clip;
  Graphic m_graphic;
  uint32_t m_pixel;
};

void EmptyTest(CFX_SkiaDeviceDriver* driver, const State&) {
  driver->SaveState();
  driver->RestoreState(true);
  driver->RestoreState(false);
}

void CommonTest(CFX_SkiaDeviceDriver* driver, const State& state) {
  FXTEXT_CHARPOS charPos[1];
  charPos[0].m_Origin = CFX_PointF(0, 1);
  charPos[0].m_GlyphIndex = 1;
  charPos[0].m_FontCharWidth = 4;

  CFX_Font font;
  FX_FLOAT fontSize = 1;
  CFX_PathData clipPath, clipPath2;
  clipPath.AppendRect(0, 0, 3, 1);
  clipPath2.AppendRect(0, 0, 2, 1);
  CFX_Matrix clipMatrix;
  CFX_Matrix clipMatrix2(1, 0, 0, 1, 0, 1);
  driver->SaveState();
  CFX_PathData path1;
  path1.AppendRect(0, 0, 1, 2);

  CFX_Matrix matrix;
  CFX_Matrix matrix2;
  matrix2.Translate(1, 0);
  CFX_GraphStateData graphState;
  if (state.m_save == State::Save::kYes)
    driver->SaveState();
  if (state.m_clip != State::Clip::kNo)
    driver->SetClip_PathFill(&clipPath, &clipMatrix, 0);
  if (state.m_graphic == State::Graphic::kPath) {
    driver->DrawPath(&path1, &matrix, &graphState, 0xFF112233, 0,
                     FXFILL_WINDING, 0);
  } else if (state.m_graphic == State::Graphic::kText) {
    driver->DrawDeviceText(SK_ARRAY_COUNT(charPos), charPos, &font, &matrix,
                           fontSize, 0xFF445566);
  }
  if (state.m_save == State::Save::kYes)
    driver->RestoreState(true);
  CFX_PathData path2;
  path2.AppendRect(0, 0, 2, 2);
  if (state.m_change == State::Change::kYes) {
    if (state.m_graphic == State::Graphic::kPath)
      graphState.m_LineCap = CFX_GraphStateData::LineCapRound;
    else if (state.m_graphic == State::Graphic::kText)
      fontSize = 2;
  }
  if (state.m_clip == State::Clip::kSame)
    driver->SetClip_PathFill(&clipPath, &clipMatrix, 0);
  else if (state.m_clip == State::Clip::kDifferentPath)
    driver->SetClip_PathFill(&clipPath2, &clipMatrix, 0);
  else if (state.m_clip == State::Clip::kDifferentMatrix)
    driver->SetClip_PathFill(&clipPath, &clipMatrix2, 0);
  if (state.m_graphic == State::Graphic::kPath) {
    driver->DrawPath(&path2, &matrix2, &graphState, 0xFF112233, 0,
                     FXFILL_WINDING, 0);
  } else if (state.m_graphic == State::Graphic::kText) {
    driver->DrawDeviceText(SK_ARRAY_COUNT(charPos), charPos, &font, &matrix2,
                           fontSize, 0xFF445566);
  }
  if (state.m_save == State::Save::kYes)
    driver->RestoreState(false);
  driver->RestoreState(false);
}

void OutOfSequenceClipTest(CFX_SkiaDeviceDriver* driver, const State&) {
  CFX_PathData clipPath;
  clipPath.AppendRect(1, 0, 3, 1);
  CFX_Matrix clipMatrix;
  driver->SaveState();
  driver->SetClip_PathFill(&clipPath, &clipMatrix, 0);
  driver->RestoreState(true);
  driver->SaveState();
  driver->SetClip_PathFill(&clipPath, &clipMatrix, 0);
  driver->RestoreState(false);
  driver->RestoreState(false);

  driver->SaveState();
  driver->SaveState();
  driver->SetClip_PathFill(&clipPath, &clipMatrix, 0);
  driver->RestoreState(true);
  driver->SetClip_PathFill(&clipPath, &clipMatrix, 0);
  driver->RestoreState(false);
  driver->RestoreState(false);
}

void Harness(void (*Test)(CFX_SkiaDeviceDriver*, const State&),
             const State& state) {
  int h = 1;
  int w = 4;
  FPDF_BITMAP bitmap = FPDFBitmap_Create(w, h, 1);
  EXPECT_NE(nullptr, bitmap);
  if (!bitmap)
    return;
  FPDFBitmap_FillRect(bitmap, 0, 0, w, h, 0x00000000);
  CFX_FxgeDevice geDevice;
  CFX_DIBitmap* pBitmap = CFXBitmapFromFPDFBitmap(bitmap);
  geDevice.Attach(pBitmap, false, nullptr, false);
  CFX_SkiaDeviceDriver* driver =
      static_cast<CFX_SkiaDeviceDriver*>(geDevice.GetDeviceDriver());
  (*Test)(driver, state);
  driver->Flush();
  uint32_t pixel = pBitmap->GetPixel(0, 0);
  EXPECT_EQ(state.m_pixel, pixel);
#ifdef SK_DEBUG
  if (!driver)  // force dump to be linked in so it can be called from debugger
    driver->Dump();
#endif
}

}  // namespace

TEST(fxge, SkiaStateEmpty) {
  Harness(&EmptyTest, {});
}

TEST(fxge, SkiaStatePath) {
  Harness(&CommonTest, {State::Change::kNo, State::Save::kYes,
                        State::Clip::kSame, State::Graphic::kPath, 0xFF112233});
  Harness(&CommonTest,
          {State::Change::kNo, State::Save::kYes, State::Clip::kDifferentPath,
           State::Graphic::kPath, 0xFF112233});
  Harness(&CommonTest, {State::Change::kNo, State::Save::kYes, State::Clip::kNo,
                        State::Graphic::kPath, 0xFF112233});
  Harness(&CommonTest, {State::Change::kYes, State::Save::kNo, State::Clip::kNo,
                        State::Graphic::kPath, 0xFF112233});
  Harness(&CommonTest, {State::Change::kNo, State::Save::kNo, State::Clip::kNo,
                        State::Graphic::kPath, 0xFF112233});
}

#ifdef _SKIA_SUPPORT_
TEST(fxge, SkiaStateText) {
  Harness(&CommonTest,
          {State::Change::kNo, State::Save::kYes, State::Clip::kDifferentMatrix,
           State::Graphic::kText, 0xFF445566});
  Harness(&CommonTest, {State::Change::kNo, State::Save::kYes,
                        State::Clip::kSame, State::Graphic::kText, 0xFF445566});
}
#endif

TEST(fxge, SkiaStateOOSClip) {
  Harness(&OutOfSequenceClipTest, {});
}
