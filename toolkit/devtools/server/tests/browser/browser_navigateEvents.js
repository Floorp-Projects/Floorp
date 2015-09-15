/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const URL1 = MAIN_DOMAIN + "navigate-first.html";
const URL2 = MAIN_DOMAIN + "navigate-second.html";

var events = require("sdk/event/core");
var client;

// State machine to check events order
var i = 0;
function assertEvent(event, data) {
  let x = 0;
  switch(i++) {
    case x++:
      is(event, "request", "Get first page load");
      is(data, URL1);
      break;
    case x++:
      is(event, "load-new-document", "Ask to load the second page");
      break;
    case x++:
      is(event, "unload-dialog", "We get the dialog on first page unload");
      break;
    case x++:
      is(event, "will-navigate", "The very first event is will-navigate on server side");
      is(data.newURI, URL2, "newURI property is correct");
      break;
    case x++:
      is(event, "tabNavigated", "Right after will-navigate, the client receive tabNavigated");
      is(data.state, "start", "state is start");
      is(data.url, URL2, "url property is correct");
      break;
    case x++:
      is(event, "request", "Given that locally, the Debugger protocol is sync, the request happens after tabNavigated");
      is(data, URL2);
      break;
    case x++:
      is(event, "DOMContentLoaded");
      is(content.document.readyState, "interactive");
      break;
    case x++:
      is(event, "load");
      is(content.document.readyState, "complete");
      break;
    case x++:
      is(event, "navigate", "Then once the second doc is loaded, we get the navigate event");
      is(content.document.readyState, "complete", "navigate is emitted only once the document is fully loaded");
      break;
    case x++:
      is(event, "tabNavigated", "Finally, the receive the client event");
      is(data.state, "stop", "state is stop");
      is(data.url, URL2, "url property is correct");

      // End of test!
      cleanup();
      break;
  }
}

function waitForOnBeforeUnloadDialog(browser, callback) {
  browser.addEventListener("DOMWillOpenModalDialog", function onModalDialog() {
    browser.removeEventListener("DOMWillOpenModalDialog", onModalDialog, true);

    executeSoon(() => {
      let stack = browser.parentNode;
      let dialogs = stack.getElementsByTagName("tabmodalprompt");
      let {button0, button1} = dialogs[0].ui;
      callback(button0, button1);
    });
  }, true);
}

var httpObserver = function (subject, topic, state) {
  let channel = subject.QueryInterface(Ci.nsIHttpChannel);
  let url = channel.URI.spec;
  // Only listen for our document request, as many other requests can happen
  if (url == URL1 || url == URL2) {
    assertEvent("request", url);
  }
};
Services.obs.addObserver(httpObserver, "http-on-modify-request", false);

function onDOMContentLoaded() {
  assertEvent("DOMContentLoaded");
}
function onLoad() {
  assertEvent("load");
}

function getServerTabActor(callback) {
  // Ensure having a minimal server
  initDebuggerServer();

  // Connect to this tab
  let transport = DebuggerServer.connectPipe();
  client = new DebuggerClient(transport);
  connectDebuggerClient(client).then(form => {
    let actorID = form.actor;
    client.attachTab(actorID, function(aResponse, aTabClient) {
      // !Hack! Retrieve a server side object, the BrowserTabActor instance
      let conn = transport._serverConnection;
      let tabActor = conn.getActor(actorID);
      callback(tabActor);
    });
  });

  client.addListener("tabNavigated", function (aEvent, aPacket) {
    assertEvent("tabNavigated", aPacket);
  });
}

function test() {
  // Open a test tab
  addTab(URL1).then(function(doc) {
    getServerTabActor(function (tabActor) {
      // In order to listen to internal will-navigate/navigate events
      events.on(tabActor, "will-navigate", function (data) {
        assertEvent("will-navigate", data);
      });
      events.on(tabActor, "navigate", function (data) {
        assertEvent("navigate", data);
      });

      // Start listening for page load events
      let browser = gBrowser.selectedBrowser;
      browser.addEventListener("DOMContentLoaded", onDOMContentLoaded, true);
      browser.addEventListener("load", onLoad, true);

      // Listen for alert() call being made in navigate-first during unload
      waitForOnBeforeUnloadDialog(browser, function (btnLeave, btnStay) {
        assertEvent("unload-dialog");
        // accept to quit this page to another
        btnLeave.click();
      });

      // Load another document in this doc to dispatch these events
      assertEvent("load-new-document");
      content.location = URL2;
    });

  });
}

function cleanup() {
  let browser = gBrowser.selectedBrowser;
  browser.removeEventListener("DOMContentLoaded", onDOMContentLoaded);
  browser.removeEventListener("load", onLoad);
  client.close(function () {
    Services.obs.addObserver(httpObserver, "http-on-modify-request", false);
    DebuggerServer.destroy();
    finish();
  });
}
