/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef tools_profiler_MemoryProfiler_h
#define tools_profiler_MemoryProfiler_h

#include "nsIMemoryProfiler.h"

#include "nsString.h"

#define MEMORY_PROFILER_CID                                     \
  { 0xf976eaa2, 0xcc1f, 0x47ee,                                 \
    { 0x81, 0x29, 0xb8, 0x26, 0x2a, 0x3d, 0xb6, 0xb2 } }

#define MEMORY_PROFILER_CONTRACT_ID "@mozilla.org/tools/memory-profiler;1"

class MemoryProfiler : public nsIMemoryProfiler
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMEMORYPROFILER

  MemoryProfiler();

private:
  virtual ~MemoryProfiler();

protected:
  /* additional members */
};

#endif
