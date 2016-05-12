/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

let { classes: Cc, interfaces: Ci } = Components;

const USER_CONTEXTS = ["default", "personal", "work"];

const COOKIE_NAMES = ["cookie0", "cookie1", "cookie2"];

const TEST_URL =
  "http://example.com/browser/netwerk/cookie/test/browser/file_empty.html";

let cm = Cc["@mozilla.org/cookiemanager;1"].getService(Ci.nsICookieManager2);

// opens `uri' in a new tab with the provided userContextId and focuses it.
// returns the newly opened tab
function* openTabInUserContext(uri, userContextId) {
  // open the tab in the correct userContextId
  let tab = gBrowser.addTab(uri, {userContextId});

  // select tab and make sure its browser is focused
  gBrowser.selectedTab = tab;
  tab.ownerDocument.defaultView.focus();

  let browser = gBrowser.getBrowserForTab(tab);
  // wait for tab load
  yield BrowserTestUtils.browserLoaded(browser);

  return {tab, browser};
}

add_task(function* setup() {
  // make sure userContext is enabled.
  yield new Promise(resolve => {
    SpecialPowers.pushPrefEnv({"set": [
      ["privacy.userContext.enabled", true]
    ]}, resolve);
  });
});

add_task(function* test() {
  // load the page in 3 different contexts and set a cookie
  // which should only be visible in that context
  for (let userContextId of Object.keys(USER_CONTEXTS)) {
    // open our tab in the given user context
    let {tab, browser} = yield* openTabInUserContext(TEST_URL, userContextId);

    yield ContentTask.spawn(browser,
        {names: COOKIE_NAMES, value: USER_CONTEXTS[userContextId]},
        function(opts) {
          for (let name of opts.names) {
            content.document.cookie = name + "=" + opts.value;
          }
        });

    // remove the tab
    gBrowser.removeTab(tab);
  }

  let expectedValues = USER_CONTEXTS.slice(0);
  yield checkCookies(expectedValues, "before removal");

  // remove cookies that belongs to user context id #1
  cm.removeCookiesWithOriginAttributes(JSON.stringify({userContextId: 1}));

  expectedValues[1] = undefined;
  yield checkCookies(expectedValues, "after removal");
});

function *checkCookies(expectedValues, time) {
  for (let userContextId of Object.keys(expectedValues)) {
    let cookiesFromTitle = yield* getCookiesFromTitle(userContextId);
    let cookiesFromManager = getCookiesFromManager(userContextId);

    let expectedValue = expectedValues[userContextId];
    for (let name of COOKIE_NAMES) {
      is(cookiesFromTitle[name], expectedValue,
          `User context ${userContextId}: ${name} should be correct from title ${time}`);
      is(cookiesFromManager[name], expectedValue,
          `User context ${userContextId}: ${name} should be correct from manager ${time}`);
    }

  }
}

function getCookiesFromManager(userContextId) {
  let cookies = {};
  let enumerator = cm.getCookiesWithOriginAttributes(JSON.stringify({userContextId}));
  while (enumerator.hasMoreElements()) {
    let cookie = enumerator.getNext().QueryInterface(Ci.nsICookie);
    cookies[cookie.name] = cookie.value;
  }
  return cookies;
}

function* getCookiesFromTitle(userContextId) {
  let {tab, browser} = yield* openTabInUserContext(TEST_URL, userContextId);

  // reflect the cookies on title
  yield ContentTask.spawn(browser, null, function(opts) {
    content.document.title = content.document.cookie;
  });

  // get the title
  let cookieString = browser.contentDocument.title.split(";");

  // check each item in the title and validate it meets expectatations
  let cookies = {};
  for (let cookie of cookieString) {
    let [name, value] = cookie.trim().split("=");
    cookies[name] = value;
  }

  gBrowser.removeTab(tab);
  return cookies;
}
