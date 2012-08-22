/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsINativeKeyBindings_h_
#define nsINativeKeyBindings_h_

#include "nsISupports.h"
#include "nsEvent.h"

#define NS_INATIVEKEYBINDINGS_IID \
{0x606c54e7, 0x0593, 0x4750, {0x99, 0xd9, 0x4e, 0x1b, 0xcc, 0xec, 0x98, 0xd9}}

#define NS_NATIVEKEYBINDINGS_CONTRACTID_PREFIX \
  "@mozilla.org/widget/native-key-bindings;1?type="

struct nsNativeKeyEvent
{
  nsEvent *nativeEvent; // see bug 406407 to see how this is used
  uint32_t keyCode;
  uint32_t charCode;
  bool     altKey;
  bool     ctrlKey;
  bool     shiftKey;
  bool     metaKey;
};

class nsINativeKeyBindings : public nsISupports
{
 public:
  typedef void (*DoCommandCallback)(const char *, void*);

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_INATIVEKEYBINDINGS_IID)

  virtual NS_HIDDEN_(bool) KeyDown(const nsNativeKeyEvent& aEvent,
                                     DoCommandCallback aCallback,
                                     void *aCallbackData) = 0;

  virtual NS_HIDDEN_(bool) KeyPress(const nsNativeKeyEvent& aEvent,
                                      DoCommandCallback aCallback,
                                      void *aCallbackData) = 0;

  virtual NS_HIDDEN_(bool) KeyUp(const nsNativeKeyEvent& aEvent,
                                   DoCommandCallback aCallback,
                                   void *aCallbackData) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsINativeKeyBindings, NS_INATIVEKEYBINDINGS_IID)

#endif
