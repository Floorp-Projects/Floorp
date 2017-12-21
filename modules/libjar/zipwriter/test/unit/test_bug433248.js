/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

function run_test()
{
  var test;
  // zipW is an uninitialised zipwriter at this point.
  try {
    test = zipW.file;
    do_throw("Should have thrown uninitialized error.");
  }
  catch (e) {
    Assert.equal(e.result, Components.results.NS_ERROR_NOT_INITIALIZED);
  }

  try {
    test = zipW.comment;
    do_throw("Should have thrown uninitialized error.");
  }
  catch (e) {
    Assert.equal(e.result, Components.results.NS_ERROR_NOT_INITIALIZED);
  }

  try {
    zipW.comment = "test";
    do_throw("Should have thrown uninitialized error.");
  }
  catch (e) {
    Assert.equal(e.result, Components.results.NS_ERROR_NOT_INITIALIZED);
  }

  try {
    zipW.addEntryDirectory("test", 0, false);
    do_throw("Should have thrown uninitialized error.");
  }
  catch (e) {
    Assert.equal(e.result, Components.results.NS_ERROR_NOT_INITIALIZED);
  }

  try {
    zipW.addEntryFile("test", Ci.nsIZipWriter.COMPRESSION_DEFAULT, tmpDir, false);
    do_throw("Should have thrown uninitialized error.");
  }
  catch (e) {
    Assert.equal(e.result, Components.results.NS_ERROR_NOT_INITIALIZED);
  }

  try {
    zipW.removeEntry("test", false);
    do_throw("Should have thrown uninitialized error.");
  }
  catch (e) {
    Assert.equal(e.result, Components.results.NS_ERROR_NOT_INITIALIZED);
  }

  try {
    zipW.processQueue(null, null);
    do_throw("Should have thrown uninitialized error.");
  }
  catch (e) {
    Assert.equal(e.result, Components.results.NS_ERROR_NOT_INITIALIZED);
  }

  try {
    zipW.close();
    do_throw("Should have thrown uninitialized error.");
  }
  catch (e) {
    Assert.equal(e.result, Components.results.NS_ERROR_NOT_INITIALIZED);
  }
}
