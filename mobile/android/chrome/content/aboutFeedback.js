/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// input.mozilla.org expects "Firefox for Android" as the product.
const FEEDBACK_PRODUCT_STRING = "Firefox for Android";

let Cc = Components.classes;
let Ci = Components.interfaces;
let Cu = Components.utils;
const HEARTY_ICON_MDPI = "chrome://browser/skin/images/icon_heart_mdpi.png";
const HEARTY_ICON_HDPI = "chrome://browser/skin/images/icon_heart_hdpi.png";
const HEARTY_ICON_XHDPI = "chrome://browser/skin/images/icon_heart_xhdpi.png";
const HEARTY_ICON_XXHDPI = "chrome://browser/skin/images/icon_heart_xxhdpi.png";

const FLOATY_ICON_MDPI = "chrome://browser/skin/images/icon_floaty_mdpi.png";
const FLOATY_ICON_HDPI = "chrome://browser/skin/images/icon_floaty_hdpi.png";
const FLOATY_ICON_XHDPI = "chrome://browser/skin/images/icon_floaty_xhdpi.png";
const FLOATY_ICON_XXHDPI = "chrome://browser/skin/images/icon_floaty_xxhdpi.png";

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Messaging.jsm");
Cu.import("resource://gre/modules/UpdateChannel.jsm");
document.addEventListener("DOMContentLoaded", init, false);

function dump(a) {
  Services.console.logStringMessage(a);
}

function init() {
  let anchors = document.querySelectorAll(".maybe-later");
  for(let anchor of anchors) {
    anchor.addEventListener("click", maybeLater, false);
  }
  document.getElementById("happy-link").addEventListener("click", function(evt) {
    switchSection("happy");
  }, false);
  document.getElementById("sad-link").addEventListener("click", function(evt) {
    switchSection("sad");
  }, false);

  let helpSectionIcon = FLOATY_ICON_XXHDPI;
  let sadThanksIcon = HEARTY_ICON_XXHDPI;

  if (window.devicePixelRatio <= 1) {
    helpSectionIcon = FLOATY_ICON_MDPI;
    sadThanksIcon = HEARTY_ICON_MDPI;
  } else if (window.devicePixelRatio <= 1.5) {
    helpSectionIcon = FLOATY_ICON_HDPI;
    sadThanksIcon = HEARTY_ICON_HDPI;
  } else if (window.devicePixelRatio <= 2) {
    helpSectionIcon = FLOATY_ICON_XHDPI;
    sadThanksIcon = HEARTY_ICON_XHDPI;
  }

  document.getElementById("sumo-icon").src = helpSectionIcon;
  document.getElementById("sad-thanks-icon").src = sadThanksIcon;

  window.addEventListener("unload", uninit, false);

  document.getElementById("open-play-store").addEventListener("click", openPlayStore, false);
  document.forms[0].addEventListener("submit", sendFeedback, false);
  for (let anchor of document.querySelectorAll(".no-thanks")) {
    anchor.addEventListener("click", evt => window.close(), false);
  }

  let sumoLink = Services.urlFormatter.formatURLPref("app.support.baseURL");
  document.getElementById("help-section").addEventListener("click", function() {
    window.open(sumoLink, "_blank");
  }, false);

  window.addEventListener("popstate", function (aEvent) {
	updateActiveSection(aEvent.state ? aEvent.state.section : "intro")
  }, false);

  // Fill "Last visited site" input with most recent history entry URL.
  Services.obs.addObserver(function observer(aSubject, aTopic, aData) {
    document.getElementById("last-url").value = aData;
    // Enable the parent div iff the URL is valid.
    if (aData.length != 0) {
      document.getElementById("last-url-div").style.display="block";
    }
  }, "Feedback:LastUrl", false);

  Messaging.sendRequest({ type: "Feedback:LastUrl" });
}

function uninit() {
  Services.obs.removeObserver(this, "Feedback:LastUrl");
}

function switchSection(aSection) {
  history.pushState({ section: aSection }, aSection);
  updateActiveSection(aSection);
}

function updateActiveSection(aSection) {
  document.querySelector("section[active]").removeAttribute("active");
  document.getElementById(aSection).setAttribute("active", true);
}

function openPlayStore() {
  Messaging.sendRequest({ type: "Feedback:OpenPlayStore" });

  window.close();
}

function maybeLater() {
  window.close();

  Messaging.sendRequest({ type: "Feedback:MaybeLater" });
}

function sendFeedback(aEvent) {
  // Prevent the page from reloading.
  aEvent.preventDefault();

  let section = history.state.section;

  // Sanity check.
  if (section != "sad") {
	Cu.reportError("Trying to send feedback from an invalid section: " + section);
	return;
  }

  let sectionElement = document.getElementById(section);
  let descriptionElement = sectionElement.querySelector(".description");

  let data = {};
  data["happy"] = false;
  data["description"] = descriptionElement.value;
  data["product"] = FEEDBACK_PRODUCT_STRING;

  let urlCheckBox = document.getElementById("last-checkbox");
  let urlElement = document.getElementById("last-url");
  // Only send a URL string if the user provided one.
  if (urlCheckBox.checked) {
    data["url"] = urlElement.value;
  }

  data["device"] = Services.sysinfo.get("device");
  data["manufacturer"] = Services.sysinfo.get("manufacturer");
  data["platform"] = Services.appinfo.OS;
  data["version"] = Services.appinfo.version;
  data["locale"] = Services.locale.getSystemLocale().getCategory("NSILOCALE_CTYPE");
  data["channel"] = UpdateChannel.get();

  let req = new XMLHttpRequest();
  req.addEventListener("error", function() {
	Cu.reportError("Error sending feedback to input.mozilla.org: " + req.statusText);
  }, false);
  req.addEventListener("abort", function() {
	Cu.reportError("Aborted sending feedback to input.mozilla.org: " + req.statusText);
  }, false);

  let postURL = Services.urlFormatter.formatURLPref("app.feedback.postURL");
  req.open("POST", postURL, true);
  req.setRequestHeader("Content-type", "application/json");
  req.send(JSON.stringify(data));

  switchSection("thanks-" + section);
}
