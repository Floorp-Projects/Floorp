/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Home", "resource://gre/modules/Home.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task", "resource://gre/modules/Task.jsm");

XPCOMUtils.defineLazyGetter(this, "gEncoder", function() { return new gChromeWin.TextEncoder(); });
XPCOMUtils.defineLazyGetter(this, "gDecoder", function() { return new gChromeWin.TextDecoder(); });

// URL to fetch snippets, in the urlFormatter service format.
const SNIPPETS_UPDATE_URL_PREF = "browser.snippets.updateUrl";

// URL to send stats data to metrics.
const SNIPPETS_STATS_URL_PREF = "browser.snippets.statsUrl";

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

// Where we cache snippets data
XPCOMUtils.defineLazyGetter(this, "gSnippetsPath", function() {
  return OS.Path.join(OS.Constants.Path.profileDir, "snippets.json");
});

XPCOMUtils.defineLazyGetter(this, "gStatsURL", function() {
  return Services.prefs.getCharPref(SNIPPETS_STATS_URL_PREF);
});

// Where we store stats about which snippets have been shown
XPCOMUtils.defineLazyGetter(this, "gStatsPath", function() {
  return OS.Path.join(OS.Constants.Path.profileDir, "snippets-stats.txt");
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
  let data = gEncoder.encode(response);
  let promise = OS.File.writeAtomic(gSnippetsPath, data, { tmpPath: gSnippetsPath + ".tmp" });
  promise.then(null, e => Cu.reportError("Error caching snippets: " + e));
}

/**
 * Loads snippets from cached `snippets.json`.
 */
function loadSnippetsFromCache() {
  let promise = OS.File.read(gSnippetsPath);
  promise.then(array => updateBanner(gDecoder.decode(array)), e => {
    if (e instanceof OS.File.Error && e.becauseNoSuchFile) {
      Cu.reportError("Couldn't show snippets because cache does not exist yet.");
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
        let parentId = gChromeWin.BrowserApp.selectedTab.id;
        gChromeWin.BrowserApp.addTab(message.url, { parentId: parentId });
      },
      onshown: function() {
        // 10% of the time, record the snippet id and a timestamp
        if (Math.random() < .1) {
          writeStat(message.id, new Date().toISOString());
        }
      }
    });
    // Keep track of the message we added so that we can remove it later.
    gMessageIds.push(id);
  });
}

/**
 * Appends snippet id and timestamp to the end of `snippets-stats.txt`.
 *
 * @param snippetId unique id for snippet, sent from snippets server
 * @param timestamp in ISO8601
 */
function writeStat(snippetId, timestamp) {
  let data = gEncoder.encode(snippetId + "," + timestamp + ";");

  Task.spawn(function() {
    try {
      let file = yield OS.File.open(gStatsPath, { append: true, write: true });
      try {
        yield file.write(data);
      } finally {
        yield file.close();
      }
    } catch (ex if ex instanceof OS.File.Error && ex.becauseNoSuchFile) {
      // If the file doesn't exist yet, create it.
      yield OS.File.writeAtomic(gStatsPath, data, { tmpPath: gStatsPath + ".tmp" });
    }
  }).then(null, e => Cu.reportError("Error writing snippets stats: " + e));
}

/**
 * Reads snippets stats data from `snippets-stats.txt` and sends the data to metrics.
 */
function sendStats() {
  let promise = OS.File.read(gStatsPath);
  promise.then(array => sendStatsRequest(gDecoder.decode(array)), e => {
    if (e instanceof OS.File.Error && e.becauseNoSuchFile) {
      // If the file doesn't exist, there aren't any stats to send.
    } else {
      Cu.reportError("Error eading snippets stats: " + e);
    }
  });
}

/**
 * Sends stats to metrics about which snippets have been shown.
 * Appends snippet ids and timestamps as parameters to a GET request.
 * e.g. https://snippets-stats.mozilla.org/mobile?s1=3825&t1=2013-11-17T18:27Z&s2=6326&t2=2013-11-18T18:27Z
 *
 * @param data contents of stats data file
 */
function sendStatsRequest(data) {
  let params = [];
  let stats = data.split(";");

  // The last item in the array will be an empty string, so stop before then.
  for (let i = 0; i < stats.length - 1; i++) {
    let stat = stats[i].split(",");
    params.push("s" + i + "=" + encodeURIComponent(stat[0]));
    params.push("t" + i + "=" + encodeURIComponent(stat[1]));
  }

  let url = gStatsURL + "?" + params.join("&");

  // Remove the file after succesfully sending the data.
  _httpGetRequest(url, removeStats);
}

/**
 * Removes text file where we store snippets stats.
 */
function removeStats() {
  let promise = OS.File.remove(gStatsPath);
  promise.then(null, e => Cu.reportError("Error removing snippets stats: " + e));
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

function loadSyncPromoBanner() {
  // XXX: Use Accounts.jsm to check if a sync account exists (bug 917942).
  let syncAccountExists = false;
  if (syncAccountExists) {
    // Don't show the promo banner if a sync account already exists.
    return;
  }

  let stringBundle = Services.strings.createBundle("chrome://browser/locale/sync.properties");
  let text = stringBundle.GetStringFromName("promoBanner.message.text");
  let link = stringBundle.GetStringFromName("promoBanner.message.link");

  Home.banner.add({
    text: text + "<a href=\"#\">" + link + "</a>",
    icon: "drawable://sync_promo",
    onclick: function() {
      // XXX: Use Accounts.jsm to launch sync set-up activity (bug 917942).
      gChromeWin.alert("Launch sync set-up activity!");
    }
  });
}

function Snippets() {}

Snippets.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver, Ci.nsITimerCallback]),
  classID: Components.ID("{a78d7e59-b558-4321-a3d6-dffe2f1e76dd}"),

  observe: function(subject, topic, data) {
    switch(topic) {
      case "profile-after-change":
        if (Services.prefs.getBoolPref("browser.snippets.syncPromo.enabled")) {
          loadSyncPromoBanner();
        }

        if (Services.prefs.getBoolPref("browser.snippets.enabled")) {
          loadSnippetsFromCache();
        }
        break;
    }
  },

  // By default, this timer fires once every 24 hours. See the "browser.snippets.updateInterval" pref.
  notify: function(timer) {
    if (!Services.prefs.getBoolPref("browser.snippets.enabled")) {
      return;
    }
    update();
    sendStats();
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([Snippets]);
