"use strict";

const testURLs = [
  ["http://example.com/<", "http://example.com/%3C"],
  ["http://example.com/>", "http://example.com/%3E"],
  ["http://example.com/'", "http://example.com/'"],
  ['http://example.com/"', "http://example.com/%22"],
  ["http://example.com/?<", "http://example.com/?%3C"],
  ["http://example.com/?>", "http://example.com/?%3E"],
  ["http://example.com/?'", "http://example.com/?%27"],
  ['http://example.com/?"', "http://example.com/?%22"],
];

function run_test() {
  for (var i = 0; i < testURLs.length; i++) {
    var uri = Services.io.newURI(testURLs[i][0]);
    Assert.equal(uri.spec, testURLs[i][1]);
  }
}
