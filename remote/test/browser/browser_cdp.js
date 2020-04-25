/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test very basic CDP features.
add_task(async function testCDP({ client }) {
  const { Browser, Log, Page } = client;

  ok("Browser" in client, "Browser domain is available");
  ok("Log" in client, "Log domain is available");
  ok("Page" in client, "Page domain is available");

  const version = await Browser.getVersion();
  const { isHeadless } = Cc["@mozilla.org/gfx/info;1"].getService(
    Ci.nsIGfxInfo
  );
  is(
    version.product,
    isHeadless ? "Headless Firefox" : "Firefox",
    "Browser.getVersion works and depends on headless mode"
  );
  is(
    version.userAgent,
    window.navigator.userAgent,
    "Browser.getVersion().userAgent is correct"
  );

  // receive console.log messages and print them
  Log.enable();
  info("Log domain has been enabled");

  Log.entryAdded(({ entry }) => {
    const { timestamp, level, text, args } = entry;
    const msg = text || args.join(" ");
    console.log(`${new Date(timestamp)}\t${level.toUpperCase()}\t${msg}`);
  });

  // turn on navigation related events, such as DOMContentLoaded et al.
  await Page.enable();
  info("Page domain has been enabled");

  const frameStoppedLoading = Page.frameStoppedLoading();
  const navigatedWithinDocument = Page.navigatedWithinDocument();
  const loadEventFired = Page.loadEventFired();
  await Page.navigate({
    url: toDataURL(`<script>console.log("foo")</script>`),
  });
  info("A new page has been requested");

  await loadEventFired;
  info("`Page.loadEventFired` fired");

  await frameStoppedLoading;
  info("`Page.frameStoppedLoading` fired");

  await navigatedWithinDocument;
  info("`Page.navigatedWithinDocument` fired");
});
