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

// tests the registerDirectory API

const BASE = "http://localhost:4444";

var paths =
  [
/* 0*/ BASE + "/test_registerdirectory.js", // without a base path
/* 1*/ BASE + "/test_registerdirectory.js", // with a base path
/* 2*/ BASE + "/test_registerdirectory.js", // without a base path

/* 3*/ BASE + "/test_registerdirectory.js", // no registered path handler
/* 4*/ BASE + "/test_registerdirectory.js", // registered path handler
/* 5*/ BASE + "/test_registerdirectory.js", // removed path handler

/* 6*/ BASE + "/test_registerdirectory.js", // with a base path
/* 7*/ BASE + "/test_registerdirectory.js", // ...and a path handler
/* 8*/ BASE + "/test_registerdirectory.js", // removed base handler
/* 9*/ BASE + "/test_registerdirectory.js",  // removed path handler

/*10*/ BASE + "/foo/test_registerdirectory.js", // mapping set up, works
/*11*/ BASE + "/foo/test_registerdirectory.js/test_registerdirectory.js", // no mapping, fails
/*12*/ BASE + "/foo/test_registerdirectory.js/test_registerdirectory.js", // mapping, works
/*13*/ BASE + "/foo/test_registerdirectory.js", // two mappings set up, still works
/*14*/ BASE + "/foo/test_registerdirectory.js", // mapping was removed
/*15*/ BASE + "/foo/test_registerdirectory.js/test_registerdirectory.js", // mapping still present, works
/*16*/ BASE + "/foo/test_registerdirectory.js/test_registerdirectory.js", // mapping removed
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
      var ch = request.QueryInterface(Ci.nsIHttpChannel);

      switch (currPathIndex)
      {
        case 0:
          do_check_eq(ch.responseStatus, 404);
          do_check_false(ch.requestSucceeded);
          break;

        case 1:
          do_check_eq(ch.responseStatus, 200);
          do_check_true(ch.requestSucceeded);

          var actualFile = serverBasePath.clone();
          actualFile.append("test_registerdirectory.js");
          do_check_eq(ch.getResponseHeader("Content-Length"),
                      actualFile.fileSize.toString());
          break;

        case 2:
          do_check_eq(ch.responseStatus, 404);
          do_check_false(ch.requestSucceeded);
          break;

        case 3:
          do_check_eq(ch.responseStatus, 404);
          do_check_false(ch.requestSucceeded);
          break;

        case 4:
          do_check_eq(ch.responseStatus, 200);
          do_check_eq(ch.responseStatusText, "OK");
          do_check_true(ch.requestSucceeded);
          do_check_eq(ch.getResponseHeader("Override-Succeeded"), "yes");
          break;

        case 5:
          do_check_eq(ch.responseStatus, 404);
          do_check_false(ch.requestSucceeded);
          break;

        case 6:
          do_check_eq(ch.responseStatus, 200);
          do_check_true(ch.requestSucceeded);

          var actualFile = serverBasePath.clone();
          actualFile.append("test_registerdirectory.js");
          do_check_eq(ch.getResponseHeader("Content-Length"),
                      actualFile.fileSize.toString());
          break;

        case 7:
        case 8:
          do_check_eq(ch.responseStatus, 200);
          do_check_eq(ch.responseStatusText, "OK");
          do_check_true(ch.requestSucceeded);
          do_check_eq(ch.getResponseHeader("Override-Succeeded"), "yes");
          break;

        case 9:
          do_check_eq(ch.responseStatus, 404);
          do_check_false(ch.requestSucceeded);
          break;

        case 10:
          do_check_eq(ch.responseStatus, 200);
          do_check_eq(ch.responseStatusText, "OK");
          break;

        case 11:
          do_check_eq(ch.responseStatus, 404);
          do_check_false(ch.requestSucceeded);
          break;

        case 12:
        case 13:
          do_check_eq(ch.responseStatus, 200);
          do_check_eq(ch.responseStatusText, "OK");
          break;

        case 14:
          do_check_eq(ch.responseStatus, 404);
          do_check_false(ch.requestSucceeded);
          break;

        case 15:
          do_check_eq(ch.responseStatus, 200);
          do_check_eq(ch.responseStatusText, "OK");
          break;

        case 16:
          do_check_eq(ch.responseStatus, 404);
          do_check_false(ch.requestSucceeded);
          break;
      }
    },
    onStopRequest: function(request, cx, status)
    {
      switch (currPathIndex)
      {
        case 0:
          // now set a base path
          serverBasePath = testsDirectory.clone();
          srv.registerDirectory("/", serverBasePath);
          break;

        case 1:
          // remove base path
          serverBasePath = null;
          srv.registerDirectory("/", serverBasePath);
          break;

        case 3:
          // register overriding path
          srv.registerPathHandler("/test_registerdirectory.js",
                                  override_test_registerdirectory);
          break;

        case 4:
          // unregister overriding path
          srv.registerPathHandler("/test_registerdirectory.js", null);
          break;

        case 5:
          // set the base path again
          serverBasePath = testsDirectory.clone();
          srv.registerDirectory("/", serverBasePath);
          break;

        case 6:
          // register overriding path
          srv.registerPathHandler("/test_registerdirectory.js",
                                  override_test_registerdirectory);
          break;

        case 7:
          // remove base path
          serverBasePath = null;
          srv.registerDirectory("/", serverBasePath);
          break;

        case 8:
          // unregister overriding path
          srv.registerPathHandler("/test_registerdirectory.js", null);
          break;

        case 9:
          // register /foo/ as a base path
          serverBasePath = testsDirectory.clone();
          srv.registerDirectory("/foo/", serverBasePath);
          break;

        case 10:
          // do nothing
          break;

        case 11:
          // now register an overriding path to handle the URL that just failed
          srv.registerDirectory("/foo/test_registerdirectory.js/", serverBasePath);
          break;

        case 12:
          // do nothing
          break;

        case 13:
          srv.registerDirectory("/foo/", null);
          break;

        case 14:
          // do nothing
          break;

        case 15:
          srv.registerDirectory("/foo/test_registerdirectory.js/", null);
          break;
      }

      if (!paths[++currPathIndex])
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
  ch.loadFlags |= Ci.nsIRequest.LOAD_BYPASS_CACHE; // important!
  ch.asyncOpen(listener, null);
}

var srv;
var serverBasePath;
var testsDirectory;

function run_test()
{
  testsDirectory = do_get_topsrcdir();
  testsDirectory.append("netwerk");
  testsDirectory.append("test");
  testsDirectory.append("httpserver");
  testsDirectory.append("test");

  srv = createServer();
  srv.start(4444);

  performNextTest();
}

// PATH HANDLERS

// override of /test_registerdirectory.js
function override_test_registerdirectory(metadata, response)
{
  response.setStatusLine("1.1", 200, "OK");
  response.setHeader("Override-Succeeded", "yes");

  var body = "success!";
  response.bodyOutputStream.write(body, body.length);
}
