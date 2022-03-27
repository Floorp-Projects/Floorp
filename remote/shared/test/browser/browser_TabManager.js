/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { TabManager } = ChromeUtils.import(
  "chrome://remote/content/shared/TabManager.jsm"
);

const FRAME_URL = "https://example.com/document-builder.sjs?html=frame";
const FRAME_MARKUP = `<iframe src="${encodeURI(FRAME_URL)}"></iframe>`;
const TEST_URL = `https://example.com/document-builder.sjs?html=${encodeURI(
  FRAME_MARKUP
)}`;

add_task(async function test_getBrowsingContextById() {
  const browser = gBrowser.selectedBrowser;

  is(TabManager.getBrowsingContextById(null), null);
  is(TabManager.getBrowsingContextById(undefined), null);
  is(TabManager.getBrowsingContextById("wrong-id"), null);

  info(`Navigate to ${TEST_URL}`);
  const loaded = BrowserTestUtils.browserLoaded(browser);
  BrowserTestUtils.loadURI(browser, TEST_URL);
  await loaded;

  const contexts = browser.browsingContext.getAllBrowsingContextsInSubtree();
  is(contexts.length, 2, "Top context has 1 child");

  const topContextId = TabManager.getIdForBrowsingContext(contexts[0]);
  is(TabManager.getBrowsingContextById(topContextId), contexts[0]);
  const childContextId = TabManager.getIdForBrowsingContext(contexts[1]);
  is(TabManager.getBrowsingContextById(childContextId), contexts[1]);
});
