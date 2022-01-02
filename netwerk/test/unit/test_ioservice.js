"use strict";

add_task(function test_extractScheme() {
  equal(Services.io.extractScheme("HtTp://example.com"), "http");
  Assert.throws(
    () => {
      Services.io.extractScheme("://example.com");
    },
    /NS_ERROR_MALFORMED_URI/,
    "missing scheme"
  );
  Assert.throws(
    () => {
      Services.io.extractScheme("ht%tp://example.com");
    },
    /NS_ERROR_MALFORMED_URI/,
    "bad scheme"
  );
});
