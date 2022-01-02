/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WebExecutorSupport_h__
#define WebExecutorSupport_h__

#include "mozilla/java/GeckoWebExecutorNatives.h"
#include "mozilla/java/GeckoResultWrappers.h"
#include "mozilla/java/WebRequestWrappers.h"

namespace mozilla {
namespace widget {

class WebExecutorSupport final
    : public java::GeckoWebExecutor::Natives<WebExecutorSupport> {
 public:
  static void Fetch(jni::Object::Param request, int32_t flags,
                    jni::Object::Param result);
  static void Resolve(jni::String::Param aUri, jni::Object::Param result);

 protected:
  static nsresult CreateStreamLoader(java::WebRequest::Param aRequest,
                                     int32_t aFlags,
                                     java::GeckoResult::Param aResult);
};

}  // namespace widget
}  // namespace mozilla

#endif  // WebExecutorSupport_h__
