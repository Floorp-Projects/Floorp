const Cu = Components.utils;
Cu.import("resource://gre/modules/Services.jsm");
let tempScope = {};
Cu.import("resource://gre/modules/devtools/dbg-client.jsm", tempScope);
Cu.import("resource://gre/modules/devtools/dbg-server.jsm", tempScope);
Cu.import("resource://gre/modules/Promise.jsm", tempScope);
let {DebuggerServer, DebuggerClient, Promise} = tempScope;
tempScope = null;

const {StorageFront} = require("devtools/server/actors/storage");
let gTests;
let gExpected;
let index = 0;

const beforeReload = {
  cookies: ["test1.example.org", "sectest1.example.org"],
  localStorage: ["http://test1.example.org", "http://sectest1.example.org"],
  sessionStorage: ["http://test1.example.org", "http://sectest1.example.org"],
};

// Always log packets when running tests.
Services.prefs.setBoolPref("devtools.debugger.log", true);
registerCleanupFunction(function() {
  Services.prefs.clearUserPref("devtools.debugger.log");
});

function finishTests(client) {
  // Forcing GC/CC to get rid of docshells and windows created by this test.
  forceCollections();
  client.close(() => {
    forceCollections();
    DebuggerServer.destroy();
    forceCollections();
    DebuggerClient = DebuggerServer = gTests = null;
    finish();
  });
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
    if (!Object.keys(toBeEmptied[storageType]).length) {
      delete toBeEmptied[storageType];
    }
  }
}

function onStoresCleared(data) {
  if (data.sessionStorage || data.localStorage) {
    let hosts = data.sessionStorage || data.localStorage;
    info("Stores cleared required for session storage");
    is(hosts.length, 1, "number of hosts is 1");
    is(hosts[0], "http://test1.example.org",
       "host matches for " + Object.keys(data)[0]);
    gTests.next();
  }
  else {
    ok(false, "Stores cleared should only be for local and sesion storage");
  }

}

function onStoresUpdate({added, changed, deleted}) {
  info("inside stores update for index " + index);

  // Here, added, changed and deleted might be null even if they are required as
  // per gExpected. This is fine as they might come in the next stores-update
  // call or have already come in the previous one.
  if (added) {
    info("matching added object for index " + index);
    markOutMatched(gExpected.added, added);
  }
  if (changed) {
    info("matching changed object for index " + index);
    markOutMatched(gExpected.changed, changed);
  }
  if (deleted) {
    info("matching deleted object for index " + index);
    markOutMatched(gExpected.deleted, deleted);
  }

  if ((!gExpected.added || !Object.keys(gExpected.added).length) &&
      (!gExpected.changed || !Object.keys(gExpected.changed).length) &&
      (!gExpected.deleted || !Object.keys(gExpected.deleted).length)) {
    info("Everything expected has been received for index " + index);
    index++;
    gTests.next();
  }
  else {
    info("Still some updates pending for index " + index);
  }
}

function* UpdateTests(front, win, client) {
  front.on("stores-update", onStoresUpdate);

  // index 0
  gExpected = {
    added: {
      cookies: {
        "test1.example.org": ["c1", "c2"]
      },
      localStorage: {
        "http://test1.example.org": ["l1"]
      }
    }
  };
  win.addCookie("c1", "foobar1");
  win.addCookie("c2", "foobar2");
  win.localStorage.setItem("l1", "foobar1");
  yield undefined;

  // index 1
  gExpected = {
    changed: {
      cookies: {
        "test1.example.org": ["c1"]
      }
    },
    added: {
      localStorage: {
        "http://test1.example.org": ["l2"]
      }
    }
  };
  win.addCookie("c1", "new_foobar1");
  win.localStorage.setItem("l2", "foobar2");
  yield undefined;

  // index 2
  gExpected = {
    deleted: {
      cookies: {
        "test1.example.org": ["c2"]
      },
      localStorage: {
        "http://test1.example.org": ["l1"]
      }
    },
    added: {
      localStorage: {
        "http://test1.example.org": ["l3"]
      }
    }
  };
  win.removeCookie("c2");
  win.localStorage.removeItem("l1");
  win.localStorage.setItem("l3", "foobar3");
  yield undefined;

  // index 3
  gExpected = {
    added: {
      cookies: {
        "test1.example.org": ["c3"]
      },
      sessionStorage: {
        "http://test1.example.org": ["s1", "s2"]
      }
    },
    changed: {
      localStorage: {
        "http://test1.example.org": ["l3"]
      }
    },
    deleted: {
      cookies: {
        "test1.example.org": ["c1"]
      },
      localStorage: {
        "http://test1.example.org": ["l2"]
      }
    }
  };
  win.removeCookie("c1");
  win.addCookie("c3", "foobar3");
  win.localStorage.removeItem("l2");
  win.sessionStorage.setItem("s1", "foobar1");
  win.sessionStorage.setItem("s2", "foobar2");
  win.localStorage.setItem("l3", "new_foobar3");
  yield undefined;

  // index 4
  gExpected = {
    deleted: {
      sessionStorage: {
        "http://test1.example.org": ["s1"]
      }
    }
  };
  win.sessionStorage.removeItem("s1");
  yield undefined;

  // index 5
  gExpected = {
    deleted: {
      cookies: {
        "test1.example.org": ["c3"]
      }
    }
  };
  front.on("stores-cleared", onStoresCleared);
  win.clear();
  yield undefined;
  // Another 2 more yield undefined s so as to wait for the "stores-cleared" to
  // fire. One for Local Storage and other for Session Storage
  yield undefined;
  yield undefined;

  front.off("stores-cleared", onStoresCleared);
  front.off("stores-update", onStoresUpdate);
  finishTests(client);
}


function test() {
  waitForExplicitFinish();
  addTab(MAIN_DOMAIN + "storage-updates.html", function(doc) {
    try {
      // Sometimes debugger server does not get destroyed correctly by previous
      // tests.
      DebuggerServer.destroy();
    } catch (ex) { }
    DebuggerServer.init(function () { return true; });
    DebuggerServer.addBrowserActors();

    let client = new DebuggerClient(DebuggerServer.connectPipe());
    client.connect(function onConnect() {
      client.listTabs(function onListTabs(aResponse) {
        let form = aResponse.tabs[aResponse.selected];
        let front = StorageFront(client, form);
        gTests = UpdateTests(front, doc.defaultView.wrappedJSObject,
                             client);
        // Make an initial call to initialize the actor
        front.listStores().then(() => gTests.next());
      });
    });
  })
}
