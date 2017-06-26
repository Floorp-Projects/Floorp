/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 sts=2 et
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file tests the methods on NetUtil.jsm.
 */

Cu.import("resource://testing-common/httpd.js");

Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/Services.jsm");

// We need the profile directory so the test harness will clean up our test
// files.
do_get_profile();

const OUTPUT_STREAM_CONTRACT_ID = "@mozilla.org/network/file-output-stream;1";
const SAFE_OUTPUT_STREAM_CONTRACT_ID = "@mozilla.org/network/safe-file-output-stream;1";

////////////////////////////////////////////////////////////////////////////////
//// Helper Methods

/**
 * Reads the contents of a file and returns it as a string.
 *
 * @param aFile
 *        The file to return from.
 * @return the contents of the file in the form of a string.
 */
function getFileContents(aFile)
{
  "use strict";

  let fstream = Cc["@mozilla.org/network/file-input-stream;1"].
                createInstance(Ci.nsIFileInputStream);
  fstream.init(aFile, -1, 0, 0);

  let cstream = Cc["@mozilla.org/intl/converter-input-stream;1"].
                createInstance(Ci.nsIConverterInputStream);
  cstream.init(fstream, "UTF-8", 0, 0);

  let string  = {};
  cstream.readString(-1, string);
  cstream.close();
  return string.value;
}


/**
 * Tests asynchronously writing a file using NetUtil.asyncCopy.
 *
 * @param aContractId
 *        The contract ID to use for the output stream
 * @param aDeferOpen
 *        Whether to use DEFER_OPEN in the output stream.
 */
function async_write_file(aContractId, aDeferOpen)
{
  do_test_pending();

  // First, we need an output file to write to.
  let file = Cc["@mozilla.org/file/directory_service;1"].
             getService(Ci.nsIProperties).
             get("ProfD", Ci.nsIFile);
  file.append("NetUtil-async-test-file.tmp");
  file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);

  // Then, we need an output stream to our output file.
  let ostream = Cc[aContractId].createInstance(Ci.nsIFileOutputStream);
  ostream.init(file, -1, -1, aDeferOpen ? Ci.nsIFileOutputStream.DEFER_OPEN : 0);

  // Finally, we need an input stream to take data from.
  const TEST_DATA = "this is a test string";
  let istream = Cc["@mozilla.org/io/string-input-stream;1"].
                createInstance(Ci.nsIStringInputStream);
  istream.setData(TEST_DATA, TEST_DATA.length);

  NetUtil.asyncCopy(istream, ostream, function(aResult) {
    // Make sure the copy was successful!
    do_check_true(Components.isSuccessCode(aResult));

    // Check the file contents.
    do_check_eq(TEST_DATA, getFileContents(file));

    // Finish the test.
    do_test_finished();
    run_next_test();
  });
}

////////////////////////////////////////////////////////////////////////////////
//// Tests

// Test NetUtil.asyncCopy for all possible buffering scenarios
function test_async_copy()
{
  // Create a data sample
  function make_sample(text) {
    let data = [];
    for (let i = 0; i <= 100; ++i) {
      data.push(text);
    }
    return data.join();
  }

  // Create an input buffer holding some data
  function make_input(isBuffered, data) {
    if (isBuffered) {
      // String input streams are buffered
      let istream = Cc["@mozilla.org/io/string-input-stream;1"].
        createInstance(Ci.nsIStringInputStream);
      istream.setData(data, data.length);
      return istream;
    }

    // File input streams are not buffered, so let's create a file
    let file = Cc["@mozilla.org/file/directory_service;1"].
      getService(Ci.nsIProperties).
      get("ProfD", Ci.nsIFile);
    file.append("NetUtil-asyncFetch-test-file.tmp");
    file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);

    let ostream = Cc["@mozilla.org/network/file-output-stream;1"].
      createInstance(Ci.nsIFileOutputStream);
    ostream.init(file, -1, -1, 0);
    ostream.write(data, data.length);
    ostream.close();

    let istream = Cc["@mozilla.org/network/file-input-stream;1"].
      createInstance(Ci.nsIFileInputStream);
    istream.init(file, -1, 0, 0);

    return istream;
  }

  // Create an output buffer holding some data
  function make_output(isBuffered) {
    let file = Cc["@mozilla.org/file/directory_service;1"].
      getService(Ci.nsIProperties).
      get("ProfD", Ci.nsIFile);
    file.append("NetUtil-asyncFetch-test-file.tmp");
    file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);

    let ostream = Cc["@mozilla.org/network/file-output-stream;1"].
      createInstance(Ci.nsIFileOutputStream);
    ostream.init(file, -1, -1, 0);

    if (!isBuffered) {
      return {file: file, sink: ostream};
    }

    let bstream = Cc["@mozilla.org/network/buffered-output-stream;1"].
      createInstance(Ci.nsIBufferedOutputStream);
    bstream.init(ostream, 256);
    return {file: file, sink: bstream};
  }
  (async function() {
    do_test_pending();
    for (let bufferedInput of [true, false]) {
      for (let bufferedOutput of [true, false]) {
        let text = "test_async_copy with "
          + (bufferedInput?"buffered input":"unbuffered input")
          + ", "
          + (bufferedOutput?"buffered output":"unbuffered output");
        do_print(text);
        let TEST_DATA = "[" + make_sample(text) + "]";
        let source = make_input(bufferedInput, TEST_DATA);
        let {file, sink} = make_output(bufferedOutput);
        let result = await new Promise(resolve => {
          NetUtil.asyncCopy(source, sink, resolve);
        });

        // Make sure the copy was successful!
        if (!Components.isSuccessCode(result)) {
          do_throw(new Components.Exception("asyncCopy error", result));
        }

        // Check the file contents.
        do_check_eq(TEST_DATA, getFileContents(file));
      }
    }

    do_test_finished();
    run_next_test();
  })();
}

function test_async_write_file() {
  async_write_file(OUTPUT_STREAM_CONTRACT_ID);
}

function test_async_write_file_deferred() {
  async_write_file(OUTPUT_STREAM_CONTRACT_ID, true);
}

function test_async_write_file_safe() {
  async_write_file(SAFE_OUTPUT_STREAM_CONTRACT_ID);
}

function test_async_write_file_safe_deferred() {
  async_write_file(SAFE_OUTPUT_STREAM_CONTRACT_ID, true);
}

function test_newURI_no_spec_throws()
{
  try {
    NetUtil.newURI();
    do_throw("should throw!");
  }
  catch (e) {
    do_check_eq(e.result, Cr.NS_ERROR_INVALID_ARG);
  }

  run_next_test();
}

function test_newURI()
{
  let ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);

  // Check that we get the same URI back from the IO service and the utility
  // method.
  const TEST_URI = "http://mozilla.org";
  let iosURI = ios.newURI(TEST_URI);
  let NetUtilURI = NetUtil.newURI(TEST_URI);
  do_check_true(iosURI.equals(NetUtilURI));

  run_next_test();
}

function test_newURI_takes_nsIFile()
{
  let ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);

  // Create a test file that we can pass into NetUtil.newURI
  let file = Cc["@mozilla.org/file/directory_service;1"].
             getService(Ci.nsIProperties).
             get("ProfD", Ci.nsIFile);
  file.append("NetUtil-test-file.tmp");

  // Check that we get the same URI back from the IO service and the utility
  // method.
  let iosURI = ios.newFileURI(file);
  let NetUtilURI = NetUtil.newURI(file);
  do_check_true(iosURI.equals(NetUtilURI));

  run_next_test();
}

function test_ioService()
{
  do_check_true(NetUtil.ioService instanceof Ci.nsIIOService);
  run_next_test();
}

function test_asyncFetch_no_channel()
{
  try {
    NetUtil.asyncFetch(null, function() { });
    do_throw("should throw!");
  }
  catch (e) {
    do_check_eq(e.result, Cr.NS_ERROR_INVALID_ARG);
  }

  run_next_test();
}

function test_asyncFetch_no_callback()
{
  try {
    NetUtil.asyncFetch({ });
    do_throw("should throw!");
  }
  catch (e) {
    do_check_eq(e.result, Cr.NS_ERROR_INVALID_ARG);
  }

  run_next_test();
}

function test_asyncFetch_with_nsIChannel()
{
  const TEST_DATA = "this is a test string";

  // Start the http server, and register our handler.
  let server = new HttpServer();
  server.registerPathHandler("/test", function(aRequest, aResponse) {
    aResponse.setStatusLine(aRequest.httpVersion, 200, "OK");
    aResponse.setHeader("Content-Type", "text/plain", false);
    aResponse.write(TEST_DATA);
  });
  server.start(-1);

  // Create our channel.
  let channel = NetUtil.newChannel({
    uri: "http://localhost:" + server.identity.primaryPort + "/test",
    loadUsingSystemPrincipal: true,
  });

  // Open our channel asynchronously.
  NetUtil.asyncFetch(channel, function(aInputStream, aResult) {
    // Check that we had success.
    do_check_true(Components.isSuccessCode(aResult));

    // Check that we got the right data.
    do_check_eq(aInputStream.available(), TEST_DATA.length);
    let is = Cc["@mozilla.org/scriptableinputstream;1"].
             createInstance(Ci.nsIScriptableInputStream);
    is.init(aInputStream);
    let result = is.read(TEST_DATA.length);
    do_check_eq(TEST_DATA, result);

    server.stop(run_next_test);
  });
}

function test_asyncFetch_with_nsIURI()
{
  const TEST_DATA = "this is a test string";

  // Start the http server, and register our handler.
  let server = new HttpServer();
  server.registerPathHandler("/test", function(aRequest, aResponse) {
    aResponse.setStatusLine(aRequest.httpVersion, 200, "OK");
    aResponse.setHeader("Content-Type", "text/plain", false);
    aResponse.write(TEST_DATA);
  });
  server.start(-1);

  // Create our URI.
  let uri = NetUtil.newURI("http://localhost:" +
                           server.identity.primaryPort + "/test");

  // Open our URI asynchronously.
  NetUtil.asyncFetch({
    uri,
    loadUsingSystemPrincipal: true,
  }, function(aInputStream, aResult) {
    // Check that we had success.
    do_check_true(Components.isSuccessCode(aResult));

    // Check that we got the right data.
    do_check_eq(aInputStream.available(), TEST_DATA.length);
    let is = Cc["@mozilla.org/scriptableinputstream;1"].
             createInstance(Ci.nsIScriptableInputStream);
    is.init(aInputStream);
    let result = is.read(TEST_DATA.length);
    do_check_eq(TEST_DATA, result);

    server.stop(run_next_test);
  },
  null,      // aLoadingNode
  Services.scriptSecurityManager.getSystemPrincipal(),
  null,      // aTriggeringPrincipal
  Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
  Ci.nsIContentPolicy.TYPE_OTHER);
}

function test_asyncFetch_with_string()
{
  const TEST_DATA = "this is a test string";

  // Start the http server, and register our handler.
  let server = new HttpServer();
  server.registerPathHandler("/test", function(aRequest, aResponse) {
    aResponse.setStatusLine(aRequest.httpVersion, 200, "OK");
    aResponse.setHeader("Content-Type", "text/plain", false);
    aResponse.write(TEST_DATA);
  });
  server.start(-1);

  // Open our location asynchronously.
  NetUtil.asyncFetch({
    uri: "http://localhost:" + server.identity.primaryPort + "/test",
    loadUsingSystemPrincipal: true,
  }, function(aInputStream, aResult) {
    // Check that we had success.
    do_check_true(Components.isSuccessCode(aResult));

    // Check that we got the right data.
    do_check_eq(aInputStream.available(), TEST_DATA.length);
    let is = Cc["@mozilla.org/scriptableinputstream;1"].
             createInstance(Ci.nsIScriptableInputStream);
    is.init(aInputStream);
    let result = is.read(TEST_DATA.length);
    do_check_eq(TEST_DATA, result);

    server.stop(run_next_test);
  },
  null,      // aLoadingNode
  Services.scriptSecurityManager.getSystemPrincipal(),
  null,      // aTriggeringPrincipal
  Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
  Ci.nsIContentPolicy.TYPE_OTHER);
}

function test_asyncFetch_with_nsIFile()
{
  const TEST_DATA = "this is a test string";

  // First we need a file to read from.
  let file = Cc["@mozilla.org/file/directory_service;1"].
             getService(Ci.nsIProperties).
             get("ProfD", Ci.nsIFile);
  file.append("NetUtil-asyncFetch-test-file.tmp");
  file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);

  // Write the test data to the file.
  let ostream = Cc["@mozilla.org/network/file-output-stream;1"].
                createInstance(Ci.nsIFileOutputStream);
  ostream.init(file, -1, -1, 0);
  ostream.write(TEST_DATA, TEST_DATA.length);

  // Sanity check to make sure the data was written.
  do_check_eq(TEST_DATA, getFileContents(file));

  // Open our file asynchronously.
  // Note that this causes main-tread I/O and should be avoided in production.
  NetUtil.asyncFetch({
    uri: NetUtil.newURI(file),
    loadUsingSystemPrincipal: true,
  }, function(aInputStream, aResult) {
    // Check that we had success.
    do_check_true(Components.isSuccessCode(aResult));

    // Check that we got the right data.
    do_check_eq(aInputStream.available(), TEST_DATA.length);
    let is = Cc["@mozilla.org/scriptableinputstream;1"].
             createInstance(Ci.nsIScriptableInputStream);
    is.init(aInputStream);
    let result = is.read(TEST_DATA.length);
    do_check_eq(TEST_DATA, result);

    run_next_test();
  },
  null,      // aLoadingNode
  Services.scriptSecurityManager.getSystemPrincipal(),
  null,      // aTriggeringPrincipal
  Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
  Ci.nsIContentPolicy.TYPE_OTHER);
}

function test_asyncFetch_with_nsIInputString()
{
  const TEST_DATA = "this is a test string";
  let istream = Cc["@mozilla.org/io/string-input-stream;1"].
                createInstance(Ci.nsIStringInputStream);
  istream.setData(TEST_DATA, TEST_DATA.length);

  // Read the input stream asynchronously.
  NetUtil.asyncFetch(istream, function(aInputStream, aResult) {
    // Check that we had success.
    do_check_true(Components.isSuccessCode(aResult));

    // Check that we got the right data.
    do_check_eq(aInputStream.available(), TEST_DATA.length);
    do_check_eq(NetUtil.readInputStreamToString(aInputStream, TEST_DATA.length),
                TEST_DATA);

    run_next_test();
  },
  null,      // aLoadingNode
  Services.scriptSecurityManager.getSystemPrincipal(),
  null,      // aTriggeringPrincipal
  Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
  Ci.nsIContentPolicy.TYPE_OTHER);
}

function test_asyncFetch_does_not_block()
{
  // Create our channel that has no data.
  let channel = NetUtil.newChannel({
    uri: "data:text/plain,",
    loadUsingSystemPrincipal: true,
  });

  // Open our channel asynchronously.
  NetUtil.asyncFetch(channel, function(aInputStream, aResult) {
    // Check that we had success.
    do_check_true(Components.isSuccessCode(aResult));

    // Check that reading a byte throws that the stream was closed (as opposed
    // saying it would block).
    let is = Cc["@mozilla.org/scriptableinputstream;1"].
             createInstance(Ci.nsIScriptableInputStream);
    is.init(aInputStream);
    try {
      is.read(1);
      do_throw("should throw!");
    }
    catch (e) {
      do_check_eq(e.result, Cr.NS_BASE_STREAM_CLOSED);
    }

    run_next_test();
  });
}

function test_newChannel_no_specifier()
{
  try {
    NetUtil.newChannel();
    do_throw("should throw!");
  }
  catch (e) {
    do_check_eq(e.result, Cr.NS_ERROR_INVALID_ARG);
  }

  run_next_test();
}

function test_newChannel_with_string()
{
  const TEST_SPEC = "http://mozilla.org";

  // Check that we get the same URI back from channel the IO service creates and
  // the channel the utility method creates.
  let ios = NetUtil.ioService;
  let iosChannel = ios.newChannel2(TEST_SPEC,
                                   null,
                                   null,
                                   null,      // aLoadingNode
                                   Services.scriptSecurityManager.getSystemPrincipal(),
                                   null,      // aTriggeringPrincipal
                                   Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
                                   Ci.nsIContentPolicy.TYPE_OTHER);
  let NetUtilChannel = NetUtil.newChannel({
    uri: TEST_SPEC,
    loadUsingSystemPrincipal: true
  });
  do_check_true(iosChannel.URI.equals(NetUtilChannel.URI));

  run_next_test();
}

function test_newChannel_with_nsIURI()
{
  const TEST_SPEC = "http://mozilla.org";

  // Check that we get the same URI back from channel the IO service creates and
  // the channel the utility method creates.
  let uri = NetUtil.newURI(TEST_SPEC);
  let iosChannel = NetUtil.ioService.newChannelFromURI2(uri,
                                                        null,      // aLoadingNode
                                                        Services.scriptSecurityManager.getSystemPrincipal(),
                                                        null,      // aTriggeringPrincipal
                                                        Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
                                                        Ci.nsIContentPolicy.TYPE_OTHER);
  let NetUtilChannel = NetUtil.newChannel({
    uri: uri,
    loadUsingSystemPrincipal: true
  });
  do_check_true(iosChannel.URI.equals(NetUtilChannel.URI));

  run_next_test();
}

function test_newChannel_with_options()
{
  let uri = "data:text/plain,";

  let iosChannel = NetUtil.ioService.newChannelFromURI2(NetUtil.newURI(uri),
                                                        null,      // aLoadingNode
                                                        Services.scriptSecurityManager.getSystemPrincipal(),
                                                        null,      // aTriggeringPrincipal
                                                        Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
                                                        Ci.nsIContentPolicy.TYPE_OTHER);

  function checkEqualToIOSChannel(channel) {
    do_check_true(iosChannel.URI.equals(channel.URI));  
  }

  checkEqualToIOSChannel(NetUtil.newChannel({
    uri,
    loadingPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    contentPolicyType: Ci.nsIContentPolicy.TYPE_OTHER,
  }));

  checkEqualToIOSChannel(NetUtil.newChannel({
    uri,
    loadUsingSystemPrincipal: true,
  }));

  run_next_test();
}

function test_newChannel_with_wrong_options()
{
  let uri = "data:text/plain,";
  let systemPrincipal = Services.scriptSecurityManager.getSystemPrincipal();

  Assert.throws(() => {
    NetUtil.newChannel({ uri, loadUsingSystemPrincipal: true }, null, null);
  }, /requires a single object argument/);

  Assert.throws(() => {
    NetUtil.newChannel({});
  }, /requires the 'uri' property/);

  Assert.throws(() => {
    NetUtil.newChannel({ uri });
  }, /requires at least one of the 'loadingNode'/);

  Assert.throws(() => {
    NetUtil.newChannel({
      uri,
      loadingPrincipal: systemPrincipal,
    });
  }, /requires the 'contentPolicyType'/);

  Assert.throws(() => {
    NetUtil.newChannel({
      uri,
      loadUsingSystemPrincipal: systemPrincipal,
    });
  }, /to be 'true' or 'undefined'/);

  Assert.throws(() => {
    NetUtil.newChannel({
      uri,
      loadingPrincipal: systemPrincipal,
      loadUsingSystemPrincipal: true,
    });
  }, /does not accept 'loadUsingSystemPrincipal'/);

  run_next_test();
}

function test_deprecated_newChannel_API_with_string() {
  const TEST_SPEC = "http://mozilla.org";
  let uri = NetUtil.newURI(TEST_SPEC);
  let oneArgChannel = NetUtil.newChannel(TEST_SPEC);
  let threeArgChannel = NetUtil.newChannel(TEST_SPEC, null, null);
  do_check_true(uri.equals(oneArgChannel.URI));
  do_check_true(uri.equals(threeArgChannel.URI));

  run_next_test();
}

function test_deprecated_newChannel_API_with_nsIFile()
{
  const TEST_DATA = "this is a test string";

  // First we need a file to read from.
  let file = Cc["@mozilla.org/file/directory_service;1"].
             getService(Ci.nsIProperties).
             get("ProfD", Ci.nsIFile);
  file.append("NetUtil-deprecated-newchannel-api-test-file.tmp");
  file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);

  // Write the test data to the file.
  let ostream = Cc["@mozilla.org/network/file-output-stream;1"].
                createInstance(Ci.nsIFileOutputStream);
  ostream.init(file, -1, -1, 0);
  ostream.write(TEST_DATA, TEST_DATA.length);

  // Sanity check to make sure the data was written.
  do_check_eq(TEST_DATA, getFileContents(file));

  // create a channel using the file
  let channel = NetUtil.newChannel(file);

  // Create a pipe that will create our output stream that we can use once
  // we have gotten all the data.
  let pipe = Cc["@mozilla.org/pipe;1"].createInstance(Ci.nsIPipe);
  pipe.init(true, true, 0, 0, null);

  let listener = Cc["@mozilla.org/network/simple-stream-listener;1"].
                   createInstance(Ci.nsISimpleStreamListener);
  listener.init(pipe.outputStream, {
    onStartRequest: function(aRequest, aContext) {},
    onStopRequest: function(aRequest, aContext, aStatusCode) {
      pipe.outputStream.close();
      do_check_true(Components.isSuccessCode(aContext));

      // Check that we got the right data.
      do_check_eq(pipe.inputStream.available(), TEST_DATA.length);
      let is = Cc["@mozilla.org/scriptableinputstream;1"].
               createInstance(Ci.nsIScriptableInputStream);
      is.init(pipe.inputStream);
      let result = is.read(TEST_DATA.length);
      do_check_eq(TEST_DATA, result);
      run_next_test();
    }
  });
  channel.asyncOpen2(listener);
}

function test_readInputStreamToString()
{
  const TEST_DATA = "this is a test string\0 with an embedded null";
  let istream = Cc["@mozilla.org/io/string-input-stream;1"].
                createInstance(Ci.nsISupportsCString);
  istream.data = TEST_DATA;

  do_check_eq(NetUtil.readInputStreamToString(istream, TEST_DATA.length),
              TEST_DATA);

  run_next_test();
}

function test_readInputStreamToString_no_input_stream()
{
  try {
    NetUtil.readInputStreamToString("hi", 2);
    do_throw("should throw!");
  }
  catch (e) {
    do_check_eq(e.result, Cr.NS_ERROR_INVALID_ARG);
  }

  run_next_test();
}

function test_readInputStreamToString_no_bytes_arg()
{
  const TEST_DATA = "this is a test string";
  let istream = Cc["@mozilla.org/io/string-input-stream;1"].
                createInstance(Ci.nsIStringInputStream);
  istream.setData(TEST_DATA, TEST_DATA.length);

  try {
    NetUtil.readInputStreamToString(istream);
    do_throw("should throw!");
  }
  catch (e) {
    do_check_eq(e.result, Cr.NS_ERROR_INVALID_ARG);
  }

  run_next_test();
}

function test_readInputStreamToString_blocking_stream()
{
  let pipe = Cc["@mozilla.org/pipe;1"].createInstance(Ci.nsIPipe);
  pipe.init(true, true, 0, 0, null);

  try {
    NetUtil.readInputStreamToString(pipe.inputStream, 10);
    do_throw("should throw!");
  }
  catch (e) {
    do_check_eq(e.result, Cr.NS_BASE_STREAM_WOULD_BLOCK);
  }
  run_next_test();
}

function test_readInputStreamToString_too_many_bytes()
{
  const TEST_DATA = "this is a test string";
  let istream = Cc["@mozilla.org/io/string-input-stream;1"].
                createInstance(Ci.nsIStringInputStream);
  istream.setData(TEST_DATA, TEST_DATA.length);

  try {
    NetUtil.readInputStreamToString(istream, TEST_DATA.length + 10);
    do_throw("should throw!");
  }
  catch (e) {
    do_check_eq(e.result, Cr.NS_ERROR_FAILURE);
  }

  run_next_test();
}

function test_readInputStreamToString_with_charset()
{
  const TEST_DATA = "\uff10\uff11\uff12\uff13";
  const TEST_DATA_UTF8 = "\xef\xbc\x90\xef\xbc\x91\xef\xbc\x92\xef\xbc\x93";
  const TEST_DATA_SJIS = "\x82\x4f\x82\x50\x82\x51\x82\x52";

  let istream = Cc["@mozilla.org/io/string-input-stream;1"].
                createInstance(Ci.nsIStringInputStream);

  istream.setData(TEST_DATA_UTF8, TEST_DATA_UTF8.length);
  do_check_eq(NetUtil.readInputStreamToString(istream,
                                              TEST_DATA_UTF8.length,
                                              { charset: "UTF-8"}),
              TEST_DATA);

  istream.setData(TEST_DATA_SJIS, TEST_DATA_SJIS.length);
  do_check_eq(NetUtil.readInputStreamToString(istream,
                                              TEST_DATA_SJIS.length,
                                              { charset: "Shift_JIS"}),
              TEST_DATA);

  run_next_test();
}

function test_readInputStreamToString_invalid_sequence()
{
  const TEST_DATA = "\ufffd\ufffd\ufffd\ufffd";
  const TEST_DATA_UTF8 = "\xaa\xaa\xaa\xaa";

  let istream = Cc["@mozilla.org/io/string-input-stream;1"].
                createInstance(Ci.nsIStringInputStream);

  istream.setData(TEST_DATA_UTF8, TEST_DATA_UTF8.length);
  try {
    NetUtil.readInputStreamToString(istream,
                                    TEST_DATA_UTF8.length,
                                    { charset: "UTF-8" });
    do_throw("should throw!");
  } catch (e) {
    do_check_eq(e.result, Cr.NS_ERROR_ILLEGAL_INPUT);
  }

  istream.setData(TEST_DATA_UTF8, TEST_DATA_UTF8.length);
  do_check_eq(NetUtil.readInputStreamToString(istream,
                                              TEST_DATA_UTF8.length, {
                                                charset: "UTF-8",
                                                replacement: Ci.nsIConverterInputStream.DEFAULT_REPLACEMENT_CHARACTER}),
              TEST_DATA);

  run_next_test();
}


////////////////////////////////////////////////////////////////////////////////
//// Test Runner

[
  test_async_copy,
  test_async_write_file,
  test_async_write_file_deferred,
  test_async_write_file_safe,
  test_async_write_file_safe_deferred,
  test_newURI_no_spec_throws,
  test_newURI,
  test_newURI_takes_nsIFile,
  test_ioService,
  test_asyncFetch_no_channel,
  test_asyncFetch_no_callback,
  test_asyncFetch_with_nsIChannel,
  test_asyncFetch_with_nsIURI,
  test_asyncFetch_with_string,
  test_asyncFetch_with_nsIFile,
  test_asyncFetch_with_nsIInputString,
  test_asyncFetch_does_not_block,
  test_newChannel_no_specifier,
  test_newChannel_with_string,
  test_newChannel_with_nsIURI,
  test_newChannel_with_options,
  test_newChannel_with_wrong_options,
  test_deprecated_newChannel_API_with_string,
  test_deprecated_newChannel_API_with_nsIFile,
  test_readInputStreamToString,
  test_readInputStreamToString_no_input_stream,
  test_readInputStreamToString_no_bytes_arg,
  test_readInputStreamToString_blocking_stream,
  test_readInputStreamToString_too_many_bytes,
  test_readInputStreamToString_with_charset,
  test_readInputStreamToString_invalid_sequence,
].forEach(add_test);
var index = 0;

function run_test()
{
  run_next_test();
}

