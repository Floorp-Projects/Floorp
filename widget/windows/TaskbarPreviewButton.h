/* vim: se cin sw=2 ts=2 et : */
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __mozilla_widget_TaskbarPreviewButton_h__
#define __mozilla_widget_TaskbarPreviewButton_h__

#include <windows.h>
#include <shobjidl.h>
#undef LogSeverity // SetupAPI.h #defines this as DWORD

#include "mozilla/RefPtr.h"
#include <nsITaskbarPreviewButton.h>
#include <nsString.h>
#include "nsWeakReference.h"

namespace mozilla {
namespace widget {

class TaskbarWindowPreview;
class TaskbarPreviewButton : public nsITaskbarPreviewButton, public nsSupportsWeakReference
{
  virtual ~TaskbarPreviewButton();

public:
  TaskbarPreviewButton(TaskbarWindowPreview* preview, uint32_t index);

  NS_DECL_ISUPPORTS
  NS_DECL_NSITASKBARPREVIEWBUTTON

private:
  THUMBBUTTON&            Button();
  nsresult                Update();

  RefPtr<TaskbarWindowPreview> mPreview;
  uint32_t                mIndex;
  nsString                mTooltip;
  nsCOMPtr<imgIContainer> mImage;
};

} // namespace widget
} // namespace mozilla

#endif /* __mozilla_widget_TaskbarPreviewButton_h__ */

