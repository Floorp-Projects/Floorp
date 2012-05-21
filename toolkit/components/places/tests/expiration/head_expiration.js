/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");

// Import common head.
let (commonFile = do_get_file("../head_common.js", false)) {
  let uri = Services.io.newFileURI(commonFile);
  Services.scriptloader.loadSubScript(uri.spec, this);
}

// Put any other stuff relative to this test folder below.


// Simulates an expiration at shutdown.
function shutdownExpiration()
{
  let expire = Cc["@mozilla.org/places/expiration;1"].getService(Ci.nsIObserver);
  expire.observe(null, "places-will-close-connection", null);
}


/**
 * Causes expiration component to start, otherwise it would wait for the first
 * history notification.
 */
function force_expiration_start() {
  Cc["@mozilla.org/places/expiration;1"].getService(Ci.nsISupports);
}


/**
 * Forces an expiration run.
 *
 * @param [optional] aLimit
 *        Limit for the expiration.  Pass -1 for unlimited.
 *        Any other non-positive value will just expire orphans.
 */
function force_expiration_step(aLimit) {
  const TOPIC_DEBUG_START_EXPIRATION = "places-debug-start-expiration";
  let expire = Cc["@mozilla.org/places/expiration;1"].getService(Ci.nsIObserver);
  expire.observe(null, TOPIC_DEBUG_START_EXPIRATION, aLimit);
}


/**
 * Expiration preferences helpers.
 */

function setInterval(aNewInterval) {
  Services.prefs.setIntPref("places.history.expiration.interval_seconds", aNewInterval);
}
function getInterval() {
  return Services.prefs.getIntPref("places.history.expiration.interval_seconds");
}
function clearInterval() {
  try {
    Services.prefs.clearUserPref("places.history.expiration.interval_seconds");
  }
  catch(ex) {}
}


function setMaxPages(aNewMaxPages) {
  Services.prefs.setIntPref("places.history.expiration.max_pages", aNewMaxPages);
}
function getMaxPages() {
  return Services.prefs.getIntPref("places.history.expiration.max_pages");
}
function clearMaxPages() {
  try {
    Services.prefs.clearUserPref("places.history.expiration.max_pages");
  }
  catch(ex) {}
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
  }
  catch(ex) {}
}

/**
 * Returns a PRTime in the past usable to add expirable visits.
 *
 * @note Expiration ignores any visit added in the last 7 days, but it's
 *       better be safe against DST issues, by going back one day more.
 */
function getExpirablePRTime() {
  let dateObj = new Date();
  // Normalize to midnight
  dateObj.setHours(0);
  dateObj.setMinutes(0);
  dateObj.setSeconds(0);
  dateObj.setMilliseconds(0);
  dateObj = new Date(dateObj.getTime() - 8 * 86400000);
  return dateObj.getTime() * 1000;
}
