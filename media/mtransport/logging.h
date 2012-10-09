/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#ifndef logging_h__
#define logging_h__

#include <sstream>

#include <prlog.h>
#include "mozilla/Scoped.h"

namespace mozilla {

class LogCtx {
 public:
  LogCtx(const char* name) : module_(PR_NewLogModule(name)) {}
  LogCtx(std::string& name) : module_(PR_NewLogModule(name.c_str())) {}

  PRLogModuleInfo* module() const { return module_; }

private:
  PRLogModuleInfo* module_;
};


#define MOZ_MTLOG_MODULE(n) \
  static ScopedDeletePtr<LogCtx> mlog_ctx;      \
  static const char *mlog_name = n

#define MOZ_MTLOG(level, b) \
  do { if (!mlog_ctx) mlog_ctx = new LogCtx(mlog_name);    \
    if (mlog_ctx) {                                             \
    std::stringstream str;                                              \
    str << b;                                                           \
    PR_LOG(mlog_ctx->module(), level, ("%s", str.str().c_str())); }} while(0)
}  // close namespace
#endif
