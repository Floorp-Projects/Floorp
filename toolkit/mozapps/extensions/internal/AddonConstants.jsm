/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [ "ADDON_SIGNING", "REQUIRE_SIGNING" ];

// Make these non-changable properties so they can't be manipulated from other
// code in the app.
Object.defineProperty(this, "ADDON_SIGNING", {
  configurable: false,
  enumerable: false,
  writable: false,
#ifdef MOZ_ADDON_SIGNING
  value: true,
#else
  value: false,
#endif
});

Object.defineProperty(this, "REQUIRE_SIGNING", {
  configurable: false,
  enumerable: false,
  writable: false,
#ifdef MOZ_REQUIRE_SIGNING
  value: true,
#else
  value: false,
#endif
});
