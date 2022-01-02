/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _SECURITY_SANDBOX_TRACKED_OBJECTS_H_
#define _SECURITY_SANDBOX_TRACKED_OBJECTS_H_

#include "mozilla/Assertions.h"

namespace tracked_objects
{
  class ThreadData
  {
  public:
    static void InitializeThreadContext(const std::string& name)
    {
      MOZ_CRASH();
    }
  };
}
#endif
