/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/WidgetUtils.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/Components.h"
#include "mozilla/Unused.h"
#include "nsContentUtils.h"
#include "nsIBidiKeyboard.h"
#include "nsIStringBundle.h"
#include "nsTArray.h"
#include "prenv.h"

namespace mozilla {

gfx::Matrix ComputeTransformForRotation(const nsIntRect& aBounds,
                                        ScreenRotation aRotation) {
  gfx::Matrix transform;
  static const gfx::Float floatPi = static_cast<gfx::Float>(M_PI);

  switch (aRotation) {
    case ROTATION_0:
      break;
    case ROTATION_90:
      transform.PreTranslate(aBounds.Width(), 0);
      transform.PreRotate(floatPi / 2);
      break;
    case ROTATION_180:
      transform.PreTranslate(aBounds.Width(), aBounds.Height());
      transform.PreRotate(floatPi);
      break;
    case ROTATION_270:
      transform.PreTranslate(0, aBounds.Height());
      transform.PreRotate(floatPi * 3 / 2);
      break;
    default:
      MOZ_CRASH("Unknown rotation");
  }
  return transform;
}

gfx::Matrix ComputeTransformForUnRotation(const nsIntRect& aBounds,
                                          ScreenRotation aRotation) {
  gfx::Matrix transform;
  static const gfx::Float floatPi = static_cast<gfx::Float>(M_PI);

  switch (aRotation) {
    case ROTATION_0:
      break;
    case ROTATION_90:
      transform.PreTranslate(0, aBounds.Height());
      transform.PreRotate(floatPi * 3 / 2);
      break;
    case ROTATION_180:
      transform.PreTranslate(aBounds.Width(), aBounds.Height());
      transform.PreRotate(floatPi);
      break;
    case ROTATION_270:
      transform.PreTranslate(aBounds.Width(), 0);
      transform.PreRotate(floatPi / 2);
      break;
    default:
      MOZ_CRASH("Unknown rotation");
  }
  return transform;
}

nsIntRect RotateRect(nsIntRect aRect, const nsIntRect& aBounds,
                     ScreenRotation aRotation) {
  switch (aRotation) {
    case ROTATION_0:
      return aRect;
    case ROTATION_90:
      return nsIntRect(aRect.Y(), aBounds.Width() - aRect.XMost(),
                       aRect.Height(), aRect.Width());
    case ROTATION_180:
      return nsIntRect(aBounds.Width() - aRect.XMost(),
                       aBounds.Height() - aRect.YMost(), aRect.Width(),
                       aRect.Height());
    case ROTATION_270:
      return nsIntRect(aBounds.Height() - aRect.YMost(), aRect.X(),
                       aRect.Height(), aRect.Width());
    default:
      MOZ_CRASH("Unknown rotation");
  }
}

namespace widget {

// static
void WidgetUtils::SendBidiKeyboardInfoToContent() {
  nsCOMPtr<nsIBidiKeyboard> bidiKeyboard = nsContentUtils::GetBidiKeyboard();
  if (!bidiKeyboard) {
    return;
  }

  bool rtl;
  if (NS_FAILED(bidiKeyboard->IsLangRTL(&rtl))) {
    return;
  }
  bool bidiKeyboards = false;
  bidiKeyboard->GetHaveBidiKeyboards(&bidiKeyboards);

  nsTArray<dom::ContentParent*> children;
  dom::ContentParent::GetAll(children);
  for (uint32_t i = 0; i < children.Length(); i++) {
    Unused << children[i]->SendBidiKeyboardNotify(rtl, bidiKeyboards);
  }
}

// static
void WidgetUtils::GetBrandShortName(nsAString& aBrandName) {
  aBrandName.Truncate();

  nsCOMPtr<nsIStringBundleService> bundleService =
      mozilla::components::StringBundle::Service();

  nsCOMPtr<nsIStringBundle> bundle;
  if (bundleService) {
    bundleService->CreateBundle("chrome://branding/locale/brand.properties",
                                getter_AddRefs(bundle));
  }

  if (bundle) {
    bundle->GetStringFromName("brandShortName", aBrandName);
  }
}

}  // namespace widget
}  // namespace mozilla
