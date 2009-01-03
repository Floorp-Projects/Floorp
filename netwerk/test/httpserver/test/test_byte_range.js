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
   new Test(PREFIX + "/range.txt",
            init_byterange, start_byterange, stop_byterange),
   new Test(PREFIX + "/range.txt",
            init_byterange2, start_byterange2),
   new Test(PREFIX + "/range.txt",
            init_byterange3, start_byterange3, stop_byterange3),
   new Test(PREFIX + "/range.txt",
            init_byterange4, start_byterange4),
   new Test(PREFIX + "/range.txt",
            init_byterange5, start_byterange5, stop_byterange5),
   new Test(PREFIX + "/range.txt",
            init_byterange6, start_byterange6, stop_byterange6),
   new Test(PREFIX + "/range.txt",
            init_byterange7, start_byterange7, stop_byterange7),
   new Test(PREFIX + "/range.txt",
            init_byterange8, start_byterange8, stop_byterange8),
   new Test(PREFIX + "/range.txt",
            init_byterange9, start_byterange9, stop_byterange9),
   new Test(PREFIX + "/range.txt",
            init_byterange10, start_byterange10),
   new Test(PREFIX + "/range.txt",
            init_byterange11, start_byterange11, stop_byterange11),
   new Test(PREFIX + "/empty.txt",
            null, start_byterange12, stop_byterange12),
   new Test(PREFIX + "/range.txt",
            null, start_normal, stop_normal)
   ];

function run_test()
{
  var srv = createServer();
  var dir = do_get_file("netwerk/test/httpserver/test/data/ranges/");
  srv.registerDirectory("/", dir);

  srv.start(4444);

  runHttpTests(tests, function() { srv.stop(); });
}

function start_normal(ch, cx)
{
  do_check_eq(ch.responseStatus, 200);
  do_check_eq(ch.getResponseHeader("Content-Length"), "21");
  do_check_eq(ch.getResponseHeader("Content-Type"), "text/plain");
}

function stop_normal(ch, cx, status, data)
{
  do_check_eq(data.length, 21);
  do_check_eq(data[0],  0x54);
  do_check_eq(data[20], 0x0a);
}

function init_byterange(ch)
{
  ch.setRequestHeader("Range", "bytes=10-", false);
}

function start_byterange(ch, cx)
{
  do_check_eq(ch.responseStatus, 206);
  do_check_eq(ch.getResponseHeader("Content-Length"), "11");
  do_check_eq(ch.getResponseHeader("Content-Type"), "text/plain");
  do_check_eq(ch.getResponseHeader("Content-Range"), "bytes 10-20/21");
}

function stop_byterange(ch, cx, status, data)
{
  do_check_eq(data.length, 11);
  do_check_eq(data[0], 0x64);
  do_check_eq(data[10], 0x0a);
}

function init_byterange2(ch)
{
  ch.setRequestHeader("Range", "bytes=21-", false);
}

function start_byterange2(ch, cx)
{
  do_check_eq(ch.responseStatus, 416);
}

function init_byterange3(ch)
{
  ch.setRequestHeader("Range", "bytes=10-15", false);
}

function start_byterange3(ch, cx)
{
  do_check_eq(ch.responseStatus, 206);
  do_check_eq(ch.getResponseHeader("Content-Length"), "6");
  do_check_eq(ch.getResponseHeader("Content-Type"), "text/plain");
  do_check_eq(ch.getResponseHeader("Content-Range"), "bytes 10-15/21");
}

function stop_byterange3(ch, cx, status, data)
{
  do_check_eq(data.length, 6);
  do_check_eq(data[0], 0x64);
  do_check_eq(data[1], 0x20);
  do_check_eq(data[2], 0x62);
  do_check_eq(data[3], 0x65);
  do_check_eq(data[4], 0x20);
  do_check_eq(data[5], 0x73);
}

function init_byterange4(ch)
{
  ch.setRequestHeader("Range", "xbytes=21-", false);
}

function start_byterange4(ch, cx)
{
  do_check_eq(ch.responseStatus, 400);
}

function init_byterange5(ch)
{
  ch.setRequestHeader("Range", "bytes=-5", false);
}

function start_byterange5(ch, cx)
{
  do_check_eq(ch.responseStatus, 206);
}

function stop_byterange5(ch, cx, status, data)
{
  do_check_eq(data.length, 5);
  do_check_eq(data[0], 0x65);
  do_check_eq(data[1], 0x65);
  do_check_eq(data[2], 0x6e);
  do_check_eq(data[3], 0x2e);
  do_check_eq(data[4], 0x0a);
}

function init_byterange6(ch)
{
  ch.setRequestHeader("Range", "bytes=15-12", false);
}

function start_byterange6(ch, cx)
{
  do_check_eq(ch.responseStatus, 200);
}

function stop_byterange6(ch, cx, status, data)
{
  do_check_eq(data.length, 21);
  do_check_eq(data[0],  0x54);
  do_check_eq(data[20], 0x0a);
}

function init_byterange7(ch)
{
  ch.setRequestHeader("Range", "bytes=0-5", false);
}

function start_byterange7(ch, cx)
{
  do_check_eq(ch.responseStatus, 206);
  do_check_eq(ch.getResponseHeader("Content-Length"), "6");
  do_check_eq(ch.getResponseHeader("Content-Type"), "text/plain");
  do_check_eq(ch.getResponseHeader("Content-Range"), "bytes 0-5/21");
}

function stop_byterange7(ch, cx, status, data)
{
  do_check_eq(data.length, 6);
  do_check_eq(data[0], 0x54);
  do_check_eq(data[1], 0x68);
  do_check_eq(data[2], 0x69);
  do_check_eq(data[3], 0x73);
  do_check_eq(data[4], 0x20);
  do_check_eq(data[5], 0x73);
}

function init_byterange8(ch)
{
  ch.setRequestHeader("Range", "bytes=20-21", false);
}

function start_byterange8(ch, cx)
{
  do_check_eq(ch.responseStatus, 206);
  do_check_eq(ch.getResponseHeader("Content-Range"), "bytes 20-20/21");
}

function stop_byterange8(ch, cx, status, data)
{
  do_check_eq(data.length, 1);
  do_check_eq(data[0], 0x0a);
}

function init_byterange9(ch)
{
  ch.setRequestHeader("Range", "bytes=020-021", false);
}

function start_byterange9(ch, cx)
{
  do_check_eq(ch.responseStatus, 206);
}

function stop_byterange9(ch, cx, status, data)
{
  do_check_eq(data.length, 1);
  do_check_eq(data[0], 0x0a);
}

function init_byterange10(ch)
{
  ch.setRequestHeader("Range", "bytes=-", false);
}

function start_byterange10(ch, cx)
{
  do_check_eq(ch.responseStatus, 400);
}

function init_byterange11(ch)
{
  ch.setRequestHeader("Range", "bytes=-500", false);
}

function start_byterange11(ch, cx)
{
  do_check_eq(ch.responseStatus, 206);
}

function stop_byterange11(ch, cx, status, data)
{
  do_check_eq(data.length, 21);
  do_check_eq(data[0],  0x54);
  do_check_eq(data[20], 0x0a);
}

function start_byterange12(ch, cx)
{
  do_check_eq(ch.responseStatus, 200);
  do_check_eq(ch.getResponseHeader("Content-Length"), "0");
}

function stop_byterange12(ch, cx, status, data)
{
  do_check_eq(data.length, 0);
}
