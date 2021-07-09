/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function singleEntry({ client }) {
  const { Page } = client;

  const data = generateHistoryData(1);
  for (const entry of data) {
    await loadURL(entry.userTypedURL);
  }

  const history = await Page.getNavigationHistory();
  assertHistoryEntries(history, data, 0);
});

add_task(async function multipleEntriesWithLastIndex({ client }) {
  const { Page } = client;

  const data = generateHistoryData(3);
  for (const entry of data) {
    await loadURL(entry.userTypedURL);
  }

  const history = await Page.getNavigationHistory();
  assertHistoryEntries(history, data, data.length - 1);
});

add_task(async function multipleEntriesWithFirstIndex({ client }) {
  const { Page } = client;

  const data = generateHistoryData(3);
  for (const entry of data) {
    await loadURL(entry.userTypedURL);
  }

  await gotoHistoryIndex(0);

  const history = await Page.getNavigationHistory();
  assertHistoryEntries(history, data, 0);
});

add_task(async function locationRedirect({ client }) {
  const { Page } = client;

  const pageEmptyURL =
    "http://example.com/browser/remote/cdp/test/browser/page/doc_empty.html";
  const sjsURL =
    "http://example.com/browser/remote/cdp/test/browser/page/sjs_redirect.sjs";
  const redirectURL = `${sjsURL}?${pageEmptyURL}`;

  const data = [
    {
      url: pageEmptyURL,
      userTypedURL: redirectURL,
      title: "Empty page",
    },
  ];

  await loadURL(redirectURL, pageEmptyURL);

  const history = await Page.getNavigationHistory();
  assertHistoryEntries(history, data, 0);
});
