/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1713692 - Shim Google Page Ad conversion tracker
 *
 * This shim stubs out the simple API for converstion tracking with
 * Google Page Ad, mitigating major breakage on pages which presume
 * the API will always successfully load.
 */

if (!window.google_trackConversion) {
  window.google_trackConversion = () => {};
}
