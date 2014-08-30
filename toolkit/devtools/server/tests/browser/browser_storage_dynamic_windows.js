const Cu = Components.utils;
Cu.import("resource://gre/modules/Services.jsm");
let tempScope = {};
Cu.import("resource://gre/modules/devtools/dbg-client.jsm", tempScope);
Cu.import("resource://gre/modules/devtools/dbg-server.jsm", tempScope);
Cu.import("resource://gre/modules/Promise.jsm", tempScope);
let {DebuggerServer, DebuggerClient, Promise} = tempScope;
tempScope = null;

const {StorageFront} = require("devtools/server/actors/storage");
let gFront, gWindow;

const beforeReload = {
  cookies: {
    "test1.example.org": ["c1", "cs2", "c3", "uc1"],
    "sectest1.example.org": ["uc1", "cs2"]
  },
  localStorage: {
    "http://test1.example.org": ["ls1", "ls2"],
    "http://sectest1.example.org": ["iframe-u-ls1"]
  },
  sessionStorage: {
    "http://test1.example.org": ["ss1"],
    "http://sectest1.example.org": ["iframe-u-ss1", "iframe-u-ss2"]
  },
  indexedDB: {
    "http://test1.example.org": [
      JSON.stringify(["idb1", "obj1"]),
      JSON.stringify(["idb1", "obj2"]),
      JSON.stringify(["idb2", "obj3"]),
    ],
    "http://sectest1.example.org": []
  }
};

function finishTests(client) {
  // Cleanup so that indexed db created from this test do not interfere next ones

  /**
   * This method iterates over iframes in a window and clears the indexed db
   * created by this test.
   */
  let clearIDB = (w, i, c) => {
    if (w[i] && w[i].clear) {
      w[i].clearIterator = w[i].clear(() => clearIDB(w, i + 1, c));
      w[i].clearIterator.next();
    }
    else if (w[i] && w[i + 1]) {
      clearIDB(w, i + 1, c);
    }
    else {
      c();
    }
  };

  let closeConnection = () => {
    // Forcing GC/CC to get rid of docshells and windows created by this test.
    forceCollections();
    client.close(() => {
      forceCollections();
      DebuggerServer.destroy();
      forceCollections();
      gFront = gWindow = DebuggerClient = DebuggerServer = null;
      finish();
    });
  }
  gWindow.clearIterator = gWindow.clear(() => {
    clearIDB(gWindow, 0, closeConnection);
  });
  gWindow.clearIterator.next();
}

function testStores(data, client) {
  testWindowsBeforeReload(data);
  testReload().then(() =>
  testAddIframe()).then(() =>
  testRemoveIframe()).then(() =>
  finishTests(client));
}

function testWindowsBeforeReload(data) {
  for (let storageType in beforeReload) {
    ok(data[storageType], storageType + " storage actor is present");
    is(Object.keys(data[storageType].hosts).length,
       Object.keys(beforeReload[storageType]).length,
       "Number of hosts for " + storageType + "match");
    for (let host in beforeReload[storageType]) {
      ok(data[storageType].hosts[host], "Host " + host + " is present");
    }
  }
}

function markOutMatched(toBeEmptied, data, deleted) {
  if (!Object.keys(toBeEmptied).length) {
    info("Object empty")
    return;
  }
  ok(Object.keys(data).length,
     "Atleast some storage types should be present in deleted");
  for (let storageType in toBeEmptied) {
    if (!data[storageType]) {
      continue;
    }
    info("Testing for " + storageType);
    for (let host in data[storageType]) {
      ok(toBeEmptied[storageType][host], "Host " + host + " found");
      if (!deleted) {
        for (let item of data[storageType][host]) {
          let index = toBeEmptied[storageType][host].indexOf(item);
          ok(index > -1, "Item found - " + item);
          if (index > -1) {
            toBeEmptied[storageType][host].splice(index, 1);
          }
        }
        if (!toBeEmptied[storageType][host].length) {
          delete toBeEmptied[storageType][host];
        }
      }
      else {
        delete toBeEmptied[storageType][host];
      }
    }
    if (!Object.keys(toBeEmptied[storageType]).length) {
      delete toBeEmptied[storageType];
    }
  }
}

function testReload() {
  info("Testing if reload works properly");

  let shouldBeEmptyFirst = Cu.cloneInto(beforeReload,  {});
  let shouldBeEmptyLast = Cu.cloneInto(beforeReload,  {});
  let reloaded = Promise.defer();

  let onStoresUpdate = data => {
    info("in stores update of testReload");
    // This might be second time stores update is happening, in which case,
    // data.deleted will be null.
    // OR.. This might be the first time on a super slow machine where both
    // data.deleted and data.added is missing in the first update.
    if (data.deleted) {
      markOutMatched(shouldBeEmptyFirst, data.deleted, true);
    }

    if (!Object.keys(shouldBeEmptyFirst).length) {
      info("shouldBeEmptyFirst is empty now");
    }

    // stores-update call might not have data.added for the first time on slow
    // machines, in which case, data.added will be null
    if (data.added) {
      markOutMatched(shouldBeEmptyLast, data.added);
    }

    if (!Object.keys(shouldBeEmptyLast).length) {
      info("Everything to be received is received.");
      endTestReloaded();
    }
  };

  let endTestReloaded = () => {
    gFront.off("stores-update", onStoresUpdate);
    reloaded.resolve();
  };

  gFront.on("stores-update", onStoresUpdate);

  content.location.reload();
  return reloaded.promise;
}

function testAddIframe() {
  info("Testing if new iframe addition works properly");
  let reloaded = Promise.defer();

  let shouldBeEmpty = {
    localStorage: {
      "https://sectest1.example.org": ["iframe-s-ls1"]
    },
    sessionStorage: {
      "https://sectest1.example.org": ["iframe-s-ss1"]
    },
    cookies: {
      "sectest1.example.org": ["sc1"]
    },
    indexedDB: {
      // empty because indexed db creation happens after the page load, so at
      // the time of window-ready, there was no indexed db present.
      "https://sectest1.example.org": []
    }
  };

  let onStoresUpdate = data => {
    info("checking if the hosts list is correct for this iframe addition");

    markOutMatched(shouldBeEmpty, data.added);

    ok(!data.changed || !data.changed.cookies ||
       !data.changed.cookies["https://sectest1.example.org"],
       "Nothing got changed for cookies");
    ok(!data.changed || !data.changed.localStorage ||
       !data.changed.localStorage["https://sectest1.example.org"],
       "Nothing got changed for local storage");
    ok(!data.changed || !data.changed.sessionStorage ||
       !data.changed.sessionStorage["https://sectest1.example.org"],
       "Nothing got changed for session storage");
    ok(!data.changed || !data.changed.indexedDB ||
       !data.changed.indexedDB["https://sectest1.example.org"],
       "Nothing got changed for indexed db");

    ok(!data.deleted || !data.deleted.cookies ||
       !data.deleted.cookies["https://sectest1.example.org"],
       "Nothing got deleted for cookies");
    ok(!data.deleted || !data.deleted.localStorage ||
       !data.deleted.localStorage["https://sectest1.example.org"],
       "Nothing got deleted for local storage");
    ok(!data.deleted || !data.deleted.sessionStorage ||
       !data.deleted.sessionStorage["https://sectest1.example.org"],
       "Nothing got deleted for session storage");
    ok(!data.deleted || !data.deleted.indexedDB ||
       !data.deleted.indexedDB["https://sectest1.example.org"],
       "Nothing got deleted for indexed db");

    if (!Object.keys(shouldBeEmpty).length) {
      info("Everything to be received is received.");
      endTestReloaded();
    }
  };

  let endTestReloaded = () => {
    gFront.off("stores-update", onStoresUpdate);
    reloaded.resolve();
  };

  gFront.on("stores-update", onStoresUpdate);

  let iframe = content.document.createElement("iframe");
  iframe.src = ALT_DOMAIN_SECURED + "storage-secured-iframe.html";
  content.document.querySelector("body").appendChild(iframe);
  return reloaded.promise;
}

function testRemoveIframe() {
  info("Testing if iframe removal works properly");
  let reloaded = Promise.defer();

  let shouldBeEmpty = {
    localStorage: {
      "http://sectest1.example.org": []
    },
    sessionStorage: {
      "http://sectest1.example.org": []
    }
  };

  let onStoresUpdate = data => {
    info("checking if the hosts list is correct for this iframe deletion");

    markOutMatched(shouldBeEmpty, data.deleted, true);

    ok(!data.deleted.cookies || !data.deleted.cookies["sectest1.example.org"],
       "Nothing got deleted for Cookies as the same hostname is still present");

    ok(!data.changed || !data.changed.cookies ||
       !data.changed.cookies["http://sectest1.example.org"],
       "Nothing got changed for cookies");
    ok(!data.changed || !data.changed.localStorage ||
       !data.changed.localStorage["http://sectest1.example.org"],
       "Nothing got changed for local storage");
    ok(!data.changed || !data.changed.sessionStorage ||
       !data.changed.sessionStorage["http://sectest1.example.org"],
       "Nothing got changed for session storage");

    ok(!data.added || !data.added.cookies ||
       !data.added.cookies["http://sectest1.example.org"],
       "Nothing got added for cookies");
    ok(!data.added || !data.added.localStorage ||
       !data.added.localStorage["http://sectest1.example.org"],
       "Nothing got added for local storage");
    ok(!data.added || !data.added.sessionStorage ||
       !data.added.sessionStorage["http://sectest1.example.org"],
       "Nothing got added for session storage");

    if (!Object.keys(shouldBeEmpty).length) {
      info("Everything to be received is received.");
      endTestReloaded();
    }
  };

  let endTestReloaded = () => {
    gFront.off("stores-update", onStoresUpdate);
    reloaded.resolve();
  };

  gFront.on("stores-update", onStoresUpdate);

  for (let iframe of content.document.querySelectorAll("iframe")) {
    if (iframe.src.startsWith("http:")) {
      iframe.remove();
      break;
    }
  }
  return reloaded.promise;
}

function test() {
  waitForExplicitFinish();
  addTab(MAIN_DOMAIN + "storage-dynamic-windows.html", function(doc) {
    try {
      // Sometimes debugger server does not get destroyed correctly by previous
      // tests.
      DebuggerServer.destroy();
    } catch (ex) { }
    DebuggerServer.init(function () { return true; });
    DebuggerServer.addBrowserActors();

    let createConnection = () => {
      let client = new DebuggerClient(DebuggerServer.connectPipe());
      client.connect(function onConnect() {
        client.listTabs(function onListTabs(aResponse) {
          let form = aResponse.tabs[aResponse.selected];
          gFront = StorageFront(client, form);

          gFront.listStores().then(data => testStores(data, client));
        });
      });
    };

    /**
     * This method iterates over iframes in a window and setups the indexed db
     * required for this test.
     */
    let setupIDBInFrames = (w, i, c) => {
      if (w[i] && w[i].idbGenerator) {
        w[i].setupIDB = w[i].idbGenerator(() => setupIDBInFrames(w, i + 1, c));
        w[i].setupIDB.next();
      }
      else if (w[i] && w[i + 1]) {
        setupIDBInFrames(w, i + 1, c);
      }
      else {
        c();
      }
    };
    // Setup the indexed db in main window.
    gWindow = doc.defaultView.wrappedJSObject;
    gWindow.setupIDB = gWindow.idbGenerator(() => {
      setupIDBInFrames(gWindow, 0, createConnection);
    });
    gWindow.setupIDB.next();
  });
}
