if (typeof classifierHelper == "undefined") {
  var classifierHelper = {};
}

const CLASSIFIER_COMMON_URL = SimpleTest.getTestFileURL("classifierCommon.js");
var gScript = SpecialPowers.loadChromeScript(CLASSIFIER_COMMON_URL);

const PREFS = {
  PROVIDER_LISTS: "browser.safebrowsing.provider.mozilla.lists",
  DISALLOW_COMPLETIONS: "urlclassifier.disallow_completions",
  PROVIDER_GETHASHURL: "browser.safebrowsing.provider.mozilla.gethashURL",
};

classifierHelper._curAddChunkNum = 1;

// addUrlToDB is asynchronous, queue the task to ensure
// the callback follow correct order.
classifierHelper._updates = [];

// Keep urls added to database, those urls should be automatically
// removed after test complete.
classifierHelper._updatesToCleanup = [];

classifierHelper._initsCB = [];

// This function return a Promise, promise is resolved when SafeBrowsing.jsm
// is initialized.
classifierHelper.waitForInit = function() {
  return new Promise(function(resolve, reject) {
    classifierHelper._initsCB.push(resolve);
    gScript.sendAsyncMessage("waitForInit");
  });
};

// This function is used to allow completion for specific "list",
// some lists like "test-malware-simple" is default disabled to ask for complete.
// "list" is the db we would like to allow it
// "url" is the completion server
classifierHelper.allowCompletion = async function(lists, url) {
  for (var list of lists) {
    // Add test db to provider
    var pref = await SpecialPowers.getParentCharPref(PREFS.PROVIDER_LISTS);
    pref += "," + list;
    await SpecialPowers.setCharPref(PREFS.PROVIDER_LISTS, pref);

    // Rename test db so we will not disallow it from completions
    pref = await SpecialPowers.getParentCharPref(PREFS.DISALLOW_COMPLETIONS);
    pref = pref.replace(list, list + "-backup");
    await SpecialPowers.setCharPref(PREFS.DISALLOW_COMPLETIONS, pref);
  }

  // Set get hash url
  await SpecialPowers.setCharPref(PREFS.PROVIDER_GETHASHURL, url);
};

// Pass { url: ..., db: ... } to add url to database,
// onsuccess/onerror will be called when update complete.
classifierHelper.addUrlToDB = function(updateData) {
  return new Promise(function(resolve, reject) {
    var testUpdate = "";
    for (var update of updateData) {
      var LISTNAME = update.db;
      var CHUNKDATA = update.url;
      var CHUNKLEN = CHUNKDATA.length;
      var HASHLEN = update.len ? update.len : 32;

      update.addChunk = classifierHelper._curAddChunkNum;
      classifierHelper._curAddChunkNum += 1;

      classifierHelper._updatesToCleanup.push(update);
      testUpdate +=
        "n:1000\n" +
        "i:" +
        LISTNAME +
        "\n" +
        "ad:1\n" +
        "a:" +
        update.addChunk +
        ":" +
        HASHLEN +
        ":" +
        CHUNKLEN +
        "\n" +
        CHUNKDATA;
    }

    classifierHelper._update(testUpdate, resolve, reject);
  });
};

// This API is used to expire all add/sub chunks we have updated
// by using addUrlToDB.
classifierHelper.resetDatabase = function() {
  function removeDatabase() {
    return new Promise(function(resolve, reject) {
      var testUpdate = "";
      for (var update of classifierHelper._updatesToCleanup) {
        testUpdate +=
          "n:1000\ni:" + update.db + "\nad:" + update.addChunk + "\n";
      }

      classifierHelper._update(testUpdate, resolve, reject);
    });
  }

  // Remove and then reload will ensure both database and cache will
  // be cleared.
  return Promise.resolve()
    .then(removeDatabase)
    .then(classifierHelper.reloadDatabase);
};

classifierHelper.reloadDatabase = function() {
  return new Promise(function(resolve, reject) {
    gScript.addMessageListener("reloadSuccess", function handler() {
      gScript.removeMessageListener("reloadSuccess", handler);
      resolve();
    });

    gScript.sendAsyncMessage("doReload");
  });
};

classifierHelper._update = function(testUpdate, onsuccess, onerror) {
  // Queue the task if there is still an on-going update
  classifierHelper._updates.push({
    data: testUpdate,
    onsuccess,
    onerror,
  });
  if (classifierHelper._updates.length != 1) {
    return;
  }

  gScript.sendAsyncMessage("doUpdate", { testUpdate });
};

classifierHelper._updateSuccess = function() {
  var update = classifierHelper._updates.shift();
  update.onsuccess();

  if (classifierHelper._updates.length) {
    var testUpdate = classifierHelper._updates[0].data;
    gScript.sendAsyncMessage("doUpdate", { testUpdate });
  }
};

classifierHelper._updateError = function(errorCode) {
  var update = classifierHelper._updates.shift();
  update.onerror(errorCode);

  if (classifierHelper._updates.length) {
    var testUpdate = classifierHelper._updates[0].data;
    gScript.sendAsyncMessage("doUpdate", { testUpdate });
  }
};

classifierHelper._inited = function() {
  classifierHelper._initsCB.forEach(function(cb) {
    cb();
  });
  classifierHelper._initsCB = [];
};

classifierHelper._setup = function() {
  gScript.addMessageListener("updateSuccess", classifierHelper._updateSuccess);
  gScript.addMessageListener("updateError", classifierHelper._updateError);
  gScript.addMessageListener("safeBrowsingInited", classifierHelper._inited);

  // cleanup will be called at end of each testcase to remove all the urls added to database.
  SimpleTest.registerCleanupFunction(classifierHelper._cleanup);
};

classifierHelper._cleanup = function() {
  // clean all the preferences may touch by helper
  for (var pref in PREFS) {
    SpecialPowers.clearUserPref(pref);
  }

  if (!classifierHelper._updatesToCleanup) {
    return Promise.resolve();
  }

  return classifierHelper.resetDatabase();
};

classifierHelper._setup();
