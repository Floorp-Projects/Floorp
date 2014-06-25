/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var charset = {};
var charsetStart = {};
var charsetEnd = {};
var hadCharset;

function reset() {
  delete charset.value;
  delete charsetStart.value;
  delete charsetEnd.value;
  hadCharset = undefined;
}

function check(aHadCharset, aCharset, aCharsetStart, aCharsetEnd) {
  do_check_eq(aHadCharset, hadCharset);
  do_check_eq(aCharset, charset.value);
  do_check_eq(aCharsetStart, charsetStart.value);
  do_check_eq(aCharsetEnd, charsetEnd.value);
}

function run_test() {
  var netutil = Components.classes["@mozilla.org/network/util;1"]
                          .getService(Components.interfaces.nsINetUtil);
  hadCharset =
    netutil.extractCharsetFromContentType("text/html", charset, charsetStart,
					  charsetEnd);
  check(false, "", 9, 9);

  hadCharset =
    netutil.extractCharsetFromContentType("TEXT/HTML", charset, charsetStart,
					  charsetEnd);
  check(false, "", 9, 9);

  hadCharset =
    netutil.extractCharsetFromContentType("text/html, text/html", charset,
					  charsetStart, charsetEnd);
  check(false, "", 9, 9);

  hadCharset =
    netutil.extractCharsetFromContentType("text/html, text/plain",
					  charset, charsetStart, charsetEnd);
  check(false, "", 21, 21);

  hadCharset =
    netutil.extractCharsetFromContentType('text/html, ', charset, charsetStart,
					  charsetEnd);
  check(false, "", 9, 9);

  hadCharset =
    netutil.extractCharsetFromContentType('text/html, */*', charset,
					  charsetStart, charsetEnd);
  check(false, "", 9, 9);

  hadCharset =
    netutil.extractCharsetFromContentType('text/html, foo', charset,
					  charsetStart, charsetEnd);
  check(false, "", 9, 9);

  hadCharset =
    netutil.extractCharsetFromContentType("text/html; charset=ISO-8859-1",
					  charset, charsetStart, charsetEnd);
  check(true, "ISO-8859-1", 9, 29);

  hadCharset =
    netutil.extractCharsetFromContentType("text/html  ;    charset=ISO-8859-1",
					  charset, charsetStart, charsetEnd);
  check(true, "ISO-8859-1", 11, 34);

  hadCharset =
    netutil.extractCharsetFromContentType("text/html  ;    charset=ISO-8859-1  ",
					  charset, charsetStart, charsetEnd);
  check(true, "ISO-8859-1", 11, 36);

  hadCharset =
    netutil.extractCharsetFromContentType("text/html  ;    charset=ISO-8859-1 ; ",
					  charset, charsetStart, charsetEnd);
  check(true, "ISO-8859-1", 11, 35);

  hadCharset =
    netutil.extractCharsetFromContentType('text/html; charset="ISO-8859-1"',
					  charset, charsetStart, charsetEnd);
  check(true, "ISO-8859-1", 9, 31);

  hadCharset =
    netutil.extractCharsetFromContentType("text/html; charset='ISO-8859-1'",
					  charset, charsetStart, charsetEnd);
  check(true, "'ISO-8859-1'", 9, 31);

  hadCharset =
    netutil.extractCharsetFromContentType("text/html; charset=\"ISO-8859-1\", text/html",
					  charset, charsetStart, charsetEnd);
  check(true, "ISO-8859-1", 9, 31);

  hadCharset =
    netutil.extractCharsetFromContentType("text/html; charset=\"ISO-8859-1\", text/html; charset=UTF8",
					  charset, charsetStart, charsetEnd);
  check(true, "UTF8", 42, 56);

  hadCharset =
    netutil.extractCharsetFromContentType("text/html; charset=ISO-8859-1, TEXT/HTML",
					  charset, charsetStart, charsetEnd);
  check(true, "ISO-8859-1", 9, 29);

  hadCharset =
    netutil.extractCharsetFromContentType("text/html; charset=ISO-8859-1, TEXT/plain",
					  charset, charsetStart, charsetEnd);
  check(false, "", 41, 41);

  hadCharset =
    netutil.extractCharsetFromContentType("text/plain, TEXT/HTML; charset=\"ISO-8859-1\", text/html, TEXT/HTML",
					  charset, charsetStart, charsetEnd);
  check(true, "ISO-8859-1", 21, 43);

  hadCharset =
    netutil.extractCharsetFromContentType('text/plain, TEXT/HTML; param="charset=UTF8"; charset="ISO-8859-1"; param2="charset=UTF16", text/html, TEXT/HTML',
					  charset, charsetStart, charsetEnd);
  check(true, "ISO-8859-1", 43, 65);

  hadCharset =
    netutil.extractCharsetFromContentType('text/plain, TEXT/HTML; param=charset=UTF8; charset="ISO-8859-1"; param2=charset=UTF16, text/html, TEXT/HTML',
					  charset, charsetStart, charsetEnd);
  check(true, "ISO-8859-1", 41, 63);

  hadCharset =
    netutil.extractCharsetFromContentType("text/plain; param= , text/html",
					  charset, charsetStart, charsetEnd);
  check(false, "", 30, 30);

  hadCharset =
    netutil.extractCharsetFromContentType('text/plain; param=", text/html"',
					  charset, charsetStart, charsetEnd);
  check(false, "", 10, 10);

  hadCharset =
    netutil.extractCharsetFromContentType('text/plain; param=", \\" , text/html"',
					  charset, charsetStart, charsetEnd);
  check(false, "", 10, 10);

  hadCharset =
    netutil.extractCharsetFromContentType('text/plain; param=", \\" , text/html , "',
					  charset, charsetStart, charsetEnd);
  check(false, "", 10, 10);

  hadCharset =
    netutil.extractCharsetFromContentType('text/plain param=", \\" , text/html , "',
					  charset, charsetStart, charsetEnd);
  check(false, "", 38, 38);

  hadCharset =
    netutil.extractCharsetFromContentType('text/plain charset=UTF8',
					  charset, charsetStart, charsetEnd);
  check(false, "", 23, 23);

  hadCharset =
    netutil.extractCharsetFromContentType('text/plain, TEXT/HTML; param="charset=UTF8"; ; param2="charset=UTF16", text/html, TEXT/HTML',
					  charset, charsetStart, charsetEnd);
  check(false, "", 21, 21);
}
