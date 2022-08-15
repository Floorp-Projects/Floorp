/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* global __LOCATION__ */
/* import-globals-from ../httpd.js */

var _HTTPD_JS_PATH = __LOCATION__.parent;
_HTTPD_JS_PATH.append("httpd.js");
load(_HTTPD_JS_PATH.path);

// if these tests fail, we'll want the debug output
var linDEBUG = true;

var { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
var { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

/**
 * Constructs a new nsHttpServer instance.  This function is intended to
 * encapsulate construction of a server so that at some point in the future it
 * is possible to run these tests (with at most slight modifications) against
 * the server when used as an XPCOM component (not as an inline script).
 */
function createServer() {
  return new nsHttpServer();
}

/**
 * Creates a new HTTP channel.
 *
 * @param url
 *   the URL of the channel to create
 */
function makeChannel(url) {
  return NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
}

/**
 * Make a binary input stream wrapper for the given stream.
 *
 * @param stream
 *   the nsIInputStream to wrap
 */
function makeBIS(stream) {
  return new BinaryInputStream(stream);
}

/**
 * Returns the contents of the file as a string.
 *
 * @param file : nsIFile
 *   the file whose contents are to be read
 * @returns string
 *   the contents of the file
 */
function fileContents(file) {
  const PR_RDONLY = 0x01;
  var fis = new FileInputStream(
    file,
    PR_RDONLY,
    0o444,
    Ci.nsIFileInputStream.CLOSE_ON_EOF
  );
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
function* LineIterator(data) {
  var index = 0;
  do {
    index = data.indexOf("\r\n");
    if (index >= 0) {
      yield data.substring(0, index);
    } else {
      yield data;
    }

    data = data.substring(index + 2);
  } while (index >= 0);
}

/**
 * Throws if iter does not contain exactly the CRLF-separated lines in the
 * array expectedLines.
 *
 * @param iter : Iterator
 *   an Iterator which returns lines of text
 * @param expectedLines : [string]
 *   an array of the expected lines of text
 * @throws an error message if iter doesn't agree with expectedLines
 */
function expectLines(iter, expectedLines) {
  var index = 0;
  for (var line of iter) {
    if (expectedLines.length == index) {
      throw new Error(
        `Error: got more than ${expectedLines.length} expected lines!`
      );
    }

    var expected = expectedLines[index++];
    if (expected !== line) {
      throw new Error(`Error on line ${index}!
  actual: '${line}',
  expect: '${expected}'`);
    }
  }

  if (expectedLines.length !== index) {
    throw new Error(
      `Expected more lines!  Got ${index}, expected ${expectedLines.length}`
    );
  }
}

/**
 * Spew a bunch of HTTP metadata from request into the body of response.
 *
 * @param request : nsIHttpRequest
 *   the request whose metadata should be output
 * @param response : nsIHttpResponse
 *   the response to which the metadata is written
 */
function writeDetails(request, response) {
  response.write("Method:  " + request.method + "\r\n");
  response.write("Path:    " + request.path + "\r\n");
  response.write("Query:   " + request.queryString + "\r\n");
  response.write("Version: " + request.httpVersion + "\r\n");
  response.write("Scheme:  " + request.scheme + "\r\n");
  response.write("Host:    " + request.host + "\r\n");
  response.write("Port:    " + request.port);
}

/**
 * Advances iter past all non-blank lines and a single blank line, after which
 * point the body of the response will be returned next from the iterator.
 *
 * @param iter : Iterator
 *   an iterator over the CRLF-delimited lines in an HTTP response, currently
 *   just after the Request-Line
 */
function skipHeaders(iter) {
  var line = iter.next().value;
  while (line !== "") {
    line = iter.next().value;
  }
}

/**
 * Checks that the exception e (which may be an XPConnect-created exception
 * object or a raw nsresult number) is the given nsresult.
 *
 * @param e : Exception or nsresult
 *   the actual exception
 * @param code : nsresult
 *   the expected exception
 */
function isException(e, code) {
  if (e !== code && e.result !== code) {
    do_throw("unexpected error: " + e);
  }
}

/**
 * Calls the given function at least the specified number of milliseconds later.
 * The callback will not undershoot the given time, but it might overshoot --
 * don't expect precision!
 *
 * @param milliseconds : uint
 *   the number of milliseconds to delay
 * @param callback : function() : void
 *   the function to call
 */
function callLater(msecs, callback) {
  do_timeout(msecs, callback);
}

/** *****************************************************
 * SIMPLE SUPPORT FOR LOADING/TESTING A SERIES OF URLS *
 *******************************************************/

/**
 * Create a completion callback which will stop the given server and end the
 * test, assuming nothing else remains to be done at that point.
 */
function testComplete(srv) {
  return function complete() {
    do_test_pending();
    srv.stop(function quit() {
      do_test_finished();
    });
  };
}

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
function Test(path, initChannel, onStartRequest, onStopRequest) {
  function nil() {}

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
function runHttpTests(testArray, done) {
  /** Kicks off running the next test in the array. */
  function performNextTest() {
    if (++testIndex == testArray.length) {
      try {
        done();
      } catch (e) {
        do_report_unexpected_exception(e, "running test-completion callback");
      }
      return;
    }

    do_test_pending();

    var test = testArray[testIndex];
    var ch = makeChannel(test.path);
    try {
      test.initChannel(ch);
    } catch (e) {
      try {
        do_report_unexpected_exception(
          e,
          "testArray[" + testIndex + "].initChannel(ch)"
        );
      } catch (x) {
        /* swallow and let tests continue */
      }
    }

    listener._channel = ch;
    ch.asyncOpen(listener);
  }

  /** Index of the test being run. */
  var testIndex = -1;

  /** Stream listener for the channels. */
  var listener = {
    /** Current channel being observed by this. */
    _channel: null,
    /** Array of bytes of data in body of response. */
    _data: [],

    onStartRequest(request) {
      Assert.ok(request === this._channel);
      var ch = request
        .QueryInterface(Ci.nsIHttpChannel)
        .QueryInterface(Ci.nsIHttpChannelInternal);

      this._data.length = 0;
      try {
        try {
          testArray[testIndex].onStartRequest(ch);
        } catch (e) {
          do_report_unexpected_exception(
            e,
            "testArray[" + testIndex + "].onStartRequest"
          );
        }
      } catch (e) {
        do_note_exception(
          e,
          "!!! swallowing onStartRequest exception so onStopRequest is " +
            "called..."
        );
      }
    },
    onDataAvailable(request, inputStream, offset, count) {
      var quantum = 262144; // just above half the argument-count limit
      var bis = makeBIS(inputStream);
      for (var start = 0; start < count; start += quantum) {
        var newData = bis.readByteArray(Math.min(quantum, count - start));
        Array.prototype.push.apply(this._data, newData);
      }
    },
    onStopRequest(request, status) {
      this._channel = null;

      var ch = request
        .QueryInterface(Ci.nsIHttpChannel)
        .QueryInterface(Ci.nsIHttpChannelInternal);

      // NB: The onStopRequest callback must run before performNextTest here,
      //     because the latter runs the next test's initChannel callback, and
      //     we want one test to be sequentially processed before the next
      //     one.
      try {
        testArray[testIndex].onStopRequest(ch, status, this._data);
      } catch (e) {
        do_report_unexpected_exception(
          e,
          "testArray[" + testIndex + "].onStartRequest"
        );
      } finally {
        try {
          performNextTest();
        } finally {
          do_test_finished();
        }
      }
    },
    QueryInterface: ChromeUtils.generateQI([
      "nsIStreamListener",
      "nsIRequestObserver",
    ]),
  };

  performNextTest();
}

/** **************************************
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
 * @param data : string or [string...]
 *   either:
 *     - the raw data to send, as a string of characters with codes in the
 *       range 0-255, or
 *     - an array of such strings whose concatenation forms the raw data
 * @param responseCheck : function(string) : void
 *   a function which is provided with the data sent by the remote host which
 *   conducts whatever tests it wants on that data; useful for tweaking the test
 *   environment between tests
 */
function RawTest(host, port, data, responseCheck) {
  if (0 > port || 65535 < port || port % 1 !== 0) {
    throw new Error("bad port");
  }
  if (!(data instanceof Array)) {
    data = [data];
  }
  if (data.length <= 0) {
    throw new Error("bad data length");
  }

  if (
    !data.every(function(v) {
      // eslint-disable-next-line no-control-regex
      return /^[\x00-\xff]*$/.test(v);
    })
  ) {
    throw new Error("bad data contained non-byte-valued character");
  }

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
 * @param beforeTestCallback
 *   function to call before each test is run. Gets passed testIndex when called
 */
function runRawTests(testArray, done, beforeTestCallback) {
  do_test_pending();

  var sts = Cc["@mozilla.org/network/socket-transport-service;1"].getService(
    Ci.nsISocketTransportService
  );

  var currentThread = Cc["@mozilla.org/thread-manager;1"].getService()
    .currentThread;

  /** Kicks off running the next test in the array. */
  function performNextTest() {
    if (++testIndex == testArray.length) {
      do_test_finished();
      try {
        done();
      } catch (e) {
        do_report_unexpected_exception(e, "running test-completion callback");
      }
      return;
    }

    if (beforeTestCallback) {
      try {
        beforeTestCallback(testIndex);
      } catch (e) {
        /* We don't care if this call fails */
      }
    }

    var rawTest = testArray[testIndex];

    var transport = sts.createTransport(
      [],
      rawTest.host,
      rawTest.port,
      null,
      null
    );

    var inStream = transport.openInputStream(0, 0, 0);
    var outStream = transport.openOutputStream(0, 0, 0);

    // reset
    dataIndex = 0;
    received = "";

    waitForMoreInput(inStream);
    waitToWriteOutput(outStream);
  }

  function waitForMoreInput(stream) {
    reader.stream = stream;
    stream = stream.QueryInterface(Ci.nsIAsyncInputStream);
    stream.asyncWait(reader, 0, 0, currentThread);
  }

  function waitToWriteOutput(stream) {
    // Do the QueryInterface here, not earlier, because there is no
    // guarantee that 'stream' passed in here been QIed to nsIAsyncOutputStream
    // since the last GC.
    stream = stream.QueryInterface(Ci.nsIAsyncOutputStream);
    stream.asyncWait(
      writer,
      0,
      testArray[testIndex].data[dataIndex].length,
      currentThread
    );
  }

  /** Index of the test being run. */
  var testIndex = -1;

  /**
   * Index of remaining data strings to be written to the socket in current
   * test.
   */
  var dataIndex = 0;

  /** Data received so far from the server. */
  var received = "";

  /** Reads data from the socket. */
  var reader = {
    onInputStreamReady(stream) {
      Assert.ok(stream === this.stream);
      try {
        var bis = new BinaryInputStream(stream);

        var av = 0;
        try {
          av = bis.available();
        } catch (e) {
          /* default to 0 */
          do_note_exception(e);
        }

        if (av > 0) {
          var quantum = 262144;
          for (var start = 0; start < av; start += quantum) {
            var bytes = bis.readByteArray(Math.min(quantum, av - start));
            received += String.fromCharCode.apply(null, bytes);
          }
          waitForMoreInput(stream);
          return;
        }
      } catch (e) {
        do_report_unexpected_exception(e);
      }

      var rawTest = testArray[testIndex];
      try {
        rawTest.responseCheck(received);
      } catch (e) {
        do_report_unexpected_exception(e);
      } finally {
        try {
          stream.close();
          performNextTest();
        } catch (e) {
          do_report_unexpected_exception(e);
        }
      }
    },
  };

  /** Writes data to the socket. */
  var writer = {
    onOutputStreamReady(stream) {
      var str = testArray[testIndex].data[dataIndex];

      var written = 0;
      try {
        written = stream.write(str, str.length);
        if (written == str.length) {
          dataIndex++;
        } else {
          testArray[testIndex].data[dataIndex] = str.substring(written);
        }
      } catch (e) {
        do_note_exception(e);
        /* stream could have been closed, just ignore */
      }

      try {
        // Keep writing data while we can write and
        // until there's no more data to read
        if (written > 0 && dataIndex < testArray[testIndex].data.length) {
          waitToWriteOutput(stream);
        } else {
          stream.close();
        }
      } catch (e) {
        do_report_unexpected_exception(e);
      }
    },
  };

  performNextTest();
}
