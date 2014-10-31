/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
const {console} = Cu.import("resource://gre/modules/devtools/Console.jsm", {});
const {devtools: {require}} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
const {DebuggerClient} = Cu.import("resource://gre/modules/devtools/dbg-client.jsm", {});
const {DebuggerServer} = Cu.import("resource://gre/modules/devtools/dbg-server.jsm", {});

const PATH = "browser/toolkit/devtools/server/tests/browser/";
const MAIN_DOMAIN = "http://test1.example.org/" + PATH;
const ALT_DOMAIN = "http://sectest1.example.org/" + PATH;
const ALT_DOMAIN_SECURED = "https://sectest1.example.org:443/" + PATH;

// All tests are asynchronous.
waitForExplicitFinish();

/**
 * Define an async test based on a generator function.
 */
function asyncTest(generator) {
  return () => Task.spawn(generator).then(null, ok.bind(null, false)).then(finish);
}

/**
 * Add a new test tab in the browser and load the given url.
 * @param {String} url The url to be loaded in the new tab
 * @return a promise that resolves to the document when the url is loaded
 */
let addTab = Task.async(function* (url) {
  info("Adding a new tab with URL: '" + url + "'");
  let tab = gBrowser.selectedTab = gBrowser.addTab();
  let loaded = once(gBrowser.selectedBrowser, "load", true);

  content.location = url;
  yield loaded;

  info("URL '" + url + "' loading complete");

  yield new Promise(resolve => {
    let isBlank = url == "about:blank";
    waitForFocus(resolve, content, isBlank);
  });;

  return tab.linkedBrowser.contentWindow.document;
});

function initDebuggerServer() {
  try {
    // Sometimes debugger server does not get destroyed correctly by previous
    // tests.
    DebuggerServer.destroy();
  } catch (ex) { }
  DebuggerServer.init(() => true);
  DebuggerServer.addBrowserActors();
}

/**
 * Connect a debugger client.
 * @param {DebuggerClient}
 * @return {Promise} Resolves to the selected tabActor form when the client is
 * connected.
 */
function connectDebuggerClient(client) {
  return new Promise(resolve => {
    client.connect(() => {
      client.listTabs(tabs => {
        resolve(tabs.tabs[tabs.selected]);
      });
    });
  });
}

/**
 * Close a debugger client's connection.
 * @param {DebuggerClient}
 * @return {Promise} Resolves when the connection is closed.
 */
function closeDebuggerClient(client) {
  return new Promise(resolve => client.close(resolve));
}

/**
 * Wait for eventName on target.
 * @param {Object} target An observable object that either supports on/off or
 * addEventListener/removeEventListener
 * @param {String} eventName
 * @param {Boolean} useCapture Optional, for addEventListener/removeEventListener
 * @return A promise that resolves when the event has been handled
 */
function once(target, eventName, useCapture=false) {
  info("Waiting for event: '" + eventName + "' on " + target + ".");

  return new Promise(resolve => {

    for (let [add, remove] of [
      ["addEventListener", "removeEventListener"],
      ["addListener", "removeListener"],
      ["on", "off"]
    ]) {
      if ((add in target) && (remove in target)) {
        target[add](eventName, function onEvent(...aArgs) {
          info("Got event: '" + eventName + "' on " + target + ".");
          target[remove](eventName, onEvent, useCapture);
          resolve(...aArgs);
        }, useCapture);
        break;
      }
    }
  });
}

/**
 * Forces GC, CC and Shrinking GC to get rid of disconnected docshells and
 * windows.
 */
function forceCollections() {
  Cu.forceGC();
  Cu.forceCC();
  Cu.forceShrinkingGC();
}

registerCleanupFunction(function tearDown() {
  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }
});
