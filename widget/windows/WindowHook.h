/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __mozilla_WindowHook_h__
#define __mozilla_WindowHook_h__

#include <windows.h>

#include <nsHashKeys.h>
#include <nsClassHashtable.h>
#include <nsTArray.h>

#include "nsAppShell.h"

class nsWindow;

namespace mozilla {
namespace widget {

class WindowHook {
public:

  // It is expected that most callbacks will return false
  typedef bool (*Callback)(void *aContext, HWND hWnd, UINT nMsg,
                             WPARAM wParam, LPARAM lParam, LRESULT *aResult);

  nsresult AddHook(UINT nMsg, Callback callback, void *context);
  nsresult RemoveHook(UINT nMsg, Callback callback, void *context);
  nsresult AddMonitor(UINT nMsg, Callback callback, void *context);
  nsresult RemoveMonitor(UINT nMsg, Callback callback, void *context);

private:
  struct CallbackData {
    Callback cb;
    void *context;

    CallbackData() : cb(nullptr), context(nullptr) {}
    CallbackData(Callback cb, void *ctx) : cb(cb), context(ctx) {}
    bool Invoke(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam,
                  LRESULT *aResult);
    bool operator== (const CallbackData &rhs) const {
      return cb == rhs.cb && context == rhs.context;
    }
    bool operator!= (const CallbackData &rhs) const {
      return !(*this == rhs);
    }
    operator bool () const {
      return !!cb;
    }
  };

  typedef nsTArray<CallbackData> CallbackDataArray;
  struct MessageData {
    UINT nMsg;
    CallbackData hook;
    CallbackDataArray monitors;
  };

  bool Notify(HWND hWnd, UINT nMsg, WPARAM wParam, LPARAM lParam,
                LRESULT *aResult);

  MessageData *Lookup(UINT nMsg);
  MessageData *LookupOrCreate(UINT nMsg);
  void DeleteIfEmpty(MessageData *data);

  typedef nsTArray<MessageData> MessageDataArray;
  MessageDataArray mMessageData;

  // For Notify
  friend class ::nsWindow;
};

}
}

#endif // __mozilla_WindowHook_h__
