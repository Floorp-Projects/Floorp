/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ALOG
#  if defined(DEBUG) || defined(FORCE_ALOG)
#    define ALOG(args...) __android_log_print(ANDROID_LOG_INFO, "Gecko", ##args)
#  else
#    define ALOG(args...) ((void)0)
#  endif
#endif

#ifdef DEBUG
#  define ALOG_BRIDGE(args...) ALOG(args)
#else
#  define ALOG_BRIDGE(args...) ((void)0)
#endif
