/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
let Cc = Components.classes;
let Ci = Components.interfaces;
let Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");

function pref(name, value) {
  return {
    name: name,
    value: value
  }
}

var WebAppRT = {
  prefs: [
    // Disable all add-on locations other than the profile (which can't be disabled this way)
    pref("extensions.enabledScopes", 1),
    // Auto-disable any add-ons that are "dropped in" to the profile
    pref("extensions.autoDisableScopes", 1),
    // Disable add-on installation via the web-exposed APIs
    pref("xpinstall.enabled", false),
    // Disable the telemetry prompt in webapps
    pref("toolkit.telemetry.prompted", 2)
  ],

  init: function(isUpdate) {
    this.deck = document.getElementById("browsers");
    this.deck.addEventListener("click", this, false, true);

    // on first run, update any prefs
    if (isUpdate == "new") {
      this.prefs.forEach(function(aPref) {
        switch (typeof aPref.value) {
          case "string":
            Services.prefs.setCharPref(aPref.name, aPref.value);
            break;
          case "boolean":
            Services.prefs.setBoolPref(aPref.name, aPref.value);
            break;
          case "number":
            Services.prefs.setIntPref(aPref.name, aPref.value);
            break;
        }
      });

      // update the blocklist url to use a different app id
      var blocklist = Services.prefs.getCharPref("extensions.blocklist.url");
      blocklist = blocklist.replace(/%APP_ID%/g, "webapprt-mobile@mozilla.org");
      Services.prefs.setCharPref("extensions.blocklist.url", blocklist);
    }
  },

  handleEvent: function(event) {
    let target = event.target;
  
    if (!(target instanceof HTMLAnchorElement) ||
        target.getAttribute("target") != "_blank") {
      return;
    }
  
    let uri = Services.io.newURI(target.href,
                                 target.ownerDocument.characterSet,
                                 null);
  
    // Direct the URL to the browser.
    Cc["@mozilla.org/uriloader/external-protocol-service;1"].
      getService(Ci.nsIExternalProtocolService).
      getProtocolHandlerInfo(uri.scheme).
      launchWithURI(uri);
  
    // Prevent the runtime from loading the URL.  We do this after directing it
    // to the browser to give the runtime a shot at handling the URL if we fail
    // to direct it to the browser for some reason.
    event.preventDefault();
  }
}
