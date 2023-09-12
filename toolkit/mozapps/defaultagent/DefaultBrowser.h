/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __DEFAULT_BROWSER_DEFAULT_BROWSER_H__
#define __DEFAULT_BROWSER_DEFAULT_BROWSER_H__

#include <string>

#include "mozilla/DefineEnum.h"
#include "mozilla/WinHeaderOnlyUtils.h"

namespace mozilla::default_agent {

MOZ_DEFINE_ENUM_CLASS(Browser,
                      (Error, Unknown, Firefox, Chrome, EdgeWithEdgeHTML,
                       EdgeWithBlink, InternetExplorer, Opera, Brave, Yandex,
                       QQBrowser, _360Browser, Sogou, DuckDuckGo));

struct DefaultBrowserInfo {
  Browser currentDefaultBrowser;
  Browser previousDefaultBrowser;
};

using DefaultBrowserResult = mozilla::WindowsErrorResult<DefaultBrowserInfo>;

DefaultBrowserResult GetDefaultBrowserInfo();

std::string GetStringForBrowser(Browser browser);
void MaybeMigrateCurrentDefault();

}  // namespace mozilla::default_agent

#endif  // __DEFAULT_BROWSER_DEFAULT_BROWSER_H__
