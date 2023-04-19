/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * Bug 1722955 - Add UA override for frontgate.com
 * Webcompat issue #36277 - https://github.com/webcompat/web-bugs/issues/36277
 *
 * The website is sending the desktop version to Firefox on mobile devices
 * based on UA sniffing. Spoofing as Chrome fixes this.
 */

/* globals exportFunction, UAHelpers */

console.info(
  "The user agent has been overridden for compatibility reasons. See https://webcompat.com/issues/36277 for details."
);

UAHelpers.overrideWithDeviceAppropriateChromeUA();
