/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef XREChildData_h
#define XREChildData_h

#include "mozilla/UniquePtr.h"

#if defined(XP_WIN) && defined(MOZ_SANDBOX)
#  include "mozilla/sandboxing/loggingTypes.h"

namespace sandbox {
class BrokerServices;
class TargetServices;
}  // namespace sandbox
#endif

/**
 * Data needed to start a child process.
 */
struct XREChildData {
#if defined(XP_WIN) && defined(MOZ_SANDBOX)
  /**
   * Chromium sandbox TargetServices.
   */
  sandbox::TargetServices* sandboxTargetServices = nullptr;

  /**
   * Function to provide a logging function to the chromium sandbox code.
   */
  mozilla::sandboxing::ProvideLogFunctionCb ProvideLogFunction = nullptr;

  /**
   * Chromium sandbox broker services; needed by the remote sandbox
   * launcher process.
   */
  sandbox::BrokerServices* sandboxBrokerServices = nullptr;
#endif
};

#endif  // XREChildData_h
