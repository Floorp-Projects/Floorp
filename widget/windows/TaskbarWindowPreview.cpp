/* vim: se cin sw=2 ts=2 et : */
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include <nsITaskbarPreviewController.h>
#include "TaskbarWindowPreview.h"
#include "WindowHook.h"
#include "nsUXThemeData.h"
#include "TaskbarPreviewButton.h"
#include "nsWindow.h"
#include "nsWindowGfx.h"

namespace mozilla {
namespace widget {

namespace {
bool WindowHookProc(void *aContext, HWND hWnd, UINT nMsg, WPARAM wParam, LPARAM lParam, LRESULT *aResult)
{
  TaskbarWindowPreview *preview = reinterpret_cast<TaskbarWindowPreview*>(aContext);
  *aResult = preview->WndProc(nMsg, wParam, lParam);
  return true;
}
}

NS_IMPL_ISUPPORTS(TaskbarWindowPreview, nsITaskbarWindowPreview,
                  nsITaskbarProgress, nsITaskbarOverlayIconController,
                  nsISupportsWeakReference)

/**
 * These correspond directly to the states defined in nsITaskbarProgress.idl, so
 * they should be kept in sync.
 */
static TBPFLAG sNativeStates[] =
{
  TBPF_NOPROGRESS,
  TBPF_INDETERMINATE,
  TBPF_NORMAL,
  TBPF_ERROR,
  TBPF_PAUSED
};

TaskbarWindowPreview::TaskbarWindowPreview(ITaskbarList4 *aTaskbar, nsITaskbarPreviewController *aController, HWND aHWND, nsIDocShell *aShell)
  : TaskbarPreview(aTaskbar, aController, aHWND, aShell),
    mCustomDrawing(false),
    mHaveButtons(false),
    mState(TBPF_NOPROGRESS),
    mCurrentValue(0),
    mMaxValue(0),
    mOverlayIcon(nullptr)
{
  // Window previews are visible by default
  (void) SetVisible(true);

  memset(mThumbButtons, 0, sizeof mThumbButtons);
  for (int32_t i = 0; i < nsITaskbarWindowPreview::NUM_TOOLBAR_BUTTONS; i++) {
    mThumbButtons[i].dwMask = THB_FLAGS | THB_ICON | THB_TOOLTIP;
    mThumbButtons[i].iId = i;
    mThumbButtons[i].dwFlags = THBF_HIDDEN;
  }

  WindowHook &hook = GetWindowHook();
  if (!CanMakeTaskbarCalls())
    hook.AddMonitor(nsAppShell::GetTaskbarButtonCreatedMessage(),
                    TaskbarWindowHook, this);
}

TaskbarWindowPreview::~TaskbarWindowPreview() {
  if (mOverlayIcon) {
    ::DestroyIcon(mOverlayIcon);
    mOverlayIcon = nullptr;
  }

  if (IsWindowAvailable()) {
    DetachFromNSWindow();
  } else {
    mWnd = nullptr;
  }
}

nsresult
TaskbarWindowPreview::ShowActive(bool active) {
  return FAILED(mTaskbar->ActivateTab(active ? mWnd : nullptr))
       ? NS_ERROR_FAILURE
       : NS_OK;

}

HWND &
TaskbarWindowPreview::PreviewWindow() {
  return mWnd;
}

nsresult
TaskbarWindowPreview::GetButton(uint32_t index, nsITaskbarPreviewButton **_retVal) {
  if (index >= nsITaskbarWindowPreview::NUM_TOOLBAR_BUTTONS)
    return NS_ERROR_INVALID_ARG;

  nsCOMPtr<nsITaskbarPreviewButton> button(do_QueryReferent(mWeakButtons[index]));

  if (!button) {
    // Lost reference
    button = new TaskbarPreviewButton(this, index);
    if (!button) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    mWeakButtons[index] = do_GetWeakReference(button);
  }

  if (!mHaveButtons) {
    mHaveButtons = true;

    WindowHook &hook = GetWindowHook();
    (void) hook.AddHook(WM_COMMAND, WindowHookProc, this);

    if (mVisible && FAILED(mTaskbar->ThumbBarAddButtons(mWnd, nsITaskbarWindowPreview::NUM_TOOLBAR_BUTTONS, mThumbButtons))) {
      return NS_ERROR_FAILURE;
    }
  }
  button.forget(_retVal);
  return NS_OK;
}

NS_IMETHODIMP
TaskbarWindowPreview::SetEnableCustomDrawing(bool aEnable) {
  if (aEnable == mCustomDrawing)
    return NS_OK;
  mCustomDrawing = aEnable;
  TaskbarPreview::EnableCustomDrawing(mWnd, aEnable);

  WindowHook &hook = GetWindowHook();
  if (aEnable) {
    (void) hook.AddHook(WM_DWMSENDICONICTHUMBNAIL, WindowHookProc, this);
    (void) hook.AddHook(WM_DWMSENDICONICLIVEPREVIEWBITMAP, WindowHookProc, this);
  } else {
    (void) hook.RemoveHook(WM_DWMSENDICONICLIVEPREVIEWBITMAP, WindowHookProc, this);
    (void) hook.RemoveHook(WM_DWMSENDICONICTHUMBNAIL, WindowHookProc, this);
  }
  return NS_OK;
}

NS_IMETHODIMP
TaskbarWindowPreview::GetEnableCustomDrawing(bool *aEnable) {
  *aEnable = mCustomDrawing;
  return NS_OK;
}

NS_IMETHODIMP
TaskbarWindowPreview::SetProgressState(nsTaskbarProgressState aState,
                                       uint64_t aCurrentValue,
                                       uint64_t aMaxValue)
{
  NS_ENSURE_ARG_RANGE(aState,
                      nsTaskbarProgressState(0),
                      nsTaskbarProgressState(ArrayLength(sNativeStates) - 1));

  TBPFLAG nativeState = sNativeStates[aState];
  if (nativeState == TBPF_NOPROGRESS || nativeState == TBPF_INDETERMINATE) {
    NS_ENSURE_TRUE(aCurrentValue == 0, NS_ERROR_INVALID_ARG);
    NS_ENSURE_TRUE(aMaxValue == 0, NS_ERROR_INVALID_ARG);
  }

  if (aCurrentValue > aMaxValue)
    return NS_ERROR_ILLEGAL_VALUE;

  mState = nativeState;
  mCurrentValue = aCurrentValue;
  mMaxValue = aMaxValue;

  // Only update if we can
  return CanMakeTaskbarCalls() ? UpdateTaskbarProgress() : NS_OK;
}

NS_IMETHODIMP
TaskbarWindowPreview::SetOverlayIcon(imgIContainer* aStatusIcon,
                                     const nsAString& aStatusDescription) {
  nsresult rv;
  if (aStatusIcon) {
    // The image shouldn't be animated
    bool isAnimated;
    rv = aStatusIcon->GetAnimated(&isAnimated);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_FALSE(isAnimated, NS_ERROR_INVALID_ARG);
  }

  HICON hIcon = nullptr;
  if (aStatusIcon) {
    rv = nsWindowGfx::CreateIcon(aStatusIcon, false, 0, 0,
                                 nsWindowGfx::GetIconMetrics(nsWindowGfx::kSmallIcon),
                                 &hIcon);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (mOverlayIcon)
    ::DestroyIcon(mOverlayIcon);
  mOverlayIcon = hIcon;
  mIconDescription = aStatusDescription;

  // Only update if we can
  return CanMakeTaskbarCalls() ? UpdateOverlayIcon() : NS_OK;
}

nsresult
TaskbarWindowPreview::UpdateTaskbarProperties() {
  if (mHaveButtons) {
    if (FAILED(mTaskbar->ThumbBarAddButtons(mWnd, nsITaskbarWindowPreview::NUM_TOOLBAR_BUTTONS, mThumbButtons)))
      return NS_ERROR_FAILURE;
  }
  nsresult rv = UpdateTaskbarProgress();
  NS_ENSURE_SUCCESS(rv, rv);
  rv = UpdateOverlayIcon();
  NS_ENSURE_SUCCESS(rv, rv);
  return TaskbarPreview::UpdateTaskbarProperties();
}

nsresult
TaskbarWindowPreview::UpdateTaskbarProgress() {
  HRESULT hr = mTaskbar->SetProgressState(mWnd, mState);
  if (SUCCEEDED(hr) && mState != TBPF_NOPROGRESS &&
      mState != TBPF_INDETERMINATE)
    hr = mTaskbar->SetProgressValue(mWnd, mCurrentValue, mMaxValue);

  return SUCCEEDED(hr) ? NS_OK : NS_ERROR_FAILURE;
}

nsresult
TaskbarWindowPreview::UpdateOverlayIcon() {
  HRESULT hr = mTaskbar->SetOverlayIcon(mWnd, mOverlayIcon,
                                        mIconDescription.get());
  return SUCCEEDED(hr) ? NS_OK : NS_ERROR_FAILURE;
}

LRESULT
TaskbarWindowPreview::WndProc(UINT nMsg, WPARAM wParam, LPARAM lParam) {
  RefPtr<TaskbarWindowPreview> kungFuDeathGrip(this);
  switch (nMsg) {
    case WM_COMMAND:
      {
        uint32_t id = LOWORD(wParam);
        uint32_t index = id;
        nsCOMPtr<nsITaskbarPreviewButton> button;
        nsresult rv = GetButton(index, getter_AddRefs(button));
        if (NS_SUCCEEDED(rv))
          mController->OnClick(button);
      }
      return 0;
  }
  return TaskbarPreview::WndProc(nMsg, wParam, lParam);
}

/* static */
bool
TaskbarWindowPreview::TaskbarWindowHook(void *aContext,
                                        HWND hWnd, UINT nMsg,
                                        WPARAM wParam, LPARAM lParam,
                                        LRESULT *aResult)
{
  NS_ASSERTION(nMsg == nsAppShell::GetTaskbarButtonCreatedMessage(),
               "Window hook proc called with wrong message");
  TaskbarWindowPreview *preview =
    reinterpret_cast<TaskbarWindowPreview*>(aContext);
  // Now we can make all the calls to mTaskbar
  preview->UpdateTaskbarProperties();
  return false;
}

nsresult
TaskbarWindowPreview::Enable() {
  nsresult rv = TaskbarPreview::Enable();
  NS_ENSURE_SUCCESS(rv, rv);

  return FAILED(mTaskbar->AddTab(mWnd))
       ? NS_ERROR_FAILURE
       : NS_OK;
}

nsresult
TaskbarWindowPreview::Disable() {
  nsresult rv = TaskbarPreview::Disable();
  NS_ENSURE_SUCCESS(rv, rv);

  return FAILED(mTaskbar->DeleteTab(mWnd))
       ? NS_ERROR_FAILURE
       : NS_OK;
}

void
TaskbarWindowPreview::DetachFromNSWindow() {
  // Remove the hooks we have for drawing
  SetEnableCustomDrawing(false);

  WindowHook &hook = GetWindowHook();
  (void) hook.RemoveHook(WM_COMMAND, WindowHookProc, this);
  (void) hook.RemoveMonitor(nsAppShell::GetTaskbarButtonCreatedMessage(),
                            TaskbarWindowHook, this);

  TaskbarPreview::DetachFromNSWindow();
}

nsresult
TaskbarWindowPreview::UpdateButtons() {
  NS_ASSERTION(mVisible, "UpdateButtons called on invisible preview");

  if (FAILED(mTaskbar->ThumbBarUpdateButtons(mWnd, nsITaskbarWindowPreview::NUM_TOOLBAR_BUTTONS, mThumbButtons)))
    return NS_ERROR_FAILURE;
  return NS_OK;
}

nsresult
TaskbarWindowPreview::UpdateButton(uint32_t index) {
  if (index >= nsITaskbarWindowPreview::NUM_TOOLBAR_BUTTONS)
    return NS_ERROR_INVALID_ARG;
  if (mVisible) {
    if (FAILED(mTaskbar->ThumbBarUpdateButtons(mWnd, 1, &mThumbButtons[index])))
      return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

} // namespace widget
} // namespace mozilla

