/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from ../head.js */

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/remote/test/browser/head.js",
  this
);

function assertHistoryEntries(history, expectedData, expectedIndex) {
  const { currentIndex, entries } = history;

  is(currentIndex, expectedIndex, "Got expected current index");
  is(
    entries.length,
    expectedData.length,
    "Found expected count of history entries"
  );

  entries.forEach((entry, index) => {
    ok(!!entry.id, "History entry has an id set");
    is(
      entry.url,
      expectedData[index].url,
      "History entry has the correct URL set"
    );
    is(
      entry.userTypedURL,
      expectedData[index].userTypedURL,
      "History entry has the correct user typed URL set"
    );
    is(
      entry.title,
      expectedData[index].title,
      "History entry has the correct title set"
    );
  });
}

async function getContentSize() {
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    const docEl = content.document.documentElement;

    return {
      x: 0,
      y: 0,
      width: docEl.scrollWidth,
      height: docEl.scrollHeight,
    };
  });
}

async function getViewportSize() {
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    return {
      x: content.pageXOffset,
      y: content.pageYOffset,
      width: content.innerWidth,
      height: content.innerHeight,
    };
  });
}

function generateHistoryData(count) {
  const data = [];

  for (let index = 0; index < count; index++) {
    const url = toDataURL(`<head><title>Test ${index + 1}</title></head>`);
    data.push({
      url,
      userTypedURL: url,
      title: `Test ${index + 1}`,
    });
  }

  return data;
}
