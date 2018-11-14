"use strict";

const gPrefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);

function symmetricEquality(expect, a, b)
{
  /* Use if/else instead of |do_check_eq(expect, a.spec == b.spec)| so
     that we get the specs output on the console if the check fails.
   */
  if (expect) {
    /* Check all the sub-pieces too, since that can help with
       debugging cases when equals() returns something unexpected */
    /* We don't check port in the loop, because it can be defaulted in
       some cases. */
    ["spec", "prePath", "scheme", "userPass", "username", "password",
     "hostPort", "host", "pathQueryRef", "filePath", "query",
     "ref", "directory", "fileName", "fileBaseName", "fileExtension"]
      .map(function(prop) {
	dump("Testing '"+ prop + "'\n");
	Assert.equal(a[prop], b[prop]);
      });
  } else {
    Assert.notEqual(a.spec, b.spec);
  }
  Assert.equal(expect, a.equals(b));
  Assert.equal(expect, b.equals(a));
}

function stringToURL(str) {
  return Cc["@mozilla.org/network/standard-url-mutator;1"]
         .createInstance(Ci.nsIStandardURLMutator)
         .init(Ci.nsIStandardURL.URLTYPE_AUTHORITY, 80, str, "UTF-8", null)
         .finalize()
         .QueryInterface(Ci.nsIURL);
}

function pairToURLs(pair) {
  Assert.equal(pair.length, 2);
  return pair.map(stringToURL);
}

add_test(function test_setEmptyPath()
{
  var pairs =
    [
     ["http://example.com", "http://example.com/tests/dom/tests"],
     ["http://example.com:80", "http://example.com/tests/dom/tests"],
     ["http://example.com:80/", "http://example.com/tests/dom/test"],
     ["http://example.com/", "http://example.com/tests/dom/tests"],
     ["http://example.com/a", "http://example.com/tests/dom/tests"],
     ["http://example.com:80/a", "http://example.com/tests/dom/tests"],
    ].map(pairToURLs);

  for (var [provided, target] of pairs)
  {
    symmetricEquality(false, target, provided);

    provided = provided.mutate().setPathQueryRef("").finalize();
    target = target.mutate().setPathQueryRef("").finalize();

    Assert.equal(provided.spec, target.spec);
    symmetricEquality(true, target, provided);
  }
  run_next_test();
});

add_test(function test_setQuery()
{
  var pairs =
    [
     ["http://example.com", "http://example.com/?foo"],
     ["http://example.com/bar", "http://example.com/bar?foo"],
     ["http://example.com#bar", "http://example.com/?foo#bar"],
     ["http://example.com/#bar", "http://example.com/?foo#bar"],
     ["http://example.com/?longerthanfoo#bar", "http://example.com/?foo#bar"],
     ["http://example.com/?longerthanfoo", "http://example.com/?foo"],
     /* And one that's nonempty but shorter than "foo" */
     ["http://example.com/?f#bar", "http://example.com/?foo#bar"],
     ["http://example.com/?f", "http://example.com/?foo"],
    ].map(pairToURLs);

  for (var [provided, target] of pairs) {
    symmetricEquality(false, provided, target);

    provided = provided.mutate().setQuery("foo").finalize().QueryInterface(Ci.nsIURL);

    Assert.equal(provided.spec, target.spec);
    symmetricEquality(true, provided, target);
  }

  [provided, target] =
    ["http://example.com/#", "http://example.com/?foo#bar"].map(stringToURL);
  symmetricEquality(false, provided, target);
  provided = provided.mutate().setQuery("foo").finalize().QueryInterface(Ci.nsIURL);
  symmetricEquality(false, provided, target);

  var newProvided = Cc["@mozilla.org/network/io-service;1"]
                      .getService(Ci.nsIIOService)
                      .newURI("#bar", null, provided)
                      .QueryInterface(Ci.nsIURL);

  Assert.equal(newProvided.spec, target.spec);
  symmetricEquality(true, newProvided, target);
  run_next_test();
});

add_test(function test_setRef()
{
  var tests =
    [
     ["http://example.com",      "", "http://example.com/"],
     ["http://example.com:80",   "", "http://example.com:80/"],
     ["http://example.com:80/",  "", "http://example.com:80/"],
     ["http://example.com/",     "", "http://example.com/"],
     ["http://example.com/a",    "", "http://example.com/a"],
     ["http://example.com:80/a", "", "http://example.com:80/a"],

     ["http://example.com",      "x", "http://example.com/#x"],
     ["http://example.com:80",   "x", "http://example.com:80/#x"],
     ["http://example.com:80/",  "x", "http://example.com:80/#x"],
     ["http://example.com/",     "x", "http://example.com/#x"],
     ["http://example.com/a",    "x", "http://example.com/a#x"],
     ["http://example.com:80/a", "x", "http://example.com:80/a#x"],

     ["http://example.com",      "xx", "http://example.com/#xx"],
     ["http://example.com:80",   "xx", "http://example.com:80/#xx"],
     ["http://example.com:80/",  "xx", "http://example.com:80/#xx"],
     ["http://example.com/",     "xx", "http://example.com/#xx"],
     ["http://example.com/a",    "xx", "http://example.com/a#xx"],
     ["http://example.com:80/a", "xx", "http://example.com:80/a#xx"],

     ["http://example.com",      "xxxxxxxxxxxxxx", "http://example.com/#xxxxxxxxxxxxxx"],
     ["http://example.com:80",   "xxxxxxxxxxxxxx", "http://example.com:80/#xxxxxxxxxxxxxx"],
     ["http://example.com:80/",  "xxxxxxxxxxxxxx", "http://example.com:80/#xxxxxxxxxxxxxx"],
     ["http://example.com/",     "xxxxxxxxxxxxxx", "http://example.com/#xxxxxxxxxxxxxx"],
     ["http://example.com/a",    "xxxxxxxxxxxxxx", "http://example.com/a#xxxxxxxxxxxxxx"],
     ["http://example.com:80/a", "xxxxxxxxxxxxxx", "http://example.com:80/a#xxxxxxxxxxxxxx"],
    ];

  for (var [before, ref, result] of tests)
  {
    /* Test1: starting with empty ref */
    var a = stringToURL(before);
    a = a.mutate().setRef(ref).finalize().QueryInterface(Ci.nsIURL);
    var b = stringToURL(result);

    Assert.equal(a.spec, b.spec);
    Assert.equal(ref, b.ref);
    symmetricEquality(true, a, b);

    /* Test2: starting with non-empty */
    a = a.mutate().setRef("yyyy").finalize().QueryInterface(Ci.nsIURL);
    var c = stringToURL(before);
    c = c.mutate().setRef("yyyy").finalize().QueryInterface(Ci.nsIURL);
    symmetricEquality(true, a, c);

    /* Test3: reset the ref */
    a = a.mutate().setRef("").finalize().QueryInterface(Ci.nsIURL);
    symmetricEquality(true, a, stringToURL(before));

    /* Test4: verify again after reset */
    a = a.mutate().setRef(ref).finalize().QueryInterface(Ci.nsIURL);
    symmetricEquality(true, a, b);
  }
  run_next_test();
});

// Bug 960014 - Make nsStandardURL::SetHost less magical around IPv6
add_test(function test_ipv6()
{
  var url = stringToURL("http://example.com");
  url = url.mutate().setHost("[2001::1]").finalize();
  Assert.equal(url.host, "2001::1");

  url = stringToURL("http://example.com");
  url = url.mutate().setHostPort("[2001::1]:30").finalize();
  Assert.equal(url.host, "2001::1");
  Assert.equal(url.port, 30);
  Assert.equal(url.hostPort, "[2001::1]:30");

  url = stringToURL("http://example.com");
  url = url.mutate().setHostPort("2001:1").finalize();
  Assert.equal(url.host, "0.0.7.209");
  Assert.equal(url.port, 1);
  Assert.equal(url.hostPort, "0.0.7.209:1");
  run_next_test();
});

add_test(function test_ipv6_fail()
{
  var url = stringToURL("http://example.com");

  Assert.throws(() => { url = url.mutate().setHost("2001::1").finalize(); },
                /NS_ERROR_MALFORMED_URI/, "missing brackets");
  Assert.throws(() => { url = url.mutate().setHost("[2001::1]:20").finalize(); },
                /NS_ERROR_MALFORMED_URI/, "url.host with port");
  Assert.throws(() => { url = url.mutate().setHost("[2001::1").finalize(); },
                /NS_ERROR_MALFORMED_URI/, "missing last bracket");
  Assert.throws(() => { url = url.mutate().setHost("2001::1]").finalize(); },
                /NS_ERROR_MALFORMED_URI/, "missing first bracket");
  Assert.throws(() => { url = url.mutate().setHost("2001[::1]").finalize(); },
                /NS_ERROR_MALFORMED_URI/, "bad bracket position");
  Assert.throws(() => { url = url.mutate().setHost("[]").finalize(); },
                /NS_ERROR_MALFORMED_URI/, "empty IPv6 address");
  Assert.throws(() => { url = url.mutate().setHost("[hello]").finalize(); },
                /NS_ERROR_MALFORMED_URI/, "bad IPv6 address");
  Assert.throws(() => { url = url.mutate().setHost("[192.168.1.1]").finalize(); },
                /NS_ERROR_MALFORMED_URI/, "bad IPv6 address");
  Assert.throws(() => { url = url.mutate().setHostPort("2001::1").finalize(); },
                /NS_ERROR_MALFORMED_URI/, "missing brackets");
  Assert.throws(() => { url = url.mutate().setHostPort("[2001::1]30").finalize(); },
                /NS_ERROR_MALFORMED_URI/, "missing : after IP");
  Assert.throws(() => { url = url.mutate().setHostPort("[2001:1]").finalize(); },
                /NS_ERROR_MALFORMED_URI/, "bad IPv6 address");
  Assert.throws(() => { url = url.mutate().setHostPort("[2001:1]10").finalize(); },
                /NS_ERROR_MALFORMED_URI/, "bad IPv6 address");
  Assert.throws(() => { url = url.mutate().setHostPort("[2001:1]10:20").finalize(); },
                /NS_ERROR_MALFORMED_URI/, "bad IPv6 address");
  Assert.throws(() => { url = url.mutate().setHostPort("[2001:1]:10:20").finalize(); },
                /NS_ERROR_MALFORMED_URI/, "bad IPv6 address");
  Assert.throws(() => { url = url.mutate().setHostPort("[2001:1").finalize(); },
                /NS_ERROR_MALFORMED_URI/, "bad IPv6 address");
  Assert.throws(() => { url = url.mutate().setHostPort("2001]:1").finalize(); },
                /NS_ERROR_MALFORMED_URI/, "bad IPv6 address");
  Assert.throws(() => { url = url.mutate().setHostPort("2001:1]").finalize(); },
                /NS_ERROR_MALFORMED_URI/, "bad IPv6 address");
  Assert.throws(() => { url = url.mutate().setHostPort("").finalize(); },
                /NS_ERROR_UNEXPECTED/, "Empty hostPort should fail");

  // These checks used to fail, but now don't (see bug 1433958 comment 57)
  url = url.mutate().setHostPort("[2001::1]:").finalize();
  Assert.equal(url.spec, "http://[2001::1]/");
  url = url.mutate().setHostPort("[2002::1]:bad").finalize();
  Assert.equal(url.spec, "http://[2002::1]/");

  run_next_test();
});

add_test(function test_clearedSpec()
{
  var url = stringToURL("http://example.com/path");
  Assert.throws(() => { url = url.mutate().setSpec("http: example").finalize(); },
                /NS_ERROR_MALFORMED_URI/, "set bad spec");
  Assert.throws(() => { url = url.mutate().setSpec("").finalize(); },
                /NS_ERROR_MALFORMED_URI/, "set empty spec");
  Assert.equal(url.spec, "http://example.com/path");
  url = url.mutate().setHost("allizom.org").finalize().QueryInterface(Ci.nsIURL);

  var ref = stringToURL("http://allizom.org/path");
  symmetricEquality(true, url, ref);
  run_next_test();
});

add_test(function test_escapeBrackets()
{
  // Query
  var url = stringToURL("http://example.com/?a[x]=1");
  Assert.equal(url.spec, "http://example.com/?a[x]=1");

  url = stringToURL("http://example.com/?a%5Bx%5D=1");
  Assert.equal(url.spec, "http://example.com/?a%5Bx%5D=1");

  url = stringToURL("http://[2001::1]/?a[x]=1");
  Assert.equal(url.spec, "http://[2001::1]/?a[x]=1");

  url = stringToURL("http://[2001::1]/?a%5Bx%5D=1");
  Assert.equal(url.spec, "http://[2001::1]/?a%5Bx%5D=1");

  // Path
  url = stringToURL("http://example.com/brackets[x]/test");
  Assert.equal(url.spec, "http://example.com/brackets[x]/test");

  url = stringToURL("http://example.com/a%5Bx%5D/test");
  Assert.equal(url.spec, "http://example.com/a%5Bx%5D/test");
  run_next_test();
});

add_test(function test_escapeQuote()
{
  var url = stringToURL("http://example.com/#'");
  Assert.equal(url.spec, "http://example.com/#'");
  Assert.equal(url.ref, "'");
  url = url.mutate().setRef("test'test").finalize();
  Assert.equal(url.spec, "http://example.com/#test'test");
  Assert.equal(url.ref, "test'test");
  run_next_test();
});

add_test(function test_apostropheEncoding()
{
  // For now, single quote is escaped everywhere _except_ the path.
  // This policy is controlled by the bitmask in nsEscape.cpp::EscapeChars[]
  var url = stringToURL("http://example.com/dir'/file'.ext'");
  Assert.equal(url.spec, "http://example.com/dir'/file'.ext'");
  run_next_test();
});

add_test(function test_accentEncoding()
{
  var url = stringToURL("http://example.com/?hello=`");
  Assert.equal(url.spec, "http://example.com/?hello=`");
  Assert.equal(url.query, "hello=`");

  url = stringToURL("http://example.com/?hello=%2C");
  Assert.equal(url.spec, "http://example.com/?hello=%2C");
  Assert.equal(url.query, "hello=%2C");
  run_next_test();
});

add_test(function test_percentDecoding()
{
  var url = stringToURL("http://%70%61%73%74%65%62%69%6E.com");
  Assert.equal(url.spec, "http://pastebin.com/");

  // We shouldn't unescape characters that are not allowed in the hostname.
  url = stringToURL("http://example.com%0a%23.google.com/");
  Assert.equal(url.spec, "http://example.com%0a%23.google.com/");
  run_next_test();
});

add_test(function test_hugeStringThrows()
{
  let prefs = Cc["@mozilla.org/preferences-service;1"]
                .getService(Ci.nsIPrefService);
  let maxLen = prefs.getIntPref("network.standard-url.max-length");
  let url = stringToURL("http://test:test@example.com");

  let hugeString = new Array(maxLen + 1).fill("a").join("");
  let setters = [
    { method: "setSpec", qi: Ci.nsIURIMutator },
    { method: "setUsername", qi: Ci.nsIURIMutator },
    { method: "setPassword", qi: Ci.nsIURIMutator },
    { method: "setFilePath", qi: Ci.nsIURIMutator },
    { method: "setHostPort", qi: Ci.nsIURIMutator },
    { method: "setHost", qi: Ci.nsIURIMutator },
    { method: "setUserPass", qi: Ci.nsIURIMutator },
    { method: "setPathQueryRef", qi: Ci.nsIURIMutator },
    { method: "setQuery", qi: Ci.nsIURIMutator },
    { method: "setRef", qi: Ci.nsIURIMutator },
    { method: "setScheme", qi: Ci.nsIURIMutator },
    { method: "setFileName", qi: Ci.nsIURLMutator },
    { method: "setFileExtension", qi: Ci.nsIURLMutator },
    { method: "setFileBaseName", qi: Ci.nsIURLMutator },
  ];

  for (let prop of setters) {
    Assert.throws(() => url = url.mutate().QueryInterface(prop.qi)[prop.method](hugeString).finalize(),
                  /NS_ERROR_MALFORMED_URI/,
                  `Passing a huge string to "${prop.method}" should throw`);
  }

  run_next_test();
});

add_test(function test_filterWhitespace()
{
  var url = stringToURL(" \r\n\th\nt\rt\tp://ex\r\n\tample.com/path\r\n\t/\r\n\tto the/fil\r\n\te.e\r\n\txt?que\r\n\try#ha\r\n\tsh \r\n\t ");
  Assert.equal(url.spec, "http://example.com/path/to%20the/file.ext?query#hash");

  // These setters should escape \r\n\t, not filter them.
  var url = stringToURL("http://test.com/path?query#hash");
  url = url.mutate().setFilePath("pa\r\n\tth").finalize();
  Assert.equal(url.spec, "http://test.com/pa%0D%0A%09th?query#hash");
  url = url.mutate().setQuery("qu\r\n\tery").finalize();
  Assert.equal(url.spec, "http://test.com/pa%0D%0A%09th?qu%0D%0A%09ery#hash");
  url = url.mutate().setRef("ha\r\n\tsh").finalize();
  Assert.equal(url.spec, "http://test.com/pa%0D%0A%09th?qu%0D%0A%09ery#ha%0D%0A%09sh");
  url = url.mutate().QueryInterface(Ci.nsIURLMutator).setFileName("fi\r\n\tle.name").finalize();
  Assert.equal(url.spec, "http://test.com/fi%0D%0A%09le.name?qu%0D%0A%09ery#ha%0D%0A%09sh");

  run_next_test();
});

add_test(function test_backslashReplacement()
{
  var url = stringToURL("http:\\\\test.com\\path/to\\file?query\\backslash#hash\\");
  Assert.equal(url.spec, "http://test.com/path/to/file?query\\backslash#hash\\");

  url = stringToURL("http:\\\\test.com\\example.org/path\\to/file");
  Assert.equal(url.spec, "http://test.com/example.org/path/to/file");
  Assert.equal(url.host, "test.com");
  Assert.equal(url.pathQueryRef, "/example.org/path/to/file");

  run_next_test();
});

add_test(function test_authority_host()
{
  Assert.throws(() => { stringToURL("http:"); },
                /NS_ERROR_MALFORMED_URI/, "TYPE_AUTHORITY should have host");
  Assert.throws(() => { stringToURL("http:///"); },
                /NS_ERROR_MALFORMED_URI/, "TYPE_AUTHORITY should have host");

  run_next_test();
});

add_test(function test_trim_C0_and_space()
{
  var url = stringToURL("\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f http://example.com/ \x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f ");
  Assert.equal(url.spec, "http://example.com/");
  url = url.mutate()
           .setSpec("\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f http://test.com/ \x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f ")
           .finalize();
  Assert.equal(url.spec, "http://test.com/");
  Assert.throws(() => { url = url.mutate().setSpec("\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19 ").finalize(); },
                /NS_ERROR_MALFORMED_URI/, "set empty spec");
  run_next_test();
});

// This tests that C0-and-space characters in the path, query and ref are
// percent encoded.
add_test(function test_encode_C0_and_space()
{
  function toHex(d) {
    var hex = d.toString(16);
    if (hex.length == 1)
      hex = "0"+hex;
    return hex.toUpperCase();
  }

  for (var i=0x0; i<=0x20; i++) {
    // These characters get filtered - they are not encoded.
    if (String.fromCharCode(i) == '\r' ||
        String.fromCharCode(i) == '\n' ||
        String.fromCharCode(i) == '\t') {
      continue;
    }
    var url = stringToURL("http://example.com/pa" + String.fromCharCode(i) + "th?qu" + String.fromCharCode(i) +"ery#ha" + String.fromCharCode(i) + "sh");
    Assert.equal(url.spec, "http://example.com/pa%" + toHex(i) + "th?qu%" + toHex(i) + "ery#ha%" + toHex(i) + "sh");
  }

  // Additionally, we need to check the setters.
  var url = stringToURL("http://example.com/path?query#hash");
  url = url.mutate().setFilePath("pa\0th").finalize();
  Assert.equal(url.spec, "http://example.com/pa%00th?query#hash");
  url = url.mutate().setQuery("qu\0ery").finalize();
  Assert.equal(url.spec, "http://example.com/pa%00th?qu%00ery#hash");
  url = url.mutate().setRef("ha\0sh").finalize();
  Assert.equal(url.spec, "http://example.com/pa%00th?qu%00ery#ha%00sh");
  url = url.mutate().QueryInterface(Ci.nsIURLMutator).setFileName("fi\0le.name").finalize();
  Assert.equal(url.spec, "http://example.com/fi%00le.name?qu%00ery#ha%00sh");

  run_next_test();
});

add_test(function test_ipv4Normalize()
{
  var localIPv4s =
    ["http://127.0.0.1",
     "http://127.0.1",
     "http://127.1",
     "http://2130706433",
     "http://0177.00.00.01",
     "http://0177.00.01",
     "http://0177.01",
     "http://00000000000000000000000000177.0000000.0000000.0001",
     "http://000000177.0000001",
     "http://017700000001",
     "http://0x7f.0x00.0x00.0x01",
     "http://0x7f.0x01",
     "http://0x7f000001",
     "http://0x007f.0x0000.0x0000.0x0001",
     "http://000177.0.00000.0x0001",
     "http://127.0.0.1.",
    ].map(stringToURL);

  var url;
  for (url of localIPv4s) {
    Assert.equal(url.spec, "http://127.0.0.1/");
  }

  // These should treated as a domain instead of an IPv4.
  var nonIPv4s =
    ["http://0xfffffffff/",
     "http://0x100000000/",
     "http://4294967296/",
     "http://1.2.0x10000/",
     "http://1.0x1000000/",
     "http://256.0.0.1/",
     "http://1.256.1/",
     "http://-1.0.0.0/",
     "http://1.2.3.4.5/",
     "http://010000000000000000/",
     "http://2+3/",
     "http://0.0.0.-1/",
     "http://1.2.3.4../",
     "http://1..2/",
     "http://.1.2.3.4/",
     "resource://123/",
     "resource://4294967296/",
    ];
  var spec;
  for (spec of nonIPv4s) {
    url = stringToURL(spec);
    Assert.equal(url.spec, spec);
  }

  var url = stringToURL("resource://path/to/resource/");
  url = url.mutate().setHost("123").finalize();
  Assert.equal(url.host, "123");

  run_next_test();
});

add_test(function test_invalidHostChars() {
  var url = stringToURL("http://example.org/");
  for (let i = 0; i <= 0x20; i++) {
    Assert.throws(() => { url = url.mutate().setHost("a" + String.fromCharCode(i) + "b").finalize(); },
                  /NS_ERROR_MALFORMED_URI/, "Trying to set hostname containing char code: " + i);
  }
  for (let c of "@[]*<>|:\"") {
    Assert.throws(() => { url = url.mutate().setHost("a" + c).finalize(); },
                  /NS_ERROR_MALFORMED_URI/, "Trying to set hostname containing char: " + c);
  }

  // It also can't contain /, \, #, ?, but we treat these characters as
  // hostname separators, so there is no way to set them and fail.
  run_next_test();
});

add_test(function test_normalize_ipv6() {
  var url = stringToURL("http://example.com");
  url = url.mutate().setHost("[::192.9.5.5]").finalize();
  Assert.equal(url.spec, "http://[::c009:505]/");

  run_next_test();
});

add_test(function test_emptyPassword() {
  var url = stringToURL("http://a:@example.com");
  Assert.equal(url.spec, "http://a@example.com/");
  url = url.mutate().setPassword("pp").finalize();
  Assert.equal(url.spec, "http://a:pp@example.com/");
  url = url.mutate().setPassword("").finalize();
  Assert.equal(url.spec, "http://a@example.com/");
  url = url.mutate().setUserPass("xxx:").finalize();
  Assert.equal(url.spec, "http://xxx@example.com/");
  url = url.mutate().setPassword("zzzz").finalize();
  Assert.equal(url.spec, "http://xxx:zzzz@example.com/");
  url = url.mutate().setUserPass("xxxxx:yyyyyy").finalize();
  Assert.equal(url.spec, "http://xxxxx:yyyyyy@example.com/");
  url = url.mutate().setUserPass("z:").finalize();
  Assert.equal(url.spec, "http://z@example.com/");
  url = url.mutate().setPassword("ppppppppppp").finalize();
  Assert.equal(url.spec, "http://z:ppppppppppp@example.com/");

  url = url.mutate().setUsername("").finalize(); // Should clear password too
  Assert.equal(url.spec, "http://example.com/");
  url = url.mutate().setPassword("").finalize(); // Still empty. Should work.
  Assert.equal(url.spec, "http://example.com/");

  run_next_test();
});

registerCleanupFunction(function () {
  gPrefs.clearUserPref("network.standard-url.punycode-host");
});

add_test(function test_idna_host() {
  // See bug 945240 - this test makes sure that URLs return a punycode hostname
  // when the pref is set, or unicode otherwise.

  // First we test that the old behaviour still works properly for all methods
  // that return strings containing the hostname

  gPrefs.setBoolPref("network.standard-url.punycode-host", false);
  let url = stringToURL("http://user:password@ält.example.org:8080/path?query#etc");

  equal(url.host, "ält.example.org");
  equal(url.hostPort, "ält.example.org:8080");
  equal(url.prePath, "http://user:password@ält.example.org:8080");
  equal(url.spec, "http://user:password@ält.example.org:8080/path?query#etc");
  equal(url.specIgnoringRef, "http://user:password@ält.example.org:8080/path?query");
  equal(url.QueryInterface(Ci.nsISensitiveInfoHiddenURI).getSensitiveInfoHiddenSpec(), "http://user:****@ält.example.org:8080/path?query#etc");

  equal(url.displayHost, "ält.example.org");
  equal(url.displayHostPort, "ält.example.org:8080");
  equal(url.displaySpec, "http://user:password@ält.example.org:8080/path?query#etc");

  equal(url.asciiHost, "xn--lt-uia.example.org");
  equal(url.asciiHostPort, "xn--lt-uia.example.org:8080");
  equal(url.asciiSpec, "http://user:password@xn--lt-uia.example.org:8080/path?query#etc");

  url = url.mutate().setRef("").finalize(); // SetRef calls InvalidateCache()
  equal(url.spec, "http://user:password@ält.example.org:8080/path?query");
  equal(url.displaySpec, "http://user:password@ält.example.org:8080/path?query");
  equal(url.asciiSpec, "http://user:password@xn--lt-uia.example.org:8080/path?query");

  url = stringToURL("http://user:password@www.ält.com:8080/path?query#etc");
  url = url.mutate().setRef("").finalize();
  equal(url.spec, "http://user:password@www.ält.com:8080/path?query");

  // We also check that the default behaviour changes once we filp the pref
  gPrefs.setBoolPref("network.standard-url.punycode-host", true);

  url = stringToURL("http://user:password@ält.example.org:8080/path?query#etc");
  equal(url.host, "xn--lt-uia.example.org");
  equal(url.hostPort, "xn--lt-uia.example.org:8080");
  equal(url.prePath, "http://user:password@xn--lt-uia.example.org:8080");
  equal(url.spec, "http://user:password@xn--lt-uia.example.org:8080/path?query#etc");
  equal(url.specIgnoringRef, "http://user:password@xn--lt-uia.example.org:8080/path?query");
  equal(url.QueryInterface(Ci.nsISensitiveInfoHiddenURI).getSensitiveInfoHiddenSpec(), "http://user:****@xn--lt-uia.example.org:8080/path?query#etc");

  equal(url.displayHost, "ält.example.org");
  equal(url.displayHostPort, "ält.example.org:8080");
  equal(url.displaySpec, "http://user:password@ält.example.org:8080/path?query#etc");

  equal(url.asciiHost, "xn--lt-uia.example.org");
  equal(url.asciiHostPort, "xn--lt-uia.example.org:8080");
  equal(url.asciiSpec, "http://user:password@xn--lt-uia.example.org:8080/path?query#etc");

  url = url.mutate().setRef("").finalize(); // SetRef calls InvalidateCache()
  equal(url.spec, "http://user:password@xn--lt-uia.example.org:8080/path?query");
  equal(url.displaySpec, "http://user:password@ält.example.org:8080/path?query");
  equal(url.asciiSpec, "http://user:password@xn--lt-uia.example.org:8080/path?query");

  url = stringToURL("http://user:password@www.ält.com:8080/path?query#etc");
  url = url.mutate().setRef("").finalize();
  equal(url.spec, "http://user:password@www.xn--lt-uia.com:8080/path?query");

  run_next_test();
});
