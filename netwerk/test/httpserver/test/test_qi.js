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
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jeff Walden <jwalden+code@mit.edu> (original author)
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

/*
 * Verify the presence of explicit QueryInterface methods on XPCOM objects
 * exposed by httpd.js, rather than allowing QueryInterface to be implicitly
 * created by XPConnect.
 */

var tests =
  [
   new Test("http://localhost:4444/test",
            null, start_test, null),
   new Test("http://localhost:4444/sjs/qi.sjs",
            null, start_sjs_qi, null),
  ];

function run_test()
{
  var srv = createServer();

  var qi;
  try
  {
    qi = srv.identity.QueryInterface(Ci.nsIHttpServerIdentity);
  }
  catch (e)
  {
    var exstr = ("" + e).split(/[\x09\x20-\x7f\x81-\xff]+/)[0];
    do_throw("server identity didn't QI: " + exstr);
    return;
  }

  srv.registerPathHandler("/test", testHandler);
  srv.registerDirectory("/", do_get_file("data/"));
  srv.registerContentType("sjs", "sjs");
  srv.start(4444);

  runHttpTests(tests, testComplete(srv));
}


// TEST DATA

function start_test(ch, cx)
{
  do_check_eq(ch.responseStatusText, "QI Tests Passed");
  do_check_eq(ch.responseStatus, 200);
}

function start_sjs_qi(ch, cx)
{
  do_check_eq(ch.responseStatusText, "SJS QI Tests Passed");
  do_check_eq(ch.responseStatus, 200);
}


function testHandler(request, response)
{
  var exstr;
  var qid;

  response.setStatusLine(request.httpVersion, 500, "FAIL");

  var passed = false;
  try
  {
    qid = request.QueryInterface(Ci.nsIHttpRequest);
    passed = qid === request;
  }
  catch (e)
  {
    exstr = ("" + e).split(/[\x09\x20-\x7f\x81-\xff]+/)[0];
    response.setStatusLine(request.httpVersion, 500,
                           "request doesn't QI: " + exstr);
    return;
  }
  if (!passed)
  {
    response.setStatusLine(request.httpVersion, 500, "request QI'd wrongly?");
    return;
  }

  passed = false;
  try
  {
    qid = response.QueryInterface(Ci.nsIHttpResponse);
    passed = qid === response;
  }
  catch (e)
  {
    exstr = ("" + e).split(/[\x09\x20-\x7f\x81-\xff]+/)[0];
    response.setStatusLine(request.httpVersion, 500,
                           "response doesn't QI: " + exstr);
    return;
  }
  if (!passed)
  {
    response.setStatusLine(request.httpVersion, 500, "response QI'd wrongly?");
    return;
  }

  response.setStatusLine(request.httpVersion, 200, "QI Tests Passed");
}
