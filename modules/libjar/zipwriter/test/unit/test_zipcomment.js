/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

const DATA = "ZIP WRITER TEST COMMENT";
const DATA2 = "ANOTHER ONE";

function run_test()
{
  zipW.open(tmpFile, PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE);
  zipW.comment = DATA;
  zipW.close();

  // Should have created a zip file
  Assert.ok(tmpFile.exists());

  // Empty zip file should just be the end of central directory marker
  // and comment
  Assert.equal(tmpFile.fileSize, ZIP_EOCDR_HEADER_SIZE + DATA.length);

  zipW.open(tmpFile, PR_RDWR);
  // Should have the set comment
  Assert.equal(zipW.comment, DATA);
  zipW.comment = DATA2;
  zipW.close();

  // Certain platforms cache the file size so get a fresh file to check.
  tmpFile = tmpFile.clone();

  // Empty zip file should just be the end of central directory marker
  // and comment. This should now be shorter
  Assert.equal(tmpFile.fileSize, ZIP_EOCDR_HEADER_SIZE + DATA2.length);
}
