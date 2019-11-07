/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from ../head.js */

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/remote/test/browser/head.js",
  this
);

// Wait for all Target.targetCreated events. One for each tab, plus the one
// for the main process target.
async function getDiscoveredTargets(Target) {
  return new Promise(resolve => {
    const targets = [];

    const unsubscribe = Target.targetCreated(target => {
      targets.push(target);

      if (targets.length >= gBrowser.tabs.length + 1) {
        unsubscribe();
        resolve(targets);
      }
    });

    Target.setDiscoverTargets({ discover: true });
  });
}

async function openTab(Target) {
  info("Create a new tab and wait for the target to be created");

  const targetCreated = Target.targetCreated();
  const newTab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  const { targetInfo } = await targetCreated;

  is(targetInfo.type, "page");
  info(`New tab with target id ${targetInfo.targetId} created`);

  return { targetInfo, newTab };
}

async function openWindow(Target) {
  info("Create a new window and wait for the target to be created");

  const targetCreated = Target.targetCreated();
  const newWindow = await BrowserTestUtils.openNewBrowserWindow();
  const newTab = newWindow.gBrowser.selectedTab;
  const { targetInfo } = await targetCreated;
  is(targetInfo.type, "page");

  return { targetInfo, newWindow, newTab };
}
