/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsAutoWindowStateHelper_h
#define __nsAutoWindowStateHelper_h

#include "nsCOMPtr.h"
#include "nsPIDOMWindow.h"

/**
 * Helper class for dealing with notifications around opening modal
 * windows.
 */

class nsPIDOMWindowOuter;

class nsAutoWindowStateHelper {
 public:
  explicit nsAutoWindowStateHelper(nsPIDOMWindowOuter* aWindow);
  ~nsAutoWindowStateHelper();

  bool DefaultEnabled() { return mDefaultEnabled; }

 protected:
  bool DispatchEventToChrome(const char* aEventName);

  nsCOMPtr<nsPIDOMWindowOuter> mWindow;
  bool mDefaultEnabled;
};

#endif
