/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This helper contains the public API that can be used by DAMP tests.
 */

const Services = require("Services");
const { gDevTools } = require("devtools/client/framework/devtools");

// With Bug 1588203, the talos server supports a dynamic proxy that will
// redirect any http:// call to the talos server. This means we can use
// arbitrary domains for our tests.
// Iframes may be loaded via http://damp.frame1.com, http://damp.frame2.com, etc...
// or any other domain name that is sensible for a given test.
// To trigger frames to load in separate processes, they must have a different
// eTLD+1, which means public suffix (.com, .org, ...) and the label before.
const BASE_DOMAIN = "http://damp.top.com";
const PAGES_BASE_URL = BASE_DOMAIN + "/tests/devtools/addon/content/pages/";

exports.PAGES_BASE_URL = PAGES_BASE_URL;
exports.SIMPLE_URL = PAGES_BASE_URL + "simple.html";

// The test page in fis/tp5n/bild.de contains a modified version of the initial
// bild.de test website, where same-site iframes have been replaced with remote
// frames.
exports.COMPLICATED_URL =
  "http://www.bild.de-talos/fis/tp5n/bild.de/www.bild.de/index.html";

const { damp } = require("damp-test/damp");

function garbageCollect() {
  return damp.garbageCollect();
}
exports.garbageCollect = garbageCollect;

function runTest(label, record) {
  return damp.runTest(label, record);
}
exports.runTest = runTest;

exports.testSetup = function(url, { disableCache } = {}) {
  if (disableCache) {
    // Tests relying on iframes should disable the cache.
    // `browser.reload()` skips the cache for resources loaded by the main page,
    // but not for resources loaded by iframes.
    // Using the cache makes all the `complicated` tests much faster (except for
    // the first one that has to fill the cache).
    // To keep the baseline closer to the historical figures, cache is disabled.
    Services.prefs.setBoolPref("devtools.cache.disabled", true);
  }
  return damp.testSetup(url);
};

exports.testTeardown = function() {
  // Reset the "devtools.cache.disabled" preference in case it was turned on in
  // testSetup().
  Services.prefs.setBoolPref("devtools.cache.disabled", false);
  return damp.testTeardown();
};

exports.logTestResult = function(name, value) {
  damp._results.push({ name, value });
};

function getBrowserWindow() {
  return Services.wm.getMostRecentWindow("navigator:browser");
}
exports.getBrowserWindow = getBrowserWindow;

function getActiveTab() {
  return getBrowserWindow().gBrowser.selectedTab;
}
exports.getActiveTab = getActiveTab;

exports.getToolbox = async function() {
  let tab = getActiveTab();
  return gDevTools.getToolboxForTab(tab);
};

/**
 * Wait for any pending paint.
 * The tool may have touched the DOM elements at the very end of the current test.
 * We should ensure waiting for the reflow related to these changes.
 */
async function waitForPendingPaints(toolbox) {
  let panel = toolbox.getCurrentPanel();
  // All panels have its own way of exposing their window object...
  let window = panel.panelWin || panel._frameWindow || panel.panelWindow;
  return damp.waitForPendingPaints(window);
}
exports.waitForPendingPaints = waitForPendingPaints;

const openToolbox = async function(tool = "webconsole", onLoad) {
  dump(`Open toolbox on '${tool}'\n`);
  let tab = getActiveTab();

  dump(`Open toolbox - Call showToolboxForTab\n`);
  let onToolboxCreated = gDevTools.once("toolbox-created");
  let showPromise = gDevTools.showToolboxForTab(tab, { toolId: tool });

  dump(`Open toolbox - Wait for "toolbox-created"\n`);
  let toolbox = await onToolboxCreated;

  if (typeof onLoad == "function") {
    dump(`Open toolbox - Wait for custom onLoad callback\n`);
    let panel = await toolbox.getPanelWhenReady(tool);
    await onLoad(toolbox, panel);
  }

  dump(`Open toolbox - Wait for showToolbox to resolve\n`);
  await showPromise;

  return toolbox;
};
exports.openToolbox = openToolbox;

exports.closeToolbox = async function() {
  let tab = getActiveTab();
  let toolbox = await gDevTools.getToolboxForTab(tab);
  await toolbox.target.client.waitForRequestsToSettle();
  await gDevTools.closeToolboxForTab(tab);
};

// Settle test isn't recorded, it only prints the pending duration
async function recordPendingPaints(name, toolbox) {
  dump(`Wait for pending paints on '${name}'\n`);
  const test = runTest(`${name}.settle.DAMP`, false);
  await waitForPendingPaints(toolbox);
  test.done();
}
exports.recordPendingPaints = recordPendingPaints;

exports.openToolboxAndLog = async function(name, tool, onLoad) {
  const test = runTest(`${name}.open.DAMP`);

  let toolbox = await openToolbox(tool, onLoad);
  test.done();

  await recordPendingPaints(`${name}.open`, toolbox);

  // Force freeing memory after toolbox open as it creates a lot of objects
  // and for complex documents, it introduces a GC that runs during 'reload' test.
  await garbageCollect();

  return toolbox;
};

exports.closeToolboxAndLog = async function(name, toolbox) {
  let { target } = toolbox;
  dump(`Close toolbox on '${name}'\n`);
  await target.client.waitForRequestsToSettle();

  let test = runTest(`${name}.close.DAMP`);
  await toolbox.destroy();
  test.done();
};

exports.reloadPageAndLog = async function(name, toolbox, onReload) {
  dump(`Reload page on '${name}'\n`);
  let test = runTest(`${name}.reload.DAMP`);
  await damp.reloadPage(onReload);
  test.done();

  await recordPendingPaints(`${name}.reload`, toolbox);
};

exports.isFissionEnabled = function() {
  return Services.appinfo.fissionAutostart;
};

exports.waitForTick = () => new Promise(res => setTimeout(res, 0));
