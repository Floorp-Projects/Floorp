/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

var TESTS = [
  "test.txt",
  "test.png"
];

var observer = {
  onStartRequest: function(request, context)
  {
  },

  onStopRequest: function(request, context, status)
  {
    Assert.equal(status, Components.results.NS_OK);

    zipW.close();

    // Empty zip file should just be the end of central directory marker
    var newTmpFile = tmpFile.clone();
    Assert.equal(newTmpFile.fileSize, ZIP_EOCDR_HEADER_SIZE);
    do_test_finished();
  }
};

function run_test()
{
  // Copy our test zip to the tmp dir so we can modify it
  var testzip = do_get_file(DATA_DIR + "test.zip");
  testzip.copyTo(tmpDir, tmpFile.leafName);

  Assert.ok(tmpFile.exists());

  zipW.open(tmpFile, PR_RDWR);

  for (var i = 0; i < TESTS.length; i++) {
    Assert.ok(zipW.hasEntry(TESTS[i]));
    zipW.removeEntry(TESTS[i], true);
  }

  do_test_pending();
  zipW.processQueue(observer, null);
  Assert.ok(zipW.inQueue);
}
