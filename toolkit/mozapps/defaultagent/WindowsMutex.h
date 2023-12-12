/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __DEFAULT_BROWSER_AGENT_DEFAULT_AGENT_MUTEX_H__
#define __DEFAULT_BROWSER_AGENT_DEFAULT_AGENT_MUTEX_H__

#include "nsString.h"
#include "nsWindowsHelpers.h"

#include "nsIWindowsMutex.h"

namespace mozilla::default_agent {

class WindowsMutexFactory final : public nsIWindowsMutexFactory {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIWINDOWSMUTEXFACTORY

  WindowsMutexFactory() = default;

 private:
  ~WindowsMutexFactory() = default;
};

class WindowsMutex final : public nsIWindowsMutex {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIWINDOWSMUTEX

  WindowsMutex(const nsString& aName, nsAutoHandle& aMutex);

 private:
  nsAutoHandle mMutex;
  nsCString mName;
  bool mLocked;

  ~WindowsMutex();
};

}  // namespace mozilla::default_agent

#endif  // __DEFAULT_BROWSER_AGENT_DEFAULT_AGENT_MUTEX_H__
