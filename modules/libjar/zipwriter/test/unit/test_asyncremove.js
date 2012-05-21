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
    do_check_eq(status, Components.results.NS_OK);

    zipW.close();

    // Empty zip file should just be the end of central directory marker
    var newTmpFile = tmpFile.clone();
    do_check_eq(newTmpFile.fileSize, ZIP_EOCDR_HEADER_SIZE);
    do_test_finished();
  }
};

function run_test()
{
  // Copy our test zip to the tmp dir so we can modify it
  var testzip = do_get_file(DATA_DIR + "test.zip");
  testzip.copyTo(tmpDir, tmpFile.leafName);

  do_check_true(tmpFile.exists());

  zipW.open(tmpFile, PR_RDWR);

  for (var i = 0; i < TESTS.length; i++) {
    do_check_true(zipW.hasEntry(TESTS[i]));
    zipW.removeEntry(TESTS[i], true);
  }

  do_test_pending();
  zipW.processQueue(observer, null);
  do_check_true(zipW.inQueue);
}
