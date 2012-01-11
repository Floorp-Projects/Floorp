/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Rob Arnold <tellrob@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

    CallbackData() : cb(nsnull), context(nsnull) {}
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
