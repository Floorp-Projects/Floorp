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
 * Jeff Walden <jwalden+code@mit.edu>.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

// Request handlers may throw exceptions, and those exception should be caught
// by the server and converted into the proper error codes.

var tests =
  [
   new Test("http://localhost:4444/throws/exception",
            null, start_throws_exception, succeeded),
   new Test("http://localhost:4444/this/file/does/not/exist/and/404s",
            null, start_nonexistent_404_fails_so_400, succeeded),
   new Test("http://localhost:4444/attempts/404/fails/so/400/fails/so/500s",
            register400Handler, start_multiple_exceptions_500, succeeded),
  ];

var srv;

function run_test()
{
  srv = createServer();

  srv.registerErrorHandler(404, throwsException);
  srv.registerPathHandler("/throws/exception", throwsException);

  srv.start(4444);

  runHttpTests(tests, function() { srv.stop(); });
}


// TEST DATA

function checkStatusLine(channel, httpMaxVer, httpMinVer, httpCode, statusText)
{
  do_check_eq(channel.responseStatus, httpCode);
  do_check_eq(channel.responseStatusText, statusText);

  var respMaj = {}, respMin = {};
  channel.getResponseVersion(respMaj, respMin);
  do_check_eq(respMaj.value, httpMaxVer);
  do_check_eq(respMin.value, httpMinVer);
}

function start_throws_exception(ch, cx)
{
  checkStatusLine(ch, 1, 1, 500, "Internal Server Error");
}

function start_nonexistent_404_fails_so_400(ch, cx)
{
  checkStatusLine(ch, 1, 1, 400, "Bad Request");
}

function start_multiple_exceptions_500(ch, cx)
{
  checkStatusLine(ch, 1, 1, 500, "Internal Server Error");
}

function succeeded(ch, cx, status, data)
{
  do_check_true(Components.isSuccessCode(status));
}

function register400Handler(ch)
{
  srv.registerErrorHandler(400, throwsException);
}


// PATH HANDLERS

// /throws/exception (and also a 404 and 400 error handler)
function throwsException(metadata, response)
{
  throw "this shouldn't cause an exit...";
  do_throw("Not reached!");
}
