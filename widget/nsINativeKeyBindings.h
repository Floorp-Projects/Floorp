/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsINativeKeyBindings_h_
#define nsINativeKeyBindings_h_

#include "nsISupports.h"
#include "mozilla/EventForwards.h"

#define NS_INATIVEKEYBINDINGS_IID \
{0xc2baecc3, 0x1758, 0x4211, {0x96, 0xbe, 0xee, 0x1b, 0x1b, 0x7c, 0xd7, 0x6d}}

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
  typedef void (*DoCommandCallback)(const char *, void*);

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_INATIVEKEYBINDINGS_IID)

  virtual NS_HIDDEN_(bool) KeyDown(const mozilla::WidgetKeyboardEvent& aEvent,
                                   DoCommandCallback aCallback,
                                   void *aCallbackData) = 0;

  virtual NS_HIDDEN_(bool) KeyPress(const mozilla::WidgetKeyboardEvent& aEvent,
                                    DoCommandCallback aCallback,
                                    void *aCallbackData) = 0;

  virtual NS_HIDDEN_(bool) KeyUp(const mozilla::WidgetKeyboardEvent& aEvent,
                                 DoCommandCallback aCallback,
                                 void *aCallbackData) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsINativeKeyBindings, NS_INATIVEKEYBINDINGS_IID)

#endif
