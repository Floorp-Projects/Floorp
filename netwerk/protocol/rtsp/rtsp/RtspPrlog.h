/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef RTSPPRLOG_H
#define RTSPPRLOG_H

#include "mozilla/Logging.h"

extern PRLogModuleInfo* gRtspLog;

#define LOGI(msg, ...) MOZ_LOG(gRtspLog, mozilla::LogLevel::Info, (msg, ##__VA_ARGS__))
#define LOGV(msg, ...) MOZ_LOG(gRtspLog, mozilla::LogLevel::Debug, (msg, ##__VA_ARGS__))
#define LOGE(msg, ...) MOZ_LOG(gRtspLog, mozilla::LogLevel::Error, (msg, ##__VA_ARGS__))
#define LOGW(msg, ...) MOZ_LOG(gRtspLog, mozilla::LogLevel::Warning, (msg, ##__VA_ARGS__))

#endif  // RTSPPRLOG_H
