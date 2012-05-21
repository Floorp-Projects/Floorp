/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

const DIRNAME1 = "test";
const DIRNAME1_CORRECT = "test/";
const DIRNAME2 = "test2/";
const time = Date.now();

function run_test()
{
  zipW.open(tmpFile, PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE);

  zipW.addEntryDirectory(DIRNAME1, time * PR_USEC_PER_MSEC, false);
  do_check_false(zipW.hasEntry(DIRNAME1));
  do_check_true(zipW.hasEntry(DIRNAME1_CORRECT));
  var entry = zipW.getEntry(DIRNAME1_CORRECT);
  do_check_true(entry.isDirectory);

  zipW.addEntryDirectory(DIRNAME2, time * PR_USEC_PER_MSEC, false);
  do_check_true(zipW.hasEntry(DIRNAME2));
  entry = zipW.getEntry(DIRNAME2);
  do_check_true(entry.isDirectory);

  zipW.close();
}
