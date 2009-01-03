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
 *   Jeff Walden <jwalden+code@mit.edu>
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

// exercises the server's state-preservation API

const PORT = 4444;

var srv;

function run_test()
{
  srv = createServer();
  var sjsDir = do_get_file("netwerk/test/httpserver/test/data/sjs/");
  srv.registerDirectory("/", sjsDir);
  srv.registerContentType("sjs", "sjs");
  srv.registerPathHandler("/path-handler", pathHandler);
  srv.start(PORT);

  function done()
  {
    do_check_eq(srv.getState("foopy"), "done!");
    srv.stop();
  }

  runHttpTests(tests, done);
}


/************
 * HANDLERS *
 ************/

function pathHandler(request, response)
{
  response.setHeader("Cache-Control", "no-cache", false);

  var oldval = srv.getState("foopy");
  response.setHeader("X-Old-Value", oldval, false);
  var newval = "back to SJS!";
  srv.setState("foopy", newval);
  response.setHeader("X-New-Value", newval, false);
}


/***************
 * BEGIN TESTS *
 ***************/

var test;
var tests = [];


function expectValues(ch, oldval, newval)
{
  do_check_eq(ch.getResponseHeader("X-Old-Value"), oldval);
  do_check_eq(ch.getResponseHeader("X-New-Value"), newval);
}


test = new Test("http://localhost:4444/state.sjs?1",
                null, start_initial);
tests.push(test);

function start_initial(ch, cx)
{
  expectValues(ch, "", "first set!");
}


test = new Test("http://localhost:4444/state.sjs?2",
                null, start_next);
tests.push(test);

function start_next(ch, cx)
{
  expectValues(ch, "first set!", "changed!");
}


test = new Test("http://localhost:4444/path-handler",
                null, start_handler);
tests.push(test);

function start_handler(ch, cx)
{
  expectValues(ch, "changed!", "back to SJS!");
}


test = new Test("http://localhost:4444/state.sjs?3",
                null, start_last);
tests.push(test);

function start_last(ch, cx)
{
  expectValues(ch, "back to SJS!", "done!");
}
