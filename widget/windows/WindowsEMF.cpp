/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WindowsEMF.h"


namespace mozilla {
namespace widget {

WindowsEMF::WindowsEMF()
  : mEmf(nullptr)
  , mDC(nullptr)
{
}

WindowsEMF::~WindowsEMF()
{
  ReleaseAllResource();
}

bool
WindowsEMF::InitForDrawing(const wchar_t* aMetafilePath /* = nullptr */)
{
  ReleaseAllResource();

  mDC = ::CreateEnhMetaFile(nullptr, aMetafilePath, nullptr, nullptr);
  return !!mDC;
}

bool
WindowsEMF::InitFromFileContents(const wchar_t* aMetafilePath)
{
  MOZ_ASSERT(aMetafilePath);
  ReleaseAllResource();

  mEmf = ::GetEnhMetaFileW(aMetafilePath);
  return !!mEmf;
}

bool
WindowsEMF::InitFromFileContents(LPBYTE aBytes, UINT aSize)
{
  MOZ_ASSERT(aBytes && aSize != 0);
  ReleaseAllResource();

  mEmf = SetEnhMetaFileBits(aSize, aBytes);

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

void
WindowsEMF::ReleaseAllResource()
{
  FinishDocument();
  ReleaseEMFHandle();
}

bool
WindowsEMF::Playback(HDC aDeviceContext, const RECT& aRect)
{
  DebugOnly<bool> result = FinishDocument();
  MOZ_ASSERT(result, "This function should be used after InitXXX.");

  return ::PlayEnhMetaFile(aDeviceContext, mEmf, &aRect) != 0;
}

bool
WindowsEMF::SaveToFile()
{
  DebugOnly<bool> result = FinishDocument();
  MOZ_ASSERT(result, "This function should be used after InitXXX.");

  ReleaseEMFHandle();
  return true;
}

UINT
WindowsEMF::GetEMFContentSize()
{
  DebugOnly<bool> result = FinishDocument();
  MOZ_ASSERT(result, "This function should be used after InitXXX.");

  return GetEnhMetaFileBits(mEmf, 0, NULL);
}

bool
WindowsEMF::GetEMFContentBits(LPBYTE aBytes)
{
  DebugOnly<bool> result = FinishDocument();
  MOZ_ASSERT(result, "This function should be used after InitXXX.");

  UINT emfSize = GetEMFContentSize();
  if (GetEnhMetaFileBits(mEmf, emfSize, aBytes) != emfSize) {
    return false;
  }

  return true;
}

} // namespace widget
} // namespace mozilla