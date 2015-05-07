/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DataChannelLog_h
#define DataChannelLog_h

#include "base/basictypes.h"
#include "prlog.h"

extern PRLogModuleInfo* GetDataChannelLog();
extern PRLogModuleInfo* GetSCTPLog();

#undef LOG
#define LOG(args) PR_LOG(GetDataChannelLog(), PR_LOG_DEBUG, args)

#endif
