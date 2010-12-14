/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mark Banner <bugzilla@standard8.plus.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// Encoded test URI to work on all platforms/independent of file encoding
const kTestURI = "http://\u65e5\u672c\u8a93.jp/";
const kExpectedURI = "http://xn--wgv71a309e.jp/";
const kOutputFile = "result.txt";

// Try several times in case the box we're running on is slow.
const kMaxCheckExistAttempts = 30; // seconds
var gCheckExistsAttempts = 0;

function checkFile() {
  // This is where we expect the output
  var tempFile = do_get_cwd();
  tempFile.append(kOutputFile);

  if (!tempFile.exists()) {
    if (gCheckExistsAttempts >= kMaxCheckExistAttempts) {
      do_throw("Expected File " + tempFile.path + " does not exist after " +
                 kMaxCheckExistAttempts + " seconds");
    }
    else {
      ++gCheckExistsAttempts;
      // Wait a bit longer then try again
      do_timeout(1000, checkFile);
    }
  }

  // Now read it
  var fstream =
    Components.classes["@mozilla.org/network/file-input-stream;1"]
              .createInstance(Components.interfaces.nsIFileInputStream);
  var sstream =
    Components.classes["@mozilla.org/scriptableinputstream;1"]
              .createInstance(Components.interfaces.nsIScriptableInputStream);
  fstream.init(tempFile, -1, 0, 0);
  sstream.init(fstream);

  // Read the first line only as that's the one we expect WriteArguments
  // to be writing the argument to.
  var data = sstream.read(4096);

  sstream.close();
  fstream.close();

  // Now remove the old file
  tempFile.remove(false);

  // This currently fails on Mac with an argument like -psn_0_nnnnnn
  // This seems to be to do with how the executable is called, but I couldn't
  // find a way around it.
  // Additionally the lack of OS detection in xpcshell tests sucks, so we'll
  // have to check for the argument mac gives us.
  if (data.substring(0, 7) != "-psn_0_")
    do_check_eq(data, kExpectedURI);

  do_test_finished();
}

function run_test() {
  var isOSX = ("nsILocalFileMac" in Components.interfaces);
  if (isOSX) {
    dump("INFO | test_punycodeURIs.js | Skipping test on mac, bug 599475")
    return;
  }

  // set up the uri to test with
  var ioService =
    Components.classes["@mozilla.org/network/io-service;1"]
              .getService(Components.interfaces.nsIIOService);

  // set up the local handler object
  var localHandler =
    Components.classes["@mozilla.org/uriloader/local-handler-app;1"]
              .createInstance(Components.interfaces.nsILocalHandlerApp);
  localHandler.name = "Test Local Handler App";

  // WriteArgument will just dump its arguments to a file for us.
  var processDir = do_get_cwd();
  var exe = processDir.clone();
  exe.append("WriteArgument");

  if (!exe.exists()) {
    // Maybe we are on windows
    exe.leafName = "WriteArgument.exe";
    if (!exe.exists())
      do_throw("Could not locate the WriteArgument tests executable\n");
  }

  var outFile = processDir.clone();
  outFile.append(kOutputFile);

  // Set an environment variable for WriteArgument to pick up
  var envSvc =
    Components.classes["@mozilla.org/process/environment;1"]
              .getService(Components.interfaces.nsIEnvironment);

  // The Write Argument file needs to know where its libraries are, so
  // just force the path variable
  // For mac
  var greDir = HandlerServiceTest._dirSvc.get("GreD", Components.interfaces.nsIFile);

  envSvc.set("DYLD_LIBRARY_PATH", greDir.path);
  // For Linux
  envSvc.set("LD_LIBRARY_PATH", greDir.path);
  //XXX: handle windows

  // Now tell it where we want the file.
  envSvc.set("WRITE_ARGUMENT_FILE", outFile.path);

  var uri = ioService.newURI(kTestURI, null, null);

  // Just check we've got these matching, if we haven't there's a problem
  // with ascii spec or our test case.
  do_check_eq(uri.asciiSpec, kExpectedURI);

  localHandler.executable = exe;
  localHandler.launchWithURI(uri);

  do_test_pending();
  do_timeout(1000, checkFile);
}
