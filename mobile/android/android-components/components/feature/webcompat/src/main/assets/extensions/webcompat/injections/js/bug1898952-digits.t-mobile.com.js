/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1898952 - Spoof navigator.userAgentData for digits.t-mobile.com
 * Webcompat issue #119767 - https://webcompat.com/issues/119767
 *
 * The site blocks Firefox and Safari, reading info from userAgentData.
 */

/* globals exportFunction, cloneInto */

if (!navigator.userAgentData) {
  console.info(
    "navigator.userAgentData has been overridden for compatibility reasons. See https://webcompat.com/issues/119767 for details."
  );

  const ua = navigator.userAgent;
  const mobile = ua.includes("Mobile") || ua.includes("Tablet");

  // Very roughly matches Chromium's GetPlatformForUAMetadata()
  let platform = "Linux";
  if (mobile) {
    platform = "Android";
  } else if (navigator.platform.startsWith("Win")) {
    platform = "Windows";
  } else if (navigator.platform.startsWith("Mac")) {
    platform = "macOS";
  }

  const version = (ua.match(/Firefox\/([0-9.]+)/) || ["", "58.0"])[1];

  // These match Chrome's output as of version 126.
  const brands = [
    {
      brand: "Not/A)Brand",
      version: "8",
    },
    {
      brand: "Chromium",
      version,
    },
    {
      brand: "Google Chrome",
      version,
    },
  ];

  const userAgentData = cloneInto(
    {
      brands,
      mobile,
      platform,
    },
    window
  );

  Object.defineProperty(window.navigator.wrappedJSObject, "userAgentData", {
    get: exportFunction(function () {
      return userAgentData;
    }, window),

    set: exportFunction(function () {}, window),
  });
}
