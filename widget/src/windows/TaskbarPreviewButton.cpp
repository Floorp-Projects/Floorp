/* vim: se cin sw=2 ts=2 et : */
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
 * Mozilla Foundation.
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

#if MOZ_WINSDK_TARGETVER >= MOZ_NTDDI_WIN7

#include <windows.h>
#include <strsafe.h>

#include "TaskbarWindowPreview.h"
#include "TaskbarPreviewButton.h"
#include "nsWindowGfx.h"
#include <imgIContainer.h>

namespace mozilla {
namespace widget {

NS_IMPL_ISUPPORTS2(TaskbarPreviewButton, nsITaskbarPreviewButton, nsISupportsWeakReference)

TaskbarPreviewButton::TaskbarPreviewButton(TaskbarWindowPreview* preview, PRUint32 index)
  : mPreview(preview), mIndex(index)
{
}

TaskbarPreviewButton::~TaskbarPreviewButton() {
  SetVisible(false);
}

NS_IMETHODIMP
TaskbarPreviewButton::GetTooltip(nsAString &aTooltip) {
  aTooltip = mTooltip;
  return NS_OK;
}

NS_IMETHODIMP
TaskbarPreviewButton::SetTooltip(const nsAString &aTooltip) {
  mTooltip = aTooltip;
  size_t destLength = sizeof Button().szTip / (sizeof Button().szTip[0]);
  wchar_t *tooltip = &(Button().szTip[0]);
  StringCchCopyNW(tooltip,
                  destLength,
                  mTooltip.get(),
                  mTooltip.Length());
  return Update();
}

NS_IMETHODIMP
TaskbarPreviewButton::GetDismissOnClick(bool *dismiss) {
  *dismiss = (Button().dwFlags & THBF_DISMISSONCLICK) == THBF_DISMISSONCLICK;
  return NS_OK;
}

NS_IMETHODIMP
TaskbarPreviewButton::SetDismissOnClick(bool dismiss) {
  if (dismiss)
    Button().dwFlags |= THBF_DISMISSONCLICK;
  else
    Button().dwFlags &= ~THBF_DISMISSONCLICK;
  return Update();
}

NS_IMETHODIMP
TaskbarPreviewButton::GetHasBorder(bool *hasBorder) {
  *hasBorder = (Button().dwFlags & THBF_NOBACKGROUND) != THBF_NOBACKGROUND;
  return NS_OK;
}

NS_IMETHODIMP
TaskbarPreviewButton::SetHasBorder(bool hasBorder) {
  if (hasBorder)
    Button().dwFlags &= ~THBF_NOBACKGROUND;
  else
    Button().dwFlags |= THBF_NOBACKGROUND;
  return Update();
}

NS_IMETHODIMP
TaskbarPreviewButton::GetDisabled(bool *disabled) {
  *disabled = (Button().dwFlags & THBF_DISABLED) == THBF_DISABLED;
  return NS_OK;
}

NS_IMETHODIMP
TaskbarPreviewButton::SetDisabled(bool disabled) {
  if (disabled)
    Button().dwFlags |= THBF_DISABLED;
  else
    Button().dwFlags &= ~THBF_DISABLED;
  return Update();
}

NS_IMETHODIMP
TaskbarPreviewButton::GetImage(imgIContainer **img) {
  if (mImage)
    NS_ADDREF(*img = mImage);
  else
    *img = NULL;
  return NS_OK;
}

NS_IMETHODIMP
TaskbarPreviewButton::SetImage(imgIContainer *img) {
  if (Button().hIcon)
    ::DestroyIcon(Button().hIcon);
  if (img) {
    nsresult rv;
    rv = nsWindowGfx::CreateIcon(img, false, 0, 0,
                                 nsWindowGfx::GetIconMetrics(nsWindowGfx::kRegularIcon),
                                 &Button().hIcon);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    Button().hIcon = NULL;
  }
  return Update();
}

NS_IMETHODIMP
TaskbarPreviewButton::GetVisible(bool *visible) {
  *visible = (Button().dwFlags & THBF_HIDDEN) != THBF_HIDDEN;
  return NS_OK;
}

NS_IMETHODIMP
TaskbarPreviewButton::SetVisible(bool visible) {
  if (visible)
    Button().dwFlags &= ~THBF_HIDDEN;
  else
    Button().dwFlags |= THBF_HIDDEN;
  return Update();
}

THUMBBUTTON&
TaskbarPreviewButton::Button() {
  return mPreview->mThumbButtons[mIndex];
}

nsresult
TaskbarPreviewButton::Update() {
  return mPreview->UpdateButton(mIndex);
}

} // namespace widget
} // namespace mozilla

#endif // MOZ_WINSDK_TARGETVER >= MOZ_NTDDI_WIN7 

