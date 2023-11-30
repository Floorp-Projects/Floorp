/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

const ID_FIREFOX = "{ec8030f7-c20a-464f-9b0e-13a3a9e97384}";
const ID_THUNDERBIRD = "{3550f703-e582-4d05-9a08-453d09bdfdc6}";

/**
 * Extends Services.appinfo with further properties that are
 * used by different protocols as handled by the Remote Agent.
 *
 * @typedef {object} RemoteAgent.AppInfo
 * @property {boolean} isAndroid - Whether the application runs on Android.
 * @property {boolean} isLinux - Whether the application runs on Linux.
 * @property {boolean} isMac - Whether the application runs on Mac OS.
 * @property {boolean} isWindows - Whether the application runs on Windows.
 * @property {boolean} isFirefox - Whether the application is Firefox.
 * @property {boolean} isThunderbird - Whether the application is Thunderbird.
 *
 * @since 88
 */
export const AppInfo = new Proxy(
  {},
  {
    get(target, prop, receiver) {
      if (target.hasOwnProperty(prop)) {
        return target[prop];
      }

      return Services.appinfo[prop];
    },
  }
);

// Platform support

ChromeUtils.defineLazyGetter(AppInfo, "isAndroid", () => {
  return Services.appinfo.OS === "Android";
});

ChromeUtils.defineLazyGetter(AppInfo, "isLinux", () => {
  return Services.appinfo.OS === "Linux";
});

ChromeUtils.defineLazyGetter(AppInfo, "isMac", () => {
  return Services.appinfo.OS === "Darwin";
});

ChromeUtils.defineLazyGetter(AppInfo, "isWindows", () => {
  return Services.appinfo.OS === "WINNT";
});

// Application type

ChromeUtils.defineLazyGetter(AppInfo, "isFirefox", () => {
  return Services.appinfo.ID == ID_FIREFOX;
});

ChromeUtils.defineLazyGetter(AppInfo, "isThunderbird", () => {
  return Services.appinfo.ID == ID_THUNDERBIRD;
});

export function getTimeoutMultiplier() {
  if (
    AppConstants.DEBUG ||
    AppConstants.MOZ_CODE_COVERAGE ||
    AppConstants.ASAN
  ) {
    return 4;
  }
  if (AppConstants.TSAN) {
    return 8;
  }

  return 1;
}
