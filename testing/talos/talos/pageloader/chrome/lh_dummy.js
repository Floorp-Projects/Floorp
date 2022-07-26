/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

function _dummy() {
  sendAsyncMessage("PageLoader:LoadEvent", {});
}

addEventListener("load", contentLoadHandlerCallback(_dummy), true); // eslint-disable-line no-undef
