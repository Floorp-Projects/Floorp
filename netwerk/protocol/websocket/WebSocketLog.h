/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WebSocketLog_h
#define WebSocketLog_h

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG
#endif

#if defined(PR_LOG)
#error "This file must be #included before any IPDL-generated files or other files that #include prlog.h"
#endif

#include "base/basictypes.h"
#include "prlog.h"
#include "mozilla/net/NeckoChild.h"

#ifdef PR_LOGGING
extern PRLogModuleInfo* webSocketLog;
#endif

#undef LOG
#define LOG(args) PR_LOG(webSocketLog, PR_LOG_DEBUG, args)

#endif
