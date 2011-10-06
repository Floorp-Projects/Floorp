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

#include "WindowHook.h"
#include "nsWindow.h"

namespace mozilla {
namespace widget {

nsresult
WindowHook::AddHook(UINT nMsg, Callback callback, void *context) {
  MessageData *data = LookupOrCreate(nMsg);

  if (!data)
    return NS_ERROR_OUT_OF_MEMORY;

  // Ensure we don't overwrite another hook
  NS_ENSURE_TRUE(nsnull == data->hook.cb, NS_ERROR_UNEXPECTED);

  data->hook = CallbackData(callback, context);

  return NS_OK;
}

nsresult
WindowHook::RemoveHook(UINT nMsg, Callback callback, void *context) {
  CallbackData cbdata(callback, context);
  MessageData *data = Lookup(nMsg);
  if (!data)
    return NS_ERROR_UNEXPECTED;
  if (data->hook != cbdata)
    return NS_ERROR_UNEXPECTED;
  data->hook = CallbackData();

  DeleteIfEmpty(data);
  return NS_OK;
}

nsresult
WindowHook::AddMonitor(UINT nMsg, Callback callback, void *context) {
  MessageData *data = LookupOrCreate(nMsg);
  return (data && data->monitors.AppendElement(CallbackData(callback, context)))
         ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

nsresult
WindowHook::RemoveMonitor(UINT nMsg, Callback callback, void *context) {
  CallbackData cbdata(callback, context);
  MessageData *data = Lookup(nMsg);
  if (!data)
    return NS_ERROR_UNEXPECTED;
  CallbackDataArray::index_type idx = data->monitors.IndexOf(cbdata);
  if (idx == CallbackDataArray::NoIndex)
    return NS_ERROR_UNEXPECTED;
  data->monitors.RemoveElementAt(idx);
  DeleteIfEmpty(data);
  return NS_OK;
}

WindowHook::MessageData *
WindowHook::Lookup(UINT nMsg) {
  MessageDataArray::index_type idx;
  for (idx = 0; idx < mMessageData.Length(); idx++) {
    MessageData &data = mMessageData[idx];
    if (data.nMsg == nMsg)
      return &data;
  }
  return nsnull;
}

WindowHook::MessageData *
WindowHook::LookupOrCreate(UINT nMsg) {
  MessageData *data = Lookup(nMsg);
  if (!data) {
    data = mMessageData.AppendElement();

    if (!data)
      return nsnull;

    data->nMsg = nMsg;
  }
  return data;
}

void
WindowHook::DeleteIfEmpty(MessageData *data) {
  // Never remove a MessageData that has still a hook or monitor entries.
  if (data->hook || !data->monitors.IsEmpty())
    return;

  MessageDataArray::index_type idx;
  idx = data - mMessageData.Elements();
  NS_ASSERTION(idx >= 0 && idx < mMessageData.Length(), "Attempted to delete MessageData that doesn't belong to this array!");
  mMessageData.RemoveElementAt(idx);
}

bool
WindowHook::Notify(HWND hWnd, UINT nMsg, WPARAM wParam, LPARAM lParam,
                   LRESULT *aResult) {
  MessageData *data = Lookup(nMsg);
  if (!data)
    return false;

  PRUint32 length = data->monitors.Length();
  for (PRUint32 midx = 0; midx < length; midx++) {
    data->monitors[midx].Invoke(hWnd, nMsg, wParam, lParam, aResult);
  }

  return data->hook.Invoke(hWnd, nMsg, wParam, lParam, aResult);
}

bool
WindowHook::CallbackData::Invoke(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                 LRESULT *aResult) {
  if (!cb)
    return false;
  return cb(context, hWnd, msg, wParam, lParam, aResult);
}
} // namespace widget
} // namespace mozilla

