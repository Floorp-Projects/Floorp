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
