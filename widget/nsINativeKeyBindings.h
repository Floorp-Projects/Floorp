/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsINativeKeyBindings_h_
#define nsINativeKeyBindings_h_

#include "nsISupports.h"
#include "nsIWidget.h"
#include "mozilla/EventForwards.h"

#define NS_INATIVEKEYBINDINGS_IID \
{ 0x98290677, 0xfdac, 0x414a, \
  { 0x81, 0x5c, 0x20, 0xe2, 0xd4, 0xcd, 0x8c, 0x47 } }

#define NS_NATIVEKEYBINDINGS_CONTRACTID_PREFIX \
  "@mozilla.org/widget/native-key-bindings;1?type="

#define NS_NATIVEKEYBINDINGSINPUT_CONTRACTID \
NS_NATIVEKEYBINDINGS_CONTRACTID_PREFIX "input"

#define NS_NATIVEKEYBINDINGSTEXTAREA_CONTRACTID \
NS_NATIVEKEYBINDINGS_CONTRACTID_PREFIX "textarea"

#define NS_NATIVEKEYBINDINGSEDITOR_CONTRACTID \
NS_NATIVEKEYBINDINGS_CONTRACTID_PREFIX "editor"

class nsINativeKeyBindings : public nsISupports
{
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_INATIVEKEYBINDINGS_IID)

  virtual NS_HIDDEN_(bool) KeyPress(const mozilla::WidgetKeyboardEvent& aEvent,
                                    nsIWidget::DoCommandCallback aCallback,
                                    void *aCallbackData) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsINativeKeyBindings, NS_INATIVEKEYBINDINGS_IID)

#endif
