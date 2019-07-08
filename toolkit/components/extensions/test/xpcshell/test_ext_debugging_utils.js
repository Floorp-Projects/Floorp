/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { ExtensionParent } = ChromeUtils.import(
  "resource://gre/modules/ExtensionParent.jsm"
);

add_task(async function testExtensionDebuggingUtilsCleanup() {
  const extension = ExtensionTestUtils.loadExtension({
    background() {
      browser.test.sendMessage("background.ready");
    },
  });

  const expectedEmptyDebugUtils = {
    hiddenXULWindow: null,
    cacheSize: 0,
  };

  let { hiddenXULWindow, debugBrowserPromises } = ExtensionParent.DebugUtils;

  deepEqual(
    { hiddenXULWindow, cacheSize: debugBrowserPromises.size },
    expectedEmptyDebugUtils,
    "No ExtensionDebugUtils resources has been allocated yet"
  );

  await extension.startup();

  await extension.awaitMessage("background.ready");

  hiddenXULWindow = ExtensionParent.DebugUtils.hiddenXULWindow;
  deepEqual(
    { hiddenXULWindow, cacheSize: debugBrowserPromises.size },
    expectedEmptyDebugUtils,
    "No debugging resources has been yet allocated once the extension is running"
  );

  const fakeAddonActor = {
    addonId: extension.id,
  };

  const anotherAddonActor = {
    addonId: extension.id,
  };

  const waitFirstBrowser = ExtensionParent.DebugUtils.getExtensionProcessBrowser(
    fakeAddonActor
  );
  const waitSecondBrowser = ExtensionParent.DebugUtils.getExtensionProcessBrowser(
    anotherAddonActor
  );

  const addonDebugBrowser = await waitFirstBrowser;
  equal(
    addonDebugBrowser.isRemoteBrowser,
    extension.extension.remote,
    "The addon debugging browser has the expected remote type"
  );

  equal(
    await waitSecondBrowser,
    addonDebugBrowser,
    "Two addon debugging actors related to the same addon get the same browser element "
  );

  equal(
    debugBrowserPromises.size,
    1,
    "The expected resources has been allocated"
  );

  const nonExistentAddonActor = {
    addonId: "non-existent-addon@test",
  };

  const waitRejection = ExtensionParent.DebugUtils.getExtensionProcessBrowser(
    nonExistentAddonActor
  );

  await Assert.rejects(
    waitRejection,
    /Extension not found/,
    "Reject with the expected message for non existent addons"
  );

  equal(
    debugBrowserPromises.size,
    1,
    "No additional debugging resources has been allocated"
  );

  await ExtensionParent.DebugUtils.releaseExtensionProcessBrowser(
    fakeAddonActor
  );

  equal(
    debugBrowserPromises.size,
    1,
    "The addon debugging browser is cached until all the related actors have released it"
  );

  await ExtensionParent.DebugUtils.releaseExtensionProcessBrowser(
    anotherAddonActor
  );

  hiddenXULWindow = ExtensionParent.DebugUtils.hiddenXULWindow;

  deepEqual(
    { hiddenXULWindow, cacheSize: debugBrowserPromises.size },
    expectedEmptyDebugUtils,
    "All the allocated debugging resources has been cleared"
  );

  await extension.unload();
});

add_task(async function testExtensionDebuggingUtilsAddonReloaded() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: {
        gecko: {
          id: "test-reloaded@test.mozilla.com",
        },
      },
    },
    background() {
      browser.test.sendMessage("background.ready");
    },
  });

  await extension.startup();
  await extension.awaitMessage("background.ready");

  let fakeAddonActor = {
    addonId: extension.id,
  };

  const addonDebugBrowser = await ExtensionParent.DebugUtils.getExtensionProcessBrowser(
    fakeAddonActor
  );
  equal(
    addonDebugBrowser.isRemoteBrowser,
    extension.extension.remote,
    "The addon debugging browser has the expected remote type"
  );
  equal(
    ExtensionParent.DebugUtils.debugBrowserPromises.size,
    1,
    "Got the expected number of requested debug browsers"
  );

  const { chromeDocument } = ExtensionParent.DebugUtils.hiddenXULWindow;

  ok(
    addonDebugBrowser.parentElement === chromeDocument.documentElement,
    "The addon debugging browser is part of the hiddenXULWindow chromeDocument"
  );

  await extension.unload();

  // Install an extension with the same id to recreate for the DebugUtils
  // conditions similar to an addon reloaded while the Addon Debugger is opened.
  extension = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: {
        gecko: {
          id: "test-reloaded@test.mozilla.com",
        },
      },
    },
    background() {
      browser.test.sendMessage("background.ready");
    },
  });

  await extension.startup();
  await extension.awaitMessage("background.ready");

  equal(
    ExtensionParent.DebugUtils.debugBrowserPromises.size,
    1,
    "Got the expected number of requested debug browsers"
  );

  const newAddonDebugBrowser = await ExtensionParent.DebugUtils.getExtensionProcessBrowser(
    fakeAddonActor
  );

  equal(
    addonDebugBrowser,
    newAddonDebugBrowser,
    "The existent debugging browser has been reused"
  );

  equal(
    newAddonDebugBrowser.isRemoteBrowser,
    extension.extension.remote,
    "The addon debugging browser has the expected remote type"
  );

  await ExtensionParent.DebugUtils.releaseExtensionProcessBrowser(
    fakeAddonActor
  );

  equal(
    ExtensionParent.DebugUtils.debugBrowserPromises.size,
    0,
    "All the addon debugging browsers has been released"
  );

  await extension.unload();
});

add_task(async function testExtensionDebuggingUtilsWithMultipleAddons() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: {
        gecko: {
          id: "test-addon-1@test.mozilla.com",
        },
      },
    },
    background() {
      browser.test.sendMessage("background.ready");
    },
  });
  let anotherExtension = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: {
        gecko: {
          id: "test-addon-2@test.mozilla.com",
        },
      },
    },
    background() {
      browser.test.sendMessage("background.ready");
    },
  });

  await extension.startup();
  await extension.awaitMessage("background.ready");

  await anotherExtension.startup();
  await anotherExtension.awaitMessage("background.ready");

  const fakeAddonActor = {
    addonId: extension.id,
  };

  const anotherFakeAddonActor = {
    addonId: anotherExtension.id,
  };

  const { DebugUtils } = ExtensionParent;
  const debugBrowser = await DebugUtils.getExtensionProcessBrowser(
    fakeAddonActor
  );
  const anotherDebugBrowser = await DebugUtils.getExtensionProcessBrowser(
    anotherFakeAddonActor
  );

  const chromeDocument = DebugUtils.hiddenXULWindow.chromeDocument;

  equal(
    ExtensionParent.DebugUtils.debugBrowserPromises.size,
    2,
    "Got the expected number of debug browsers requested"
  );
  ok(
    debugBrowser.parentElement === chromeDocument.documentElement,
    "The first debug browser is part of the hiddenXUL chromeDocument"
  );
  ok(
    anotherDebugBrowser.parentElement === chromeDocument.documentElement,
    "The second debug browser is part of the hiddenXUL chromeDocument"
  );

  await ExtensionParent.DebugUtils.releaseExtensionProcessBrowser(
    fakeAddonActor
  );

  equal(
    ExtensionParent.DebugUtils.debugBrowserPromises.size,
    1,
    "Got the expected number of debug browsers requested"
  );

  ok(
    anotherDebugBrowser.parentElement === chromeDocument.documentElement,
    "The second debug browser is still part of the hiddenXUL chromeDocument"
  );

  ok(
    debugBrowser.parentElement == null,
    "The first debug browser has been removed from the hiddenXUL chromeDocument"
  );

  await ExtensionParent.DebugUtils.releaseExtensionProcessBrowser(
    anotherFakeAddonActor
  );

  ok(
    anotherDebugBrowser.parentElement == null,
    "The second debug browser has been removed from the hiddenXUL chromeDocument"
  );
  equal(
    ExtensionParent.DebugUtils.debugBrowserPromises.size,
    0,
    "All the addon debugging browsers has been released"
  );

  await extension.unload();
  await anotherExtension.unload();
});
