/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Boris Zbarsky <bzbarsky@mit.edu>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
