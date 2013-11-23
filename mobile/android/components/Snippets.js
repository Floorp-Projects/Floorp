/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Home", "resource://gre/modules/Home.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");

XPCOMUtils.defineLazyGetter(this, "gEncoder", function() { return new gChromeWin.TextEncoder(); });
XPCOMUtils.defineLazyGetter(this, "gDecoder", function() { return new gChromeWin.TextDecoder(); });

const SNIPPETS_ENABLED = Services.prefs.getBoolPref("browser.snippets.enabled");

// URL to fetch snippets, in the urlFormatter service format.
const SNIPPETS_UPDATE_URL_PREF = "browser.snippets.updateUrl";

// URL to fetch country code, a value that's cached and refreshed once per month.
const SNIPPETS_GEO_URL_PREF = "browser.snippets.geoUrl";

// Timestamp when we last updated the user's country code.
const SNIPPETS_GEO_LAST_UPDATE_PREF = "browser.snippets.geoLastUpdate";

// Pref where we'll cache the user's country.
const SNIPPETS_COUNTRY_CODE_PREF = "browser.snippets.countryCode";

// How frequently we update the user's country code from the server (30 days).
const SNIPPETS_GEO_UPDATE_INTERVAL_MS = 86400000*30;

// Should be bumped up if the snippets content format changes.
const SNIPPETS_VERSION = 1;

XPCOMUtils.defineLazyGetter(this, "gSnippetsURL", function() {
  let updateURL = Services.prefs.getCharPref(SNIPPETS_UPDATE_URL_PREF).replace("%SNIPPETS_VERSION%", SNIPPETS_VERSION);
  return Services.urlFormatter.formatURL(updateURL);
});

XPCOMUtils.defineLazyGetter(this, "gGeoURL", function() {
  return Services.prefs.getCharPref(SNIPPETS_GEO_URL_PREF);
});

XPCOMUtils.defineLazyGetter(this, "gCountryCode", function() {
  try {
    return Services.prefs.getCharPref(SNIPPETS_COUNTRY_CODE_PREF);
  } catch (e) {
    // Return an empty string if the country code pref isn't set yet.
    return "";
  }
});

XPCOMUtils.defineLazyGetter(this, "gChromeWin", function() {
  return Services.wm.getMostRecentWindow("navigator:browser");
});

/**
 * Updates snippet data and country code (if necessary).
 */
function update() {
  // Check to see if we should update the user's country code from the geo server.
  let lastUpdate = 0;
  try {
    lastUpdate = parseFloat(Services.prefs.getCharPref(SNIPPETS_GEO_LAST_UPDATE_PREF));
  } catch (e) {}

  if (Date.now() - lastUpdate > SNIPPETS_GEO_UPDATE_INTERVAL_MS) {
    // We should update the snippets after updating the country code,
    // so that we can filter snippets to add to the banner.
    updateCountryCode(updateSnippets);
  } else {
    updateSnippets();
  }
}

/**
 * Fetches the user's country code from the geo server and stores the value in a pref.
 *
 * @param callback function called once country code is updated
 */
function updateCountryCode(callback) {
  _httpGetRequest(gGeoURL, function(responseText) {
    // Store the country code in a pref.
    let data = JSON.parse(responseText);
    Services.prefs.setCharPref(SNIPPETS_COUNTRY_CODE_PREF, data.country_code);

    // Set last update time.
    Services.prefs.setCharPref(SNIPPETS_GEO_LAST_UPDATE_PREF, Date.now());

    callback();
  });
}

/**
 * Loads snippets from snippets server, caches the response, and
 * updates the home banner with the new set of snippets.
 */
function updateSnippets() {
  _httpGetRequest(gSnippetsURL, function(responseText) {
    cacheSnippets(responseText);
    updateBanner(responseText);
  });
}

/**
 * Caches snippets server response text to `snippets.json` in profile directory.
 *
 * @param response responseText returned from snippets server
 */
function cacheSnippets(response) {
  let path = OS.Path.join(OS.Constants.Path.profileDir, "snippets.json");
  let data = gEncoder.encode(response);
  let promise = OS.File.writeAtomic(path, data, { tmpPath: path + ".tmp" });
  promise.then(null, e => Cu.reportError("Error caching snippets: " + e));
}

/**
 * Loads snippets from cached `snippets.json`.
 */
function loadSnippetsFromCache() {
  let path = OS.Path.join(OS.Constants.Path.profileDir, "snippets.json");
  let promise = OS.File.read(path);
  promise.then(array => updateBanner(gDecoder.decode(array)), e => {
    // If snippets.json doesn't exist, update data from the server.
    if (e instanceof OS.File.Error && e.becauseNoSuchFile) {
      update();
    } else {
      Cu.reportError("Error loading snippets from cache: " + e);
    }
  });
}

// Array of the message ids added to the home banner, used to remove
// older set of snippets when new ones are available.
var gMessageIds = [];

/**
 * Updates set of snippets in the home banner message rotation.
 *
 * @param response responseText returned from snippets server.
 *   This should be a JSON array of message data JSON objects.
 *   Each message object should have the following properties:
 *     - id (?): Unique identifier for this snippets message
 *     - text (string): Text to show as banner message
 *     - url (string): URL to open when banner is clicked
 *     - icon (data URI): Icon to appear in banner
 *     - target_geo (string): Country code for where this message should be shown (e.g. "US")
 */
function updateBanner(response) {
  // Remove the current messages, if there are any.
  gMessageIds.forEach(function(id) {
    Home.banner.remove(id);
  })
  gMessageIds = [];

  let messages = JSON.parse(response);
  messages.forEach(function(message) {
    // Don't add this message to the banner if it's not supposed to be shown in this country.
    if ("target_geo" in message && message.target_geo != gCountryCode) {
      return;
    }
    let id = Home.banner.add({
      text: message.text,
      icon: message.icon,
      onclick: function() {
        gChromeWin.BrowserApp.addTab(message.url);
      },
      onshown: function() {
        // XXX: 10% of the time, let the metrics server know which message was shown (bug 937373)
      }
    });
    // Keep track of the message we added so that we can remove it later.
    gMessageIds.push(id);
  });
}

/**
 * Helper function to make HTTP GET requests.
 *
 * @param url where we send the request
 * @param callback function that is called with the xhr responseText
 */
function _httpGetRequest(url, callback) {
  let xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].createInstance(Ci.nsIXMLHttpRequest);
  try {
    xhr.open("GET", url, true);
  } catch (e) {
    Cu.reportError("Error opening request to " + url + ": " + e);
    return;
  }
  xhr.onerror = function onerror(e) {
    Cu.reportError("Error making request to " + url + ": " + e.error);
  }
  xhr.onload = function onload(event) {
    if (xhr.status !== 200) {
      Cu.reportError("Request to " + url + " returned status " + xhr.status);
      return;
    }
    if (callback) {
      callback(xhr.responseText);
    }
  }
  xhr.send(null);
}

function Snippets() {}

Snippets.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver, Ci.nsITimerCallback]),
  classID: Components.ID("{a78d7e59-b558-4321-a3d6-dffe2f1e76dd}"),

  observe: function(subject, topic, data) {
    if (!SNIPPETS_ENABLED) {
      return;
    }
    switch(topic) {
      case "profile-after-change":
        loadSnippetsFromCache();
        break;
    }
  },

  // By default, this timer fires once every 24 hours. See the "browser.snippets.updateInterval" pref.
  notify: function(timer) {
    if (!SNIPPETS_ENABLED) {
      return;
    }
    update();
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([Snippets]);
