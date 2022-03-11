/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FormAutofillNative_h
#define mozilla_dom_FormAutofillNative_h

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/ChromeUtilsBinding.h"

namespace mozilla::dom {
class Element;

class FormAutofillNative {
 public:
  static void GetFormAutofillConfidences(
      GlobalObject& aGlobal, const Sequence<OwningNonNull<Element>>& aElements,
      nsTArray<FormAutofillConfidences>& aResults, ErrorResult& aRv);
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_FormAutofillNative_h
