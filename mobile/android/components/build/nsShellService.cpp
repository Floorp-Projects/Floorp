/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_WIDGET_QT
#include <QtGui/QApplication>
#include <QtGui/QWidget>
#endif

#ifdef MOZ_WIDGET_ANDROID
#include "AndroidBridge.h"
#endif

#include "nsShellService.h"
#include "nsString.h"

NS_IMPL_ISUPPORTS1(nsShellService, nsIShellService)

NS_IMETHODIMP
nsShellService::SwitchTask()
{
#if MOZ_WIDGET_QT
  QWidget * window = QApplication::activeWindow();
  if (window)
      window->showMinimized();
  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP
nsShellService::CreateShortcut(const nsAString& aTitle, const nsAString& aURI, const nsAString& aIconData, const nsAString& aIntent)
{
  if (!aTitle.Length() || !aURI.Length() || !aIconData.Length())
    return NS_ERROR_FAILURE;

#if MOZ_WIDGET_ANDROID
  mozilla::AndroidBridge::Bridge()->CreateShortcut(aTitle, aURI, aIconData, aIntent);
  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}
