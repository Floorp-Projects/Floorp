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
 * Portions created by the Initial Developer are Copyright (C) 2007
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

// tests the registerFile API

const BASE = "http://localhost:4444";

function isException(e, code)
{
  if (e !== code && e.result !== code)
    do_throw("unexpected error: " + e);
}

var file = do_get_file("netwerk/test/httpserver/test/test_registerfile.js");

function onStart(ch, cx)
{
  do_check_eq(ch.responseStatus, 200);
}

function onStop(ch, cx, status, data)
{
  // not sufficient for equality, but not likely to be wrong!
  do_check_eq(data.length, file.fileSize);
}

var test = new Test(BASE + "/foo", null, onStart, onStop);

function run_test()
{
  var srv = createServer();

  try
  {
    srv.registerFile("/foo", do_get_file("netwerk/test/httpserver/"));
    throw "registerFile succeeded!";
  }
  catch (e)
  {
    isException(e, Cr.NS_ERROR_INVALID_ARG);
  }

  srv.registerFile("/foo", file);
  srv.start(4444);

  runHttpTests([test], function() { srv.stop(); });
}
