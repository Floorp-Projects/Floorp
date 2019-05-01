/* vim: se cin sw=2 ts=2 et : */
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include <strsafe.h>

#include "TaskbarWindowPreview.h"
#include "TaskbarPreviewButton.h"
#include "nsWindowGfx.h"
#include <imgIContainer.h>

namespace mozilla {
namespace widget {

NS_IMPL_ISUPPORTS(TaskbarPreviewButton, nsITaskbarPreviewButton,
                  nsISupportsWeakReference)

TaskbarPreviewButton::TaskbarPreviewButton(TaskbarWindowPreview* preview,
                                           uint32_t index)
    : mPreview(preview), mIndex(index) {}

TaskbarPreviewButton::~TaskbarPreviewButton() { SetVisible(false); }

NS_IMETHODIMP
TaskbarPreviewButton::GetTooltip(nsAString& aTooltip) {
  aTooltip = mTooltip;
  return NS_OK;
}

NS_IMETHODIMP
TaskbarPreviewButton::SetTooltip(const nsAString& aTooltip) {
  mTooltip = aTooltip;
  size_t destLength = sizeof Button().szTip / (sizeof Button().szTip[0]);
  wchar_t* tooltip = &(Button().szTip[0]);
  StringCchCopyNW(tooltip, destLength, mTooltip.get(), mTooltip.Length());
  return Update();
}

NS_IMETHODIMP
TaskbarPreviewButton::GetDismissOnClick(bool* dismiss) {
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
TaskbarPreviewButton::GetHasBorder(bool* hasBorder) {
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
TaskbarPreviewButton::GetDisabled(bool* disabled) {
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
TaskbarPreviewButton::GetImage(imgIContainer** img) {
  if (mImage)
    NS_ADDREF(*img = mImage);
  else
    *img = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
TaskbarPreviewButton::SetImage(imgIContainer* img) {
  if (Button().hIcon) ::DestroyIcon(Button().hIcon);
  if (img) {
    nsresult rv;
    rv = nsWindowGfx::CreateIcon(
        img, false, 0, 0,
        nsWindowGfx::GetIconMetrics(nsWindowGfx::kRegularIcon),
        &Button().hIcon);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    Button().hIcon = nullptr;
  }
  return Update();
}

NS_IMETHODIMP
TaskbarPreviewButton::GetVisible(bool* visible) {
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

THUMBBUTTON& TaskbarPreviewButton::Button() {
  return mPreview->mThumbButtons[mIndex];
}

nsresult TaskbarPreviewButton::Update() {
  return mPreview->UpdateButton(mIndex);
}

}  // namespace widget
}  // namespace mozilla
