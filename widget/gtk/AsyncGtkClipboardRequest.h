/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AsyncGtkClipboardRequest_h
#define mozilla_AsyncGtkClipboardRequest_h

#include "nsClipboard.h"

namespace mozilla {

// An asynchronous clipboard request that we wait for synchronously by
// spinning the event loop.
class MOZ_STACK_CLASS AsyncGtkClipboardRequest {
  // Heap-allocated object that we give GTK as a callback.
  struct Request {
    explicit Request(ClipboardDataType aDataType) : mDataType(aDataType) {}

    void Complete(const void*);

    const ClipboardDataType mDataType;
    Maybe<ClipboardData> mData;
    bool mTimedOut = false;
  };

  UniquePtr<Request> mRequest;

  static void OnDataReceived(GtkClipboard*, GtkSelectionData*, gpointer);
  static void OnTextReceived(GtkClipboard*, const gchar*, gpointer);

 public:
  // Launch a request for a particular GTK clipboard. The current status of the
  // request can be observed by calling HasCompleted() and TakeResult().
  AsyncGtkClipboardRequest(ClipboardDataType, int32_t aWhichClipboard,
                           const char* aMimeType = nullptr);

  // Returns whether the request has been answered already.
  bool HasCompleted() const { return mRequest->mData.isSome(); }

  // Takes the result from the current request if completed, or a
  // default-constructed data otherwise. The destructor will take care of
  // flagging the request as timed out in that case.
  ClipboardData TakeResult() {
    if (!HasCompleted()) {
      return {};
    }
    auto request = std::move(mRequest);
    return request->mData.extract();
  }

  // If completed, frees the request if needed. Otherwise, marks it as a timed
  // out request so that when it completes the Request object is properly
  // freed.
  ~AsyncGtkClipboardRequest();
};

};  // namespace mozilla

#endif
