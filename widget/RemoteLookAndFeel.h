/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_RemoteLookAndFeel_h__
#define mozilla_widget_RemoteLookAndFeel_h__

#include "mozilla/widget/nsXPLookAndFeel.h"
#include "mozilla/widget/LookAndFeelTypes.h"

namespace mozilla::widget {

/**
 * A LookAndFeel implementation whose native values are provided by the
 * parent process.
 */
class RemoteLookAndFeel final : public nsXPLookAndFeel {
 public:
  explicit RemoteLookAndFeel(FullLookAndFeel&& aTables);

  virtual ~RemoteLookAndFeel();

  void NativeInit() override {}

  nsresult NativeGetInt(IntID aID, int32_t& aResult) override;
  nsresult NativeGetFloat(FloatID aID, float& aResult) override;
  nsresult NativeGetColor(ColorID aID, nscolor& aResult) override;
  bool NativeGetFont(FontID aID, nsString& aFontName,
                     gfxFontStyle& aFontStyle) override;

  char16_t GetPasswordCharacterImpl() override;
  bool GetEchoPasswordImpl() override;

  // Sets the LookAndFeel data to be used by this content process' singleton
  // RemoteLookAndFeel object.
  void SetDataImpl(FullLookAndFeel&& aTables) override;

  // Extracts the data from the platform's default LookAndFeel implementation.
  //
  // This is called in the parent process to obtain the data to send down to
  // content processes when they are created (and when the OS theme changes).
  static FullLookAndFeel ExtractData();

 private:
  FullLookAndFeel mTables;
};

}  // namespace mozilla::widget

#endif  // mozilla_widget_RemoteLookAndFeel_h__
