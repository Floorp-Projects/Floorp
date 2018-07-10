/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This helper contains the public API that can be used by DAMP tests.
 */

const Services = require("Services");
const { gDevTools } = require("devtools/client/framework/devtools");
const { TargetFactory } = require("devtools/client/framework/target");

const webserver = Services.prefs.getCharPref("addon.test.damp.webserver");

const PAGES_BASE_URL = webserver + "/tests/devtools/addon/content/pages/";

exports.PAGES_BASE_URL = PAGES_BASE_URL;
exports.SIMPLE_URL = PAGES_BASE_URL + "simple.html";
exports.COMPLICATED_URL = webserver + "/tests/tp5n/bild.de/www.bild.de/index.html";

let damp = null;
/*
 * This method should be called by js before starting the tests.
 */
exports.initialize = function(_damp) {
  damp = _damp;
};

function garbageCollect() {
  return damp.garbageCollect();
}
exports.garbageCollect = garbageCollect;

function runTest(label, record) {
  return damp.runTest(label, record);
}
exports.runTest = runTest;

exports.testSetup = function(url) {
  return damp.testSetup(url);
};

exports.testTeardown = function() {
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

exports.getToolbox = function() {
  let tab = getActiveTab();
  let target = TargetFactory.forTab(tab);
  return gDevTools.getToolbox(target);
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

const openToolbox = async function(tool = "webconsole", onLoad) {
  let tab = getActiveTab();
  let target = TargetFactory.forTab(tab);
  let onToolboxCreated = gDevTools.once("toolbox-created");
  let showPromise = gDevTools.showToolbox(target, tool);
  let toolbox = await onToolboxCreated;

  if (typeof(onLoad) == "function") {
    let panel = await toolbox.getPanelWhenReady(tool);
    await onLoad(toolbox, panel);
  }
  await showPromise;

  return toolbox;
};
exports.openToolbox = openToolbox;

exports.closeToolbox =  async function() {
  let tab = getActiveTab();
  let target = TargetFactory.forTab(tab);
  await target.client.waitForRequestsToSettle();
  await gDevTools.closeToolbox(target);
};

exports.openToolboxAndLog = async function(name, tool, onLoad) {
  let test = runTest(`${name}.open.DAMP`);
  let toolbox = await openToolbox(tool, onLoad);
  test.done();

  // Settle test isn't recorded, it only prints the pending duration
  test = runTest(`${name}.open.settle.DAMP`, false);
  await waitForPendingPaints(toolbox);
  test.done();

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
  await gDevTools.closeToolbox(target);
  test.done();
};

exports.reloadPageAndLog = async function(name, toolbox, onReload) {
  dump(`Reload page on '${name}'\n`);
  let test = runTest(`${name}.reload.DAMP`);
  await damp.reloadPage(onReload);
  test.done();

  // Settle test isn't recorded, it only prints the pending duration
  dump(`Wait for pending paints on '${name}'\n`);
  test = runTest(`${name}.reload.settle.DAMP`, false);
  await waitForPendingPaints(toolbox);
  test.done();
};
