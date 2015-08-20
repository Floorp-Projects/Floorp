/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {StorageFront} = require("devtools/server/actors/storage");
const beforeReload = {
  cookies: ["test1.example.org", "sectest1.example.org"],
  localStorage: ["http://test1.example.org", "http://sectest1.example.org"],
  sessionStorage: ["http://test1.example.org", "http://sectest1.example.org"],
};

const TESTS = [
  // index 0
  {
    action: function(win) {
      info('win.addCookie("c1", "foobar1")');
      win.addCookie("c1", "foobar1");

      info('win.addCookie("c2", "foobar2")');
      win.addCookie("c2", "foobar2");

      info('win.localStorage.setItem("l1", "foobar1")');
      win.localStorage.setItem("l1", "foobar1");
    },
    expected: {
      added: {
        cookies: {
          "test1.example.org": ["c1", "c2"]
        },
        localStorage: {
          "http://test1.example.org": ["l1"]
        }
      }
    }
  },

  // index 1
  {
    action: function(win) {
      info('win.addCookie("c1", "new_foobar1")');
      win.addCookie("c1", "new_foobar1");

      info('win.localStorage.setItem("l2", "foobar2")');
      win.localStorage.setItem("l2", "foobar2");
    },
    expected: {
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
    }
  },

  // index 2
  {
    action: function(win) {
      info('win.removeCookie("c2")');
      win.removeCookie("c2");

      info('win.localStorage.removeItem("l1")');
      win.localStorage.removeItem("l1");

      info('win.localStorage.setItem("l3", "foobar3")');
      win.localStorage.setItem("l3", "foobar3");
    },
    expected: {
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
    }
  },

  // index 3
  {
    action: function(win) {
      info('win.removeCookie("c1")');
      win.removeCookie("c1");

      info('win.addCookie("c3", "foobar3")');
      win.addCookie("c3", "foobar3");

      info('win.localStorage.removeItem("l2")');
      win.localStorage.removeItem("l2");

      info('win.sessionStorage.setItem("s1", "foobar1")');
      win.sessionStorage.setItem("s1", "foobar1");

      info('win.sessionStorage.setItem("s2", "foobar2")');
      win.sessionStorage.setItem("s2", "foobar2");

      info('win.localStorage.setItem("l3", "new_foobar3")');
      win.localStorage.setItem("l3", "new_foobar3");
    },
    expected: {
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
    }
  },

  // index 4
  {
    action: function(win) {
      info('win.sessionStorage.removeItem("s1")');
      win.sessionStorage.removeItem("s1");
    },
    expected: {
      deleted: {
        sessionStorage: {
          "http://test1.example.org": ["s1"]
        }
      }
    }
  },

  // index 5
  {
    action: function(win) {
      info("win.clearCookies()");
      win.clearCookies();
    },
    expected: {
      deleted: {
        cookies: {
          "test1.example.org": ["c3"]
        }
      }
    }
  }
];

function markOutMatched(toBeEmptied, data) {
  if (!Object.keys(toBeEmptied).length) {
    info("Object empty");
    return;
  }
  ok(Object.keys(data).length, "At least one storage type should be present");

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

function onStoresUpdate(expected, {added, changed, deleted}, index) {
  info("inside stores update for index " + index);

  // Here, added, changed and deleted might be null even if they are required as
  // per expected. This is fine as they might come in the next stores-update
  // call or have already come in the previous one.
  if (added) {
    info("matching added object for index " + index);
    markOutMatched(expected.added, added);
  }
  if (changed) {
    info("matching changed object for index " + index);
    markOutMatched(expected.changed, changed);
  }
  if (deleted) {
    info("matching deleted object for index " + index);
    markOutMatched(expected.deleted, deleted);
  }

  if ((!expected.added || !Object.keys(expected.added).length) &&
      (!expected.changed || !Object.keys(expected.changed).length) &&
      (!expected.deleted || !Object.keys(expected.deleted).length)) {
    info("Everything expected has been received for index " + index);
  } else {
    info("Still some updates pending for index " + index);
  }
}

function runTest({action, expected}, front, win, index) {
  return new Promise(resolve => {
    front.once("stores-update", function(addedChangedDeleted) {
      onStoresUpdate(expected, addedChangedDeleted, index);
      resolve();
    });

    info("Running test at index " + index);
    action(win);
  });
}

function* testClearLocalAndSessionStores(front, win) {
  return new Promise(resolve => {
    // We need to wait until we have received stores-cleared for both local and
    // session storage.
    let localStorage = false;
    let sessionStorage = false;

    front.on("stores-cleared", function onStoresCleared(data) {
      storesCleared(data);

      if (data.localStorage) {
        localStorage = true;
      }
      if (data.sessionStorage) {
        sessionStorage = true;
      }
      if (localStorage && sessionStorage) {
        front.off("stores-cleared", onStoresCleared);
        resolve();
      }
    });

    win.clearLocalAndSessionStores();
  });
}

function storesCleared(data) {
  if (data.sessionStorage || data.localStorage) {
    let hosts = data.sessionStorage || data.localStorage;
    info("Stores cleared required for session storage");
    is(hosts.length, 1, "number of hosts is 1");
    is(hosts[0], "http://test1.example.org",
       "host matches for " + Object.keys(data)[0]);
  } else {
    ok(false, "Stores cleared should only be for local and session storage");
  }
}

function* finishTests(client) {
  yield client.close();
  DebuggerServer.destroy();
  finish();
}

add_task(function*() {
  let doc = yield addTab(MAIN_DOMAIN + "storage-updates.html");

  initDebuggerServer();

  let client = new DebuggerClient(DebuggerServer.connectPipe());
  let form = yield connectDebuggerClient(client);
  let front = StorageFront(client, form);
  let win = doc.defaultView.wrappedJSObject;

  yield front.listStores();

  for (let i = 0; i < TESTS.length; i++) {
    let test = TESTS[i];
    yield runTest(test, front, win, i);
  }

  yield testClearLocalAndSessionStores(front, win);
  yield finishTests(client);
});
