/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

function run_test() {
  var netutil = Components.classes["@mozilla.org/network/util;1"]
                          .getService(Components.interfaces.nsINetUtil);

  type = netutil.parseRequestContentType("text/html", charset, hadCharset);
  check("text/html", "", false);

  type = netutil.parseResponseContentType("text/html", charset, hadCharset);
  check("text/html", "", false);

  type = netutil.parseRequestContentType("TEXT/HTML", charset, hadCharset);
  check("text/html", "", false);

  type = netutil.parseResponseContentType("TEXT/HTML", charset, hadCharset);
  check("text/html", "", false);

  type = netutil.parseRequestContentType("text/html, text/html", charset, hadCharset);
  check("", "", false);

  type = netutil.parseResponseContentType("text/html, text/html", charset, hadCharset);
  check("text/html", "", false);

  type = netutil.parseRequestContentType("text/html, text/plain",
				  charset, hadCharset);
  check("", "", false);

  type = netutil.parseResponseContentType("text/html, text/plain",
				  charset, hadCharset);
  check("text/plain", "", false);

  type = netutil.parseRequestContentType('text/html, ', charset, hadCharset);
  check("", "", false);

  type = netutil.parseResponseContentType('text/html, ', charset, hadCharset);
  check("text/html", "", false);

  type = netutil.parseRequestContentType('text/html, */*', charset, hadCharset);
  check("", "", false);

  type = netutil.parseResponseContentType('text/html, */*', charset, hadCharset);
  check("text/html", "", false);

  type = netutil.parseRequestContentType('text/html, foo', charset, hadCharset);
  check("", "", false);

  type = netutil.parseResponseContentType('text/html, foo', charset, hadCharset);
  check("text/html", "", false);

  type = netutil.parseRequestContentType("text/html; charset=ISO-8859-1",
				  charset, hadCharset);
  check("text/html", "ISO-8859-1", true);

  type = netutil.parseResponseContentType("text/html; charset=ISO-8859-1",
				  charset, hadCharset);
  check("text/html", "ISO-8859-1", true);

  type = netutil.parseRequestContentType('text/html; charset="ISO-8859-1"',
				  charset, hadCharset);
  check("text/html", "ISO-8859-1", true);

  type = netutil.parseResponseContentType('text/html; charset="ISO-8859-1"',
				  charset, hadCharset);
  check("text/html", "ISO-8859-1", true);

  type = netutil.parseRequestContentType("text/html; charset='ISO-8859-1'",
				  charset, hadCharset);
  check("text/html", "'ISO-8859-1'", true);

  type = netutil.parseResponseContentType("text/html; charset='ISO-8859-1'",
				  charset, hadCharset);
  check("text/html", "'ISO-8859-1'", true);

  type = netutil.parseRequestContentType("text/html; charset=\"ISO-8859-1\", text/html",
				  charset, hadCharset);
  check("", "", false);

  type = netutil.parseResponseContentType("text/html; charset=\"ISO-8859-1\", text/html",
				  charset, hadCharset);
  check("text/html", "ISO-8859-1", true);

  type = netutil.parseRequestContentType("text/html; charset=\"ISO-8859-1\", text/html; charset=UTF8",
				  charset, hadCharset);
  check("", "", false);

  type = netutil.parseResponseContentType("text/html; charset=\"ISO-8859-1\", text/html; charset=UTF8",
				  charset, hadCharset);
  check("text/html", "UTF8", true);

  type = netutil.parseRequestContentType("text/html; charset=ISO-8859-1, TEXT/HTML", charset, hadCharset);
  check("", "", false);

  type = netutil.parseResponseContentType("text/html; charset=ISO-8859-1, TEXT/HTML", charset, hadCharset);
  check("text/html", "ISO-8859-1", true);

  type = netutil.parseRequestContentType("text/html; charset=ISO-8859-1, TEXT/plain", charset, hadCharset);
  check("", "", false);

  type = netutil.parseResponseContentType("text/html; charset=ISO-8859-1, TEXT/plain", charset, hadCharset);
  check("text/plain", "", true);

  type = netutil.parseRequestContentType("text/plain, TEXT/HTML; charset=ISO-8859-1, text/html, TEXT/HTML", charset, hadCharset);
  check("", "", false);

  type = netutil.parseResponseContentType("text/plain, TEXT/HTML; charset=ISO-8859-1, text/html, TEXT/HTML", charset, hadCharset);
  check("text/html", "ISO-8859-1", true);

  type = netutil.parseRequestContentType('text/plain, TEXT/HTML; param="charset=UTF8"; charset="ISO-8859-1"; param2="charset=UTF16", text/html, TEXT/HTML', charset, hadCharset);
  check("", "", false);

  type = netutil.parseResponseContentType('text/plain, TEXT/HTML; param="charset=UTF8"; charset="ISO-8859-1"; param2="charset=UTF16", text/html, TEXT/HTML', charset, hadCharset);
  check("text/html", "ISO-8859-1", true);

  type = netutil.parseRequestContentType('text/plain, TEXT/HTML; param=charset=UTF8; charset="ISO-8859-1"; param2=charset=UTF16, text/html, TEXT/HTML', charset, hadCharset);
  check("", "", false);

  type = netutil.parseResponseContentType('text/plain, TEXT/HTML; param=charset=UTF8; charset="ISO-8859-1"; param2=charset=UTF16, text/html, TEXT/HTML', charset, hadCharset);
  check("text/html", "ISO-8859-1", true);  

  type = netutil.parseRequestContentType('text/plain, TEXT/HTML; param=charset=UTF8; charset="ISO-8859-1"; param2=charset=UTF16, text/html, TEXT/HTML', charset, hadCharset);
  check("", "", false);

  type = netutil.parseRequestContentType("text/plain; param= , text/html", charset, hadCharset);
  check("", "", false);

  type = netutil.parseResponseContentType("text/plain; param= , text/html", charset, hadCharset);
  check("text/html", "", false);

  type = netutil.parseRequestContentType('text/plain; param=", text/html"', charset, hadCharset);
  check("text/plain", "", false);

  type = netutil.parseResponseContentType('text/plain; param=", text/html"', charset, hadCharset);
  check("text/plain", "", false);

  type = netutil.parseRequestContentType('text/plain; param=", \\" , text/html"', charset, hadCharset);
  check("text/plain", "", false);

  type = netutil.parseResponseContentType('text/plain; param=", \\" , text/html"', charset, hadCharset);
  check("text/plain", "", false);

  type = netutil.parseRequestContentType('text/plain; param=", \\" , text/html , "', charset, hadCharset);
  check("text/plain", "", false);

  type = netutil.parseResponseContentType('text/plain; param=", \\" , text/html , "', charset, hadCharset);
  check("text/plain", "", false);

  type = netutil.parseRequestContentType('text/plain param=", \\" , text/html , "', charset, hadCharset);
  check("", "", false);

  type = netutil.parseResponseContentType('text/plain param=", \\" , text/html , "', charset, hadCharset);
  check("text/plain", "", false);

  type = netutil.parseRequestContentType('text/plain charset=UTF8', charset, hadCharset);
  check("", "", false);

  type = netutil.parseResponseContentType('text/plain charset=UTF8', charset, hadCharset);
  check("text/plain", "", false);

  type = netutil.parseRequestContentType('text/plain, TEXT/HTML; param="charset=UTF8"; ; param2="charset=UTF16", text/html, TEXT/HTML', charset, hadCharset);
  check("", "", false);

  type = netutil.parseResponseContentType('text/plain, TEXT/HTML; param="charset=UTF8"; ; param2="charset=UTF16", text/html, TEXT/HTML', charset, hadCharset);
  check("text/html", "", false);

  // Bug 562915 - correctness: "\x" is "x"
  type = netutil.parseResponseContentType('text/plain; charset="UTF\\-8"', charset, hadCharset);
  check("text/plain", "UTF-8", true);

  // Bug 700589

  // check that single quote doesn't confuse parsing of subsequent parameters
  type = netutil.parseResponseContentType("text/plain; x='; charset=\"UTF-8\"", charset, hadCharset);
  check("text/plain", "UTF-8", true);

  // check that single quotes do not get removed from extracted charset
  type = netutil.parseResponseContentType("text/plain; charset='UTF-8'", charset, hadCharset);
  check("text/plain", "'UTF-8'", true);
}
