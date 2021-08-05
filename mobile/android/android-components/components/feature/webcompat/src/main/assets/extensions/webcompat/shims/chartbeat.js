/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1713699 - Shim ChartBeat tracking
 *
 * Sites may rely on chartbeat's tracking as they might with Google Analytics,
 * expecting it to be present for interactive site content to function. This
 * shim mitigates related breakage.
 */

window.pSUPERFLY = {
  activity() {},
  virtualPage() {},
};
