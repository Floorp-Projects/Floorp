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
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2005
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
var hadCharset = {};
var type;

function reset() {
  delete charset.value;
  delete hadCharset.value;
  type = undefined;
}

function check(aType, aCharset, aHadCharset) {
  do_check_eq(type, aType);
  do_check_eq(aCharset, charset.value);
  do_check_eq(aHadCharset, hadCharset.value);
  reset();
}

function run_test() {
  var netutil = Components.classes["@mozilla.org/network/util;1"]
                          .getService(Components.interfaces.nsINetUtil);

  type = netutil.parseContentType("text/html", charset, hadCharset);
  check("text/html", "", false);

  type = netutil.parseContentType("TEXT/HTML", charset, hadCharset);
  check("text/html", "", false);

  type = netutil.parseContentType("text/html, text/html", charset, hadCharset);
  check("text/html", "", false);

  type = netutil.parseContentType("text/html, text/plain",
				  charset, hadCharset);
  check("text/plain", "", false);

  type = netutil.parseContentType('text/html, ', charset, hadCharset);
  check("text/html", "", false);

  type = netutil.parseContentType('text/html, */*', charset, hadCharset);
  check("text/html", "", false);

  type = netutil.parseContentType('text/html, foo', charset, hadCharset);
  check("text/html", "", false);

  type = netutil.parseContentType("text/html; charset=ISO-8859-1",
				  charset, hadCharset);
  check("text/html", "ISO-8859-1", true);

  type = netutil.parseContentType('text/html; charset="ISO-8859-1"',
				  charset, hadCharset);
  check("text/html", "ISO-8859-1", true);

  type = netutil.parseContentType("text/html; charset='ISO-8859-1'",
				  charset, hadCharset);
  check("text/html", "'ISO-8859-1'", true);

  type = netutil.parseContentType("text/html; charset=\"ISO-8859-1\", text/html",
				  charset, hadCharset);
  check("text/html", "ISO-8859-1", true);

  type = netutil.parseContentType("text/html; charset=\"ISO-8859-1\", text/html; charset=UTF8",
				  charset, hadCharset);
  check("text/html", "UTF8", true);

  type = netutil.parseContentType("text/html; charset=ISO-8859-1, TEXT/HTML", charset, hadCharset);
  check("text/html", "ISO-8859-1", true);

  type = netutil.parseContentType("text/html; charset=ISO-8859-1, TEXT/plain", charset, hadCharset);
  check("text/plain", "", true);

  type = netutil.parseContentType("text/plain, TEXT/HTML; charset=ISO-8859-1, text/html, TEXT/HTML", charset, hadCharset);
  check("text/html", "ISO-8859-1", true);

  type = netutil.parseContentType('text/plain, TEXT/HTML; param="charset=UTF8"; charset="ISO-8859-1"; param2="charset=UTF16", text/html, TEXT/HTML', charset, hadCharset);
  check("text/html", "ISO-8859-1", true);

  type = netutil.parseContentType('text/plain, TEXT/HTML; param=charset=UTF8; charset="ISO-8859-1"; param2=charset=UTF16, text/html, TEXT/HTML', charset, hadCharset);
  check("text/html", "ISO-8859-1", true);  

  type = netutil.parseContentType("text/plain; param= , text/html", charset, hadCharset);
  check("text/html", "", false);

  type = netutil.parseContentType('text/plain; param=", text/html"', charset, hadCharset);
  check("text/plain", "", false);

  type = netutil.parseContentType('text/plain; param=", \\" , text/html"', charset, hadCharset);
  check("text/plain", "", false);

  type = netutil.parseContentType('text/plain; param=", \\" , text/html , "', charset, hadCharset);
  check("text/plain", "", false);

  type = netutil.parseContentType('text/plain param=", \\" , text/html , "', charset, hadCharset);
  check("text/plain", "", false);

  type = netutil.parseContentType('text/plain charset=UTF8', charset, hadCharset);
  check("text/plain", "", false);

  type = netutil.parseContentType('text/plain, TEXT/HTML; param="charset=UTF8"; ; param2="charset=UTF16", text/html, TEXT/HTML', charset, hadCharset);
  check("text/html", "", false);

  // Bug 700589

  // check that single quote doesn't confuse parsing of subsequent parameters
  type = netutil.parseContentType("text/plain; x='; charset=\"UTF-8\"", charset, hadCharset);
  check("text/plain", "UTF-8", true);

  // check that single quotes do not get removed from extracted charset
  type = netutil.parseContentType("text/plain; charset='UTF-8'", charset, hadCharset);
  check("text/plain", "'UTF-8'", true);
}
