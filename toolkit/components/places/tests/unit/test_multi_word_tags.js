/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Get history service
try {
  var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].getService(
    Ci.nsINavHistoryService
  );
} catch (ex) {
  do_throw("Could not get history service\n");
}

// Get tagging service
try {
  var tagssvc = Cc["@mozilla.org/browser/tagging-service;1"].getService(
    Ci.nsITaggingService
  );
} catch (ex) {
  do_throw("Could not get tagging service\n");
}

add_task(async function run_test() {
  var uri1 = uri("http://site.tld/1");
  var uri2 = uri("http://site.tld/2");
  var uri3 = uri("http://site.tld/3");
  var uri4 = uri("http://site.tld/4");
  var uri5 = uri("http://site.tld/5");
  var uri6 = uri("http://site.tld/6");

  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [
      { url: uri1 },
      { url: uri2 },
      { url: uri3 },
      { url: uri4 },
      { url: uri5 },
      { url: uri6 },
    ],
  });

  tagssvc.tagURI(uri1, ["foo"]);
  tagssvc.tagURI(uri2, ["bar"]);
  tagssvc.tagURI(uri3, ["cheese"]);
  tagssvc.tagURI(uri4, ["foo bar"]);
  tagssvc.tagURI(uri5, ["bar cheese"]);
  tagssvc.tagURI(uri6, ["foo bar cheese"]);

  // Search for "item", should get one result
  var options = histsvc.getNewQueryOptions();
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;

  var query = histsvc.getNewQuery();
  query.searchTerms = "foo";
  var result = histsvc.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  Assert.equal(root.childCount, 3);
  Assert.equal(root.getChild(0).uri, "http://site.tld/1");
  Assert.equal(root.getChild(1).uri, "http://site.tld/4");
  Assert.equal(root.getChild(2).uri, "http://site.tld/6");
  root.containerOpen = false;

  query.searchTerms = "bar";
  result = histsvc.executeQuery(query, options);
  root = result.root;
  root.containerOpen = true;
  Assert.equal(root.childCount, 4);
  Assert.equal(root.getChild(0).uri, "http://site.tld/2");
  Assert.equal(root.getChild(1).uri, "http://site.tld/4");
  Assert.equal(root.getChild(2).uri, "http://site.tld/5");
  Assert.equal(root.getChild(3).uri, "http://site.tld/6");
  root.containerOpen = false;

  query.searchTerms = "cheese";
  result = histsvc.executeQuery(query, options);
  root = result.root;
  root.containerOpen = true;
  Assert.equal(root.childCount, 3);
  Assert.equal(root.getChild(0).uri, "http://site.tld/3");
  Assert.equal(root.getChild(1).uri, "http://site.tld/5");
  Assert.equal(root.getChild(2).uri, "http://site.tld/6");
  root.containerOpen = false;

  query.searchTerms = "foo bar";
  result = histsvc.executeQuery(query, options);
  root = result.root;
  root.containerOpen = true;
  Assert.equal(root.childCount, 2);
  Assert.equal(root.getChild(0).uri, "http://site.tld/4");
  Assert.equal(root.getChild(1).uri, "http://site.tld/6");
  root.containerOpen = false;

  query.searchTerms = "bar foo";
  result = histsvc.executeQuery(query, options);
  root = result.root;
  root.containerOpen = true;
  Assert.equal(root.childCount, 2);
  Assert.equal(root.getChild(0).uri, "http://site.tld/4");
  Assert.equal(root.getChild(1).uri, "http://site.tld/6");
  root.containerOpen = false;

  query.searchTerms = "bar cheese";
  result = histsvc.executeQuery(query, options);
  root = result.root;
  root.containerOpen = true;
  Assert.equal(root.childCount, 2);
  Assert.equal(root.getChild(0).uri, "http://site.tld/5");
  Assert.equal(root.getChild(1).uri, "http://site.tld/6");
  root.containerOpen = false;

  query.searchTerms = "cheese bar";
  result = histsvc.executeQuery(query, options);
  root = result.root;
  root.containerOpen = true;
  Assert.equal(root.childCount, 2);
  Assert.equal(root.getChild(0).uri, "http://site.tld/5");
  Assert.equal(root.getChild(1).uri, "http://site.tld/6");
  root.containerOpen = false;

  query.searchTerms = "foo bar cheese";
  result = histsvc.executeQuery(query, options);
  root = result.root;
  root.containerOpen = true;
  Assert.equal(root.childCount, 1);
  Assert.equal(root.getChild(0).uri, "http://site.tld/6");
  root.containerOpen = false;

  query.searchTerms = "cheese foo bar";
  result = histsvc.executeQuery(query, options);
  root = result.root;
  root.containerOpen = true;
  Assert.equal(root.childCount, 1);
  Assert.equal(root.getChild(0).uri, "http://site.tld/6");
  root.containerOpen = false;

  query.searchTerms = "cheese bar foo";
  result = histsvc.executeQuery(query, options);
  root = result.root;
  root.containerOpen = true;
  Assert.equal(root.childCount, 1);
  Assert.equal(root.getChild(0).uri, "http://site.tld/6");
  root.containerOpen = false;
});
