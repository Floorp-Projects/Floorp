const StandardURL = Components.Constructor("@mozilla.org/network/standard-url;1",
                                           "nsIStandardURL",
                                           "init");
const nsIStandardURL = Components.interfaces.nsIStandardURL;

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
     "hostPort", "host", "path", "filePath", "param", "query",
     "ref", "directory", "fileName", "fileBaseName", "fileExtension"]
      .map(function(prop) {
	dump("Testing '"+ prop + "'\n");
	do_check_eq(a[prop], b[prop]);
      });  
  } else {
    do_check_neq(a.spec, b.spec);
  }
  do_check_eq(expect, a.equals(b));
  do_check_eq(expect, b.equals(a));
}

function stringToURL(str) {
  return (new StandardURL(nsIStandardURL.URLTYPE_AUTHORITY, 80,
			 str, "UTF-8", null))
         .QueryInterface(Components.interfaces.nsIURL);
}

function pairToURLs(pair) {
  do_check_eq(pair.length, 2);
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

    provided.path = "";
    target.path = "";

    do_check_eq(provided.spec, target.spec);
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

    provided.query = "foo";

    do_check_eq(provided.spec, target.spec);
    symmetricEquality(true, provided, target);
  }

  [provided, target] =
    ["http://example.com/#", "http://example.com/?foo#bar"].map(stringToURL);
  symmetricEquality(false, provided, target);
  provided.query = "foo";
  symmetricEquality(false, provided, target);

  var newProvided = Components.classes["@mozilla.org/network/io-service;1"]
                              .getService(Components.interfaces.nsIIOService)
                              .newURI("#bar", null, provided)
                              .QueryInterface(Components.interfaces.nsIURL);

  do_check_eq(newProvided.spec, target.spec);
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
    a.ref = ref;
    var b = stringToURL(result);

    do_check_eq(a.spec, b.spec);
    do_check_eq(ref, b.ref);
    symmetricEquality(true, a, b);

    /* Test2: starting with non-empty */
    a.ref = "yyyy";
    var c = stringToURL(before);
    c.ref = "yyyy";
    symmetricEquality(true, a, c);

    /* Test3: reset the ref */
    a.ref = "";
    symmetricEquality(true, a, stringToURL(before));

    /* Test4: verify again after reset */
    a.ref = ref;
    symmetricEquality(true, a, b);
  }
  run_next_test();
});

// Bug 960014 - Make nsStandardURL::SetHost less magical around IPv6
add_test(function test_ipv6()
{
  var url = stringToURL("http://example.com");
  url.host = "[2001::1]";
  do_check_eq(url.host, "2001::1");

  url = stringToURL("http://example.com");
  url.hostPort = "[2001::1]:30";
  do_check_eq(url.host, "2001::1");
  do_check_eq(url.port, 30);
  do_check_eq(url.hostPort, "[2001::1]:30");

  url = stringToURL("http://example.com");
  url.hostPort = "2001:1";
  do_check_eq(url.host, "2001");
  do_check_eq(url.port, 1);
  do_check_eq(url.hostPort, "2001:1");
  run_next_test();
});

add_test(function test_ipv6_fail()
{
  var url = stringToURL("http://example.com");

  Assert.throws(() => { url.host = "2001::1"; }, "missing brackets");
  Assert.throws(() => { url.host = "[2001::1]:20"; }, "url.host with port");
  Assert.throws(() => { url.host = "[2001::1"; }, "missing last bracket");
  Assert.throws(() => { url.host = "2001::1]"; }, "missing first bracket");
  Assert.throws(() => { url.host = "2001[::1]"; }, "bad bracket position");
  Assert.throws(() => { url.host = "[]"; }, "empty IPv6 address");
  Assert.throws(() => { url.host = "[hello]"; }, "bad IPv6 address");
  Assert.throws(() => { url.host = "[192.168.1.1]"; }, "bad IPv6 address");
  Assert.throws(() => { url.hostPort = "2001::1"; }, "missing brackets");
  Assert.throws(() => { url.hostPort = "[2001::1]30"; }, "missing : after IP");
  Assert.throws(() => { url.hostPort = "[2001:1]"; }, "bad IPv6 address");
  Assert.throws(() => { url.hostPort = "[2001:1]10"; }, "bad IPv6 address");
  Assert.throws(() => { url.hostPort = "[2001:1]10:20"; }, "bad IPv6 address");
  Assert.throws(() => { url.hostPort = "[2001:1]:10:20"; }, "bad IPv6 address");
  Assert.throws(() => { url.hostPort = "[2001:1"; }, "bad IPv6 address");
  Assert.throws(() => { url.hostPort = "2001]:1"; }, "bad IPv6 address");
  Assert.throws(() => { url.hostPort = "2001:1]"; }, "bad IPv6 address");
  Assert.throws(() => { url.hostPort = ""; }, "Empty hostPort should fail");
  Assert.throws(() => { url.hostPort = "[2001::1]:"; }, "missing port number");
  Assert.throws(() => { url.hostPort = "[2001::1]:bad"; }, "bad port number");
  run_next_test();
});

add_test(function test_clearedSpec()
{
  var url = stringToURL("http://example.com/path");
  Assert.throws(() => { url.spec = "http: example"; }, "set bad spec");
  Assert.throws(() => { url.spec = ""; }, "set empty spec");
  do_check_eq(url.spec, "http://example.com/path");
  url.host = "allizom.org";

  var ref = stringToURL("http://allizom.org/path");
  symmetricEquality(true, url, ref);
  run_next_test();
});

add_test(function test_escapeBrackets()
{
  // Query
  var url = stringToURL("http://example.com/?a[x]=1");
  do_check_eq(url.spec, "http://example.com/?a[x]=1");

  url = stringToURL("http://example.com/?a%5Bx%5D=1");
  do_check_eq(url.spec, "http://example.com/?a%5Bx%5D=1");

  url = stringToURL("http://[2001::1]/?a[x]=1");
  do_check_eq(url.spec, "http://[2001::1]/?a[x]=1");

  url = stringToURL("http://[2001::1]/?a%5Bx%5D=1");
  do_check_eq(url.spec, "http://[2001::1]/?a%5Bx%5D=1");

  // Path
  url = stringToURL("http://example.com/brackets[x]/test");
  do_check_eq(url.spec, "http://example.com/brackets[x]/test");

  url = stringToURL("http://example.com/a%5Bx%5D/test");
  do_check_eq(url.spec, "http://example.com/a%5Bx%5D/test");
  run_next_test();
});

add_test(function test_apostropheEncoding()
{
  // For now, single quote is escaped everywhere _except_ the path.
  // This policy is controlled by the bitmask in nsEscape.cpp::EscapeChars[]
  var url = stringToURL("http://example.com/dir'/file'.ext'");
  do_check_eq(url.spec, "http://example.com/dir'/file'.ext'");
  run_next_test();
});

add_test(function test_accentEncoding()
{
  var url = stringToURL("http://example.com/?hello=`");
  do_check_eq(url.spec, "http://example.com/?hello=`");
  do_check_eq(url.query, "hello=`");

  url = stringToURL("http://example.com/?hello=%2C");
  do_check_eq(url.spec, "http://example.com/?hello=%2C");
  do_check_eq(url.query, "hello=%2C");
  run_next_test();
});

add_test(function test_percentDecoding()
{
  var url = stringToURL("http://%70%61%73%74%65%62%69%6E.com");
  do_check_eq(url.spec, "http://pastebin.com/");

  // We shouldn't unescape characters that are not allowed in the hostname.
  url = stringToURL("http://example.com%0a%23.google.com/");
  do_check_eq(url.spec, "http://example.com%0a%23.google.com/");
  run_next_test();
});

add_test(function test_hugeStringThrows()
{
  let prefs = Cc["@mozilla.org/preferences-service;1"]
                .getService(Ci.nsIPrefService);
  let maxLen = prefs.getIntPref("network.standard-url.max-length");
  let url = stringToURL("http://test:test@example.com");

  let hugeString = new Array(maxLen + 1).fill("a").join("");
  let properties = ["spec", "scheme", "userPass", "username",
                    "password", "hostPort", "host", "path", "ref",
                    "query", "fileName", "filePath", "fileBaseName", "fileExtension"];
  for (let prop of properties) {
    Assert.throws(() => url[prop] = hugeString,
                  /NS_ERROR_MALFORMED_URI/,
                  `Passing a huge string to "${prop}" should throw`);
  }

  run_next_test();
});

add_test(function test_filterWhitespace()
{
  var url = stringToURL(" \r\n\th\nt\rt\tp://ex\r\n\tample.com/path\r\n\t/\r\n\tto the/fil\r\n\te.e\r\n\txt?que\r\n\try#ha\r\n\tsh \r\n\t ");
  do_check_eq(url.spec, "http://example.com/path/to%20the/file.ext?query#hash");

  // These setters should escape \r\n\t, not filter them.
  var url = stringToURL("http://test.com/path?query#hash");
  url.filePath = "pa\r\n\tth";
  do_check_eq(url.spec, "http://test.com/pa%0D%0A%09th?query#hash");
  url.query = "qu\r\n\tery";
  do_check_eq(url.spec, "http://test.com/pa%0D%0A%09th?qu%0D%0A%09ery#hash");
  url.ref = "ha\r\n\tsh";
  do_check_eq(url.spec, "http://test.com/pa%0D%0A%09th?qu%0D%0A%09ery#ha%0D%0A%09sh");
  url.fileName = "fi\r\n\tle.name";
  do_check_eq(url.spec, "http://test.com/fi%0D%0A%09le.name?qu%0D%0A%09ery#ha%0D%0A%09sh");

  run_next_test();
});

add_test(function test_backslashReplacement()
{
  var url = stringToURL("http:\\\\test.com\\path/to\\file?query\\backslash#hash\\");
  do_check_eq(url.spec, "http://test.com/path/to/file?query\\backslash#hash\\");

  url = stringToURL("http:\\\\test.com\\example.org/path\\to/file");
  do_check_eq(url.spec, "http://test.com/example.org/path/to/file");
  do_check_eq(url.host, "test.com");
  do_check_eq(url.path, "/example.org/path/to/file");

  run_next_test();
});

add_test(function test_authority_host()
{
  Assert.throws(() => { stringToURL("http:"); }, "TYPE_AUTHORITY should have host");
  Assert.throws(() => { stringToURL("http:///"); }, "TYPE_AUTHORITY should have host");

  run_next_test();
});

add_test(function test_trim_C0_and_space()
{
  var url = stringToURL("\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f http://example.com/ \x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f ");
  do_check_eq(url.spec, "http://example.com/");
  url.spec = "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f http://test.com/ \x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f ";
  do_check_eq(url.spec, "http://test.com/");
  Assert.throws(() => { url.spec = "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19 "; }, "set empty spec");
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
    do_check_eq(url.spec, "http://example.com/pa%" + toHex(i) + "th?qu%" + toHex(i) + "ery#ha%" + toHex(i) + "sh");
  }

  // Additionally, we need to check the setters.
  var url = stringToURL("http://example.com/path?query#hash");
  url.filePath = "pa\0th";
  do_check_eq(url.spec, "http://example.com/pa%00th?query#hash");
  url.query = "qu\0ery";
  do_check_eq(url.spec, "http://example.com/pa%00th?qu%00ery#hash");
  url.ref = "ha\0sh";
  do_check_eq(url.spec, "http://example.com/pa%00th?qu%00ery#ha%00sh");
  url.fileName = "fi\0le.name";
  do_check_eq(url.spec, "http://example.com/fi%00le.name?qu%00ery#ha%00sh");

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
    do_check_eq(url.spec, "http://127.0.0.1/");
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
    ];
  var spec;
  for (spec of nonIPv4s) {
    url = stringToURL(spec);
    do_check_eq(url.spec, spec);
  }

  run_next_test();
});
