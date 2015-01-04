// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { utils: Cu } = Components;

Cu.import("resource://gre/modules/ReaderMode.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");

let Reader = Services.wm.getMostRecentWindow("navigator:browser").Reader;

const URL_PREFIX = "http://mochi.test:8888/tests/robocop/reader_mode_pages/";

let TEST_PAGES = [
  {
    url: URL_PREFIX + "basic_article.html",
    expected: {
      title: "Article title",
      byline: "by Jane Doe",
      excerpt: "This is the article description.",
      length: 1931
    }
  },
  {
    url: URL_PREFIX + "addons.mozilla.org/en-US/firefox/index.html",
    expected: null
  },
  {
    url: URL_PREFIX + "developer.mozilla.org/en/XULRunner/Build_Instructions.html",
    expected: {
      title: "Building XULRunner | MDN",
      byline: null,
      excerpt: "XULRunner is built using basically the same process as Firefox or other applications. Please read and follow the general Build Documentation for instructions on how to get sources and set up build prerequisites.",
      length: 2300
    }
  },
];

add_task(function* test_article_not_found() {
  let uri = Services.io.newURI(TEST_PAGES[0].url, null, null);
  let article = yield ReaderMode.getArticleFromCache(uri);
  do_check_eq(article, null);
});

add_task(function* test_store_article() {
  // Create an article object to store in the cache.
  yield ReaderMode.storeArticleInCache({
    url: TEST_PAGES[0].url,
    content: "Lorem ipsum",
    title: TEST_PAGES[0].expected.title,
    byline: TEST_PAGES[0].expected.byline,
    excerpt: TEST_PAGES[0].expected.excerpt,
    length: TEST_PAGES[0].expected.length
  });

  let uri = Services.io.newURI(TEST_PAGES[0].url, null, null);
  let article = yield ReaderMode.getArticleFromCache(uri);
  checkArticle(article, TEST_PAGES[0]);
});

add_task(function* test_remove_article() {
  let uri = Services.io.newURI(TEST_PAGES[0].url, null, null);
  yield ReaderMode.removeArticleFromCache(uri);
  let article = yield ReaderMode.getArticleFromCache(uri);
  do_check_eq(article, null);
});

add_task(function* test_parse_articles() {
  for (let testcase of TEST_PAGES) {
    let article = yield ReaderMode.downloadAndParseDocument(testcase.url);
    checkArticle(article, testcase);
  }
});

add_task(function* test_migrate_cache() {
  // Store an article in the old indexedDB reader mode cache.
  let cacheDB = yield new Promise((resolve, reject) => {
    let win = Services.wm.getMostRecentWindow("navigator:browser");
    let request = win.indexedDB.open("about:reader", 1);
    request.onerror = event => reject(request.error);

    // This will always happen because there is no pre-existing data store.
    request.onupgradeneeded = event => {
      let cacheDB = event.target.result;
      cacheDB.createObjectStore("articles", { keyPath: "url" });
    };

    request.onsuccess = event => resolve(event.target.result);
  });

  yield new Promise((resolve, reject) => {
    let transaction = cacheDB.transaction(["articles"], "readwrite");
    let store = transaction.objectStore("articles");

    let request = store.add({
      url: TEST_PAGES[0].url,
      content: "Lorem ipsum",
      title: TEST_PAGES[0].expected.title,
      byline: TEST_PAGES[0].expected.byline,
      excerpt: TEST_PAGES[0].expected.excerpt,
      length: TEST_PAGES[0].expected.length
    });
    request.onerror = event => reject(request.error);
    request.onsuccess = event => resolve();
  });

  // Migrate the cache.
  yield Reader.migrateCache();

  // Check to make sure the article made it into the new cache.
  let uri = Services.io.newURI(TEST_PAGES[0].url, null, null);
  let article = yield ReaderMode.getArticleFromCache(uri);
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
  do_check_eq(article.length, testcase.expected.length);
}

run_next_test();
