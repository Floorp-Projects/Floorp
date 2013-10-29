// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_SANDBOX_FACTORY_H__
#define SANDBOX_SRC_SANDBOX_FACTORY_H__

#include "sandbox/win/src/sandbox.h"

// SandboxFactory is a set of static methods to get access to the broker
// or target services object. Only one of the two methods (GetBrokerServices,
// GetTargetServices) will return a non-null pointer and that should be used
// as the indication that the process is the broker or the target:
//
// BrokerServices* broker_services = SandboxFactory::GetBrokerServices();
// if (NULL != broker_services) {
//   //we are the broker, call broker api here
//   broker_services->Init();
// } else {
//   TargetServices* target_services = SandboxFactory::GetTargetServices();
//   if (NULL != target_services) {
//    //we are the target, call target api here
//    target_services->Init();
//  }
//
// The methods in this class are expected to be called from a single thread
//
// The Sandbox library needs to be linked against the main executable, but
// sometimes the API calls are issued from a DLL that loads into the exe
// process. These factory methods then need to be called from the main
// exe and the interface pointers then can be safely passed to the DLL where
// the Sandbox API calls are made.
namespace sandbox {

class SandboxFactory {
 public:
  // Returns the Broker API interface, returns NULL if this process is the
  // target.
  static BrokerServices* GetBrokerServices();

  // Returns the Target API interface, returns NULL if this process is the
  // broker.
  static TargetServices* GetTargetServices();
 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(SandboxFactory);
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_SANDBOX_FACTORY_H__
