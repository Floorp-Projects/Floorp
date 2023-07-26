/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  SESSION_DATA_SHARED_DATA_KEY:
    "chrome://remote/content/shared/messagehandler/sessiondata/SessionData.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "sharedData", () => {
  const isInParent =
    Services.appinfo.processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;

  return isInParent ? Services.ppmm.sharedData : Services.cpmm.sharedData;
});

/**
 * Returns a snapshot of the session data map, which is cloned from the
 * sessionDataMap singleton of SessionData.jsm.
 *
 *  @returns {Map.<string, Array<SessionDataItem>>}
 *     Map of session id to arrays of SessionDataItems.
 */
export const readSessionData = () =>
  lazy.sharedData.get(lazy.SESSION_DATA_SHARED_DATA_KEY) || new Map();
