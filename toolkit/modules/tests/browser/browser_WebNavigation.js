"use strict";

var { interfaces: Ci, classes: Cc, utils: Cu, results: Cr } = Components;

var {WebNavigation} = Cu.import("resource://gre/modules/WebNavigation.jsm", {});

const BASE = "http://example.com/browser/toolkit/modules/tests/browser";
const URL = BASE + "/file_WebNavigation_page1.html";
const FRAME = BASE + "/file_WebNavigation_page2.html";
const FRAME2 = BASE + "/file_WebNavigation_page3.html";

const EVENTS = [
  "onBeforeNavigate",
  "onCommitted",
  "onDOMContentLoaded",
  "onCompleted",
  "onErrorOccurred",
  "onReferenceFragmentUpdated",
];

const REQUIRED = [
  "onBeforeNavigate",
  "onCommitted",
  "onDOMContentLoaded",
  "onCompleted",
];

var expectedBrowser;
var received = [];
var completedResolve;
var waitingURL, waitingEvent;
var rootWindowID;

function gotEvent(event, details)
{
  if (!details.url.startsWith(BASE)) {
    return;
  }
  info(`Got ${event} ${details.url} ${details.windowId} ${details.parentWindowId}`);

  is(details.browser, expectedBrowser, "correct <browser> element");

  received.push({url: details.url, event});

  if (typeof(rootWindowID) == "undefined") {
    rootWindowID = details.windowId;
  }

  if (details.url == URL) {
    is(details.windowId, rootWindowID, "root window ID correct");
  } else {
    is(details.parentWindowId, rootWindowID, "parent window ID correct");
    isnot(details.windowId, rootWindowID, "window ID probably okay");
  }

  isnot(details.windowId, undefined);
  isnot(details.parentWindowId, undefined);

  if (details.url == waitingURL && event == waitingEvent) {
    completedResolve();
  }
}

function loadViaFrameScript(url, event, script)
{
  // Loading via a frame script ensures that the chrome process never
  // "gets ahead" of frame scripts in non-e10s mode.
  received = [];
  waitingURL = url;
  waitingEvent = event;
  expectedBrowser.messageManager.loadFrameScript("data:," + script, false);
  return new Promise(resolve => { completedResolve = resolve; });
}

add_task(function* webnav_ordering() {
  let listeners = {};
  for (let event of EVENTS) {
    listeners[event] = gotEvent.bind(null, event);
    WebNavigation[event].addListener(listeners[event]);
  }

  gBrowser.selectedTab = gBrowser.addTab();
  let browser = gBrowser.selectedBrowser;
  expectedBrowser = browser;

  yield BrowserTestUtils.browserLoaded(browser);

  yield loadViaFrameScript(URL, "onCompleted", `content.location = "${URL}";`);

  function checkRequired(url) {
    for (let event of REQUIRED) {
      let found = false;
      for (let r of received) {
        if (r.url == url && r.event == event) {
          found = true;
        }
      }
      ok(found, `Received event ${event} from ${url}`);
    }
  }

  checkRequired(URL);
  checkRequired(FRAME);

  function checkBefore(action1, action2) {
    function find(action) {
      for (let i = 0; i < received.length; i++) {
        if (received[i].url == action.url && received[i].event == action.event) {
          return i;
        }
      }
      return -1;
    }

    let index1 = find(action1);
    let index2 = find(action2);
    ok(index1 != -1, `Action ${JSON.stringify(action1)} happened`);
    ok(index2 != -1, `Action ${JSON.stringify(action2)} happened`);
    ok(index1 < index2, `Action ${JSON.stringify(action1)} happened before ${JSON.stringify(action2)}`);
  }

  checkBefore({url: URL, event: "onCommitted"}, {url: FRAME, event: "onBeforeNavigate"});
  checkBefore({url: FRAME, event: "onCompleted"}, {url: URL, event: "onCompleted"});

  yield loadViaFrameScript(FRAME2, "onCompleted", `content.frames[0].location = "${FRAME2}";`);

  checkRequired(FRAME2);

  yield loadViaFrameScript(FRAME2 + "#ref", "onReferenceFragmentUpdated",
                           "content.frames[0].document.getElementById('elt').click();");

  info("Received onReferenceFragmentUpdated from FRAME2");

  gBrowser.removeCurrentTab();

  for (let event of EVENTS) {
    WebNavigation[event].removeListener(listeners[event]);
  }
});

