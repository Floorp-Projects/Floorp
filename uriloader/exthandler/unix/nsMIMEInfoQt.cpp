/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_WIDGET_QT
#include <QDesktopServices>
#include <QUrl>
#include <QString>
#include <QStringList>
#endif

#include "nsMIMEInfoQt.h"
#include "nsIURI.h"
#include "nsStringGlue.h"

nsresult
nsMIMEInfoQt::LoadUriInternal(nsIURI * aURI)
{
#ifdef MOZ_WIDGET_QT
  nsAutoCString spec;
  aURI->GetAsciiSpec(spec);
  if (QDesktopServices::openUrl(QUrl(spec.get()))) {
    return NS_OK;
  }
#endif

  return NS_ERROR_FAILURE;
}
