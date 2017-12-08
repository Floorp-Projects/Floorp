/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

// This is the only implementation of nsIUrlListManager.
// A class that manages lists, namely white and black lists for
// phishing or malware protection. The ListManager knows how to fetch,
// update, and store lists.
//
// There is a single listmanager for the whole application.
//
// TODO more comprehensive update tests, for example add unittest check
//      that the listmanagers tables are properly written on updates

// Lower and upper limits on the server-provided polling frequency
const minDelayMs = 5 * 60 * 1000;
const maxDelayMs = 24 * 60 * 60 * 1000;
const defaultUpdateIntervalMs = 30 * 60 * 1000;
const PREF_DEBUG_ENABLED = "browser.safebrowsing.debug";

let loggingEnabled = false;

// Log only if browser.safebrowsing.debug is true
this.log = function log(...stuff) {
  if (!loggingEnabled) {
    return;
  }

  var d = new Date();
  let msg = "listmanager: " + d.toTimeString() + ": " + stuff.join(" ");
  msg = Services.urlFormatter.trimSensitiveURLs(msg);
  Services.console.logStringMessage(msg);
  dump(msg + "\n");
};

/**
 * A ListManager keeps track of black and white lists and knows
 * how to update them.
 *
 * @constructor
 */
this.PROT_ListManager = function PROT_ListManager() {
  loggingEnabled = Services.prefs.getBoolPref(PREF_DEBUG_ENABLED);

  log("Initializing list manager");
  this.updateInterval = defaultUpdateIntervalMs;

  // A map of tableNames to objects of type
  // { updateUrl: <updateUrl>, gethashUrl: <gethashUrl> }
  this.tablesData = {};
  // A map of updateUrls to maps of tables requiring updates, e.g.
  // { safebrowsing-update-url: { goog-phish-shavar: true,
  //                              goog-malware-shavar: true }
  this.needsUpdate_ = {};

  // A map of updateUrls to single-use nsITimer. An entry exists if and only if
  // there is at least one table with updates enabled for that url. nsITimers
  // are reset when enabling/disabling updates or on update callbacks (update
  // success, update failure, download error).
  this.updateCheckers_ = {};
  this.requestBackoffs_ = {};
  this.dbService_ = Cc["@mozilla.org/url-classifier/dbservice;1"]
                   .getService(Ci.nsIUrlClassifierDBService);

  Services.obs.addObserver(this, "quit-application");
  Services.prefs.addObserver(PREF_DEBUG_ENABLED, this);
};

/**
 * Register a new table table
 * @param tableName - the name of the table
 * @param updateUrl - the url for updating the table
 * @param gethashUrl - the url for fetching hash completions
 * @returns true if the table could be created; false otherwise
 */
PROT_ListManager.prototype.registerTable = function(tableName,
                                                    providerName,
                                                    updateUrl,
                                                    gethashUrl) {
  this.tablesData[tableName] = {};
  if (!updateUrl) {
    log("Can't register table " + tableName + " without updateUrl");
    return false;
  }
  log("registering " + tableName + " with " + updateUrl);
  this.tablesData[tableName].updateUrl = updateUrl;
  this.tablesData[tableName].gethashUrl = gethashUrl;
  this.tablesData[tableName].provider = providerName;

  // Keep track of all of our update URLs.
  if (!this.needsUpdate_[updateUrl]) {
    this.needsUpdate_[updateUrl] = {};

    // Using the V4 backoff algorithm for both V2 and V4. See bug 1273398.
    this.requestBackoffs_[updateUrl] = new RequestBackoffV4(
                                            4 /* num requests */,
                               60 * 60 * 1000 /* request time, 60 min */,
                                 providerName /* used by testcase */);
  }
  this.needsUpdate_[updateUrl][tableName] = false;

  return true;
};

/**
 * Unregister a table table from list
 */
PROT_ListManager.prototype.unregisterTable = function(tableName) {
  log("unregistering " + tableName);
  var table = this.tablesData[tableName];
  if (table) {
    if (!this.updatesNeeded_(table.updateUrl) &&
        this.updateCheckers_[table.updateUrl]) {
      this.updateCheckers_[table.updateUrl].cancel();
      this.updateCheckers_[table.updateUrl] = null;
    }
    delete this.needsUpdate_[table.updateUrl][tableName];
  }
  delete this.tablesData[tableName];

};

/**
 * Delete all of our data tables which seem to leak otherwise.
 * Remove observers
 */
PROT_ListManager.prototype.shutdown_ = function() {
  this.stopUpdateCheckers();
  for (var name in this.tablesData) {
    delete this.tablesData[name];
  }
  Services.obs.removeObserver(this, "quit-application");
  Services.prefs.removeObserver(PREF_DEBUG_ENABLED, this);
};

/**
 * xpcom-shutdown callback
 */
PROT_ListManager.prototype.observe = function(aSubject, aTopic, aData) {
  switch (aTopic) {
  case "quit-application":
    this.shutdown_();
    break;
  case "nsPref:changed":
    if (aData == PREF_DEBUG_ENABLED) {
      loggingEnabled = Services.prefs.getBoolPref(PREF_DEBUG_ENABLED);
    }
    break;
  }
};


PROT_ListManager.prototype.getGethashUrl = function(tableName) {
  if (this.tablesData[tableName] && this.tablesData[tableName].gethashUrl) {
    return this.tablesData[tableName].gethashUrl;
  }
  return "";
};

PROT_ListManager.prototype.getUpdateUrl = function(tableName) {
  if (this.tablesData[tableName] && this.tablesData[tableName].updateUrl) {
    return this.tablesData[tableName].updateUrl;
  }
  return "";
};

/**
 * Enable updates for some tables
 * @param tables - an array of table names that need updating
 */
PROT_ListManager.prototype.enableUpdate = function(tableName) {
  var table = this.tablesData[tableName];
  if (table) {
    log("Enabling table updates for " + tableName);
    this.needsUpdate_[table.updateUrl][tableName] = true;
  }
};

/**
 * Returns true if any table associated with the updateUrl requires updates.
 * @param updateUrl - the updateUrl
 */
PROT_ListManager.prototype.updatesNeeded_ = function(updateUrl) {
  let updatesNeeded = false;
  for (var tableName in this.needsUpdate_[updateUrl]) {
    if (this.needsUpdate_[updateUrl][tableName]) {
      updatesNeeded = true;
    }
  }
  return updatesNeeded;
};

/**
 * Disables updates for some tables
 * @param tables - an array of table names that no longer need updating
 */
PROT_ListManager.prototype.disableUpdate = function(tableName) {
  var table = this.tablesData[tableName];
  if (table) {
    log("Disabling table updates for " + tableName);
    this.needsUpdate_[table.updateUrl][tableName] = false;
    if (!this.updatesNeeded_(table.updateUrl) &&
        this.updateCheckers_[table.updateUrl]) {
      this.updateCheckers_[table.updateUrl].cancel();
      this.updateCheckers_[table.updateUrl] = null;
    }
  }
};

/**
 * Determine if we have some tables that need updating.
 */
PROT_ListManager.prototype.requireTableUpdates = function() {
  for (var name in this.tablesData) {
    // Tables that need updating even if other tables don't require it
    if (this.needsUpdate_[this.tablesData[name].updateUrl][name]) {
      return true;
    }
  }

  return false;
};

/**
 *  Set timer to check update after delay
 */
PROT_ListManager.prototype.setUpdateCheckTimer = function(updateUrl,
                                                          delay) {
  this.updateCheckers_[updateUrl] = Cc["@mozilla.org/timer;1"]
                                    .createInstance(Ci.nsITimer);
  this.updateCheckers_[updateUrl].initWithCallback(() => {
    this.updateCheckers_[updateUrl] = null;
    if (updateUrl && !this.checkForUpdates(updateUrl)) {
      // Make another attempt later.
      this.setUpdateCheckTimer(updateUrl, this.updateInterval);
    }
  }, delay, Ci.nsITimer.TYPE_ONE_SHOT);
};
/**
 * Acts as a nsIUrlClassifierCallback for getTables.
 */
PROT_ListManager.prototype.kickoffUpdate_ = function(onDiskTableData) {
  this.startingUpdate_ = false;
  var initialUpdateDelay = 3000;
  // Add a fuzz of 0-1 minutes for both v2 and v4 according to Bug 1305478.
  initialUpdateDelay += Math.floor(Math.random() * (1 * 60 * 1000));

  // If the user has never downloaded tables, do the check now.
  log("needsUpdate: " + JSON.stringify(this.needsUpdate_, undefined, 2));
  for (var updateUrl in this.needsUpdate_) {
    // If we haven't already kicked off updates for this updateUrl, set a
    // non-repeating timer for it. The timer delay will be reset either on
    // updateSuccess to this.updateInterval, or backed off on downloadError.
    // Don't set the updateChecker unless at least one table has updates
    // enabled.
    if (this.updatesNeeded_(updateUrl) && !this.updateCheckers_[updateUrl]) {
      let provider = null;
      Object.keys(this.tablesData).forEach(function(table) {
        if (this.tablesData[table].updateUrl === updateUrl) {
          let newProvider = this.tablesData[table].provider;
          if (provider) {
            if (newProvider !== provider) {
              log("Multiple tables for the same updateURL have a different provider?!");
            }
          } else {
            provider = newProvider;
          }
        }
      }, this);
      log("Initializing update checker for " + updateUrl
          + " provided by " + provider);

      // Use the initialUpdateDelay + fuzz unless we had previous updates
      // and the server told us when to try again.
      let updateDelay = initialUpdateDelay;
      let nextUpdatePref = "browser.safebrowsing.provider." + provider +
                           ".nextupdatetime";
      let nextUpdate = Services.prefs.getCharPref(nextUpdatePref, "");

      if (nextUpdate) {
        updateDelay = Math.min(maxDelayMs, Math.max(0, nextUpdate - Date.now()));
        log("Next update at " + nextUpdate);
      }
      log("Next update " + updateDelay / 60000 + "min from now");

      this.setUpdateCheckTimer(updateUrl, updateDelay);
    } else {
      log("No updates needed or already initialized for " + updateUrl);
    }
  }
};

PROT_ListManager.prototype.stopUpdateCheckers = function() {
  log("Stopping updates");
  for (var updateUrl in this.updateCheckers_) {
    if (this.updateCheckers_[updateUrl]) {
      this.updateCheckers_[updateUrl].cancel();
      this.updateCheckers_[updateUrl] = null;
    }
  }
};

/**
 * Determine if we have any tables that require updating.  Different
 * Wardens may call us with new tables that need to be updated.
 */
PROT_ListManager.prototype.maybeToggleUpdateChecking = function() {
  // We update tables if we have some tables that want updates.  If there
  // are no tables that want to be updated - we dont need to check anything.
  if (this.requireTableUpdates()) {
    log("Starting managing lists");

    // Get the list of existing tables from the DBService before making any
    // update requests.
    if (!this.startingUpdate_) {
      this.startingUpdate_ = true;
      // check the current state of tables in the database
      this.dbService_.getTables(this.kickoffUpdate_.bind(this));
    }
  } else {
    log("Stopping managing lists (if currently active)");
    this.stopUpdateCheckers(); // Cancel pending updates
  }
};

/**
 * Force updates for the given tables. This API may trigger more than one update
 * if the table lists provided belong to multiple updateurl (multiple provider).
 * Return false when any update is fail due to back-off algorithm.
 */
PROT_ListManager.prototype.forceUpdates = function(tables) {
  log("forceUpdates with " + tables);
  if (!tables) {
    return false;
  }

  let updateUrls = new Set();
  tables.split(",").forEach((table) => {
    if (this.tablesData[table]) {
      updateUrls.add(this.tablesData[table].updateUrl);
    }
  });

  let ret = true;

  updateUrls.forEach((url) => {
    // Cancel current update timer for the url because we are forcing an update.
    if (this.updateCheckers_[url]) {
      this.updateCheckers_[url].cancel();
      this.updateCheckers_[url] = null;
    }

    // Trigger an update for the given url.
    if (!this.checkForUpdates(url)) {
      ret = false;
    }
  });

  return ret;
};

/**
 * Updates our internal tables from the update server
 *
 * @param updateUrl: request updates for tables associated with that url, or
 * for all tables if the url is empty.
 */
PROT_ListManager.prototype.checkForUpdates = function(updateUrl) {
  log("checkForUpdates with " + updateUrl);
  // See if we've triggered the request backoff logic.
  if (!updateUrl) {
    return false;
  }
  if (!this.requestBackoffs_[updateUrl] ||
      !this.requestBackoffs_[updateUrl].canMakeRequest()) {
    log("Can't make update request");
    return false;
  }
  // Grab the current state of the tables from the database
  this.dbService_.getTables(BindToObject(this.makeUpdateRequest_, this,
                            updateUrl));
  return true;
};

/**
 * Method that fires the actual HTTP update request.
 * First we reset any tables that have disappeared.
 * @param tableData List of table data already in the database, in the form
 *        tablename;<chunk ranges>\n
 */
PROT_ListManager.prototype.makeUpdateRequest_ = function(updateUrl, tableData) {
  log("this.tablesData: " + JSON.stringify(this.tablesData, undefined, 2));
  log("existing chunks: " + tableData + "\n");
  // Disallow blank updateUrls
  if (!updateUrl) {
    return;
  }
  // An object of the form
  // { tableList: comma-separated list of tables to request,
  //   tableNames: map of tables that need updating,
  //   request: list of tables and existing chunk ranges from tableData
  // }
  var streamerMap = { tableList: null,
                      tableNames: {},
                      requestPayload: "",
                      isPostRequest: true };

  let useProtobuf = false;
  let onceThru = false;
  for (var tableName in this.tablesData) {
    // Skip tables not matching this update url
    if (this.tablesData[tableName].updateUrl != updateUrl) {
      continue;
    }

    // Check if |updateURL| is for 'proto'. (only v4 uses protobuf for now.)
    // We use the table name 'goog-*-proto' and an additional provider "google4"
    // to describe the v4 settings.
    let isCurTableProto = tableName.endsWith("-proto");
    if (!onceThru) {
      useProtobuf = isCurTableProto;
      onceThru = true;
    } else if (useProtobuf !== isCurTableProto) {
      log('ERROR: Cannot mix "proto" tables with other types ' +
          "within the same provider.");
    }

    if (this.needsUpdate_[this.tablesData[tableName].updateUrl][tableName]) {
      streamerMap.tableNames[tableName] = true;
    }
    if (!streamerMap.tableList) {
      streamerMap.tableList = tableName;
    } else {
      streamerMap.tableList += "," + tableName;
    }
  }

  if (useProtobuf) {
    let tableArray = [];
    Object.keys(streamerMap.tableNames).forEach(aTableName => {
      if (streamerMap.tableNames[aTableName]) {
        tableArray.push(aTableName);
      }
    });

    // Build the <tablename, stateBase64> mapping.
    let tableState = {};
    tableData.split("\n").forEach(line => {
      let p = line.indexOf(";");
      if (-1 === p) {
        return;
      }
      let tableName = line.substring(0, p);
      if (tableName in streamerMap.tableNames) {
        let metadata = line.substring(p + 1).split(":");
        let stateBase64 = metadata[0];
        log(tableName + " ==> " + stateBase64);
        tableState[tableName] = stateBase64;
      }
    });

    // The state is a byte stream which server told us from the
    // last table update. The state would be used to do the partial
    // update and the empty string means the table has
    // never been downloaded. See Bug 1287058 for supporting
    // partial update.
    let stateArray = [];
    tableArray.forEach(listName => {
      stateArray.push(tableState[listName] || "");
    });

    log("stateArray: " + stateArray);

    let urlUtils = Cc["@mozilla.org/url-classifier/utils;1"]
                     .getService(Ci.nsIUrlClassifierUtils);

    streamerMap.requestPayload = urlUtils.makeUpdateRequestV4(tableArray,
                                                              stateArray,
                                                              tableArray.length);
    streamerMap.isPostRequest = false;
  } else {
    // Build the request. For each table already in the database, include the
    // chunk data from the database
    var lines = tableData.split("\n");
    for (var i = 0; i < lines.length; i++) {
      var fields = lines[i].split(";");
      var name = fields[0];
      if (streamerMap.tableNames[name]) {
        streamerMap.requestPayload += lines[i] + "\n";
        delete streamerMap.tableNames[name];
      }
    }
    // For each requested table that didn't have chunk data in the database,
    // request it fresh
    for (let tableName in streamerMap.tableNames) {
      streamerMap.requestPayload += tableName + ";\n";
    }

    streamerMap.isPostRequest = true;
  }

  log("update request: " + JSON.stringify(streamerMap, undefined, 2) + "\n");

  // Don't send an empty request.
  if (streamerMap.requestPayload.length > 0) {
    this.makeUpdateRequestForEntry_(updateUrl, streamerMap.tableList,
                                    streamerMap.requestPayload,
                                    streamerMap.isPostRequest);
  } else {
    // We were disabled between kicking off getTables and now.
    log("Not sending empty request");
  }
};

PROT_ListManager.prototype.makeUpdateRequestForEntry_ = function(updateUrl,
                                                                 tableList,
                                                                 requestPayload,
                                                                 isPostRequest) {
  log("makeUpdateRequestForEntry_: requestPayload " + requestPayload +
      " update: " + updateUrl + " tablelist: " + tableList + "\n");
  var streamer = Cc["@mozilla.org/url-classifier/streamupdater;1"]
                 .getService(Ci.nsIUrlClassifierStreamUpdater);

  this.requestBackoffs_[updateUrl].noteRequest();

  if (!streamer.downloadUpdates(
        tableList,
        requestPayload,
        isPostRequest,
        updateUrl,
        BindToObject(this.updateSuccess_, this, tableList, updateUrl),
        BindToObject(this.updateError_, this, tableList, updateUrl),
        BindToObject(this.downloadError_, this, tableList, updateUrl))) {
    // Our alarm gets reset in one of the 3 callbacks.
    log("pending update, queued request until later");
  } else {
    let table = Object.keys(this.tablesData).find(key => {
      return this.tablesData[key].updateUrl === updateUrl;
    });
    let provider = this.tablesData[table].provider;
    Services.obs.notifyObservers(null, "safebrowsing-update-begin", provider);
  }
};

/**
 * Callback function if the update request succeeded.
 * @param waitForUpdate String The number of seconds that the client should
 *        wait before requesting again.
 */
PROT_ListManager.prototype.updateSuccess_ = function(tableList, updateUrl,
                                                     waitForUpdateSec) {
  log("update success for " + tableList + " from " + updateUrl + ": " +
      waitForUpdateSec + "\n");

  // The time unit below are all milliseconds if not specified.

  var delay = 0;
  if (waitForUpdateSec) {
    delay = parseInt(waitForUpdateSec, 10) * 1000;
  }
  // As long as the delay is something sane (5 min to 1 day), update
  // our delay time for requesting updates. We always use a non-repeating
  // timer since the delay is set differently at every callback.
  if (delay > maxDelayMs) {
    log("Ignoring delay from server (too long), waiting " +
        maxDelayMs / 60000 + "min");
    delay = maxDelayMs;
  } else if (delay < minDelayMs) {
    log("Ignoring delay from server (too short), waiting " +
        this.updateInterval / 60000 + "min");
    delay = this.updateInterval;
  } else {
    log("Waiting " + delay / 60000 + "min");
  }

  this.setUpdateCheckTimer(updateUrl, delay);

  // Let the backoff object know that we completed successfully.
  this.requestBackoffs_[updateUrl].noteServerResponse(200);

  // Set last update time for provider
  // Get the provider for these tables, check for consistency
  let tables = tableList.split(",");
  let provider = null;
  for (let table of tables) {
    let newProvider = this.tablesData[table].provider;
    if (provider) {
      if (newProvider !== provider) {
        log("Multiple tables for the same updateURL have a different provider?!");
      }
    } else {
      provider = newProvider;
    }
  }

  // Store the last update time (needed to know if the table is "fresh")
  // and the next update time (to know when to update next).
  let lastUpdatePref = "browser.safebrowsing.provider." + provider + ".lastupdatetime";
  let now = Date.now();
  log("Setting last update of " + provider + " to " + now);
  Services.prefs.setCharPref(lastUpdatePref, now.toString());

  let nextUpdatePref = "browser.safebrowsing.provider." + provider + ".nextupdatetime";
  let targetTime = now + delay;
  log("Setting next update of " + provider + " to " + targetTime
      + " (" + delay / 60000 + "min from now)");
  Services.prefs.setCharPref(nextUpdatePref, targetTime.toString());

  Services.obs.notifyObservers(null, "safebrowsing-update-finished", "success");
};

/**
 * Callback function if the update request succeeded.
 * @param result String The error code of the failure
 */
PROT_ListManager.prototype.updateError_ = function(table, updateUrl, result) {
  log("update error for " + table + " from " + updateUrl + ": " + result + "\n");
  // There was some trouble applying the updates. Don't try again for at least
  // updateInterval milliseconds.
  this.setUpdateCheckTimer(updateUrl, this.updateInterval);

  Services.obs.notifyObservers(null, "safebrowsing-update-finished",
                               "update error: " + result);
};

/**
 * Callback function when the download failed
 * @param status String http status or an empty string if connection refused.
 */
PROT_ListManager.prototype.downloadError_ = function(table, updateUrl, status) {
  log("download error for " + table + ": " + status + "\n");
  // If status is empty, then we assume that we got an NS_CONNECTION_REFUSED
  // error.  In this case, we treat this is a http 500 error.
  if (!status) {
    status = 500;
  }
  status = parseInt(status, 10);
  this.requestBackoffs_[updateUrl].noteServerResponse(status);
  var delay = this.updateInterval;
  if (this.requestBackoffs_[updateUrl].isErrorStatus(status)) {
    // Schedule an update for when our backoff is complete
    delay = this.requestBackoffs_[updateUrl].nextRequestDelay();
  } else {
    log("Got non error status for error callback?!");
  }

  this.setUpdateCheckTimer(updateUrl, delay);

  Services.obs.notifyObservers(null, "safebrowsing-update-finished",
                               "download error: " + status);
};

/**
 * Get back-off time for the given provider.
 * Return 0 if we are not in back-off mode.
 */
PROT_ListManager.prototype.getBackOffTime = function(provider) {
  let updateUrl = "";
  for (var table in this.tablesData) {
    if (this.tablesData[table].provider == provider) {
      updateUrl = this.tablesData[table].updateUrl;
      break;
    }
  }

  if (!updateUrl || !this.requestBackoffs_[updateUrl]) {
    return 0;
  }

  let delay = this.requestBackoffs_[updateUrl].nextRequestDelay();
  return delay == 0 ? 0 : Date.now() + delay;
};

PROT_ListManager.prototype.QueryInterface = function(iid) {
  if (iid.equals(Ci.nsISupports) ||
      iid.equals(Ci.nsIUrlListManager) ||
      iid.equals(Ci.nsIObserver) ||
      iid.equals(Ci.nsITimerCallback))
    return this;

  throw Components.results.NS_ERROR_NO_INTERFACE;
};

var modScope = this;
function Init() {
  // Pull the library in.
  var jslib = Cc["@mozilla.org/url-classifier/jslib;1"]
              .getService().wrappedJSObject;
  /* global BindToObject, RequestBackoffV4 */
  modScope.BindToObject = jslib.BindToObject;
  modScope.RequestBackoffV4 = jslib.RequestBackoffV4;

  // We only need to call Init once.
  modScope.Init = function() {};
}

function RegistrationData() {
}
RegistrationData.prototype = {
    classID: Components.ID("{ca168834-cc00-48f9-b83c-fd018e58cae3}"),
    _xpcom_factory: {
        createInstance(outer, iid) {
            if (outer != null)
                throw Components.results.NS_ERROR_NO_AGGREGATION;
            Init();
            return (new PROT_ListManager()).QueryInterface(iid);
        }
    },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([RegistrationData]);
