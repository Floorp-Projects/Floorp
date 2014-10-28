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

// Listens to feeds being loaded. Runs the tests built into the feed afterwards to veryify they
// were parsed correctly.
function FeedListener(testcase) {
  this.testcase = testcase;
}

FeedListener.prototype = {
  handleResult: function(result) {
    var feed = result.doc;
    try {
      do_print("Testing feed " + this.testcase.file.path);
      Assert.ok(isIID(feed, Ci.nsIFeed), "Has feed interface");

      if (!eval(this.testcase.expect)) {
        Assert.ok(false, "expect failed for " + this.testcase.desc);
      } else {
        Assert.ok(true, "expect passed for " + this.testcase.desc);
      }
    } catch(e) {
      Assert.ok(false, "expect failed for " + this.testcase.desc + " ---- " + e.message);
    }

    run_next_test();
  }
}

function createTest(data) {
  return function() {
    var uri;

    if (data.base == null) {
      uri = NetUtil.newURI('http://example.org/' + data.path);
    } else {
      uri = data.base;
    }

    do_print("Testing " + data.file.leafName);

    var parser = Cc["@mozilla.org/feed-processor;1"].createInstance(Ci.nsIFeedProcessor);
    var stream = Cc["@mozilla.org/network/file-input-stream;1"].createInstance(Ci.nsIFileInputStream);
    stream.init(data.file, 0x01, parseInt("0444", 8), 0);
    parser.listener = new FeedListener(data);

    try {
      parser.parseFromStream(stream, uri);
    } catch(e) {
      Assert.ok(false, "parse failed for " + data.file.leafName + " ---- " + e.message);
      // If the parser failed, the listener won't be notified, run the next test here.
      run_next_test();
    } finally {
      stream.close();
    }
  }
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
