/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests getSite and getSchemelessSite with example arguments
 */

"use strict";

add_task(() => {
  for (let [originString, result] of [
    ["http://.", null],
    ["http://com", "http://com"],
    ["http://test", "http://test"],
    ["http://test.", "http://test."],
    ["http://[::1]", "http://[::1]"],
    ["http://[::1]:8888", "http://[::1]"],
    ["http://localhost", "http://localhost"],
    ["http://127.0.0.1", "http://127.0.0.1"],
    ["http://user:pass@[::1]", "http://[::1]"],
    ["http://example.com", "http://example.com"],
    ["https://example.com", "https://example.com"],
    ["https://test.example.com", "https://example.com"],
    ["https://test1.test2.example.com", "https://example.com"],
    ["https://test1.test2.example.co.uk", "https://example.co.uk"],
    ["https://test.example.com:8888/index.html", "https://example.com"],
    [
      "https://test1.test2.example.公司.香港",
      "https://example.xn--55qx5d.xn--j6w193g",
    ],
  ]) {
    let origin = Services.io.newURI(originString);
    if (result === null) {
      Assert.throws(
        () => {
          Services.eTLD.getSite(origin);
        },
        /NS_ERROR_ILLEGAL_VALUE/,
        "Invalid origin for getSite throws"
      );
    } else {
      let answer = Services.eTLD.getSite(origin);
      Assert.equal(
        answer,
        result,
        `"${originString}" should have site ${result}, got ${answer}.`
      );
    }
  }
});

add_task(() => {
  for (let [originString, result] of [
    ["http://com", "com"],
    ["http://test", "test"],
    ["http://test.", "test."],
    ["http://[::1]", "[::1]"],
    ["http://[::1]:8888", "[::1]"],
    ["http://localhost", "localhost"],
    ["http://127.0.0.1", "127.0.0.1"],
    ["http://user:pass@[::1]", "[::1]"],
    ["http://example.com", "example.com"],
    ["https://example.com", "example.com"],
    ["https://test.example.com", "example.com"],
    ["https://test1.test2.example.com", "example.com"],
    ["https://test1.test2.example.co.uk", "example.co.uk"],
    ["https://test1.test2.example.公司.香港", "example.xn--55qx5d.xn--j6w193g"],
  ]) {
    let origin = Services.io.newURI(originString);
    let answer = Services.eTLD.getSchemelessSite(origin);
    Assert.equal(
      answer,
      result,
      `"${originString}" should have schemeless site ${result}, got ${answer}.`
    );
  }
});
