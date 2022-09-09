/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AppInfo } = ChromeUtils.import(
  "chrome://remote/content/shared/AppInfo.jsm"
);

// Minimal xpcshell tests for AppInfo; Services.appinfo.* is not available

add_test(function test_custom_properties() {
  const properties = [
    // platforms
    "isAndroid",
    "isLinux",
    "isMac",
    "isWindows",
    // applications
    "isFirefox",
    "isThunderbird",
  ];

  for (const prop of properties) {
    equal(
      typeof AppInfo[prop],
      "boolean",
      `Custom property ${prop} has expected type`
    );
  }

  run_next_test();
});
