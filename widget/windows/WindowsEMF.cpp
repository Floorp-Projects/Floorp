/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WindowsEMF.h"


namespace mozilla {
namespace widget {

WindowsEMF::WindowsEMF()
  : mDC(nullptr)
  , mEmf(nullptr)
{
}

WindowsEMF::~WindowsEMF()
{
  FinishDocument();
  ReleaseEMFHandle();
}

bool
WindowsEMF::InitForDrawing(const wchar_t* aMetafilePath /* = nullptr */)
{
  MOZ_ASSERT(!mDC && !mEmf, "InitForDrawing and InitFromFileContents is"
                            " designed to be used either one at once.");

  mDC = ::CreateEnhMetaFile(nullptr, aMetafilePath, nullptr, nullptr);
  return !!mDC;
}

bool
WindowsEMF::InitFromFileContents(const wchar_t* aMetafilePath)
{
  MOZ_ASSERT(aMetafilePath);
  MOZ_ASSERT(!mDC && !mEmf, "InitForDrawing and InitFromFileContents is"
                            " designed to be used either one at once.");

  mEmf = ::GetEnhMetaFileW(aMetafilePath);
  return !!mEmf;
}

bool
WindowsEMF::FinishDocument()
{
  if (mDC) {
     mEmf = ::CloseEnhMetaFile(mDC);
     mDC = nullptr;
  }
  return !!mEmf;
}

void
WindowsEMF::ReleaseEMFHandle()
{
  if (mEmf) {
    ::DeleteEnhMetaFile(mEmf);
    mEmf = nullptr;
  }
}

bool
WindowsEMF::Playback(HDC aDeviceContext, const RECT* aRect)
{
  MOZ_ASSERT(aRect);
  if (!FinishDocument()) {
    return false;
  }

  return ::PlayEnhMetaFile(aDeviceContext, mEmf, aRect) != 0;
}

bool
WindowsEMF::SaveToFile()
{
  if (!FinishDocument()) {
    return false;
  }
  ReleaseEMFHandle();
  return true;
}

} // namespace widget
} // namespace mozilla