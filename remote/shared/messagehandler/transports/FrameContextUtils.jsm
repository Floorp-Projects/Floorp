/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["isBrowsingContextCompatible"];

function getOsPid(browsingContext) {
  if (browsingContext instanceof CanonicalBrowsingContext) {
    return browsingContext.currentWindowGlobal.osPid;
  }

  return browsingContext.window.osPid;
}

/**
 * Check if the given browsing context is valid for the message handler
 * to use.
 *
 * @param {BrowsingContext} browsingContext
 *     The browsing context to check.
 * @param {Object=} options
 * @param {String=} options.browserId
 *    The id of the browser to filter the browsing contexts by (optional).
 * @return {Boolean}
 *     True if the browsing context is valid, false otherwise.
 */
function isBrowsingContextCompatible(browsingContext, options = {}) {
  const { browserId } = options;

  // If a browserId was provided, skip browsing contexts which are not
  // associated with this browserId.
  if (browserId !== undefined && browsingContext.browserId !== browserId) {
    return false;
  }

  // Skip window globals running in the parent process, unless we want to
  // support debugging Chrome context, see Bug 1713440.
  if (getOsPid(browsingContext) === -1) {
    return false;
  }

  return true;
}
