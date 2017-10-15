/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Created from toolkit/components/url-classifier/tests/mochitest/classifierHelper.js
// Unfortunately, browser tests cannot load that script as it is too reliant on
// being loaded in the content process.


let dbService = Cc["@mozilla.org/url-classifier/dbservice;1"]
                .getService(Ci.nsIUrlClassifierDBService);

if (typeof(classifierHelper) == "undefined") {
  var classifierHelper = {};
}

const HASHLEN = 32;

const PREFS = {
  PROVIDER_LISTS: "browser.safebrowsing.provider.mozilla.lists",
  DISALLOW_COMPLETIONS: "urlclassifier.disallow_completions",
  PROVIDER_GETHASHURL: "browser.safebrowsing.provider.mozilla.gethashURL"
};

classifierHelper._curAddChunkNum = 1;

// Keep urls added to database, those urls should be automatically
// removed after test complete.
classifierHelper._updatesToCleanup = [];

// This function returns a Promise resolved when SafeBrowsing.jsm is initialized.
// SafeBrowsing.jsm is initialized after mozEntries are added. Add observer
// to receive "finished" event. For the case when this function is called
// after the event had already been notified, we lookup entries to see if
// they are already added to database.
classifierHelper.waitForInit = function() {
  let observerService = Cc["@mozilla.org/observer-service;1"]
                        .getService(Ci.nsIObserverService);
  let secMan = Cc["@mozilla.org/scriptsecuritymanager;1"]
               .getService(Ci.nsIScriptSecurityManager);
  let iosvc = Cc["@mozilla.org/network/io-service;1"]
              .getService(Ci.nsIIOService);

  // This url must sync with the table, url in SafeBrowsing.jsm addMozEntries
  const table = "test-phish-simple";
  const url = "http://itisatrap.org/firefox/its-a-trap.html";
  let principal = secMan.createCodebasePrincipal(
    iosvc.newURI(url), {});

  return new Promise(function(resolve, reject) {
    observerService.addObserver(function() {
      resolve();
    }, "mozentries-update-finished");

    let listener = {
      QueryInterface(iid) {
        if (iid.equals(Ci.nsISupports) ||
          iid.equals(Ci.nsIUrlClassifierUpdateObserver))
          return this;
        throw Cr.NS_ERROR_NO_INTERFACE;
      },

      handleEvent(value) {
        if (value === table) {
          resolve();
        }
      },
    };
    dbService.lookup(principal, table, listener);
  });
};

// This function is used to allow completion for specific "list",
// some lists like "test-malware-simple" is default disabled to ask for complete.
// "list" is the db we would like to allow it
// "url" is the completion server
classifierHelper.allowCompletion = function(lists, url) {
  for (let list of lists) {
    // Add test db to provider
    let pref = Services.prefs.getCharPref(PREFS.PROVIDER_LISTS);
    pref += "," + list;
    Services.prefs.setCharPref(PREFS.PROVIDER_LISTS, pref);

    // Rename test db so we will not disallow it from completions
    pref = Services.prefs.getCharPref(PREFS.DISALLOW_COMPLETIONS);
    pref = pref.replace(list, list + "-backup");
    Services.prefs.setCharPref(PREFS.DISALLOW_COMPLETIONS, pref);
  }

  // Set get hash url
  Services.prefs.setCharPref(PREFS.PROVIDER_GETHASHURL, url);
};

// Pass { url: ..., db: ... } to add url to database,
// Returns a Promise.
classifierHelper.addUrlToDB = function(updateData) {
  let testUpdate = "";
  for (let update of updateData) {
    let LISTNAME = update.db;
    let CHUNKDATA = update.url;
    let CHUNKLEN = CHUNKDATA.length;
    let HASHLEN = update.len ? update.len : 32;

    update.addChunk = classifierHelper._curAddChunkNum;
    classifierHelper._curAddChunkNum += 1;

    classifierHelper._updatesToCleanup.push(update);
    testUpdate +=
      "n:1000\n" +
      "i:" + LISTNAME + "\n" +
      "ad:1\n" +
      "a:" + update.addChunk + ":" + HASHLEN + ":" + CHUNKLEN + "\n" +
      CHUNKDATA;
  }

  return classifierHelper._update(testUpdate);
};

// This API is used to expire all add/sub chunks we have updated
// by using addUrlToDB.
// Returns a Promise.
classifierHelper.resetDatabase = function() {
  var testUpdate = "";
  for (var update of classifierHelper._updatesToCleanup) {
    testUpdate +=
      "n:1000\n" +
      "i:" + update.db + "\n" +
      "ad:" + update.addChunk + "\n";
  }

  return classifierHelper._update(testUpdate);
};

classifierHelper.reloadDatabase = function() {
  dbService.reloadDatabase();
};

classifierHelper._update = function(update) {
  return (async function() {
    // beginUpdate may fail if there's an existing update in progress
    // retry until success or testcase timeout.
    let success = false;
    while (!success) {
      try {
        await new Promise((resolve, reject) => {
          let listener = {
            QueryInterface(iid) {
              if (iid.equals(Ci.nsISupports) ||
                  iid.equals(Ci.nsIUrlClassifierUpdateObserver))
                return this;

              throw Cr.NS_ERROR_NO_INTERFACE;
            },
            updateUrlRequested(url) { },
            streamFinished(status) { },
            updateError(errorCode) {
              reject(errorCode);
            },
            updateSuccess(requestedTimeout) {
              resolve();
            }
          };
          dbService.beginUpdate(listener, "", "");
          dbService.beginStream("", "");
          dbService.updateStream(update);
          dbService.finishStream();
          dbService.finishUpdate();
        });
        success = true;
      } catch (e) {
        // Wait 1 second before trying again.
        await new Promise(resolve => setTimeout(resolve, 1000));
      }
    }
  })();
};

classifierHelper._cleanup = function() {
  // Clean all the preferences that may have been touched by classifierHelper
  for (var pref in PREFS) {
    Services.prefs.clearUserPref(pref);
  }

  if (!classifierHelper._updatesToCleanup) {
    return Promise.resolve();
  }

  return classifierHelper.resetDatabase();
};
// Cleanup will be called at end of each testcase to remove all the urls added to database.
registerCleanupFunction(classifierHelper._cleanup);
