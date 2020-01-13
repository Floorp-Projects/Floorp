/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function testReload({ client }) {
  const { Page } = client;
  await loadURL(toDataURL("halløj"));

  info("Reloading document");
  await Page.enable();
  const loaded = Page.loadEventFired();
  await Page.reload();
  await loaded;

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    ok(!content.docShell.isForceReloading, "Document is not force-reloaded");
  });
});

add_task(async function testReloadIgnoreCache({ client }) {
  const { Page } = client;
  await loadURL(toDataURL("halløj"));

  info("Force-reloading document");
  await Page.enable();
  const loaded = Page.loadEventFired();
  await Page.reload({ ignoreCache: true });
  await loaded;

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    ok(content.docShell.isForceReloading, "Document is force-reloaded");
  });
});
