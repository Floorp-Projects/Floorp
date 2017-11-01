/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_WIDGET_WINDOWSEMF_H
#define MOZILLA_WIDGET_WINDOWSEMF_H

/* include windows.h for the HDC definitions that we need. */
#include <windows.h>

namespace mozilla {
namespace widget {

/**
 * Windows Enhance Metafile: https://en.wikipedia.org/wiki/Windows_Metafile
 * A metafile, also called a vector image, is an image that is stored as a
 * sequence of drawing commands and settings. The commands and settings
 * recorded in a Metafile object can be stored in memory or saved to a file.
 *
 * The metafile device context is used for all drawing operations required to
 * create the picture. When the system processes a GDI function associated with
 * a metafile DC, it converts the function into the appropriate data and stores
 * this data in a record appended to the metafile.
 */
class WindowsEMF
{
public:
  WindowsEMF();
  ~WindowsEMF();

  /**
   * Initializes the object with the path of a file where the EMF data stream
   * should be stored. Callers are then expected to call GetDC() to draw output
   * before going on to call Playback() or SaveToFile() to generate the EMF
   * output.
   */
  bool InitForDrawing(const wchar_t* aMetafilePath = nullptr);

  /**
   * Initializes the object with an existing EMF file. Consumers cannot use
   * GetDC() to obtain an HDC to modify the file. They can only use Playback().
   */
  bool InitFromFileContents(const wchar_t* aMetafilePath);

  /**
   * Creates the EMF from the specified data
   *
   * @param aByte Pointer to a buffer that contains EMF data.
   * @param aSize Specifies the size, in bytes, of aByte.
   */
  bool InitFromFileContents(PBYTE aBytes, UINT aSize);

  /**
   * If this object was initiaziled using InitForDrawing() then this function
   * returns an HDC that can be drawn to generate the EMF output. Otherwise it
   * returns null. After finishing with the HDC, consumers could call Playback()
   * to draw EMF onto the given DC or call SaveToFile() to finish writing the
   * EMF file.
   */
  HDC GetDC() const
  {
    MOZ_ASSERT(mDC, "GetDC can be used only after "
                    "InitForDrawing/ InitFromFileContents and before"
                    "Playback/ SaveToFile");
    return mDC;
  }

  /**
   * Play the EMF's drawing commands onto the given DC.
   */
  bool Playback(HDC aDeviceContext, const RECT& aRect);

  /**
   * Called to generate the EMF file once a consumer has finished drawing to
   * the HDC returned by GetDC(), if initializes the object with the path of a
   * file.
   */
  bool SaveToFile();

  /**
   * Return the size of the enhanced metafile, in bytes.
   */
  UINT GetEMFContentSize();

  /**
   * Retrieves the contents of the EMF and copies them into a buffer.
   *
   * @param aByte the buffer to receive the data.
   */
  bool GetEMFContentBits(PBYTE aBytes);

private:

  WindowsEMF(const WindowsEMF& aEMF) = delete;
  bool FinishDocument();
  void ReleaseEMFHandle();
  void ReleaseAllResource();

  /* Compiled EMF data handle. */
  HENHMETAFILE mEmf;
  HDC mDC;
};

} // namespace widget
} // namespace mozilla

#endif /* MOZILLA_WIDGET_WINDOWSEMF_H */