// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals ReaderMode */

/* eslint-disable mozilla/use-chromeutils-import */

Cu.import("resource://gre/modules/ReaderMode.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");

var Reader = Services.wm.getMostRecentWindow("navigator:browser").Reader;

const URL_PREFIX = "http://mochi.test:8888/tests/robocop/reader_mode_pages/";

var TEST_PAGES = [
  {
    url: URL_PREFIX + "basic_article.html",
    expected: {
      title: "Article title",
      byline: "by Jane Doe",
      excerpt: "This is the article description.",
    }
  },
  {
    url: URL_PREFIX + "not_an_article.html",
    expected: null
  },
  {
    url: URL_PREFIX + "developer.mozilla.org/en/XULRunner/Build_Instructions.html",
    expected: {
      title: "Building XULRunner | MDN",
      byline: null,
      excerpt: "XULRunner is built using basically the same process as Firefox or other applications. Please read and follow the general Build Documentation for instructions on how to get sources and set up build prerequisites.",
    }
  },
];

add_task(async function test_article_not_found() {
  let article = await ReaderMode.getArticleFromCache(TEST_PAGES[0].url);
  do_check_eq(article, null);
});

add_task(async function test_store_article() {
  // Create an article object to store in the cache.
  await ReaderMode.storeArticleInCache({
    url: TEST_PAGES[0].url,
    content: "Lorem ipsum",
    title: TEST_PAGES[0].expected.title,
    byline: TEST_PAGES[0].expected.byline,
    excerpt: TEST_PAGES[0].expected.excerpt,
  });

  let article = await ReaderMode.getArticleFromCache(TEST_PAGES[0].url);
  checkArticle(article, TEST_PAGES[0]);
});

add_task(async function test_remove_article() {
  await ReaderMode.removeArticleFromCache(TEST_PAGES[0].url);
  let article = await ReaderMode.getArticleFromCache(TEST_PAGES[0].url);
  do_check_eq(article, null);
});

add_task(async function test_parse_articles() {
  for (let testcase of TEST_PAGES) {
    let article = await ReaderMode.downloadAndParseDocument(testcase.url);
    checkArticle(article, testcase);
  }
});

add_task(async function test_migrate_cache() {
  // Store an article in the old indexedDB reader mode cache.
  let cacheDB = await new Promise((resolve, reject) => {
    let win = Services.wm.getMostRecentWindow("navigator:browser");
    let request = win.indexedDB.open("about:reader", 1);
    request.onerror = event => reject(request.error);

    // This will always happen because there is no pre-existing data store.
    request.onupgradeneeded = event => {
      let cacheDB2 = event.target.result;
      cacheDB2.createObjectStore("articles", { keyPath: "url" });
    };

    request.onsuccess = event => resolve(event.target.result);
  });

  await new Promise((resolve, reject) => {
    let transaction = cacheDB.transaction(["articles"], "readwrite");
    let store = transaction.objectStore("articles");

    let request = store.add({
      url: TEST_PAGES[0].url,
      content: "Lorem ipsum",
      title: TEST_PAGES[0].expected.title,
      byline: TEST_PAGES[0].expected.byline,
      excerpt: TEST_PAGES[0].expected.excerpt,
    });
    request.onerror = event => reject(request.error);
    request.onsuccess = event => resolve();
  });

  // Migrate the cache.
  await Reader.migrateCache();

  // Check to make sure the article made it into the new cache.
  let article = await ReaderMode.getArticleFromCache(TEST_PAGES[0].url);
  checkArticle(article, TEST_PAGES[0]);
});

function checkArticle(article, testcase) {
  if (testcase.expected == null) {
    do_check_eq(article, null);
    return;
  }

  do_check_neq(article, null);
  do_check_eq(!!article.content, true); // A bit of a hack to avoid spamming the test log.
  do_check_eq(article.url, testcase.url);
  do_check_eq(article.title, testcase.expected.title);
  do_check_eq(article.byline, testcase.expected.byline);
  do_check_eq(article.excerpt, testcase.expected.excerpt);
}

run_next_test();
