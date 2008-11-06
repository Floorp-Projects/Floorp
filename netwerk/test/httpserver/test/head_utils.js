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

do_import_script("netwerk/test/httpserver/httpd.js");

// if these tests fail, we'll want the debug output
DEBUG = true;


/**
 * Constructs a new nsHttpServer instance.  This function is intended to
 * encapsulate construction of a server so that at some point in the future it
 * is possible to run these tests (with at most slight modifications) against
 * the server when used as an XPCOM component (not as an inline script).
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


/**
 * Returns the contents of the file as a string.
 *
 * @param file : nsILocalFile
 *   the file whose contents are to be read
 * @returns string
 *   the contents of the file
 */
function fileContents(file)
{
  const PR_RDONLY = 0x01;
  var fis = new FileInputStream(file, PR_RDONLY, 0444,
                                Ci.nsIFileInputStream.CLOSE_ON_EOF);
  var sis = new ScriptableInputStream(fis);
  var contents = sis.read(file.fileSize);
  sis.close();
  return contents;
}

/**
 * Iterates over the lines, delimited by CRLF, in data, returning each line
 * without the trailing line separator.
 *
 * @param data : string
 *   a string consisting of lines of data separated by CRLFs
 * @returns Iterator
 *   an Iterator which returns each line from data in turn; note that this
 *   includes a final empty line if data ended with a CRLF
 */
function LineIterator(data)
{
  var start = 0, index = 0;
  do
  {
    index = data.indexOf("\r\n");
    if (index >= 0)
      yield data.substring(0, index);
    else
      yield data;

    data = data.substring(index + 2);
  }
  while (index >= 0);
}

/**
 * Throws if iter does not contain exactly the CRLF-separated lines in the
 * array expectedLines.
 *
 * @param iter : Iterator
 *   an Iterator which returns lines of text
 * @param expectedLines : [string]
 *   an array of the expected lines of text
 * @throws string
 *   an error message if iter doesn't agree with expectedLines
 */
function expectLines(iter, expectedLines)
{
  var index = 0;
  for (var line in iter)
  {
    if (expectedLines.length == index)
      throw "Error: got more than " + expectedLines.length + " expected lines!";

    var expected = expectedLines[index++];
    if (expected !== line)
      throw "Error on line " + index + "!\n" +
            "  actual: '" + line + "',\n" +
            "  expect: '" + expected + "'";
  }

  if (expectedLines.length !== index)
  {
    throw "Expected more lines!  Got " + index +
          ", expected " + expectedLines.length;
  }
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

        // NB: The onStopRequest callback must run before performNextTest here,
        //     because the latter runs the next test's initChannel callback, and
        //     we want one test to be sequentially processed before the next
        //     one.
        try
        {
          testArray[testIndex].onStopRequest(ch, cx, status, this._data);
        }
        finally
        {
          try
          {
            performNextTest();
          }
          finally
          {
            do_test_finished();
          }
        }
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


/****************************************
 * RAW REQUEST FORMAT TESTING FUNCTIONS *
 ****************************************/

/**
 * Sends a raw string of bytes to the given host and port and checks that the
 * response is acceptable.
 *
 * @param host : string
 *   the host to which a connection should be made
 * @param port : PRUint16
 *   the port to use for the connection
 * @param data : string
 *   the raw data to send, as a string of characters with codes in the range
 *   0-255
 * @param responseCheck : function(string) : void
 *   a function which is provided with the data sent by the remote host which
 *   conducts whatever tests it wants on that data; useful for tweaking the test
 *   environment between tests
 */
function RawTest(host, port, data, responseCheck)
{
  if (0 > port || 65535 < port || port % 1 !== 0)
    throw "bad port";
  if (!/^[\x00-\xff]*$/.test(data))
    throw "bad data contains non-byte-valued character";

  this.host = host;
  this.port = port;
  this.data = data;
  this.responseCheck = responseCheck;
}

/**
 * Runs all the tests in testArray, an array of RawTests.
 *
 * @param testArray : [RawTest]
 *   an array of RawTests to run, in order
 * @param done
 *   function to call when all tests have run (e.g. to shut down the server)
 */
function runRawTests(testArray, done)
{
  do_test_pending();

  var sts = Cc["@mozilla.org/network/socket-transport-service;1"]
              .getService(Ci.nsISocketTransportService);

  var currentThread = Cc["@mozilla.org/thread-manager;1"]
                        .getService()
                        .currentThread;
  
  /** Kicks off running the next test in the array. */
  function performNextTest()
  {
    if (++testIndex == testArray.length)
    {
      do_test_finished();
      done();
      return;
    }


    var rawTest = testArray[testIndex];

    var transport =
      sts.createTransport(null, 0, rawTest.host, rawTest.port, null);

    var inStream = transport.openInputStream(0, 0, 0)
                            .QueryInterface(Ci.nsIAsyncInputStream);
    var outStream  = transport.openOutputStream(0, 0, 0)
                              .QueryInterface(Ci.nsIAsyncOutputStream);

    // reset
    dataIndex = 0;
    received = "";

    waitForMoreInput(inStream);
    waitToWriteOutput(outStream);
  }

  function waitForMoreInput(stream)
  {
    stream.asyncWait(reader, 0, 0, currentThread);
  }

  function waitToWriteOutput(stream)
  {
    stream.asyncWait(writer, 0, testArray[testIndex].data.length - dataIndex,
                     currentThread);
  }

  /** Index of the test being run. */
  var testIndex = -1;

  /** Index of remaining data to be written to the socket in current test. */
  var dataIndex = 0;

  /** Data received so far from the server. */
  var received = "";

  /** Reads data from the socket. */
  var reader =
    {
      onInputStreamReady: function(stream)
      {
        var bis = new BinaryInputStream(stream);

        var av = 0;
        try
        {
          av = bis.available();
        }
        catch (e) { /* default to 0 */ }

        if (av > 0)
        {
          received += String.fromCharCode.apply(null, bis.readByteArray(av));
          waitForMoreInput(stream);
          return;
        }

        var rawTest = testArray[testIndex];
        try
        {
          rawTest.responseCheck(received);
        }
        catch (e)
        {
          do_throw("error thrown by responseCheck: " + e);
        }
        finally
        {
          stream.close();
          performNextTest();
        }
      }
    };

  /** Writes data to the socket. */
  var writer = 
    {
      onOutputStreamReady: function(stream)
      {
        var data = testArray[testIndex].data.substring(dataIndex);

        var written = 0;
        try
        {
          written = stream.write(data, data.length);
          dataIndex += written;
        }
        catch (e) { /* stream could have been closed, just ignore */ }

        // Keep reading data until there's no more data to read
        if (written != 0)
          waitToWriteOutput(stream);
        else
          stream.close();
      }
    };

  performNextTest();
}
