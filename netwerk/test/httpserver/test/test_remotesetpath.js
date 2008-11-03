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

// Make sure setIndexHandler works as expected


var paths =
  [
   "http://localhost:4444/my/first-path/override/",
   "http://localhost:4444/my/second-path/override",
   "http://localhost:4444/my/second-path/override",
   "http://localhost:4444/my/second-path/override"
  ];
  
var methods =
  [
   "PUT",
   "PUT",
   "DELETE",
   "DELETE"
  ];
  
var contents =
  [
   "First content",
   "Second content",
   // The third is missing - it is being deleted from the server
  ];

var putResponses =
  [
   200,
   200,
   200,
   204
  ];
  
var success =
  [
   true, 
   true, 
   false,
   false
  ];

var currPathIndex = 0;

var uploadListener =
  {
    // NSISTREAMLISTENER
    onDataAvailable: function(request, cx, inputStream, offset, count)
    {
      makeBIS(inputStream).readByteArray(count); // required by API
    },

    // NSIREQUESTOBSERVER
    onStartRequest: function(request, cx)
    {
      var ch = request.QueryInterface(Ci.nsIHttpChannel);
      do_check_eq(ch.responseStatus, putResponses[currPathIndex]);
    },
    onStopRequest: function(request, cx, status)
    {
      do_check_true(Components.isSuccessCode(status));
      var ch = makeChannel(paths[currPathIndex]);
      ch.asyncOpen(downloadListener, null);
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

var downloadListener =
  {
    // NSISTREAMLISTENER
    onDataAvailable: function(request, cx, inputStream, offset, count)
    {
      var content = makeBIS(inputStream).readByteArray(count); // required by API

      if (success[currPathIndex])
      {
        for (var i=0; i<count; ++i)
          do_check_eq(contents[currPathIndex].charCodeAt(i), content[i])
      }
    },

    // NSIREQUESTOBSERVER
    onStartRequest: function(request, cx)
    {
      if (success[currPathIndex])
      {
        // expected success, check we get the correct content-type
        var ch = request.QueryInterface(Ci.nsIHttpChannel)
                        .QueryInterface(Ci.nsIHttpChannelInternal);
        do_check_eq(ch.getResponseHeader("Content-Type"), "application/x-moz-put-test");
      }
    },
    onStopRequest: function(request, cx, status)
    {
      do_check_true(Components.isSuccessCode(status));

      var ch = request.QueryInterface(Ci.nsIHttpChannel)
                      .QueryInterface(Ci.nsIHttpChannelInternal);
      do_check_true(ch.requestSucceeded == success[currPathIndex]);

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

  var method = methods[currPathIndex];
  switch (method)
  {
  case "PUT":
    var stream = Cc["@mozilla.org/io/string-input-stream;1"]
        .createInstance(Ci.nsIStringInputStream);
    stream.setData(contents[currPathIndex], contents[currPathIndex].length);

    var upch = ch.QueryInterface(Ci.nsIUploadChannel);
    upch.setUploadStream(stream, "application/x-moz-put-test", stream.available());
    break;
    
  case "DELETE":
    var httpch = ch.QueryInterface(Ci.nsIHttpChannel);
    httpch.requestMethod = method;
    break;
  }

  ch.asyncOpen(uploadListener, null);
}

var srv, serverBasePath;

function run_test()
{
  srv = createServer();
  srv.start(4444);

  performNextTest();
}
