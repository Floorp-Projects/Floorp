/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "mozilla/Logging.h"

#include "TaskbarProgress.h"
#include "nsWindow.h"
#include "WidgetUtils.h"
#include "nsPIDOMWindow.h"

using mozilla::LogLevel;
static mozilla::LazyLogModule gGtkTaskbarProgressLog("nsIGtkTaskbarProgress");

/******************************************************************************
 * TaskbarProgress
 ******************************************************************************/

NS_IMPL_ISUPPORTS(TaskbarProgress, nsIGtkTaskbarProgress, nsITaskbarProgress)

TaskbarProgress::TaskbarProgress() : mPrimaryWindow(nullptr) {
  MOZ_LOG(gGtkTaskbarProgressLog, LogLevel::Info,
          ("%p TaskbarProgress()", this));
}

TaskbarProgress::~TaskbarProgress() {
  MOZ_LOG(gGtkTaskbarProgressLog, LogLevel::Info,
          ("%p ~TaskbarProgress()", this));
}

NS_IMETHODIMP
TaskbarProgress::SetProgressState(nsTaskbarProgressState aState,
                                  uint64_t aCurrentValue, uint64_t aMaxValue) {
  NS_ENSURE_ARG_RANGE(aState, 0, STATE_PAUSED);

  if (aState == STATE_NO_PROGRESS || aState == STATE_INDETERMINATE) {
    NS_ENSURE_TRUE(aCurrentValue == 0, NS_ERROR_INVALID_ARG);
    NS_ENSURE_TRUE(aMaxValue == 0, NS_ERROR_INVALID_ARG);
  }

  NS_ENSURE_TRUE((aCurrentValue <= aMaxValue), NS_ERROR_ILLEGAL_VALUE);

  // See TaskbarProgress::SetPrimaryWindow: if we're running in headless
  // mode, mPrimaryWindow will be null.
  if (!mPrimaryWindow) {
    return NS_OK;
  }

  gulong progress;

  if (aMaxValue == 0) {
    progress = 0;
  } else {
    // Rounding down to ensure we don't set to 'full' until the operation
    // is completely finished.
    progress = (gulong)(((double)aCurrentValue / aMaxValue) * 100.0);
  }

  // Check if the resultant value is the same as the previous call, and
  // ignore this update if it is.

  if (progress == mCurrentProgress) {
    return NS_OK;
  }

  mCurrentProgress = progress;

  MOZ_LOG(gGtkTaskbarProgressLog, LogLevel::Debug,
          ("GtkTaskbarProgress::SetProgressState progress: %lu", progress));

  mPrimaryWindow->SetProgress(progress);

  return NS_OK;
}

NS_IMETHODIMP
TaskbarProgress::SetPrimaryWindow(mozIDOMWindowProxy* aWindow) {
  NS_ENSURE_TRUE(aWindow != nullptr, NS_ERROR_ILLEGAL_VALUE);

  auto* parent = nsPIDOMWindowOuter::From(aWindow);
  RefPtr<nsIWidget> widget =
      mozilla::widget::WidgetUtils::DOMWindowToWidget(parent);

  // Only nsWindows have a native window, HeadlessWidgets do not.  Stop here if
  // the window does not have one.
  if (!widget->GetNativeData(NS_NATIVE_WINDOW)) {
    return NS_OK;
  }

  mPrimaryWindow = static_cast<nsWindow*>(widget.get());

  // Clear our current progress.  We get a forced update from the
  // DownloadsTaskbar after returning from this function - zeroing out our
  // progress will make sure the new window gets the property set on it
  // immediately, rather than waiting for the progress value to change (which
  // could be a while depending on size.)
  mCurrentProgress = 0;

  MOZ_LOG(gGtkTaskbarProgressLog, LogLevel::Debug,
          ("GtkTaskbarProgress::SetPrimaryWindow window: %p",
           mPrimaryWindow.get()));

  return NS_OK;
}
