/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsRemoteServer_h__
#define __nsRemoteServer_h__

#include "nsString.h"

class nsRemoteServer {
 public:
  virtual ~nsRemoteServer() = default;

  virtual nsresult Startup(const char* aAppName, const char* aProfileName) = 0;
  virtual void Shutdown() = 0;
};

#endif  // __nsRemoteServer_h__
