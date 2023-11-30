/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);

const { AppInfo, getTimeoutMultiplier } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/AppInfo.sys.mjs"
);

// Minimal xpcshell tests for AppInfo; Services.appinfo.* is not available

add_task(function test_custom_properties() {
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
});

add_task(function test_getTimeoutMultiplier() {
  const message = "Timeout multiplier has expected value";
  const timeoutMultiplier = getTimeoutMultiplier();

  if (
    AppConstants.DEBUG ||
    AppConstants.MOZ_CODE_COVERAGE ||
    AppConstants.ASAN
  ) {
    equal(timeoutMultiplier, 4, message);
  } else if (AppConstants.TSAN) {
    equal(timeoutMultiplier, 8, message);
  } else {
    equal(timeoutMultiplier, 1, message);
  }
});
