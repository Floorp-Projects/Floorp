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

do_import_script("netwerk/test/httpserver/httpd.js");

// if these tests fail, we'll want the debug output
DEBUG = true;


/**
 * Constructs a new nsHttpServer instance.  This function is intended to
 * encapsulate construction of a server so that at some point in the future
 * it is possible to run these tests (with at most slight modifications) against
 * the server when used as an XPCOM component (not as an inline script) with
 * only slight modifications.
 */
function createServer()
{
  return new nsHttpServer();
}

/**
 * Creates a new HTTP channel.
 *
 * @param url
 *   the URL of the channel to create
 */
function makeChannel(url)
{
  var ios = Cc["@mozilla.org/network/io-service;1"]
              .getService(Ci.nsIIOService);
  var chan = ios.newChannel(url, null, null)
                .QueryInterface(Ci.nsIHttpChannel);

  return chan;
}

/**
 * Make a binary input stream wrapper for the given stream.
 *
 * @param stream
 *   the nsIInputStream to wrap
 */
function makeBIS(stream)
{
  return new BinaryInputStream(stream);
}


/*******************************************************
 * SIMPLE SUPPORT FOR LOADING/TESTING A SERIES OF URLS *
 *******************************************************/

/**
 * Represents a path to load from the tested HTTP server, along with actions to
 * take before, during, and after loading the associated page.
 *
 * @param path
 *   the URL to load from the server
 * @param initChannel
 *   a function which takes as a single parameter a channel created for path and
 *   initializes its state, or null if no additional initialization is needed
 * @param onStartRequest
 *   called during onStartRequest for the load of the URL, with the same
 *   parameters; the request parameter has been QI'd to nsIHttpChannel and
 *   nsIHttpChannelInternal for convenience; may be null if nothing needs to be
 *   done
 * @param onStopRequest
 *   called during onStopRequest for the channel, with the same parameters plus
 *   a trailing parameter containing an array of the bytes of data downloaded in
 *   the body of the channel response; the request parameter has been QI'd to
 *   nsIHttpChannel and nsIHttpChannelInternal for convenience; may be null if
 *   nothing needs to be done
 */
function Test(path, initChannel, onStartRequest, onStopRequest)
{
  function nil() { }

  this.path = path;
  this.initChannel = initChannel || nil;
  this.onStartRequest = onStartRequest || nil;
  this.onStopRequest = onStopRequest || nil;
}

/**
 * Runs all the tests in testArray.
 *
 * @param testArray
 *   a non-empty array of Tests to run, in order
 * @param done
 *   function to call when all tests have run (e.g. to shut down the server)
 */
function runHttpTests(testArray, done)
{
  /** Kicks off running the next test in the array. */
  function performNextTest()
  {
    if (++testIndex == testArray.length)
    {
      done();
      return;
    }

    do_test_pending();

    var test = testArray[testIndex];
    var ch = makeChannel(test.path);
    test.initChannel(ch);

    ch.asyncOpen(listener, null);
  }

  /** Index of the test being run. */
  var testIndex = -1;

  /** Stream listener for the channels. */
  var listener =
    {
      /** Array of bytes of data in body of response. */
      _data: [],

      onStartRequest: function(request, cx)
      {
        var ch = request.QueryInterface(Ci.nsIHttpChannel)
                        .QueryInterface(Ci.nsIHttpChannelInternal);

        this._data.length = 0;
        testArray[testIndex].onStartRequest(ch, cx);
      },
      onDataAvailable: function(request, cx, inputStream, offset, count)
      {
        Array.prototype.push.apply(this._data,
                                   makeBIS(inputStream).readByteArray(count));
      },
      onStopRequest: function(request, cx, status)
      {
        var ch = request.QueryInterface(Ci.nsIHttpChannel)
                        .QueryInterface(Ci.nsIHttpChannelInternal);
      
        testArray[testIndex].onStopRequest(ch, cx, status, this._data);

        performNextTest();
        do_test_finished();
      },
      QueryInterface: function(aIID)
      {
        if (aIID.equals(Ci.nsIStreamListener) ||
            aIID.equals(Ci.nsIRequestObserver) ||
            aIID.equals(Ci.nsISupports))
          return this;
        throw Cr.NS_ERROR_NO_INTERFACE;
      }
    };

  performNextTest();
}

