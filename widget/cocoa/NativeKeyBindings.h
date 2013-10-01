/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NativeKeyBindings_h_
#define NativeKeyBindings_h_

#include "nsINativeKeyBindings.h"

#import <Cocoa/Cocoa.h>
#include "mozilla/Attributes.h"
#include "mozilla/EventForwards.h"
#include "nsDataHashtable.h"

// 8477f934-febf-4c79-b7fe-bb7f9ebb9b4f
#define NS_NATIVEKEYBINDINGS_INPUT_CID \
  { 0x8477f934, 0xfebf, 0x4c79, \
    { 0xb7, 0xfe, 0xbb, 0x7f, 0x9e, 0xbb, 0x9b, 0x4f } }

// 13a6e56f-f00b-4e19-8cf6-1a51ee7cc4bf
#define NS_NATIVEKEYBINDINGS_TEXTAREA_CID \
  { 0x13a6e56f, 0xf00b, 0x4e19, \
    { 0x8c, 0xf6, 0x1a, 0x51, 0xee, 0x7c, 0xc4, 0xbf } }

// 36bfbd29-4e02-40f4-8fff-094f1a9ec97c
#define NS_NATIVEKEYBINDINGS_EDITOR_CID \
  { 0x36bfbd29, 0x4e02, 0x40f4, \
    { 0x8f, 0xff, 0x09, 0x4f, 0x1a, 0x9e, 0xc9, 0x7c } }

namespace mozilla {
namespace widget {

enum NativeKeyBindingsType
{
  eNativeKeyBindingsType_Input,
  eNativeKeyBindingsType_TextArea,
  eNativeKeyBindingsType_Editor
};

typedef nsDataHashtable<nsPtrHashKey<struct objc_selector>, const char *>
  SelectorCommandHashtable;

class NativeKeyBindings MOZ_FINAL : public nsINativeKeyBindings
{
public:
  NativeKeyBindings();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init(NativeKeyBindingsType aType);

  // nsINativeKeyBindings
  NS_IMETHOD_(bool) KeyDown(const WidgetKeyboardEvent& aEvent,
                            DoCommandCallback aCallback,
                            void* aCallbackData);

  NS_IMETHOD_(bool) KeyPress(const WidgetKeyboardEvent& aEvent,
                             DoCommandCallback aCallback,
                             void* aCallbackData);

  NS_IMETHOD_(bool) KeyUp(const WidgetKeyboardEvent& aEvent,
                          DoCommandCallback aCallback,
                          void* aCallbackData);

private:
  SelectorCommandHashtable mSelectorToCommand;
}; // NativeKeyBindings

} // namespace widget
} // namespace mozilla

#endif /* NativeKeyBindings_h_ */
