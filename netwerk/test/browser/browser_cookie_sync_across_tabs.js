/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

add_task(async function() {
  info("Make sure cookie changes in one process are visible in the other");

  const kRoot = getRootDirectory(gTestPath).replace("chrome://mochitests/content/",
                                                    "https://example.com/");
  const kTestPage = kRoot + "dummy.html";

  Services.cookies.removeAll();

  let tab1 = await BrowserTestUtils.openNewForegroundTab({ gBrowser, url: kTestPage, forceNewProcess: true });
  let tab2 = await BrowserTestUtils.openNewForegroundTab({ gBrowser, url: kTestPage, forceNewProcess: true });

  let browser1 = gBrowser.getBrowserForTab(tab1);
  let browser2 = gBrowser.getBrowserForTab(tab2);

  let pid1 = browser1.frameLoader.tabParent.osPid;
  let pid2 = browser2.frameLoader.tabParent.osPid;

  // Note, this might not be true once fission is implemented (Bug 1451850)
  ok(pid1 != pid2, "We should have different processes here.");

  await ContentTask.spawn(browser1, null, async function() {
    is(content.document.cookie, "", "Expecting no cookies");
  });

  await ContentTask.spawn(browser2, null, async function() {
    is(content.document.cookie, "", "Expecting no cookies");
  });

  await ContentTask.spawn(browser1, null, async function() {
    content.document.cookie = "a1=test";
  });

  await ContentTask.spawn(browser2, null, async function() {
    is(content.document.cookie, "a1=test", "Cookie should be set");
    content.document.cookie = "a1=other_test";
  });

  await ContentTask.spawn(browser1, null, async function() {
    is(content.document.cookie, "a1=other_test", "Cookie should be set");
    content.document.cookie = "a2=again";
  });

  await ContentTask.spawn(browser2, null, async function() {
    is(content.document.cookie, "a1=other_test; a2=again", "Cookie should be set");
    content.document.cookie = "a1=; expires=Thu, 01-Jan-1970 00:00:01 GMT;";
    content.document.cookie = "a2=; expires=Thu, 01-Jan-1970 00:00:01 GMT;";
  });

  await ContentTask.spawn(browser1, null, async function() {
    is(content.document.cookie, "", "Cookies should be cleared");
  });

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);

  ok(true, "Got to the end of the test!");
});
