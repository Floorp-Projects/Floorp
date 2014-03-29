const Cu = Components.utils;
Cu.import("resource://gre/modules/Services.jsm");
let tempScope = {};
Cu.import("resource://gre/modules/devtools/dbg-client.jsm", tempScope);
Cu.import("resource://gre/modules/devtools/dbg-server.jsm", tempScope);
let {DebuggerServer, DebuggerClient} = tempScope;
tempScope = null;

const {StorageFront} = require("devtools/server/actors/storage");

// Always log packets when running tests.
Services.prefs.setBoolPref("devtools.debugger.log", true);
registerCleanupFunction(function() {
  Services.prefs.clearUserPref("devtools.debugger.log");
});

const storeMap = {
  cookies: {
    "test1.example.org": [
      {
        name: "c1",
        value: "foobar",
        expires: 2000000000000,
        path: "/browser",
        host: "test1.example.org",
        isDomain: false,
        isSecure: false,
      },
      {
        name: "cs2",
        value: "sessionCookie",
        path: "/",
        host: ".example.org",
        expires: 0,
        isDomain: true,
        isSecure: false,
      },
      {
        name: "c3",
        value: "foobar-2",
        expires: 2000000001000,
        path: "/",
        host: "test1.example.org",
        isDomain: false,
        isSecure: true,
      },
      {
        name: "uc1",
        value: "foobar",
        host: ".example.org",
        path: "/",
        expires: 0,
        isDomain: true,
        isSecure: true,
      }
    ],
    "sectest1.example.org": [
      {
        name: "uc1",
        value: "foobar",
        host: ".example.org",
        path: "/",
        expires: 0,
        isDomain: true,
        isSecure: true,
      },
      {
        name: "cs2",
        value: "sessionCookie",
        path: "/",
        host: ".example.org",
        expires: 0,
        isDomain: true,
        isSecure: false,
      },
      {
        name: "sc1",
        value: "foobar",
        path: "/browser/toolkit/devtools/server/tests/browser/",
        host: "sectest1.example.org",
        expires: 0,
        isDomain: false,
        isSecure: false,
      }
    ]
  },
  localStorage: {
    "http://test1.example.org": [
      {
        name: "ls1",
        value: "foobar"
      },
      {
        name: "ls2",
        value: "foobar-2"
      }
    ],
    "http://sectest1.example.org": [
      {
        name: "iframe-u-ls1",
        value: "foobar"
      }
    ],
    "https://sectest1.example.org": [
      {
        name: "iframe-s-ls1",
        value: "foobar"
      }
    ]
  },
  sessionStorage: {
    "http://test1.example.org": [
      {
        name: "ss1",
        value: "foobar-3"
      }
    ],
    "http://sectest1.example.org": [
      {
        name: "iframe-u-ss1",
        value: "foobar1"
      },
      {
        name: "iframe-u-ss2",
        value: "foobar2"
      }
    ],
    "https://sectest1.example.org": [
      {
        name: "iframe-s-ss1",
        value: "foobar-2"
      }
    ]
  }
};

function finishTests(client) {
  // Forcing GC/CC to get rid of docshells and windows created by this test.
  forceCollections();
  client.close(() => {
    forceCollections();
    DebuggerServer.destroy();
    forceCollections();
    DebuggerClient = DebuggerServer = null;
    finish();
  });
}

function testStores(data, client) {
  ok(data.cookies, "Cookies storage actor is present");
  ok(data.localStorage, "Local Storage storage actor is present");
  ok(data.sessionStorage, "Session Storage storage actor is present");
  testCookies(data.cookies).then(() =>
  testLocalStorage(data.localStorage)).then(() =>
  testSessionStorage(data.sessionStorage)).then(() =>
  finishTests(client));
}

function testCookies(cookiesActor) {
  is(Object.keys(cookiesActor.hosts).length, 2, "Correct number of host entries for cookies");
  return testCookiesObjects(0, cookiesActor.hosts, cookiesActor);
}

function testCookiesObjects(index, hosts, cookiesActor) {
  let host = Object.keys(hosts)[index];
  let matchItems = data => {
    is(data.total, storeMap.cookies[host].length,
       "Number of cookies in host " + host + " matches");
    for (let item of data.data) {
      let found = false;
      for (let toMatch of storeMap.cookies[host]) {
        if (item.name == toMatch.name) {
          found = true;
          ok(true, "Found cookie " + item.name + " in response");
          is(item.value.str, toMatch.value, "The value matches.");
          is(item.expires, toMatch.expires, "The expiry time matches.");
          is(item.path, toMatch.path, "The path matches.");
          is(item.host, toMatch.host, "The host matches.");
          is(item.isSecure, toMatch.isSecure, "The isSecure value matches.");
          is(item.isDomain, toMatch.isDomain, "The isDomain value matches.");
          break;
        }
      }
      if (!found) {
        ok(false, "cookie " + item.name + " should not exist in response;");
      }
    }
  };

  ok(!!storeMap.cookies[host], "Host is present in the list : " + host);
  if (index == Object.keys(hosts).length - 1) {
    return cookiesActor.getStoreObjects(host).then(matchItems);
  }
  return cookiesActor.getStoreObjects(host).then(matchItems).then(() => {
    return testCookiesObjects(++index, hosts, cookiesActor);
  });
}

function testLocalStorage(localStorageActor) {
  is(Object.keys(localStorageActor.hosts).length, 3,
     "Correct number of host entries for local storage");
  return testLocalStorageObjects(0, localStorageActor.hosts, localStorageActor);
}

function testLocalStorageObjects(index, hosts, localStorageActor) {
  let host = Object.keys(hosts)[index];
  let matchItems = data => {
    is(data.total, storeMap.localStorage[host].length,
       "Number of local storage items in host " + host + " matches");
    for (let item of data.data) {
      let found = false;
      for (let toMatch of storeMap.localStorage[host]) {
        if (item.name == toMatch.name) {
          found = true;
          ok(true, "Found local storage item " + item.name + " in response");
          is(item.value.str, toMatch.value, "The value matches.");
          break;
        }
      }
      if (!found) {
        ok(false, "local storage item " + item.name +
                  " should not exist in response;");
      }
    }
  };

  ok(!!storeMap.localStorage[host], "Host is present in the list : " + host);
  if (index == Object.keys(hosts).length - 1) {
    return localStorageActor.getStoreObjects(host).then(matchItems);
  }
  return localStorageActor.getStoreObjects(host).then(matchItems).then(() => {
    return testLocalStorageObjects(++index, hosts, localStorageActor);
  });
}

function testSessionStorage(sessionStorageActor) {
  is(Object.keys(sessionStorageActor.hosts).length, 3,
     "Correct number of host entries for session storage");
  return testSessionStorageObjects(0, sessionStorageActor.hosts,
                                   sessionStorageActor);
}

function testSessionStorageObjects(index, hosts, sessionStorageActor) {
  let host = Object.keys(hosts)[index];
  let matchItems = data => {
    is(data.total, storeMap.sessionStorage[host].length,
       "Number of session storage items in host " + host + " matches");
    for (let item of data.data) {
      let found = false;
      for (let toMatch of storeMap.sessionStorage[host]) {
        if (item.name == toMatch.name) {
          found = true;
          ok(true, "Found session storage item " + item.name + " in response");
          is(item.value.str, toMatch.value, "The value matches.");
          break;
        }
      }
      if (!found) {
        ok(false, "session storage item " + item.name +
                  " should not exist in response;");
      }
    }
  };

  ok(!!storeMap.sessionStorage[host], "Host is present in the list : " + host);
  if (index == Object.keys(hosts).length - 1) {
    return sessionStorageActor.getStoreObjects(host).then(matchItems);
  }
  return sessionStorageActor.getStoreObjects(host).then(matchItems).then(() => {
    return testSessionStorageObjects(++index, hosts, sessionStorageActor);
  });
}

function test() {
  waitForExplicitFinish();
  addTab(MAIN_DOMAIN + "storage-listings.html", function() {
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

        front.listStores().then(data => testStores(data, client));
      });
    });
  })
}
