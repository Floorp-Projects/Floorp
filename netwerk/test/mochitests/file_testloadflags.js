"use strict";

const SCRIPT_URL = SimpleTest.getTestFileURL(
  "file_testloadflags_chromescript.js"
);

let gScript;
var gExpectedCookies;
var gExpectedHeaders;
var gExpectedLoads;

var gObs;
var gPopup;

var gHeaders = 0;
var gLoads = 0;

// setupTest() is run from 'onload='.
function setupTest(uri, domain, cookies, loads, headers) {
  info(
    "setupTest uri: " +
      uri +
      " domain: " +
      domain +
      " cookies: " +
      cookies +
      " loads: " +
      loads +
      " headers: " +
      headers
  );

  SimpleTest.waitForExplicitFinish();

  var prefSet = new Promise(resolve => {
    SpecialPowers.pushPrefEnv(
      {
        set: [
          ["network.cookie.cookieBehavior", 1],
          ["network.cookie.sameSite.schemeful", false],
        ],
      },
      resolve
    );
  });

  gExpectedCookies = cookies;
  gExpectedLoads = loads;
  gExpectedHeaders = headers;

  gScript = SpecialPowers.loadChromeScript(SCRIPT_URL);
  gScript.addMessageListener("info", ({ str }) => info(str));
  gScript.addMessageListener("ok", ({ c, m }) => ok(c, m));
  gScript.addMessageListener("observer:gotCookie", ({ cookie, uri }) => {
    isnot(
      cookie.indexOf("oh=hai"),
      -1,
      "cookie 'oh=hai' is in header for " + uri
    );
    ++gHeaders;
  });

  var scriptReady = new Promise(resolve => {
    gScript.addMessageListener("init:return", resolve);
    gScript.sendAsyncMessage("init", { domain });
  });

  // Listen for MessageEvents.
  window.addEventListener("message", messageReceiver);

  Promise.all([prefSet, scriptReady]).then(() => {
    // load a window which contains an iframe; each will attempt to set
    // cookies from their respective domains.
    gPopup = window.open(uri, "hai", "width=100,height=100");
  });
}

function finishTest() {
  gScript.addMessageListener("shutdown:return", () => {
    gScript.destroy();
    SimpleTest.finish();
  });
  gScript.sendAsyncMessage("shutdown");
}

/** Receives MessageEvents to this window. */
// Count and check loads.
function messageReceiver(evt) {
  ok(
    evt.data == "f_lf_i msg data img" || evt.data == "f_lf_i msg data page",
    "message data received from popup"
  );
  if (evt.data == "f_lf_i msg data img") {
    info("message data received from popup for image");
  }
  if (evt.data == "f_lf_i msg data page") {
    info("message data received from popup for page");
  }
  if (evt.data != "f_lf_i msg data img" && evt.data != "f_lf_i msg data page") {
    info("got this message but don't know what it is " + evt.data);
    gPopup.close();
    window.removeEventListener("message", messageReceiver);

    finishTest();
    return;
  }

  // only run the test when all our children are done loading & setting cookies
  if (++gLoads == gExpectedLoads) {
    gPopup.close();
    window.removeEventListener("message", messageReceiver);

    runTest();
  }
}

// runTest() is run by messageReceiver().
// Check headers, and count and check cookies.
function runTest() {
  // set a cookie from a domain of "localhost"
  document.cookie = "o=noes";

  is(gHeaders, gExpectedHeaders, "number of observed request headers");
  gScript.addMessageListener("getCookieCount:return", ({ count }) => {
    is(count, gExpectedCookies, "total number of cookies");
    finishTest();
  });

  gScript.sendAsyncMessage("getCookieCount");
}
