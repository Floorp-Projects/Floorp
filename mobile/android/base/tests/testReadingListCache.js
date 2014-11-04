// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { utils: Cu } = Components;

Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");

// Values from robocop_article.html
const ARTICLE = {
  url: "http://mochi.test:8888/tests/robocop/robocop_article.html",
  title: "Article title",
  byline: "by Jane Doe",
  excerpt: "This is the article description.",
  length: 1931,
  content: "Lorem ipsum..." // This test doesn't actually compare content strings
};

const ARTICLE_URI = Services.io.newURI(ARTICLE.url, null, null);

add_task(function test_article_not_found() {
  let Reader = Services.wm.getMostRecentWindow("navigator:browser").Reader;

  let article = yield Reader.getArticleFromCache(ARTICLE_URI);
  do_check_eq(article, null);
});

add_task(function test_store_article() {
  let Reader = Services.wm.getMostRecentWindow("navigator:browser").Reader;

  yield Reader.storeArticleInCache(ARTICLE);

  let article = yield Reader.getArticleFromCache(ARTICLE_URI);
  checkArticle(article);
});

add_task(function test_remove_article() {
  let Reader = Services.wm.getMostRecentWindow("navigator:browser").Reader;

  yield Reader.removeArticleFromCache(ARTICLE_URI);

  let article = yield Reader.getArticleFromCache(ARTICLE_URI);
  do_check_eq(article, null);
});

add_test(function test_parse_article() {
  let Reader = Services.wm.getMostRecentWindow("navigator:browser").Reader;

  Reader.parseDocumentFromURL(ARTICLE.url, function parseCallback(article) {
    checkArticle(article);
    run_next_test();
  });
});

function checkArticle(article) {
  do_check_neq(article, null);
  do_check_neq(article.content, null);
  do_check_eq(article.url, ARTICLE.url);
  do_check_eq(article.title, ARTICLE.title);
  do_check_eq(article.byline, ARTICLE.byline);
  do_check_eq(article.excerpt, ARTICLE.excerpt);
  do_check_eq(article.length, ARTICLE.length);
}

run_next_test();
