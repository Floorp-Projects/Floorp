/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// tests support for server JS-generated pages

var srv = createServer();

var sjs = do_get_file("data/sjs/cgi.sjs");
// NB: The server has no state at this point -- all state is set up and torn
//     down in the tests, because we run the same tests twice with only a
//     different query string on the requests, followed by the oddball
//     test that doesn't care about throwing or not.
srv.start(-1);
const PORT = srv.identity.primaryPort;

const BASE = "http://localhost:" + PORT;
var test;
var tests = [];

/** *******************
 * UTILITY FUNCTIONS *
 *********************/

function bytesToString(bytes) {
  return bytes
    .map(function(v) {
      return String.fromCharCode(v);
    })
    .join("");
}

function skipCache(ch) {
  ch.loadFlags |= Ci.nsIRequest.LOAD_BYPASS_CACHE;
}

/** ******************
 * DEFINE THE TESTS *
 ********************/

/**
 * Adds the set of tests defined in here, differentiating between tests with a
 * SJS which throws an exception and creates a server error and tests with a
 * normal, successful SJS.
 */
function setupTests(throwing) {
  const TEST_URL = BASE + "/cgi.sjs" + (throwing ? "?throw" : "");

  //   registerFile with SJS => raw text

  function setupFile(ch) {
    srv.registerFile("/cgi.sjs", sjs);
    skipCache(ch);
  }

  function verifyRawText(channel, status, bytes) {
    dumpn(channel.originalURI.spec);
    Assert.equal(bytesToString(bytes), fileContents(sjs));
  }

  test = new Test(TEST_URL, setupFile, null, verifyRawText);
  tests.push(test);

  //   add mapping, => interpreted

  function addTypeMapping(ch) {
    srv.registerContentType("sjs", "sjs");
    skipCache(ch);
  }

  function checkType(ch) {
    if (throwing) {
      Assert.ok(!ch.requestSucceeded);
      Assert.equal(ch.responseStatus, 500);
    } else {
      Assert.equal(ch.contentType, "text/plain");
    }
  }

  function checkContents(ch, status, data) {
    if (!throwing) {
      Assert.equal("PASS", bytesToString(data));
    }
  }

  test = new Test(TEST_URL, addTypeMapping, checkType, checkContents);
  tests.push(test);

  //   remove file/type mapping, map containing directory => raw text

  function setupDirectoryAndRemoveType(ch) {
    dumpn("removing type mapping");
    srv.registerContentType("sjs", null);
    srv.registerFile("/cgi.sjs", null);
    srv.registerDirectory("/", sjs.parent);
    skipCache(ch);
  }

  test = new Test(TEST_URL, setupDirectoryAndRemoveType, null, verifyRawText);
  tests.push(test);

  //   add mapping, => interpreted

  function contentAndCleanup(ch, status, data) {
    checkContents(ch, status, data);

    // clean up state we've set up
    srv.registerDirectory("/", null);
    srv.registerContentType("sjs", null);
  }

  test = new Test(TEST_URL, addTypeMapping, checkType, contentAndCleanup);
  tests.push(test);

  // NB: No remaining state in the server right now!  If we have any here,
  //     either the second run of tests (without ?throw) or the tests added
  //     after the two sets will almost certainly fail.
}

/** ***************
 * ADD THE TESTS *
 *****************/

setupTests(true);
setupTests(false);

// Test that when extension-mappings are used, the entire filename cannot be
// treated as an extension -- there must be at least one dot for a filename to
// match an extension.

function init(ch) {
  // clean up state we've set up
  srv.registerDirectory("/", sjs.parent);
  srv.registerContentType("sjs", "sjs");
  skipCache(ch);
}

function checkNotSJS(ch, status, data) {
  Assert.notEqual("FAIL", bytesToString(data));
}

test = new Test(BASE + "/sjs", init, null, checkNotSJS);
tests.push(test);

// Test that Range requests are passed through to the SJS file without
// bounds checking.

function rangeInit(expectedRangeHeader) {
  return function setupRangeRequest(ch) {
    ch.setRequestHeader("Range", expectedRangeHeader, false);
  };
}

function checkRangeResult(ch) {
  try {
    var val = ch.getResponseHeader("Content-Range");
  } catch (e) {
    /* IDL doesn't specify a particular exception to require */
  }
  if (val !== undefined) {
    do_throw(
      "should not have gotten a Content-Range header, but got one " +
        "with this value: " +
        val
    );
  }
  Assert.equal(200, ch.responseStatus);
  Assert.equal("OK", ch.responseStatusText);
}

test = new Test(
  BASE + "/range-checker.sjs",
  rangeInit("not-a-bytes-equals-specifier"),
  checkRangeResult,
  null
);
tests.push(test);
test = new Test(
  BASE + "/range-checker.sjs",
  rangeInit("bytes=-"),
  checkRangeResult,
  null
);
tests.push(test);
test = new Test(
  BASE + "/range-checker.sjs",
  rangeInit("bytes=1000000-"),
  checkRangeResult,
  null
);
tests.push(test);
test = new Test(
  BASE + "/range-checker.sjs",
  rangeInit("bytes=1-4"),
  checkRangeResult,
  null
);
tests.push(test);
test = new Test(
  BASE + "/range-checker.sjs",
  rangeInit("bytes=-4"),
  checkRangeResult,
  null
);
tests.push(test);

// One last test: for file mappings, the content-type is determined by the
// extension of the file on the server, not by the extension of the requested
// path.

function setupFileMapping(ch) {
  srv.registerFile("/script.html", sjs);
}

function onStart(ch) {
  Assert.equal(ch.contentType, "text/plain");
}

function onStop(ch, status, data) {
  Assert.equal("PASS", bytesToString(data));
}

test = new Test(BASE + "/script.html", setupFileMapping, onStart, onStop);
tests.push(test);

/** ***************
 * RUN THE TESTS *
 *****************/

function run_test() {
  // Test for a content-type which isn't a field-value
  try {
    srv.registerContentType("foo", "bar\nbaz");
    throw new Error(
      "this server throws on content-types which aren't field-values"
    );
  } catch (e) {
    isException(e, Cr.NS_ERROR_INVALID_ARG);
  }
  runHttpTests(tests, testComplete(srv));
}
