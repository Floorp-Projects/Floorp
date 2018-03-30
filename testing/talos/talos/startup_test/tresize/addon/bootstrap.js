"use strict";

/* globals initializeBrowser */

// PLEASE NOTE:
//
// The canonical version of this file lives in testing/talos/talos, and
// is duplicated in a number of test add-ons in directories below it.
// Please do not update one withput updating all.

// Reads the chrome.manifest from a legacy non-restartless extension and loads
// its overlays into the appropriate top-level windows.

ChromeUtils.import("resource://gre/modules/NetUtil.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

function readSync(uri) {
  let channel = NetUtil.newChannel({uri, loadUsingSystemPrincipal: true});
  let buffer = NetUtil.readInputStream(channel.open2());
  return new TextDecoder().decode(buffer);
}

function startup(data, reason) {
  Services.scriptloader.loadSubScript(data.resourceURI.resolve("content/initialize_browser.js"));
  Services.obs.addObserver(function observer(window) {
    Services.obs.removeObserver(observer, "browser-delayed-startup-finished");
    initializeBrowser(window);
  }, "browser-delayed-startup-finished");
}

function shutdown(data, reason) {}
function install(data, reason) {}
function uninstall(data, reason) {}
