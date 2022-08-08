/**
 * Tests for parsing header fields using the syntax used in
 * Content-Disposition and Content-Type
 *
 * See also https://bugzilla.mozilla.org/show_bug.cgi?id=609667
 */

"use strict";

var BS = "\\";
var DQUOTE = '"';

// Test array:
//  - element 0: "Content-Disposition" header to test
//  under MIME (email):
//  - element 1: correct value returned for disposition-type (empty param name)
//  - element 2: correct value for filename returned
//  under HTTP:
// (currently supports continuations; expected results without continuations
// are commented out for now)
//  - element 3: correct value returned for disposition-type (empty param name)
//  - element 4: correct value for filename returned
//
// 3 and 4 may be left out if they are identical

var tests = [
  // No filename parameter: return nothing
  ["attachment;", "attachment", Cr.NS_ERROR_INVALID_ARG],

  // basic
  ["attachment; filename=basic", "attachment", "basic"],

  // extended
  ["attachment; filename*=UTF-8''extended", "attachment", "extended"],

  // prefer extended to basic (bug 588781)
  [
    "attachment; filename=basic; filename*=UTF-8''extended",
    "attachment",
    "extended",
  ],

  // prefer extended to basic (bug 588781)
  [
    "attachment; filename*=UTF-8''extended; filename=basic",
    "attachment",
    "extended",
  ],

  // use first basic value (invalid; error recovery)
  ["attachment; filename=first; filename=wrong", "attachment", "first"],

  // old school bad HTTP servers: missing 'attachment' or 'inline'
  // (invalid; error recovery)
  ["filename=old", "filename=old", "old"],

  ["attachment; filename*=UTF-8''extended", "attachment", "extended"],

  // continuations not part of RFC 5987 (bug 610054)
  [
    "attachment; filename*0=foo; filename*1=bar",
    "attachment",
    "foobar",
    /* "attachment", Cr.NS_ERROR_INVALID_ARG */
  ],

  // Return first continuation (invalid; error recovery)
  [
    "attachment; filename*0=first; filename*0=wrong; filename=basic",
    "attachment",
    "first",
    /* "attachment", "basic" */
  ],

  // Only use correctly ordered continuations  (invalid; error recovery)
  [
    "attachment; filename*0=first; filename*1=second; filename*0=wrong",
    "attachment",
    "firstsecond",
    /* "attachment", Cr.NS_ERROR_INVALID_ARG */
  ],

  // prefer continuation to basic (unless RFC 5987)
  [
    "attachment; filename=basic; filename*0=foo; filename*1=bar",
    "attachment",
    "foobar",
    /* "attachment", "basic" */
  ],

  // Prefer extended to basic and/or (broken or not) continuation
  // (invalid; error recovery)
  [
    "attachment; filename=basic; filename*0=first; filename*0=wrong; filename*=UTF-8''extended",
    "attachment",
    "extended",
  ],

  // RFC 2231 not clear on correct outcome: we prefer non-continued extended
  // (invalid; error recovery)
  [
    "attachment; filename=basic; filename*=UTF-8''extended; filename*0=foo; filename*1=bar",
    "attachment",
    "extended",
  ],

  // Gaps should result in returning only value until gap hit
  // (invalid; error recovery)
  [
    "attachment; filename*0=foo; filename*2=bar",
    "attachment",
    "foo",
    /* "attachment", Cr.NS_ERROR_INVALID_ARG */
  ],

  // Don't allow leading 0's (*01) (invalid; error recovery)
  [
    "attachment; filename*0=foo; filename*01=bar",
    "attachment",
    "foo",
    /* "attachment", Cr.NS_ERROR_INVALID_ARG */
  ],

  // continuations should prevail over non-extended (unless RFC 5987)
  [
    "attachment; filename=basic; filename*0*=UTF-8''multi;\r\n" +
      " filename*1=line;\r\n" +
      " filename*2*=%20extended",
    "attachment",
    "multiline extended",
    /* "attachment", "basic" */
  ],

  // Gaps should result in returning only value until gap hit
  // (invalid; error recovery)
  [
    "attachment; filename=basic; filename*0*=UTF-8''multi;\r\n" +
      " filename*1=line;\r\n" +
      " filename*3*=%20extended",
    "attachment",
    "multiline",
    /* "attachment", "basic" */
  ],

  // First series, only please, and don't slurp up higher elements (*2 in this
  // case) from later series into earlier one (invalid; error recovery)
  [
    "attachment; filename=basic; filename*0*=UTF-8''multi;\r\n" +
      " filename*1=line;\r\n" +
      " filename*0*=UTF-8''wrong;\r\n" +
      " filename*1=bad;\r\n" +
      " filename*2=evil",
    "attachment",
    "multiline",
    /* "attachment", "basic" */
  ],

  // RFC 2231 not clear on correct outcome: we prefer non-continued extended
  // (invalid; error recovery)
  [
    "attachment; filename=basic; filename*0=UTF-8''multi\r\n;" +
      " filename*=UTF-8''extended;\r\n" +
      " filename*1=line;\r\n" +
      " filename*2*=%20extended",
    "attachment",
    "extended",
  ],

  // sneaky: if unescaped, make sure we leave UTF-8'' in value
  [
    "attachment; filename*0=UTF-8''unescaped;\r\n" +
      " filename*1*=%20so%20includes%20UTF-8''%20in%20value",
    "attachment",
    "UTF-8''unescaped so includes UTF-8'' in value",
    /* "attachment", Cr.NS_ERROR_INVALID_ARG */
  ],

  // sneaky: if unescaped, make sure we leave UTF-8'' in value
  [
    "attachment; filename=basic; filename*0=UTF-8''unescaped;\r\n" +
      " filename*1*=%20so%20includes%20UTF-8''%20in%20value",
    "attachment",
    "UTF-8''unescaped so includes UTF-8'' in value",
    /* "attachment", "basic" */
  ],

  // Prefer basic over invalid continuation
  // (invalid; error recovery)
  [
    "attachment; filename=basic; filename*1=multi;\r\n" +
      " filename*2=line;\r\n" +
      " filename*3*=%20extended",
    "attachment",
    "basic",
  ],

  // support digits over 10
  [
    "attachment; filename=basic; filename*0*=UTF-8''0;\r\n" +
      " filename*1=1; filename*2=2;filename*3=3;filename*4=4;filename*5=5;\r\n" +
      " filename*6=6; filename*7=7;filename*8=8;filename*9=9;filename*10=a;\r\n" +
      " filename*11=b; filename*12=c;filename*13=d;filename*14=e;filename*15=f\r\n",
    "attachment",
    "0123456789abcdef",
    /* "attachment", "basic" */
  ],

  // support digits over 10 (detect gaps)
  [
    "attachment; filename=basic; filename*0*=UTF-8''0;\r\n" +
      " filename*1=1; filename*2=2;filename*3=3;filename*4=4;filename*5=5;\r\n" +
      " filename*6=6; filename*7=7;filename*8=8;filename*9=9;filename*10=a;\r\n" +
      " filename*11=b; filename*12=c;filename*14=e\r\n",
    "attachment",
    "0123456789abc",
    /* "attachment", "basic" */
  ],

  // return nothing: invalid
  // (invalid; error recovery)
  [
    "attachment; filename*1=multi;\r\n" +
      " filename*2=line;\r\n" +
      " filename*3*=%20extended",
    "attachment",
    Cr.NS_ERROR_INVALID_ARG,
  ],

  // Bug 272541: Empty disposition type treated as "attachment"

  // sanity check
  [
    "attachment; filename=foo.html",
    "attachment",
    "foo.html",
    "attachment",
    "foo.html",
  ],

  // the actual bug
  [
    "; filename=foo.html",
    Cr.NS_ERROR_FIRST_HEADER_FIELD_COMPONENT_EMPTY,
    "foo.html",
    Cr.NS_ERROR_FIRST_HEADER_FIELD_COMPONENT_EMPTY,
    "foo.html",
  ],

  // regression check, but see bug 671204
  [
    "filename=foo.html",
    "filename=foo.html",
    "foo.html",
    "filename=foo.html",
    "foo.html",
  ],

  // Bug 384571: RFC 2231 parameters not decoded when appearing in reversed order

  // check ordering
  [
    "attachment; filename=basic; filename*0*=UTF-8''0;\r\n" +
      " filename*1=1; filename*2=2;filename*3=3;filename*4=4;filename*5=5;\r\n" +
      " filename*6=6; filename*7=7;filename*8=8;filename*9=9;filename*10=a;\r\n" +
      " filename*11=b; filename*12=c;filename*13=d;filename*15=f;filename*14=e;\r\n",
    "attachment",
    "0123456789abcdef",
    /* "attachment", "basic" */
  ],

  // check non-digits in sequence numbers
  [
    "attachment; filename=basic; filename*0*=UTF-8''0;\r\n" +
      " filename*1a=1\r\n",
    "attachment",
    "0",
    /* "attachment", "basic" */
  ],

  // check duplicate sequence numbers
  [
    "attachment; filename=basic; filename*0*=UTF-8''0;\r\n" +
      " filename*0=bad; filename*1=1;\r\n",
    "attachment",
    "0",
    /* "attachment", "basic" */
  ],

  // check overflow
  [
    "attachment; filename=basic; filename*0*=UTF-8''0;\r\n" +
      " filename*11111111111111111111111111111111111111111111111111111111111=1",
    "attachment",
    "0",
    /* "attachment", "basic" */
  ],

  // check underflow
  [
    // eslint-disable-next-line no-useless-concat
    "attachment; filename=basic; filename*0*=UTF-8''0;\r\n" + " filename*-1=1",
    "attachment",
    "0",
    /* "attachment", "basic" */
  ],

  // check mixed token/quoted-string
  [
    'attachment; filename=basic; filename*0="0";\r\n' +
      " filename*1=1;\r\n" +
      " filename*2*=%32",
    "attachment",
    "012",
    /* "attachment", "basic" */
  ],

  // check empty sequence number
  [
    "attachment; filename=basic; filename**=UTF-8''0\r\n",
    "attachment",
    "basic",
    "attachment",
    "basic",
  ],

  // Bug 419157: ensure that a MIME parameter with no charset information
  // fallbacks to Latin-1

  [
    "attachment;filename=IT839\x04\xB5(m8)2.pdf;",
    "attachment",
    "IT839\u0004\u00b5(m8)2.pdf",
  ],

  // Bug 588389: unescaping backslashes in quoted string parameters

  // '\"', should be parsed as '"'
  [
    "attachment; filename=" + DQUOTE + (BS + DQUOTE) + DQUOTE,
    "attachment",
    DQUOTE,
  ],

  // 'a\"b', should be parsed as 'a"b'
  [
    "attachment; filename=" + DQUOTE + "a" + (BS + DQUOTE) + "b" + DQUOTE,
    "attachment",
    "a" + DQUOTE + "b",
  ],

  // '\x', should be parsed as 'x'
  ["attachment; filename=" + DQUOTE + (BS + "x") + DQUOTE, "attachment", "x"],

  // test empty param (quoted-string)
  ["attachment; filename=" + DQUOTE + DQUOTE, "attachment", ""],

  // test empty param
  ["attachment; filename=", "attachment", ""],

  // Bug 601933: RFC 2047 does not apply to parameters (at least in HTTP)
  [
    "attachment; filename==?ISO-8859-1?Q?foo-=E4.html?=",
    "attachment",
    "foo-\u00e4.html",
    /* "attachment", "=?ISO-8859-1?Q?foo-=E4.html?=" */
  ],

  [
    'attachment; filename="=?ISO-8859-1?Q?foo-=E4.html?="',
    "attachment",
    "foo-\u00e4.html",
    /* "attachment", "=?ISO-8859-1?Q?foo-=E4.html?=" */
  ],

  // format sent by GMail as of 2012-07-23 (5987 overrides 2047)
  [
    "attachment; filename=\"=?ISO-8859-1?Q?foo-=E4.html?=\"; filename*=UTF-8''5987",
    "attachment",
    "5987",
  ],

  // Bug 651185: double quotes around 2231/5987 encoded param
  // Change reverted to backwards compat issues with various web services,
  // such as OWA (Bug 703015), plus similar problems in Thunderbird. If this
  // is tried again in the future, email probably needs to be special-cased.

  // sanity check
  ["attachment; filename*=utf-8''%41", "attachment", "A"],

  // the actual bug
  [
    "attachment; filename*=" + DQUOTE + "utf-8''%41" + DQUOTE,
    "attachment",
    "A",
  ],
  // previously with the fix for 651185:
  // "attachment", Cr.NS_ERROR_INVALID_ARG],

  // Bug 670333: Content-Disposition parser does not require presence of "="
  // in params

  // sanity check
  ["attachment; filename*=UTF-8''foo-%41.html", "attachment", "foo-A.html"],

  // the actual bug
  [
    "attachment; filename *=UTF-8''foo-%41.html",
    "attachment",
    Cr.NS_ERROR_INVALID_ARG,
  ],

  // the actual bug, without 2231/5987 encoding
  ["attachment; filename X", "attachment", Cr.NS_ERROR_INVALID_ARG],

  // sanity check with WS on both sides
  ["attachment; filename = foo-A.html", "attachment", "foo-A.html"],

  // Bug 685192: in RFC2231/5987 encoding, a missing charset field should be
  // treated as error

  // the actual bug
  ["attachment; filename*=''foo", "attachment", "foo"],
  // previously with the fix for 692574:
  // "attachment", Cr.NS_ERROR_INVALID_ARG],

  // sanity check
  ["attachment; filename*=a''foo", "attachment", "foo"],

  // Bug 692574: RFC2231/5987 decoding should not tolerate missing single
  // quotes

  // one missing
  ["attachment; filename*=UTF-8'foo-%41.html", "attachment", "foo-A.html"],
  // previously with the fix for 692574:
  // "attachment", Cr.NS_ERROR_INVALID_ARG],

  // both missing
  ["attachment; filename*=foo-%41.html", "attachment", "foo-A.html"],
  // previously with the fix for 692574:
  // "attachment", Cr.NS_ERROR_INVALID_ARG],

  // make sure fallback works
  [
    "attachment; filename*=UTF-8'foo-%41.html; filename=bar.html",
    "attachment",
    "foo-A.html",
  ],
  // previously with the fix for 692574:
  // "attachment", "bar.html"],

  // Bug 693806: RFC2231/5987 encoding: charset information should be treated
  // as authoritative

  // UTF-8 labeled ISO-8859-1
  ["attachment; filename*=ISO-8859-1''%c3%a4", "attachment", "\u00c3\u00a4"],

  // UTF-8 labeled ISO-8859-1, but with octets not allowed in ISO-8859-1
  // accepts x82, understands it as Win1252, maps it to Unicode \u20a1
  [
    "attachment; filename*=ISO-8859-1''%e2%82%ac",
    "attachment",
    "\u00e2\u201a\u00ac",
  ],

  // defective UTF-8
  ["attachment; filename*=UTF-8''A%e4B", "attachment", Cr.NS_ERROR_INVALID_ARG],

  // defective UTF-8, with fallback
  [
    "attachment; filename*=UTF-8''A%e4B; filename=fallback",
    "attachment",
    "fallback",
  ],

  // defective UTF-8 (continuations), with fallback
  [
    "attachment; filename*0*=UTF-8''A%e4B; filename=fallback",
    "attachment",
    "fallback",
  ],

  // check that charsets aren't mixed up
  [
    "attachment; filename*0*=ISO-8859-15''euro-sign%3d%a4; filename*=ISO-8859-1''currency-sign%3d%a4",
    "attachment",
    "currency-sign=\u00a4",
  ],

  // same as above, except reversed
  [
    "attachment; filename*=ISO-8859-1''currency-sign%3d%a4; filename*0*=ISO-8859-15''euro-sign%3d%a4",
    "attachment",
    "currency-sign=\u00a4",
  ],

  // Bug 704989: add workaround for broken Outlook Web App (OWA)
  // attachment handling

  ['attachment; filename*="a%20b"', "attachment", "a b"],

  // Bug 717121: crash nsMIMEHeaderParamImpl::DoParameterInternal

  ['attachment; filename="', "attachment", ""],

  // We used to read past string if last param w/o = and ;
  // Note: was only detected on windows PGO builds
  ["attachment; filename=foo; trouble", "attachment", "foo"],

  // Same, followed by space, hits another case
  ["attachment; filename=foo; trouble ", "attachment", "foo"],

  ["attachment", "attachment", Cr.NS_ERROR_INVALID_ARG],

  // Bug 730574: quoted-string in RFC2231-continuations not handled

  [
    'attachment; filename=basic; filename*0="foo"; filename*1="\\b\\a\\r.html"',
    "attachment",
    "foobar.html",
    /* "attachment", "basic" */
  ],

  // unmatched escape char
  [
    'attachment; filename=basic; filename*0="foo"; filename*1="\\b\\a\\',
    "attachment",
    "fooba\\",
    /* "attachment", "basic" */
  ],

  // Bug 732369: Content-Disposition parser does not require presence of ";" between params
  // optimally, this would not even return the disposition type "attachment"

  [
    "attachment; extension=bla filename=foo",
    "attachment",
    Cr.NS_ERROR_INVALID_ARG,
  ],

  // Bug 1440677 - spaces inside filenames ought to be quoted, but too many
  // servers do the wrong thing and most browsers accept this, so we were
  // forced to do the same for compat.
  ["attachment; filename=foo extension=bla", "attachment", "foo extension=bla"],

  ["attachment filename=foo", "attachment", Cr.NS_ERROR_INVALID_ARG],

  // Bug 777687: handling of broken %escapes

  ["attachment; filename*=UTF-8''f%oo; filename=bar", "attachment", "bar"],

  ["attachment; filename*=UTF-8''foo%; filename=bar", "attachment", "bar"],

  // Bug 783502 - xpcshell test netwerk/test/unit/test_MIME_params.js fails on AddressSanitizer
  ['attachment; filename="\\b\\a\\', "attachment", "ba\\"],

  // Bug 1412213 - do continue to parse, behind an empty parameter
  ["attachment; ; filename=foo", "attachment", "foo"],

  // Bug 1412213 - do continue to parse, behind a parameter w/o =
  ["attachment; badparameter; filename=foo", "attachment", "foo"],

  // Bug 1440677 - spaces inside filenames ought to be quoted, but too many
  // servers do the wrong thing and most browsers accept this, so we were
  // forced to do the same for compat.
  ["attachment; filename=foo bar.html", "attachment", "foo bar.html"],
  // Note: we keep the tab character, but later validation will replace with a space,
  // as file systems do not like tab characters.
  ["attachment; filename=foo\tbar.html", "attachment", "foo\tbar.html"],
  // Newlines get stripped completely (in practice, http header parsing may
  // munge these into spaces before they get to us, but we should check we deal
  // with them either way):
  ["attachment; filename=foo\nbar.html", "attachment", "foobar.html"],
  ["attachment; filename=foo\r\nbar.html", "attachment", "foobar.html"],
  ["attachment; filename=foo\rbar.html", "attachment", "foobar.html"],

  // Trailing rubbish shouldn't matter:
  ["attachment; filename=foo bar; garbage", "attachment", "foo bar"],
  ["attachment; filename=foo bar; extension=blah", "attachment", "foo bar"],

  // Check that whitespace processing can't crash.
  ["attachment; filename =      ", "attachment", ""],
];

var rfc5987paramtests = [
  [
    // basic test
    "UTF-8'language'value",
    "value",
    "language",
    Cr.NS_OK,
  ],
  [
    // percent decoding
    "UTF-8''1%202",
    "1 2",
    "",
    Cr.NS_OK,
  ],
  [
    // UTF-8
    "UTF-8''%c2%a3%20and%20%e2%82%ac%20rates",
    "\u00a3 and \u20ac rates",
    "",
    Cr.NS_OK,
  ],
  [
    // missing charset
    "''abc",
    "",
    "",
    Cr.NS_ERROR_INVALID_ARG,
  ],
  [
    // ISO-8859-1: unsupported
    "ISO-8859-1''%A3%20rates",
    "",
    "",
    Cr.NS_ERROR_INVALID_ARG,
  ],
  [
    // unknown charset
    "foo''abc",
    "",
    "",
    Cr.NS_ERROR_INVALID_ARG,
  ],
  [
    // missing component
    "abc",
    "",
    "",
    Cr.NS_ERROR_INVALID_ARG,
  ],
  [
    // missing component
    "'abc",
    "",
    "",
    Cr.NS_ERROR_INVALID_ARG,
  ],
  [
    // illegal chars
    "UTF-8''a b",
    "",
    "",
    Cr.NS_ERROR_INVALID_ARG,
  ],
  [
    // broken % escapes
    "UTF-8''a%zz",
    "",
    "",
    Cr.NS_ERROR_INVALID_ARG,
  ],
  [
    // broken % escapes
    "UTF-8''a%b",
    "",
    "",
    Cr.NS_ERROR_INVALID_ARG,
  ],
  [
    // broken % escapes
    "UTF-8''a%",
    "",
    "",
    Cr.NS_ERROR_INVALID_ARG,
  ],
  [
    // broken UTF-8
    "UTF-8''%A3%20rates",
    "",
    "",
    0x8050000e /* NS_ERROR_UDEC_ILLEGALINPUT */,
  ],
];

function do_tests(whichRFC) {
  var mhp = Cc["@mozilla.org/network/mime-hdrparam;1"].getService(
    Ci.nsIMIMEHeaderParam
  );

  var unused = { value: null };

  for (var i = 0; i < tests.length; ++i) {
    dump("Testing #" + i + ": " + tests[i] + "\n");

    // check disposition type
    var expectedDt =
      tests[i].length == 3 || whichRFC == 0 ? tests[i][1] : tests[i][3];

    try {
      let result;

      if (whichRFC == 0) {
        result = mhp.getParameter(tests[i][0], "", "UTF-8", true, unused);
      } else {
        result = mhp.getParameterHTTP(tests[i][0], "", "UTF-8", true, unused);
      }

      Assert.equal(result, expectedDt);
    } catch (e) {
      // Tests can also succeed by expecting to fail with given error code
      if (e.result) {
        // Allow following tests to run by catching exception from do_check_eq()
        try {
          Assert.equal(e.result, expectedDt);
        } catch (e) {}
      }
      continue;
    }

    // check filename parameter
    var expectedFn =
      tests[i].length == 3 || whichRFC == 0 ? tests[i][2] : tests[i][4];

    try {
      let result;

      if (whichRFC == 0) {
        result = mhp.getParameter(
          tests[i][0],
          "filename",
          "UTF-8",
          true,
          unused
        );
      } else {
        result = mhp.getParameterHTTP(
          tests[i][0],
          "filename",
          "UTF-8",
          true,
          unused
        );
      }

      Assert.equal(result, expectedFn);
    } catch (e) {
      // Tests can also succeed by expecting to fail with given error code
      if (e.result) {
        // Allow following tests to run by catching exception from do_check_eq()
        try {
          Assert.equal(e.result, expectedFn);
        } catch (e) {}
      }
      continue;
    }
  }
}

function test_decode5987Param() {
  var mhp = Cc["@mozilla.org/network/mime-hdrparam;1"].getService(
    Ci.nsIMIMEHeaderParam
  );

  for (var i = 0; i < rfc5987paramtests.length; ++i) {
    dump("Testing #" + i + ": " + rfc5987paramtests[i] + "\n");

    var lang = {};
    try {
      var decoded = mhp.decodeRFC5987Param(rfc5987paramtests[i][0], lang);
      if (rfc5987paramtests[i][3] == Cr.NS_OK) {
        Assert.equal(rfc5987paramtests[i][1], decoded);
        Assert.equal(rfc5987paramtests[i][2], lang.value);
      } else {
        Assert.equal(rfc5987paramtests[i][3], "instead got: " + decoded);
      }
    } catch (e) {
      Assert.equal(rfc5987paramtests[i][3], e.result);
    }
  }
}

function run_test() {
  // Test RFC 2231 (complete header field values)
  do_tests(0);

  // Test RFC 5987 (complete header field values)
  do_tests(1);

  // tests for RFC5987 parameter parsing
  test_decode5987Param();
}
