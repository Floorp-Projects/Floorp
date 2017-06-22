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
async function openTabInUserContext(uri, userContextId) {
  // open the tab in the correct userContextId
  let tab = BrowserTestUtils.addTab(gBrowser, uri, {userContextId});

  // select tab and make sure its browser is focused
  gBrowser.selectedTab = tab;
  tab.ownerGlobal.focus();

  let browser = gBrowser.getBrowserForTab(tab);
  // wait for tab load
  await BrowserTestUtils.browserLoaded(browser);

  return {tab, browser};
}

add_task(async function setup() {
  // make sure userContext is enabled.
  await new Promise(resolve => {
    SpecialPowers.pushPrefEnv({"set": [
      ["privacy.userContext.enabled", true]
    ]}, resolve);
  });
});

add_task(async function test() {
  // load the page in 3 different contexts and set a cookie
  // which should only be visible in that context
  for (let userContextId of Object.keys(USER_CONTEXTS)) {
    // open our tab in the given user context
    let {tab, browser} = await openTabInUserContext(TEST_URL, userContextId);

    await ContentTask.spawn(browser,
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
  await checkCookies(expectedValues, "before removal");

  // remove cookies that belongs to user context id #1
  cm.removeCookiesWithOriginAttributes(JSON.stringify({userContextId: 1}));

  expectedValues[1] = undefined;
  await checkCookies(expectedValues, "after removal");
});

async function checkCookies(expectedValues, time) {
  for (let userContextId of Object.keys(expectedValues)) {
    let cookiesFromTitle = await getCookiesFromJS(userContextId);
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

async function getCookiesFromJS(userContextId) {
  let {tab, browser} = await openTabInUserContext(TEST_URL, userContextId);

  // get the cookies
  let cookieString = await ContentTask.spawn(browser, null, function() {
    return content.document.cookie;
  });

  // check each item in the title and validate it meets expectatations
  let cookies = {};
  for (let cookie of cookieString.split(";")) {
    let [name, value] = cookie.trim().split("=");
    cookies[name] = value;
  }

  gBrowser.removeTab(tab);
  return cookies;
}
