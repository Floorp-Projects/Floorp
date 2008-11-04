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

// basic functionality test, from the client programmer's POV


var paths =
  [
   "http://localhost:4444/objHandler",
   "http://localhost:4444/functionHandler",
   "http://localhost:4444/non-existent-path"  // intended to produce 404
  ];
var currPathIndex = 0;

var listener =
  {
    // NSISTREAMLISTENER
    onDataAvailable: function(request, cx, inputStream, offset, count)
    {
      makeBIS(inputStream).readByteArray(count); // required by API
    },
    // NSIREQUESTOBSERVER
    onStartRequest: function(request, cx)
    {
      var ch = request.QueryInterface(Ci.nsIHttpChannel)
                      .QueryInterface(Ci.nsIHttpChannelInternal);

      // common properties *always* appended by server or invariants for every
      // URL in paths
      do_check_true(ch.contentLength > -1);
      do_check_eq(ch.getResponseHeader("connection"), "close"); 
      do_check_false(ch.isNoStoreResponse());

      var reqMin = {}, reqMaj = {}, respMin = {}, respMaj = {};
      switch (currPathIndex)
      {
        case 0:
          do_check_eq(ch.responseStatus, 200);
          do_check_true(ch.requestSucceeded);
          do_check_eq(ch.getResponseHeader("content-type"), "text/plain");
          do_check_eq(ch.responseStatusText, "OK");

          ch.getRequestVersion(reqMaj, reqMin);
          ch.getResponseVersion(respMaj, respMin);
          do_check_true(reqMaj.value == respMaj.value &&
                        reqMin.value == respMin.value);
          break;

        case 1:
          do_check_eq(ch.responseStatus, 404);
          do_check_false(ch.requestSucceeded);
          do_check_eq(ch.getResponseHeader("foopy"), "quux-baz");
          do_check_eq(ch.responseStatusText, "Page Not Found");

          ch.getResponseVersion(respMaj, respMin);
          do_check_true(respMaj.value == 1 && respMin.value == 1);
          break;

        case 2:
          do_check_eq(ch.responseStatus, 404);
          do_check_false(ch.requestSucceeded);
          break;
      }
    },
    onStopRequest: function(request, cx, status)
    {
      if (++currPathIndex == paths.length)
        srv.stop();
      else
        performNextTest();
      do_test_finished();
    },
    // NSISUPPORTS
    QueryInterface: function(aIID)
    {
      if (aIID.equals(Ci.nsIStreamListener) ||
          aIID.equals(Ci.nsIRequestObserver) ||
          aIID.equals(Ci.nsISupports))
        return this;
      throw Cr.NS_ERROR_NO_INTERFACE;
    }
  };


function performNextTest()
{
  do_test_pending();

  var ch = makeChannel(paths[currPathIndex]);
  ch.asyncOpen(listener, null);
}

var srv;

function run_test()
{
  srv = createServer();

  // base path
  // XXX should actually test this works with a file by comparing streams!
  var dirServ = Cc["@mozilla.org/file/directory_service;1"]
                  .getService(Ci.nsIProperties);
  var path = dirServ.get("CurProcD", Ci.nsILocalFile);
  srv.registerDirectory("/", path);

  // register a few test paths
  srv.registerPathHandler("/objHandler", objHandler);
  srv.registerPathHandler("/functionHandler", functionHandler);

  srv.start(4444);

  performNextTest();
}

// PATH HANDLERS

// /objHandler
var objHandler =
  {
    handle: function(metadata, response)
    {
      response.setStatusLine(metadata.httpVersion, 200, "OK");
      response.setHeader("Content-Type", "text/plain", false);

      var body = "Request (slightly reformatted):\n\n";
      body += metadata.method + " " + metadata.path;

      do_check_eq(metadata.port, 4444);

      if (metadata.queryString)
        body +=  "?" + metadata.queryString;

      body += " HTTP/" + metadata.httpVersion + "\n";
        
      var headEnum = metadata.headers;
      while (headEnum.hasMoreElements())
      {
        var fieldName = headEnum.getNext()
                                .QueryInterface(Ci.nsISupportsString)
                                .data;
        body += fieldName + ": " + metadata.getHeader(fieldName) + "\n";
      }

      response.bodyOutputStream.write(body, body.length);
    },
    QueryInterface: function(id)
    {
      if (id.equals(Ci.nsISupports) || id.equals(Ci.nsIHttpRequestHandler))
        return this;
      throw Cr.NS_ERROR_NOINTERFACE;
    }
  };

// /functionHandler
function functionHandler(metadata, response)
{
  response.setStatusLine("1.1", 404, "Page Not Found");
  response.setHeader("foopy", "quux-baz", false);

  do_check_eq(metadata.port, 4444);
  do_check_eq(metadata.host, "localhost");
  do_check_eq(metadata.path.charAt(0), "/");

  var body = "this is text\n";
  response.bodyOutputStream.write(body, body.length);
}
