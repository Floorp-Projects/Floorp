/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __DEFAULT_BROWSER_DEFAULT_BROWSER_H__
#define __DEFAULT_BROWSER_DEFAULT_BROWSER_H__

#include <string>

#include "mozilla/WinHeaderOnlyUtils.h"

enum struct Browser {
  Unknown,
  Firefox,
  Chrome,
  EdgeWithEdgeHTML,
  EdgeWithBlink,
  InternetExplorer,
  Opera,
  Brave,
};

struct DefaultBrowserInfo {
  Browser currentDefaultBrowser;
  Browser previousDefaultBrowser;
};

using DefaultBrowserResult = mozilla::WindowsErrorResult<DefaultBrowserInfo>;

DefaultBrowserResult GetDefaultBrowserInfo();

std::string GetStringForBrowser(Browser browser);
void MaybeMigrateCurrentDefault();

#endif  // __DEFAULT_BROWSER_DEFAULT_BROWSER_H__
