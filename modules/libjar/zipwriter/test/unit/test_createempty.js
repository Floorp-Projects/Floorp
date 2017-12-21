/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

function run_test()
{
  zipW.open(tmpFile, PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE);
  zipW.close();

  // Should have created a zip file
  Assert.ok(tmpFile.exists());

  // Empty zip file should just be the end of central directory marker
  Assert.equal(tmpFile.fileSize, ZIP_EOCDR_HEADER_SIZE);
}
