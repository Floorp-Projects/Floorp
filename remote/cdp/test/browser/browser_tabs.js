/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test very basic CDP features.

const TEST_URL = toDataURL("default-test-page");

add_task(async function({ CDP }) {
  // Use gBrowser.addTab instead of BrowserTestUtils as it creates the tab differently.
  // It demonstrates a race around tab.linkedBrowser.browsingContext being undefined
  // when accessing this property early.
  const tab = gBrowser.addTab(TEST_URL, {
    skipAnimation: true,
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
  });

  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  let targets = await getTargets(CDP);
  ok(
    targets.some(target => target.url == TEST_URL),
    "Found the tab in target list"
  );

  BrowserTestUtils.removeTab(tab);

  targets = await getTargets(CDP);
  ok(
    !targets.some(target => target.url == TEST_URL),
    "Tab has been removed from the target list"
  );
});
