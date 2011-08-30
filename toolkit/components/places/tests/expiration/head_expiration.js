/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Places Unit Tests.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Marco Bonardo <mak77@bonardo.net> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
