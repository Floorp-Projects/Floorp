/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

var charset = {};
var hadCharset = {};
var type;

function reset() {
  delete charset.value;
  delete hadCharset.value;
  type = undefined;
}

function check(aType, aCharset, aHadCharset) {
  Assert.equal(type, aType);
  Assert.equal(aCharset, charset.value);
  Assert.equal(aHadCharset, hadCharset.value);
  reset();
}

add_task(function test_parseResponseContentType() {
  var netutil = Cc["@mozilla.org/network/util;1"].getService(Ci.nsINetUtil);

  type = netutil.parseRequestContentType("text/html", charset, hadCharset);
  check("text/html", "", false);

  type = netutil.parseResponseContentType("text/html", charset, hadCharset);
  check("text/html", "", false);

  type = netutil.parseRequestContentType("TEXT/HTML", charset, hadCharset);
  check("text/html", "", false);

  type = netutil.parseResponseContentType("TEXT/HTML", charset, hadCharset);
  check("text/html", "", false);

  type = netutil.parseRequestContentType(
    "text/html, text/html",
    charset,
    hadCharset
  );
  check("", "", false);

  type = netutil.parseResponseContentType(
    "text/html, text/html",
    charset,
    hadCharset
  );
  check("text/html", "", false);

  type = netutil.parseRequestContentType(
    "text/html, text/plain",
    charset,
    hadCharset
  );
  check("", "", false);

  type = netutil.parseResponseContentType(
    "text/html, text/plain",
    charset,
    hadCharset
  );
  check("text/plain", "", false);

  type = netutil.parseRequestContentType("text/html, ", charset, hadCharset);
  check("", "", false);

  type = netutil.parseResponseContentType("text/html, ", charset, hadCharset);
  check("text/html", "", false);

  type = netutil.parseRequestContentType("text/html, */*", charset, hadCharset);
  check("", "", false);

  type = netutil.parseResponseContentType(
    "text/html, */*",
    charset,
    hadCharset
  );
  check("text/html", "", false);

  type = netutil.parseRequestContentType("text/html, foo", charset, hadCharset);
  check("", "", false);

  type = netutil.parseResponseContentType(
    "text/html, foo",
    charset,
    hadCharset
  );
  check("text/html", "", false);

  type = netutil.parseRequestContentType(
    "text/html; charset=ISO-8859-1",
    charset,
    hadCharset
  );
  check("text/html", "ISO-8859-1", true);

  type = netutil.parseResponseContentType(
    "text/html; charset=ISO-8859-1",
    charset,
    hadCharset
  );
  check("text/html", "ISO-8859-1", true);

  type = netutil.parseRequestContentType(
    'text/html; charset="ISO-8859-1"',
    charset,
    hadCharset
  );
  check("text/html", "ISO-8859-1", true);

  type = netutil.parseResponseContentType(
    'text/html; charset="ISO-8859-1"',
    charset,
    hadCharset
  );
  check("text/html", "ISO-8859-1", true);

  type = netutil.parseRequestContentType(
    "text/html; charset='ISO-8859-1'",
    charset,
    hadCharset
  );
  check("text/html", "'ISO-8859-1'", true);

  type = netutil.parseResponseContentType(
    "text/html; charset='ISO-8859-1'",
    charset,
    hadCharset
  );
  check("text/html", "'ISO-8859-1'", true);

  type = netutil.parseRequestContentType(
    'text/html; charset="ISO-8859-1", text/html',
    charset,
    hadCharset
  );
  check("", "", false);

  type = netutil.parseResponseContentType(
    'text/html; charset="ISO-8859-1", text/html',
    charset,
    hadCharset
  );
  check("text/html", "ISO-8859-1", true);

  type = netutil.parseRequestContentType(
    'text/html; charset="ISO-8859-1", text/html; charset=UTF8',
    charset,
    hadCharset
  );
  check("", "", false);

  type = netutil.parseResponseContentType(
    'text/html; charset="ISO-8859-1", text/html; charset=UTF8',
    charset,
    hadCharset
  );
  check("text/html", "UTF8", true);

  type = netutil.parseRequestContentType(
    "text/html; charset=ISO-8859-1, TEXT/HTML",
    charset,
    hadCharset
  );
  check("", "", false);

  type = netutil.parseResponseContentType(
    "text/html; charset=ISO-8859-1, TEXT/HTML",
    charset,
    hadCharset
  );
  check("text/html", "ISO-8859-1", true);

  type = netutil.parseRequestContentType(
    "text/html; charset=ISO-8859-1, TEXT/plain",
    charset,
    hadCharset
  );
  check("", "", false);

  type = netutil.parseResponseContentType(
    "text/html; charset=ISO-8859-1, TEXT/plain",
    charset,
    hadCharset
  );
  check("text/plain", "", true);

  type = netutil.parseRequestContentType(
    "text/plain, TEXT/HTML; charset=ISO-8859-1, text/html, TEXT/HTML",
    charset,
    hadCharset
  );
  check("", "", false);

  type = netutil.parseResponseContentType(
    "text/plain, TEXT/HTML; charset=ISO-8859-1, text/html, TEXT/HTML",
    charset,
    hadCharset
  );
  check("text/html", "ISO-8859-1", true);

  type = netutil.parseRequestContentType(
    'text/plain, TEXT/HTML; param="charset=UTF8"; charset="ISO-8859-1"; param2="charset=UTF16", text/html, TEXT/HTML',
    charset,
    hadCharset
  );
  check("", "", false);

  type = netutil.parseResponseContentType(
    'text/plain, TEXT/HTML; param="charset=UTF8"; charset="ISO-8859-1"; param2="charset=UTF16", text/html, TEXT/HTML',
    charset,
    hadCharset
  );
  check("text/html", "ISO-8859-1", true);

  type = netutil.parseRequestContentType(
    'text/plain, TEXT/HTML; param=charset=UTF8; charset="ISO-8859-1"; param2=charset=UTF16, text/html, TEXT/HTML',
    charset,
    hadCharset
  );
  check("", "", false);

  type = netutil.parseResponseContentType(
    'text/plain, TEXT/HTML; param=charset=UTF8; charset="ISO-8859-1"; param2=charset=UTF16, text/html, TEXT/HTML',
    charset,
    hadCharset
  );
  check("text/html", "ISO-8859-1", true);

  type = netutil.parseRequestContentType(
    'text/plain, TEXT/HTML; param=charset=UTF8; charset="ISO-8859-1"; param2=charset=UTF16, text/html, TEXT/HTML',
    charset,
    hadCharset
  );
  check("", "", false);

  type = netutil.parseRequestContentType(
    "text/plain; param= , text/html",
    charset,
    hadCharset
  );
  check("", "", false);

  type = netutil.parseResponseContentType(
    "text/plain; param= , text/html",
    charset,
    hadCharset
  );
  check("text/html", "", false);

  type = netutil.parseRequestContentType(
    'text/plain; param=", text/html"',
    charset,
    hadCharset
  );
  check("text/plain", "", false);

  type = netutil.parseResponseContentType(
    'text/plain; param=", text/html"',
    charset,
    hadCharset
  );
  check("text/plain", "", false);

  type = netutil.parseRequestContentType(
    'text/plain; param=", \\" , text/html"',
    charset,
    hadCharset
  );
  check("text/plain", "", false);

  type = netutil.parseResponseContentType(
    'text/plain; param=", \\" , text/html"',
    charset,
    hadCharset
  );
  check("text/plain", "", false);

  type = netutil.parseRequestContentType(
    'text/plain; param=", \\" , text/html , "',
    charset,
    hadCharset
  );
  check("text/plain", "", false);

  type = netutil.parseResponseContentType(
    'text/plain; param=", \\" , text/html , "',
    charset,
    hadCharset
  );
  check("text/plain", "", false);

  type = netutil.parseRequestContentType(
    'text/plain param=", \\" , text/html , "',
    charset,
    hadCharset
  );
  check("", "", false);

  type = netutil.parseResponseContentType(
    'text/plain param=", \\" , text/html , "',
    charset,
    hadCharset
  );
  check("text/plain", "", false);

  type = netutil.parseRequestContentType(
    "text/plain charset=UTF8",
    charset,
    hadCharset
  );
  check("", "", false);

  type = netutil.parseResponseContentType(
    "text/plain charset=UTF8",
    charset,
    hadCharset
  );
  check("text/plain", "", false);

  type = netutil.parseRequestContentType(
    'text/plain, TEXT/HTML; param="charset=UTF8"; ; param2="charset=UTF16", text/html, TEXT/HTML',
    charset,
    hadCharset
  );
  check("", "", false);

  type = netutil.parseResponseContentType(
    'text/plain, TEXT/HTML; param="charset=UTF8"; ; param2="charset=UTF16", text/html, TEXT/HTML',
    charset,
    hadCharset
  );
  check("text/html", "", false);

  // Bug 562915 - correctness: "\x" is "x"
  type = netutil.parseResponseContentType(
    'text/plain; charset="UTF\\-8"',
    charset,
    hadCharset
  );
  check("text/plain", "UTF-8", true);

  // Bug 700589

  // check that single quote doesn't confuse parsing of subsequent parameters
  type = netutil.parseResponseContentType(
    'text/plain; x=\'; charset="UTF-8"',
    charset,
    hadCharset
  );
  check("text/plain", "UTF-8", true);

  // check that single quotes do not get removed from extracted charset
  type = netutil.parseResponseContentType(
    "text/plain; charset='UTF-8'",
    charset,
    hadCharset
  );
  check("text/plain", "'UTF-8'", true);
});

function checkSnapshotValue(s, name, expectedValue) {
  function telemetryCategoryIndex(name) {
    let labels = [
      "NoHeader",
      "EmptyHeader",
      "FailedToParse",
      "ParsedTextCSS",
      "ParsedOther",
    ];
    for (let i in labels) {
      if (labels[i] == name) {
        return i;
      }
    }
    ok(false, "invalid category");
    return;
  }

  if (!name) {
    Assert.deepEqual(s.values, {}, "No probes should be recorded");
    return;
  }

  let index = telemetryCategoryIndex(name);
  equal(
    s.values[index],
    expectedValue,
    `Checking the value of ${name} to be ${expectedValue} - recorded probes: ${JSON.stringify(
      s.values
    )} index: ${index}`
  );

  for (let i in s.values) {
    if (i != index && s.values[i] != 0) {
      equal(s.values[i], 0, `The record at ${i} should be 0`);
    }
  }
}

function makeChan(url, type = Ci.nsIContentPolicy.TYPE_STYLESHEET) {
  let principal = Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI("http://example.com"),
    {
      privateBrowsingId: 0,
    }
  );

  return NetUtil.newChannel({
    uri: url,
    loadingPrincipal: principal,
    contentPolicyType: type,
    securityFlags: Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
  }).QueryInterface(Ci.nsIHttpChannel);
}

add_task(async function check_channels() {
  do_get_profile();
  // This test uses a histogram which isn't enabled on for all products
  // (Thunderbird is missing it in this case).
  Services.prefs.setBoolPref(
    "toolkit.telemetry.testing.overrideProductsCheck",
    true
  );
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref(
      "toolkit.telemetry.testing.overrideProductsCheck"
    );
  });

  let contentType = "text/css;hi";
  let content = `.identity-color-blue {
    --identity-tab-color: #37adff;
    --identity-icon-color: #37adff;
}`;

  function handler(metadata, response) {
    if (contentType !== undefined) {
      response.setHeader("Content-Type", contentType);
    }
    response.bodyOutputStream.write(content, content.length);
  }
  let httpserv = new HttpServer();
  httpserv.registerPathHandler("/", handler);
  httpserv.start(-1);
  const URL = `http://localhost:${httpserv.identity.primaryPort}/`;

  let h = Services.telemetry.getHistogramById(
    "NETWORK_CROSS_ORIGIN_STYLESHEET_CONTENT_TYPE"
  );
  h.clear();

  function make_test(
    inContentType,
    expectedContentType,
    expectedProbe,
    typeHint,
    contentPolicyType
  ) {
    return new Promise(resolve => {
      contentType = inContentType;
      let channel = makeChan(URL, contentPolicyType);
      if (typeHint) {
        channel.contentType = typeHint;
      }
      let p = new Promise(resolve1 =>
        channel.asyncOpen(new ChannelListener(resolve1))
      );
      p.then(() => {
        equal(channel.contentType, expectedContentType);
        checkSnapshotValue(h.snapshot(), expectedProbe, 1);
        h.clear();
        resolve();
      });
    });
  }

  // If contentTypeHint != text/css and contentPolicy is not TYPE_STYLESHEET
  // no telemetry probes should be recorded.
  await make_test(
    undefined,
    "text/plain",
    undefined,
    undefined,
    Ci.nsIContentPolicy.TYPE_OTHER
  );
  await make_test(
    undefined,
    "text/plain",
    undefined,
    "text/plain",
    Ci.nsIContentPolicy.TYPE_OTHER
  );

  // We should record a probe if the contentTypeHint is text/css
  await make_test(
    undefined,
    "text/css",
    "NoHeader",
    "text/css",
    Ci.nsIContentPolicy.TYPE_OTHER
  );

  // The rest of the tests will default to contentPolicy = TYPE_STYLESHEET
  // One telemetry probe should be recorded for each.
  await make_test(undefined, "text/plain", "NoHeader");
  await make_test(undefined, "text/abc", "NoHeader", "text/abc");
  await make_test(undefined, "text/css", "NoHeader", "text/css");
  await make_test("", "text/plain", "EmptyHeader");
  await make_test("", "text/abc", "EmptyHeader", "text/abc");
  await make_test("", "text/css", "EmptyHeader", "text/css");
  await make_test("failure", "text/plain", "FailedToParse");
  await make_test("failure", "text/abc", "FailedToParse", "text/abc");
  await make_test("failure", "text/css", "FailedToParse", "text/css");
  await make_test("text/css", "text/css", "ParsedTextCSS");
  await make_test("text/css;hi", "text/css", "ParsedTextCSS");
  await make_test("text/plain", "text/plain", "ParsedOther");
  await make_test("text/abc", "text/abc", "ParsedOther");
  await make_test("text/abc;hi", "text/abc", "ParsedOther");
  await make_test("text/abc", "text/abc", "ParsedOther", "text/css");

  await new Promise(resolve => httpserv.stop(resolve));
});
