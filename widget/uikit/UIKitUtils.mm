/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UIKitUtils.h"

namespace mozilla::widget {

// static
UIKeyboardType UIKitUtils::GetUIKeyboardType(const InputContext& aContext) {
  if (aContext.mHTMLInputMode.EqualsLiteral("email")) {
    return UIKeyboardTypeEmailAddress;
  }
  if (aContext.mHTMLInputMode.EqualsLiteral("deciaml")) {
    return UIKeyboardTypeDecimalPad;
  }
  if (aContext.mHTMLInputMode.EqualsLiteral("numeric")) {
    return UIKeyboardTypeNumberPad;
  }
  if (aContext.mHTMLInputMode.EqualsLiteral("search")) {
    return UIKeyboardTypeWebSearch;
  }
  if (aContext.mHTMLInputMode.EqualsLiteral("tel")) {
    return UIKeyboardTypePhonePad;
  }
  if (aContext.mHTMLInputMode.EqualsLiteral("url")) {
    return UIKeyboardTypeURL;
  }

  if (aContext.mHTMLInputType.EqualsLiteral("email")) {
    return UIKeyboardTypeEmailAddress;
  }
  if (aContext.mHTMLInputType.EqualsLiteral("number")) {
    return UIKeyboardTypeNumberPad;
  }
  if (aContext.mHTMLInputType.EqualsLiteral("tel")) {
    return UIKeyboardTypePhonePad;
  }
  if (aContext.mHTMLInputType.EqualsLiteral("url")) {
    return UIKeyboardTypeURL;
  }

  return UIKeyboardTypeDefault;
}

// static
UIReturnKeyType UIKitUtils::GetUIReturnKeyType(const InputContext& aContext) {
  if (aContext.mActionHint.EqualsLiteral("done")) {
    return UIReturnKeyDone;
  }
  if (aContext.mActionHint.EqualsLiteral("go")) {
    return UIReturnKeyGo;
  }
  if (aContext.mActionHint.EqualsLiteral("next") ||
      aContext.mActionHint.EqualsLiteral("maybenext")) {
    return UIReturnKeyNext;
  }
  if (aContext.mActionHint.EqualsLiteral("search")) {
    return UIReturnKeySearch;
  }
  if (aContext.mActionHint.EqualsLiteral("send")) {
    return UIReturnKeySend;
  }

  return UIReturnKeyDefault;
}

// static
UITextAutocapitalizationType UIKitUtils::GetUITextAutocapitalizationType(
    const InputContext& aContext) {
  if (aContext.mAutocapitalize.EqualsLiteral("characters")) {
    return UITextAutocapitalizationTypeAllCharacters;
  }
  if (aContext.mAutocapitalize.EqualsLiteral("none")) {
    return UITextAutocapitalizationTypeNone;
  }
  if (aContext.mAutocapitalize.EqualsLiteral("sentences")) {
    return UITextAutocapitalizationTypeSentences;
  }
  if (aContext.mAutocapitalize.EqualsLiteral("words")) {
    return UITextAutocapitalizationTypeWords;
  }
  // TODO(m_kato):
  // Infer autocapitalization type by input type like GeckoView.
  return UITextAutocapitalizationTypeNone;
}

}  // namespace mozilla::widget
