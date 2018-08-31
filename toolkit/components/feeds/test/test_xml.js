/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* test_xml.js
 * This file sets up the unit test environment by building an array of files
 * to be tested. It assumes it lives in a folder adjacent to the a folder
 * called 'xml', where the testcases live.
 *
 * The directory layout looks something like this:
 *
 * tests/test_xml.js*
 *      |
 *      - head.js
 *      |
 *      - xml/ -- rss1/...
 *             |
 *             -- rss2/...
 *             |
 *             -- atom/testcase.xml
 *
 * To add more tests, just include the file in the xml subfolder and add its name to xpcshell.ini
 */

"use strict";

ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

// Listens to feeds being loaded. Runs the tests built into the feed afterwards to veryify they
// were parsed correctly.
function FeedListener(testcase) {
  this.testcase = testcase;
}

FeedListener.prototype = {
  handleResult(result) {
    var feed = result.doc;
    try {
      info("Testing feed " + this.testcase.file.path);
      Assert.ok(isIID(feed, Ci.nsIFeed), "Has feed interface");

      // eslint-disable-next-line no-eval
      if (!eval(this.testcase.expect)) {
        Assert.ok(false, "expect failed for " + this.testcase.desc);
      } else {
        Assert.ok(true, "expect passed for " + this.testcase.desc);
      }
    } catch (e) {
      Assert.ok(false, "expect failed for " + this.testcase.desc + " ---- " + e.message);
    }

    run_next_test();
  },
};

function createTest(data) {
  return function() {
    var uri;

    if (data.base == null) {
      uri = NetUtil.newURI("http://example.org/" + data.path);
    } else {
      uri = data.base;
    }

    info("Testing " + data.file.leafName);

    var parser = Cc["@mozilla.org/feed-processor;1"].createInstance(Ci.nsIFeedProcessor);
    var stream = Cc["@mozilla.org/network/file-input-stream;1"].createInstance(Ci.nsIFileInputStream);
    stream.init(data.file, 0x01, parseInt("0444", 8), 0);
    var bStream = Cc["@mozilla.org/network/buffered-input-stream;1"].createInstance(Ci.nsIBufferedInputStream);
    bStream.init(stream, 4096);
    parser.listener = new FeedListener(data);

    try {
      let channel = Cc["@mozilla.org/network/input-stream-channel;1"].
        createInstance(Ci.nsIInputStreamChannel);
      channel.setURI(uri);
      channel.contentStream = bStream;
      channel.QueryInterface(Ci.nsIChannel);
      channel.contentType = "text/xml";

      parser.parseAsync(null, uri);
      parser.onStartRequest(channel, uri);

      let pos = 0;
      let count = bStream.available();
      while (count > 0) {
        parser.onDataAvailable(channel, null, bStream, pos, count);
        pos += count;
        count = bStream.available();
      }
      parser.onStopRequest(channel, null, Cr.NS_OK);
    } catch (e) {
      Assert.ok(false, "parse failed for " + data.file.leafName + " ---- " + e.message);
      // If the parser failed, the listener won't be notified, run the next test here.
      run_next_test();
    } finally {
      bStream.close();
    }
  };
}

function run_test() {
  // Get the 'xml' directory in here
  var topDir = Services.dirsvc.get("CurWorkD", Ci.nsIFile);
  topDir.append("xml");

  // Every file in the test dir contains an encapulated RSS "test". Iterate through
  // them all and add them to the test runner.
  iterateDir(topDir, true, file => {
    var data = readTestData(file);
    add_test(createTest(data));
  });

  // Now run!
  run_next_test();
}
