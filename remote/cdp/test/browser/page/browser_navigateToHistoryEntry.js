/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function toUnknownEntryId({ client }) {
  const { Page } = client;

  const { entries } = await Page.getNavigationHistory();
  const ids = entries.map(entry => entry.id);

  await Assert.rejects(
    Page.navigateToHistoryEntry({ entryId: Math.max(...ids) + 1 }),
    /No entry with passed id/,
    "Unknown entry id raised error"
  );
});

add_task(async function toSameEntry({ client }) {
  const { Page } = client;

  const data = generateHistoryData(1);
  for (const entry of data) {
    await loadURL(entry.userTypedURL);
  }

  const { currentIndex, entries } = await Page.getNavigationHistory();
  await Page.navigateToHistoryEntry({ entryId: entries[currentIndex].id });

  const history = await Page.getNavigationHistory();
  assertHistoryEntries(history, data, 0);

  is(
    gBrowser.selectedBrowser.currentURI.spec,
    data[0].url,
    "Expected URL loaded"
  );
});

add_task(async function oneEntryBackInHistory({ client }) {
  const { Page } = client;

  const data = generateHistoryData(3);
  for (const entry of data) {
    await loadURL(entry.userTypedURL);
  }

  const { currentIndex, entries } = await Page.getNavigationHistory();
  await Page.navigateToHistoryEntry({ entryId: entries[currentIndex - 1].id });

  const history = await Page.getNavigationHistory();
  assertHistoryEntries(history, data, currentIndex - 1);

  is(
    gBrowser.selectedBrowser.currentURI.spec,
    data[currentIndex - 1].url,
    "Expected URL loaded"
  );
});

add_task(async function oneEntryForwardInHistory({ client }) {
  const { Page } = client;

  const data = generateHistoryData(3);
  for (const entry of data) {
    await loadURL(entry.userTypedURL);
  }

  await gotoHistoryIndex(0);

  const { currentIndex, entries } = await Page.getNavigationHistory();
  await Page.navigateToHistoryEntry({ entryId: entries[currentIndex + 1].id });

  const history = await Page.getNavigationHistory();
  assertHistoryEntries(history, data, currentIndex + 1);

  is(
    gBrowser.selectedBrowser.currentURI.spec,
    data[currentIndex + 1].url,
    "Expected URL loaded"
  );
});

add_task(async function toFirstEntryInHistory({ client }) {
  const { Page } = client;

  const data = generateHistoryData(3);
  for (const entry of data) {
    await loadURL(entry.userTypedURL);
  }

  const { entries } = await Page.getNavigationHistory();
  await Page.navigateToHistoryEntry({ entryId: entries[0].id });

  const history = await Page.getNavigationHistory();
  assertHistoryEntries(history, data, 0);

  is(
    gBrowser.selectedBrowser.currentURI.spec,
    data[0].url,
    "Expected URL loaded"
  );
});

add_task(async function toLastEntryInHistory({ client }) {
  const { Page } = client;

  const data = generateHistoryData(3);
  for (const entry of data) {
    await loadURL(entry.userTypedURL);
  }

  await gotoHistoryIndex(0);

  const { entries } = await Page.getNavigationHistory();
  await Page.navigateToHistoryEntry({
    entryId: entries[entries.length - 1].id,
  });

  const history = await Page.getNavigationHistory();
  assertHistoryEntries(history, data, data.length - 1);

  is(
    gBrowser.selectedBrowser.currentURI.spec,
    data[data.length - 1].url,
    "Expected URL loaded"
  );
});
