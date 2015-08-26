/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
const {console} = Cu.import("resource://gre/modules/devtools/Console.jsm", {});
const {require} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
const {DebuggerClient} = require("devtools/toolkit/client/main");
const {DebuggerServer} = require("devtools/server/main");
const {defer} = require("promise");
const DevToolsUtils = require("devtools/toolkit/DevToolsUtils");

const PATH = "browser/toolkit/devtools/server/tests/browser/";
const MAIN_DOMAIN = "http://test1.example.org/" + PATH;
const ALT_DOMAIN = "http://sectest1.example.org/" + PATH;
const ALT_DOMAIN_SECURED = "https://sectest1.example.org:443/" + PATH;

// All tests are asynchronous.
waitForExplicitFinish();

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
  });

  return tab.linkedBrowser.contentWindow.document;
});

function initDebuggerServer() {
  try {
    // Sometimes debugger server does not get destroyed correctly by previous
    // tests.
    DebuggerServer.destroy();
  } catch (ex) { }
  DebuggerServer.init();
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

/**
 * Get a mock tabActor from a given window.
 * This is sometimes useful to test actors or classes that use the tabActor in
 * isolation.
 * @param {DOMWindow} win
 * @return {Object}
 */
function getMockTabActor(win) {
  return {
    window: win,
    isRootActor: true
  };
}

registerCleanupFunction(function tearDown() {
  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }
});

function idleWait(time) {
  return DevToolsUtils.waitForTime(time);
}

function busyWait(time) {
  let start = Date.now();
  let stack;
  while (Date.now() - start < time) { stack = Components.stack; }
}

/**
 * Waits until a predicate returns true.
 *
 * @param function predicate
 *        Invoked once in a while until it returns true.
 * @param number interval [optional]
 *        How often the predicate is invoked, in milliseconds.
 */
function waitUntil(predicate, interval = 10) {
  if (predicate()) {
    return Promise.resolve(true);
  }
  return new Promise(resolve => {
    setTimeout(function() {
      waitUntil(predicate).then(() => resolve(true));
    }, interval);
  });
}

function waitForMarkerType(front, types, predicate) {
  types = [].concat(types);
  predicate = predicate || function(){ return true; };
  let filteredMarkers = [];
  let { promise, resolve } = defer();

  info("Waiting for markers of type: " + types);

  function handler (name, data) {
    if (name !== "markers") {
      return;
    }

    let markers = data.markers;
    info("Got markers: " + JSON.stringify(markers, null, 2));

    filteredMarkers = filteredMarkers.concat(markers.filter(m => types.indexOf(m.name) !== -1));

    if (types.every(t => filteredMarkers.some(m => m.name === t)) && predicate(filteredMarkers)) {
      front.off("timeline-data", handler);
      resolve(filteredMarkers);
    }
  }
  front.on("timeline-data", handler);

  return promise;
}
