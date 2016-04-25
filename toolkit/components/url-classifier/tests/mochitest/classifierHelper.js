if (typeof(classifierHelper) == "undefined") {
  var classifierHelper = {};
}

const CLASSIFIER_COMMON_URL = SimpleTest.getTestFileURL("classifierCommon.js");
var classifierCommonScript = SpecialPowers.loadChromeScript(CLASSIFIER_COMMON_URL);

const ADD_CHUNKNUM = 524;
const SUB_CHUNKNUM = 523;
const HASHLEN = 32;

// addUrlToDB & removeUrlFromDB are asynchronous, queue the task to ensure
// the callback follow correct order.
classifierHelper._updates = [];

// Keep urls added to database, those urls should be automatically
// removed after test complete.
classifierHelper._updatesToCleanup = [];

// Pass { url: ..., db: ... } to add url to database,
// onsuccess/onerror will be called when update complete.
classifierHelper.addUrlToDB = function(updateData, onsuccess, onerror) {
  var testUpdate = "";
  for (var update of updateData) {
    var LISTNAME = update.db;
    var CHUNKDATA = update.url;
    var CHUNKLEN = CHUNKDATA.length;

    classifierHelper._updatesToCleanup.push(update);
    testUpdate +=
      "n:1000\n" +
      "i:" + LISTNAME + "\n" +
      "ad:1\n" +
      "a:" + ADD_CHUNKNUM + ":" + HASHLEN + ":" + CHUNKLEN + "\n" +
      CHUNKDATA;
  }

  classifierHelper._update(testUpdate, onsuccess, onerror);
}

// Pass { url: ..., db: ... } to remove url from database,
// onsuccess/onerror will be called when update complete.
classifierHelper.removeUrlFromDB = function(updateData, onsuccess, onerror) {
  var testUpdate = "";
  for (var update of updateData) {
    var LISTNAME = update.db;
    var CHUNKDATA = ADD_CHUNKNUM + ":" + update.url;
    var CHUNKLEN = CHUNKDATA.length;

    testUpdate +=
      "n:1000\n" +
      "i:" + LISTNAME + "\n" +
      "s:" + SUB_CHUNKNUM + ":" + HASHLEN + ":" + CHUNKLEN + "\n" +
      CHUNKDATA;
  }

  classifierHelper._updatesToCleanup =
    classifierHelper._updatesToCleanup.filter((v) => {
      return updateData.indexOf(v) == -1;
    });

  classifierHelper._update(testUpdate, onsuccess, onerror);
};

classifierHelper._update = function(testUpdate, onsuccess, onerror) {
  // Queue the task if there is still an on-going update
  classifierHelper._updates.push({"data": testUpdate,
                                  "onsuccess": onsuccess,
                                  "onerror": onerror});
  if (classifierHelper._updates.length != 1) {
    return;
  }

  classifierCommonScript.sendAsyncMessage("doUpdate", { testUpdate });
};

classifierHelper._updateSuccess = function() {
  var update = classifierHelper._updates.shift();
  update.onsuccess();

  if (classifierHelper._updates.length) {
    var testUpdate = classifierHelper._updates[0].data;
    classifierCommonScript.sendAsyncMessage("doUpdate", { testUpdate });
  }
};

classifierHelper._updateError = function(errorCode) {
  var update = classifierHelper._updates.shift();
  update.onerror(errorCode);

  if (classifierHelper._updates.length) {
    var testUpdate = classifierHelper._updates[0].data;
    classifierCommonScript.sendAsyncMessage("doUpdate", { testUpdate });
  }
};

classifierHelper._setup = function() {
  classifierCommonScript.addMessageListener("updateSuccess", classifierHelper._updateSuccess);
  classifierCommonScript.addMessageListener("updateError", classifierHelper._updateError);

  // cleanup will be called at end of each testcase to remove all the urls added to database.
  SimpleTest.registerCleanupFunction(classifierHelper._cleanup);
};

classifierHelper._cleanup = function() {
  if (!classifierHelper._updatesToCleanup) {
    return Promise.resolve();
  }

  return new Promise(function(resolve, reject) {
    classifierHelper.removeUrlFromDB(classifierHelper._updatesToCleanup, resolve, reject);
  });
};

classifierHelper._setup();
