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
 * The Original Code is MozJSHTTP code.
 *
 * The Initial Developer of the Original Code is
 * Chris Double <chris.double@double.co.nz>.
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

// checks if a byte range request and non-byte range request retrieve the
// correct data.

const PREFIX = "http://localhost:4444";

var tests =
  [
   new  Test(PREFIX + "/normal-file.txt",
             init_byterange, start_byterange, stop_byterange),
   new  Test(PREFIX + "/normal-file.txt",
             null, start_normal, stop_normal)
   ];


function run_test()
{
  var srv = createServer();
  var dir = do_get_file("netwerk/test/httpserver/test/data/name-scheme/");
  srv.registerDirectory("/", dir);

  srv.start(4444);

  runHttpTests(tests, function() { srv.stop(); });
}

function start_normal(ch, cx)
{
  do_check_eq(ch.responseStatus, 200);
  do_check_eq(ch.getResponseHeader("Content-Type"), "text/plain");
}

function stop_normal(ch, cx, status, data)
{
  do_check_eq(data[0], 84);
  do_check_eq(data[20], 10);
  do_check_eq(data.length, 21);
}

function init_byterange(ch)
{
  ch.setRequestHeader("Range", "bytes=10-", false);
}


function start_byterange(ch, cx)
{
  do_check_eq(ch.responseStatus, 206);
  do_check_eq(ch.getResponseHeader("Content-Type"), "text/plain");
}

function stop_byterange(ch, cx, status, data)
{
  do_check_eq(data[0], 100);
  do_check_eq(data[10], 10);
  do_check_eq(data.length, 11);
}

