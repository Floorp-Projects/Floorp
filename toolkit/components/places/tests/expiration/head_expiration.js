/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Import common head.
{
  /* import-globals-from ../head_common.js */
  let commonFile = do_get_file("../head_common.js", false);
  let uri = Services.io.newFileURI(commonFile);
  Services.scriptloader.loadSubScript(uri.spec, this);
}

// Put any other stuff relative to this test folder below.

/**
 * Causes expiration component to start, otherwise it would wait for the first
 * history notification.
 */
function force_expiration_start() {
  Cc["@mozilla.org/places/expiration;1"]
    .getService(Ci.nsIObserver)
    .observe(null, "testing-mode", null);
}

/**
 * Forces an expiration run.
 *
 * @param [optional] aLimit
 *        Limit for the expiration.  Pass -1 for unlimited.
 *        Any other non-positive value will just expire orphans.
 *
 * @return {Promise}
 * @resolves When expiration finishes.
 * @rejects Never.
 */
function promiseForceExpirationStep(aLimit) {
  let promise = promiseTopicObserved(PlacesUtils.TOPIC_EXPIRATION_FINISHED);
  let expire = Cc["@mozilla.org/places/expiration;1"].getService(
    Ci.nsIObserver
  );
  expire.observe(null, "places-debug-start-expiration", aLimit);
  return promise;
}

/**
 * Expiration preferences helpers.
 */

function setInterval(aNewInterval) {
  Services.prefs.setIntPref(
    "places.history.expiration.interval_seconds",
    aNewInterval
  );
}
function getInterval() {
  return Services.prefs.getIntPref(
    "places.history.expiration.interval_seconds"
  );
}
function clearInterval() {
  try {
    Services.prefs.clearUserPref("places.history.expiration.interval_seconds");
  } catch (ex) {}
}

function setMaxPages(aNewMaxPages) {
  Services.prefs.setIntPref(
    "places.history.expiration.max_pages",
    aNewMaxPages
  );
}
function getMaxPages() {
  return Services.prefs.getIntPref("places.history.expiration.max_pages");
}
function clearMaxPages() {
  try {
    Services.prefs.clearUserPref("places.history.expiration.max_pages");
  } catch (ex) {}
}

function setHistoryEnabled(aHistoryEnabled) {
  Services.prefs.setBoolPref("places.history.enabled", aHistoryEnabled);
}
function getHistoryEnabled() {
  return Services.prefs.getBoolPref("places.history.enabled");
}
function clearHistoryEnabled() {
  try {
    Services.prefs.clearUserPref("places.history.enabled");
  } catch (ex) {}
}

/**
 * Returns a PRTime in the past usable to add expirable visits.
 *
 * param [optional] daysAgo
 *       Expiration ignores any visit added in the last 7 days, so by default
 *       this will be set to 7.
 * @note to be safe against DST issues we go back one day more.
 */
function getExpirablePRTime(daysAgo = 7) {
  let dateObj = new Date();
  // Normalize to midnight
  dateObj.setHours(0);
  dateObj.setMinutes(0);
  dateObj.setSeconds(0);
  dateObj.setMilliseconds(0);
  dateObj = new Date(dateObj.getTime() - (daysAgo + 1) * 86400000);
  return dateObj.getTime() * 1000;
}
