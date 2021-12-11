/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

// get release notes and vendor URL from prefs
var releaseNotesURL = Services.urlFormatter.formatURLPref(
  "app.releaseNotesURL"
);
if (releaseNotesURL != "about:blank") {
  var relnotes = document.getElementById("releaseNotesURL");
  relnotes.setAttribute("href", releaseNotesURL);
  relnotes.parentNode.removeAttribute("hidden");
}

var vendorURL = Services.urlFormatter.formatURLPref("app.vendorURL");
if (vendorURL != "about:blank") {
  var vendor = document.getElementById("vendorURL");
  vendor.setAttribute("href", vendorURL);
}

// insert the version of the XUL application (!= XULRunner platform version)
var versionNum = Services.appinfo.version;
var version = document.getElementById("version");
version.textContent += " " + versionNum;

// append user agent
var ua = navigator.userAgent;
if (ua) {
  document.getElementById("buildID").textContent += " " + ua;
}
