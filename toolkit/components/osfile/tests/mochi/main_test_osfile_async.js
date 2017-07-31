"use strict";

Components.utils.import("resource://gre/modules/osfile.jsm");
Components.utils.import("resource://gre/modules/AsyncShutdown.jsm");

// The following are used to compare against a well-tested reference
// implementation of file I/O.
Components.utils.import("resource://gre/modules/NetUtil.jsm");
Components.utils.import("resource://gre/modules/FileUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

var myok = ok;
var myis = is;
var myinfo = info;
var myisnot = isnot;

var isPromise = function ispromise(value) {
  return value != null && typeof value == "object" && "then" in value;
};

var maketest = function(prefix, test) {
  let utils = {
    ok: function ok(t, m) {
      myok(t, prefix + ": " + m);
    },
    is: function is(l, r, m) {
      myis(l, r, prefix + ": " + m);
    },
    isnot: function isnot(l, r, m) {
      myisnot(l, r, prefix + ": " + m);
    },
    info: function info(m) {
      myinfo(prefix + ": " + m);
    },
    fail: function fail(m) {
      utils.ok(false, m);
    },
    okpromise: function okpromise(t, m) {
      return t.then(
        function onSuccess() {
          util.ok(true, m);
        },
        function onFailure() {
          util.ok(false, m);
        }
      );
    }
  };
  return function runtest() {
    utils.info("Entering");
    try {
      let result = test.call(this, utils);
      if (!isPromise(result)) {
        throw new TypeError("The test did not return a promise");
      }
      utils.info("This was a promise");
      // The test returns a promise
      result = result.then(function test_complete() {
        utils.info("Complete");
      }, function catch_uncaught_errors(err) {
        utils.fail("Uncaught error " + err);
        if (err && typeof err == "object" && "message" in err) {
          utils.fail("(" + err.message + ")");
        }
        if (err && typeof err == "object" && "stack" in err) {
          utils.fail("at " + err.stack);
        }
      });
      return result;
    } catch (x) {
      utils.fail("Error " + x + " at " + x.stack);
      return null;
    }
  };
};

/**
 * Fetch asynchronously the contents of a file using xpcom.
 *
 * Used for comparing xpcom-based results to os.file-based results.
 *
 * @param {string} path The _absolute_ path to the file.
 * @return {promise}
 * @resolves {string} The contents of the file.
 */
var reference_fetch_file = function reference_fetch_file(path, test) {
  test.info("Fetching file " + path);
  return new Promise((resolve, reject) => {
    let file = new FileUtils.File(path);
    NetUtil.asyncFetch({
      uri: NetUtil.newURI(file),
      loadUsingSystemPrincipal: true
    }, function(stream, status) {
        if (!Components.isSuccessCode(status)) {
          reject(status);
          return;
        }
        let result, reject;
        try {
          result = NetUtil.readInputStreamToString(stream, stream.available());
        } catch (x) {
          reject = x;
        }
        stream.close();
        if (reject) {
          reject(reject);
        } else {
          resolve(result);
        }
      });

  });
};

var reference_dir_contents = function reference_dir_contents(path) {
  let result = [];
  let entries = new FileUtils.File(path).directoryEntries;
  while (entries.hasMoreElements()) {
    let entry = entries.getNext().QueryInterface(Components.interfaces.nsILocalFile);
    result.push(entry.path);
  }
  return result;
};

// Set/Unset OS.Shared.DEBUG, OS.Shared.TEST and a console listener.
function toggleDebugTest (pref, consoleListener) {
  Services.prefs.setBoolPref("toolkit.osfile.log", pref);
  Services.prefs.setBoolPref("toolkit.osfile.log.redirect", pref);
  Services.console[pref ? "registerListener" : "unregisterListener"](
    consoleListener);
}

var test = maketest("Main", function main(test) {
  return (async function() {
    SimpleTest.waitForExplicitFinish();
    await test_stat();
    await test_debug();
    await test_info_features_detect();
    await test_position();
    await test_iter();
    await test_exists();
    await test_debug_test();
    info("Test is over");
    SimpleTest.finish();
  })();
});

/**
 * A file that we know exists and that can be used for reading.
 */
var EXISTING_FILE = OS.Path.join("chrome", "toolkit", "components",
  "osfile", "tests", "mochi", "main_test_osfile_async.js");

/**
 * Test OS.File.stat and OS.File.prototype.stat
 */
var test_stat = maketest("stat", function stat(test) {
  return (async function() {
    // Open a file and stat it
    let file = await OS.File.open(EXISTING_FILE);
    let stat1;

    try {
      test.info("Stating file");
      stat1 = await file.stat();
      test.ok(true, "stat has worked " + stat1);
      test.ok(stat1, "stat is not empty");
    } finally {
      await file.close();
    }

    // Stat the same file without opening it
    test.info("Stating a file without opening it");
    let stat2 = await OS.File.stat(EXISTING_FILE);
    test.ok(true, "stat 2 has worked " + stat2);
    test.ok(stat2, "stat 2 is not empty");
    for (let key in stat2) {
      test.is("" + stat1[key], "" + stat2[key], "Stat field " + key + "is the same");
    }
  })();
});

/**
 * Test feature detection using OS.File.Info.prototype on main thread
 */
var test_info_features_detect = maketest("features_detect", function features_detect(test) {
  return (async function() {
    if (OS.Constants.Win) {
      // see if winBirthDate is defined
      if ("winBirthDate" in OS.File.Info.prototype) {
        test.ok(true, "winBirthDate is defined");
      } else {
        test.fail("winBirthDate not defined though we are under Windows");
      }
    } else if (OS.Constants.libc) {
      // see if unixGroup is defined
      if ("unixGroup" in OS.File.Info.prototype) {
        test.ok(true, "unixGroup is defined");
      } else {
        test.fail("unixGroup is not defined though we are under Unix");
      }
    }
  })();
});

/**
 * Test file.{getPosition, setPosition}
 */
var test_position = maketest("position", function position(test) {
  return (async function() {
    let file = await OS.File.open(EXISTING_FILE);

    try {
      let view = await file.read();
      test.info("First batch of content read");
      let CHUNK_SIZE = 178;// An arbitrary number of bytes to read from the file
      let pos = await file.getPosition();
      test.info("Obtained position");
      test.is(pos, view.byteLength, "getPosition returned the end of the file");
      pos = await file.setPosition(-CHUNK_SIZE, OS.File.POS_END);
      test.info("Changed position");
      test.is(pos, view.byteLength - CHUNK_SIZE, "setPosition returned the correct position");

      let view2 = await file.read();
      test.info("Read the end of the file");
      for (let i = 0; i < CHUNK_SIZE; ++i) {
        if (view2[i] != view[i + view.byteLength - CHUNK_SIZE]) {
          test.is(view2[i], view[i], "setPosition put us in the right position");
        }
      }
    } finally {
      await file.close();
    }
  })();
});

/**
 * Test OS.File.prototype.{DirectoryIterator}
 */
var test_iter = maketest("iter", function iter(test) {
  return (async function() {
    let currentDir = await OS.File.getCurrentDirectory();

    // Trivial walks through the directory
    test.info("Preparing iteration");
    let iterator = new OS.File.DirectoryIterator(currentDir);
    let temporary_file_name = OS.Path.join(currentDir, "empty-temporary-file.tmp");
    try {
      await OS.File.remove(temporary_file_name);
    } catch (err) {
      // Ignore errors removing file
    }
    let allFiles1 = await iterator.nextBatch();
    test.info("Obtained all files through nextBatch");
    test.isnot(allFiles1.length, 0, "There is at least one file");
    test.isnot(allFiles1[0].path, null, "Files have a path");

    // Ensure that we have the same entries with |reference_dir_contents|
    let referenceEntries = new Set();
    for (let entry of reference_dir_contents(currentDir)) {
      referenceEntries.add(entry);
    }
    test.is(referenceEntries.size, allFiles1.length, "All the entries in the directory have been listed");
    for (let entry of allFiles1) {
      test.ok(referenceEntries.has(entry.path), "File " + entry.path + " effectively exists");
      // Ensure that we have correct isDir and isSymLink
      // Current directory is {objdir}/_tests/testing/mochitest/, assume it has some dirs and symlinks.
      var f = new FileUtils.File(entry.path);
      test.is(entry.isDir, f.isDirectory(), "Get file " + entry.path + " isDir correctly");
      test.is(entry.isSymLink, f.isSymlink(), "Get file " + entry.path + " isSymLink correctly");
    }

    await iterator.close();
    test.info("Closed iterator");

    test.info("Double closing DirectoryIterator");
    iterator = new OS.File.DirectoryIterator(currentDir);
    await iterator.close();
    await iterator.close(); //double closing |DirectoryIterator|
    test.ok(true, "|DirectoryIterator| was closed twice successfully");

    let allFiles2 = [];
    let i = 0;
    iterator = new OS.File.DirectoryIterator(currentDir);
    await iterator.forEach(function(entry, index) {
      test.is(i++, index, "Getting the correct index");
      allFiles2.push(entry);
    });
    test.info("Obtained all files through forEach");
    is(allFiles1.length, allFiles2.length, "Both runs returned the same number of files");
    for (let i = 0; i < allFiles1.length; ++i) {
      if (allFiles1[i].path != allFiles2[i].path) {
        test.is(allFiles1[i].path, allFiles2[i].path, "Both runs return the same files");
        break;
      }
    }

    // Testing batch iteration + whether an iteration can be stopped early
    let BATCH_LENGTH = 10;
    test.info("Getting some files through nextBatch");
    await iterator.close();

    iterator = new OS.File.DirectoryIterator(currentDir);
    let someFiles1 = await iterator.nextBatch(BATCH_LENGTH);
    let someFiles2 = await iterator.nextBatch(BATCH_LENGTH);
    await iterator.close();

    iterator = new OS.File.DirectoryIterator(currentDir);
    await iterator.forEach(function cb(entry, index, iterator) {
      if (index < BATCH_LENGTH) {
        test.is(entry.path, someFiles1[index].path, "Both runs return the same files (part 1)");
      } else if (index < 2*BATCH_LENGTH) {
        test.is(entry.path, someFiles2[index - BATCH_LENGTH].path, "Both runs return the same files (part 2)");
      } else if (index == 2 * BATCH_LENGTH) {
        test.info("Attempting to stop asynchronous forEach");
        return iterator.close();
      } else {
        test.fail("Can we stop an asynchronous forEach? " + index);
      }
      return null;
    });
    await iterator.close();

    // Ensuring that we find new files if they appear
    let file = await OS.File.open(temporary_file_name, { write: true } );
    file.close();
    iterator = new OS.File.DirectoryIterator(currentDir);
    try {
      let files = await iterator.nextBatch();
      is(files.length, allFiles1.length + 1, "The directory iterator has noticed the new file");
      let exists = await iterator.exists();
      test.ok(exists, "After nextBatch, iterator detects that the directory exists");
    } finally {
      await iterator.close();
    }

    // Ensuring that opening a non-existing directory fails consistently
    // once iteration starts.
    try {
      iterator = null;
      iterator = new OS.File.DirectoryIterator("/I do not exist");
      let exists = await iterator.exists();
      test.ok(!exists, "Before any iteration, iterator detects that the directory doesn't exist");
      let exn = null;
      try {
        await iterator.next();
      } catch (ex) {
        if (ex instanceof OS.File.Error && ex.becauseNoSuchFile) {
          exn = ex;
          let exists = await iterator.exists();
          test.ok(!exists, "After one iteration, iterator detects that the directory doesn't exist");
        } else {
          throw ex;
        }
      }
      test.ok(exn, "Iterating through a directory that does not exist has failed with becauseNoSuchFile");
    } finally {
      if (iterator) {
        iterator.close();
      }
    }
    test.ok(!!iterator, "The directory iterator for a non-existing directory was correctly created");
  })();
});

/**
 * Test OS.File.prototype.{exists}
 */
var test_exists = maketest("exists", function exists(test) {
  return (async function() {
    let fileExists = await OS.File.exists(EXISTING_FILE);
    test.ok(fileExists, "file exists");
    fileExists = await OS.File.exists(EXISTING_FILE + ".tmp");
    test.ok(!fileExists, "file does not exists");
  })();
});

/**
 * Test changes to OS.Shared.DEBUG flag.
 */
var test_debug = maketest("debug", function debug(test) {
  return (async function() {
    function testSetDebugPref (pref) {
      try {
        Services.prefs.setBoolPref("toolkit.osfile.log", pref);
      } catch (x) {
        test.fail("Setting OS.Shared.DEBUG to " + pref +
          " should not cause error.");
      } finally {
        test.is(OS.Shared.DEBUG, pref, "OS.Shared.DEBUG is set correctly.");
      }
    }
    testSetDebugPref(true);
    let workerDEBUG = await OS.File.GET_DEBUG();
    test.is(workerDEBUG, true, "Worker's DEBUG is set.");
    testSetDebugPref(false);
    workerDEBUG = await OS.File.GET_DEBUG();
    test.is(workerDEBUG, false, "Worker's DEBUG is unset.");
  })();
});

/**
 * Test logging in the main thread with set OS.Shared.DEBUG and
 * OS.Shared.TEST flags.
 */
var test_debug_test = maketest("debug_test", function debug_test(test) {
  return (async function() {
    // Create a console listener.
    let consoleListener = {
      observe: function (aMessage) {
        // Ignore unexpected messages.
        if (!(aMessage instanceof Components.interfaces.nsIConsoleMessage)) {
          return;
        }
        if (aMessage.message.indexOf("TEST OS") < 0) {
          return;
        }
        test.ok(true, "DEBUG TEST messages are logged correctly.");
      }
    };
    toggleDebugTest(true, consoleListener);
    // Execution of OS.File.exist method will trigger OS.File.LOG several times.
    let fileExists = await OS.File.exists(EXISTING_FILE);
    toggleDebugTest(false, consoleListener);
  })();
});
