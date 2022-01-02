/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["AppInfo"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",
});

const ID_FIREFOX = "{ec8030f7-c20a-464f-9b0e-13a3a9e97384}";
const ID_THUNDERBIRD = "{3550f703-e582-4d05-9a08-453d09bdfdc6}";

/**
 * Extends Services.appinfo with further properties that are
 * used across Marionette.
 *
 * @typedef {object} Marionette.AppInfo
 * @property {Boolean} isAndroid - Whether the application runs on Android.
 * @property {Boolean} isLinux - Whether the application runs on Linux.
 * @property {Boolean} isMac - Whether the application runs on Mac OS.
 * @property {Boolean} isWindows - Whether the application runs on Windows.
 * @property {Boolean} isFirefox - Whether the application is Firefox.
 * @property {Boolean} isThunderbird - Whether the application is Thunderbird.
 *
 * @since 88
 */
const AppInfo = new Proxy(
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

XPCOMUtils.defineLazyGetter(AppInfo, "isAndroid", () => {
  return Services.appinfo.OS === "Android";
});

XPCOMUtils.defineLazyGetter(AppInfo, "isLinux", () => {
  return Services.appinfo.OS === "Linux";
});

XPCOMUtils.defineLazyGetter(AppInfo, "isMac", () => {
  return Services.appinfo.OS === "Darwin";
});

XPCOMUtils.defineLazyGetter(AppInfo, "isWindows", () => {
  return Services.appinfo.OS === "WINNT";
});

// Application type

XPCOMUtils.defineLazyGetter(AppInfo, "isFirefox", () => {
  return Services.appinfo.ID == ID_FIREFOX;
});

XPCOMUtils.defineLazyGetter(AppInfo, "isThunderbird", () => {
  return Services.appinfo.ID == ID_THUNDERBIRD;
});
