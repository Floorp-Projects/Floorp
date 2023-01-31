/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/remote/cdp/test/browser/head.js",
  this
);

const { PollPromise } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/Sync.sys.mjs"
);

const BASE_ORIGIN = "https://example.com";
const BASE_PATH = `${BASE_ORIGIN}/browser/remote/cdp/test/browser/page`;
const FRAMESET_MULTI_URL = `${BASE_PATH}/doc_frameset_multi.html`;
const FRAMESET_NESTED_URL = `${BASE_PATH}/doc_frameset_nested.html`;
const FRAMESET_SINGLE_URL = `${BASE_PATH}/doc_frameset_single.html`;
const PAGE_FRAME_URL = `${BASE_PATH}/doc_frame.html`;
const PAGE_URL = `${BASE_PATH}/doc_empty.html`;

const TIMEOUT_SET_HISTORY_INDEX = 1000;

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

function getCurrentHistoryIndex() {
  return new Promise(resolve => {
    SessionStore.getSessionHistory(window.gBrowser.selectedTab, history => {
      resolve(history.index);
    });
  });
}

async function gotoHistoryIndex(index) {
  gBrowser.gotoIndex(index);

  // On some platforms the requested index isn't set immediately.
  await PollPromise(
    async (resolve, reject) => {
      const currentIndex = await getCurrentHistoryIndex();
      if (currentIndex == index) {
        resolve();
      } else {
        reject();
      }
    },
    { timeout: TIMEOUT_SET_HISTORY_INDEX }
  );
}
