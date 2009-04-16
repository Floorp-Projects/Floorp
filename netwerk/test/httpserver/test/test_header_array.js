/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is httpd.js code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Honza Bambas <honzab@firemni.cz>
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

// test that special headers are sent as an array of headers with the same name

const PORT = 4444;

function run_test()
{
  var srv;

  srv = createServer();
  srv.registerPathHandler("/path-handler", pathHandler);
  srv.start(PORT);

  runHttpTests(tests, testComplete(srv));
}


/************
 * HANDLERS *
 ************/

function pathHandler(request, response)
{
  response.setHeader("Cache-Control", "no-cache", false);

  response.setHeader("Proxy-Authenticate", "First line 1", true);
  response.setHeader("Proxy-Authenticate", "Second line 1", true);
  response.setHeader("Proxy-Authenticate", "Third line 1", true);

  response.setHeader("WWW-Authenticate", "Not merged line 1", false);
  response.setHeader("WWW-Authenticate", "Not merged line 2", true);

  response.setHeader("WWW-Authenticate", "First line 2", false);
  response.setHeader("WWW-Authenticate", "Second line 2", true);
  response.setHeader("WWW-Authenticate", "Third line 2", true);

  response.setHeader("X-Single-Header-Merge", "Single 1", true);
  response.setHeader("X-Single-Header-Merge", "Single 2", true);
}

/***************
 * BEGIN TESTS *
 ***************/

var tests = [
  new Test("http://localhost:4444/path-handler",
           null, check)];

function check(ch, cx)
{
  var headerValue;

  headerValue = ch.getResponseHeader("Proxy-Authenticate");
  do_check_eq(headerValue, "First line 1\nSecond line 1\nThird line 1");
  headerValue = ch.getResponseHeader("WWW-Authenticate");
  do_check_eq(headerValue, "First line 2\nSecond line 2\nThird line 2");
  headerValue = ch.getResponseHeader("X-Single-Header-Merge");
  do_check_eq(headerValue, "Single 1,Single 2");
}
