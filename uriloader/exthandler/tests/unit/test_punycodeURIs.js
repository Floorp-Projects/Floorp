/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Encoded test URI to work on all platforms/independent of file encoding
const kTestURI = "http://\u65e5\u672c\u8a93.jp/";
const kExpectedURI = "http://xn--wgv71a309e.jp/";
const kOutputFile = "result.txt";

// Try several times in case the box we're running on is slow.
const kMaxCheckExistAttempts = 30; // seconds
var gCheckExistsAttempts = 0;

const tempDir = do_get_tempdir();

function checkFile() {
  // This is where we expect the output
  var tempFile = tempDir.clone();
  tempFile.append(kOutputFile);

  if (!tempFile.exists()) {
    if (gCheckExistsAttempts >= kMaxCheckExistAttempts) {
      do_throw(
        "Expected File " +
          tempFile.path +
          " does not exist after " +
          kMaxCheckExistAttempts +
          " seconds"
      );
    } else {
      ++gCheckExistsAttempts;
      // Wait a bit longer then try again
      do_timeout(1000, checkFile);
      return;
    }
  }

  // Now read it
  var fstream = Cc["@mozilla.org/network/file-input-stream;1"].createInstance(
    Ci.nsIFileInputStream
  );
  var sstream = Cc["@mozilla.org/scriptableinputstream;1"].createInstance(
    Ci.nsIScriptableInputStream
  );
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
  if (data.substring(0, 7) != "-psn_0_") {
    Assert.equal(data, kExpectedURI);
  }

  do_test_finished();
}

function run_test() {
  if (mozinfo.os == "mac") {
    dump("INFO | test_punycodeURIs.js | Skipping test on mac, bug 599475");
    return;
  }

  // set up the uri to test with
  var ioService = Services.io;

  // set up the local handler object
  var localHandler = Cc[
    "@mozilla.org/uriloader/local-handler-app;1"
  ].createInstance(Ci.nsILocalHandlerApp);
  localHandler.name = "Test Local Handler App";

  // WriteArgument will just dump its arguments to a file for us.
  var processDir = do_get_cwd();
  var exe = processDir.clone();
  exe.append("WriteArgument");

  if (!exe.exists()) {
    // Maybe we are on windows
    exe.leafName = "WriteArgument.exe";
    if (!exe.exists()) {
      do_throw("Could not locate the WriteArgument tests executable\n");
    }
  }

  var outFile = tempDir.clone();
  outFile.append(kOutputFile);

  // Set an environment variable for WriteArgument to pick up
  var envSvc = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );

  // The Write Argument file needs to know where its libraries are, so
  // just force the path variable
  // For mac
  var greDir = Services.dirsvc.get("GreD", Ci.nsIFile);

  envSvc.set("DYLD_LIBRARY_PATH", greDir.path);
  // For Linux
  envSvc.set("LD_LIBRARY_PATH", greDir.path);
  // XXX: handle windows

  // Now tell it where we want the file.
  envSvc.set("WRITE_ARGUMENT_FILE", outFile.path);

  var uri = ioService.newURI(kTestURI);

  // Just check we've got these matching, if we haven't there's a problem
  // with ascii spec or our test case.
  Assert.equal(uri.asciiSpec, kExpectedURI);

  localHandler.executable = exe;
  localHandler.launchWithURI(uri);

  do_test_pending();
  do_timeout(1000, checkFile);
}
