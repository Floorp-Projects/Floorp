"use strict";

Components.utils.import("resource://gre/modules/osfile.jsm");
Components.utils.import("resource://gre/modules/Promise.jsm");
Components.utils.import("resource://gre/modules/Task.jsm");
Components.utils.import("resource://gre/modules/AsyncShutdown.jsm");

// The following are used to compare against a well-tested reference
// implementation of file I/O.
Components.utils.import("resource://gre/modules/NetUtil.jsm");
Components.utils.import("resource://gre/modules/FileUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

let myok = ok;
let myis = is;
let myinfo = info;
let myisnot = isnot;

let isPromise = function ispromise(value) {
  return value != null && typeof value == "object" && "then" in value;
};

let maketest = function(prefix, test) {
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
let reference_fetch_file = function reference_fetch_file(path, test) {
  test.info("Fetching file " + path);
  let promise = Promise.defer();
  let file = new FileUtils.File(path);
  NetUtil.asyncFetch(file,
    function(stream, status) {
      if (!Components.isSuccessCode(status)) {
        promise.reject(status);
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
        promise.reject(reject);
      } else {
        promise.resolve(result);
      }
  });
  return promise.promise;
};

/**
 * Compare asynchronously the contents two files using xpcom.
 *
 * Used for comparing xpcom-based results to os.file-based results.
 *
 * @param {string} a The _absolute_ path to the first file.
 * @param {string} b The _absolute_ path to the second file.
 *
 * @resolves {null}
 */
let reference_compare_files = function reference_compare_files(a, b, test) {
  test.info("Comparing files " + a + " and " + b);
  let a_contents = yield reference_fetch_file(a, test);
  let b_contents = yield reference_fetch_file(b, test);
  is(a_contents, b_contents, "Contents of files " + a + " and " + b + " match");
};

let reference_dir_contents = function reference_dir_contents(path) {
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

let test = maketest("Main", function main(test) {
  return Task.spawn(function() {
    SimpleTest.waitForExplicitFinish();
    yield test_constants();
    yield test_path();
    yield test_stat();
    yield test_debug();
    yield test_info_features_detect();
    yield test_read_write();
    yield test_read_write_all();
    yield test_position();
    yield test_iter();
    yield test_exists();
    yield test_debug_test();
    yield test_duration();
    info("Test is over");
    SimpleTest.finish();
  });
});

/**
 * A file that we know exists and that can be used for reading.
 */
let EXISTING_FILE = OS.Path.join("chrome", "toolkit", "components",
  "osfile", "tests", "mochi", "main_test_osfile_async.js");

/**
 * Test that OS.Constants is defined correctly.
 */
let test_constants = maketest("constants", function constants(test) {
  return Task.spawn(function() {
    test.isnot(OS.Constants, null, "OS.Constants exists");
    test.ok(OS.Constants.Win || OS.Constants.libc, "OS.Constants.Win exists or OS.Constants.Unix exists");
    test.isnot(OS.Constants.Path, null, "OS.Constants.Path exists");
    test.isnot(OS.Constants.Sys, null, "OS.Constants.Sys exists");
  });
});

/**
 * Test that OS.Constants.Path paths are consistent.
 */
let test_path = maketest("path",  function path(test) {
  return Task.spawn(function() {
    test.ok(OS.Path, "OS.Path exists");
    test.ok(OS.Constants.Path, "OS.Constants.Path exists");
    test.is(OS.Constants.Path.tmpDir, Services.dirsvc.get("TmpD", Components.interfaces.nsIFile).path, "OS.Constants.Path.tmpDir is correct");
    test.is(OS.Constants.Path.profileDir, Services.dirsvc.get("ProfD", Components.interfaces.nsIFile).path, "OS.Constants.Path.profileDir is correct");
    test.is(OS.Constants.Path.localProfileDir, Services.dirsvc.get("ProfLD", Components.interfaces.nsIFile).path, "OS.Constants.Path.localProfileDir is correct");
  });
});

/**
 * Test OS.File.stat and OS.File.prototype.stat
 */
let test_stat = maketest("stat", function stat(test) {
  return Task.spawn(function() {
    // Open a file and stat it
    let file = yield OS.File.open(EXISTING_FILE);
    let stat1;

    try {
      test.info("Stating file");
      stat1 = yield file.stat();
      test.ok(true, "stat has worked " + stat1);
      test.ok(stat1, "stat is not empty");
    } finally {
      yield file.close();
    }

    // Stat the same file without opening it
    test.info("Stating a file without opening it");
    let stat2 = yield OS.File.stat(EXISTING_FILE);
    test.ok(true, "stat 2 has worked " + stat2);
    test.ok(stat2, "stat 2 is not empty");
    for (let key in stat2) {
      test.is("" + stat1[key], "" + stat2[key], "Stat field " + key + "is the same");
    }
  });
});

/**
 * Test feature detection using OS.File.Info.prototype on main thread
 */
let test_info_features_detect = maketest("features_detect", function features_detect(test) {
  return Task.spawn(function() {
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
  });
});

/**
 * Test OS.File.prototype.{read, readTo, write}
 */
let test_read_write = maketest("read_write", function read_write(test) {
  return Task.spawn(function() {
    // Test readTo/write
    let currentDir = yield OS.File.getCurrentDirectory();
    let pathSource = OS.Path.join(currentDir, EXISTING_FILE);
    let pathDest = OS.Path.join(OS.Constants.Path.tmpDir,
      "osfile async test.tmp");

    let fileSource = yield OS.File.open(pathSource);
    test.info("Input file opened");
    let fileDest = yield OS.File.open(pathDest,
      { truncate: true, read: true, write: true});
    test.info("Output file opened");

    let stat = yield fileSource.stat();
    test.info("Input stat worked");
    let size = stat.size;
    let array = new Uint8Array(size);

    try {
      test.info("Now calling readTo");
      let readLength = yield fileSource.readTo(array);
      test.info("ReadTo worked");
      test.is(readLength, size, "ReadTo got all bytes");
      let writeLength = yield fileDest.write(array);
      test.info("Write worked");
      test.is(writeLength, size, "Write wrote all bytes");

      // Test read
      yield fileSource.setPosition(0);
      let readAllResult = yield fileSource.read();
      test.info("ReadAll worked");
      test.is(readAllResult.length, size, "ReadAll read all bytes");
      test.is(Array.prototype.join.call(readAllResult),
              Array.prototype.join.call(array),
              "ReadAll result is correct");
    } finally {
      // Close stuff
      yield fileSource.close();
      yield fileDest.close();
      test.info("Files are closed");
    }

    stat = yield OS.File.stat(pathDest);
    test.is(stat.size, size, "Both files have the same size");
    yield reference_compare_files(pathSource, pathDest, test);

    // Cleanup.
    OS.File.remove(pathDest);
  });
});

/**
 * Test OS.File.writeAtomic
 */
let test_read_write_all = maketest("read_write_all", function read_write_all(test) {
  return Task.spawn(function() {
    let pathDest = OS.Path.join(OS.Constants.Path.tmpDir,
      "osfile async test read writeAtomic.tmp");
    let tmpPath = pathDest + ".tmp";

    let test_with_options = function(options, suffix) {
      return Task.spawn(function() {
        let optionsBackup = JSON.parse(JSON.stringify(options));

        // Check that read + writeAtomic performs a correct copy
        let currentDir = yield OS.File.getCurrentDirectory();
        let pathSource = OS.Path.join(currentDir, EXISTING_FILE);
        let contents = yield OS.File.read(pathSource);
        test.ok(contents, "Obtained contents");
        let bytesWritten = yield OS.File.writeAtomic(pathDest, contents, options);
        test.is(contents.byteLength, bytesWritten, "Wrote the correct number of bytes (" + suffix + ")");

        // Check that options are not altered
        test.is(Object.keys(options).length, Object.keys(optionsBackup).length,
          "The number of options was not changed");
        for (let k in options) {
          test.is(options[k], optionsBackup[k], "Option was not changed (" + suffix + ")");
        }
        yield reference_compare_files(pathSource, pathDest, test);

        // Check that temporary file was removed or doesn't exist
        test.info("Compare complete");
        test.ok(!(new FileUtils.File(tmpPath).exists()), "No temporary file at the end of the run (" + suffix + ")");

        // Check that writeAtomic fails if noOverwrite is true and the destination
        // file already exists!
        let view = new Uint8Array(contents.buffer, 10, 200);
        try {
          let opt = JSON.parse(JSON.stringify(options));
          opt.noOverwrite = true;
          yield OS.File.writeAtomic(pathDest, view, opt);
          test.fail("With noOverwrite, writeAtomic should have refused to overwrite file (" + suffix + ")");
        } catch (err) {
          test.info("With noOverwrite, writeAtomic correctly failed (" + suffix + ")");
          test.ok(err instanceof OS.File.Error, "writeAtomic correctly failed with a file error (" + suffix + ")");
          test.ok(err.becauseExists, "writeAtomic file error confirmed that the file already exists (" + suffix + ")");
        }
        yield reference_compare_files(pathSource, pathDest, test);
        test.ok(!(new FileUtils.File(tmpPath).exists()), "Temporary file was removed");

        // Now write a subset
        let START = 10;
        let LENGTH = 100;
        view = new Uint8Array(contents.buffer, START, LENGTH);
        bytesWritten = yield OS.File.writeAtomic(pathDest, view, options);
        test.is(bytesWritten, LENGTH, "Partial write wrote the correct number of bytes (" + suffix + ")");
        let array2 = yield OS.File.read(pathDest);
        let view1 = new Uint8Array(contents.buffer, START, LENGTH);
        test.is(view1.length, array2.length, "Re-read partial write with the correct number of bytes (" + suffix + ")");
        let decoder = new TextDecoder();
        test.is(decoder.decode(view1), decoder.decode(array2), "Comparing re-read of partial write (" + suffix + ")");

        // Write strings, default encoding
        let ARBITRARY_STRING = "aeiouyâêîôûçß•";
        yield OS.File.writeAtomic(pathDest, ARBITRARY_STRING, options);
        let array = yield OS.File.read(pathDest);
        let IN_STRING = decoder.decode(array);
        test.is(ARBITRARY_STRING, IN_STRING, "String write + read with default encoding works (" + suffix + ")");

        let opt16 = JSON.parse(JSON.stringify(options));
        opt16.encoding = "utf-16";
        yield OS.File.writeAtomic(pathDest, ARBITRARY_STRING, opt16);
        array = yield OS.File.read(pathDest);
        IN_STRING = (new TextDecoder("utf-16")).decode(array);
        test.is(ARBITRARY_STRING, IN_STRING, "String write + read with utf-16 encoding works (" + suffix + ")");

        // Cleanup.
        OS.File.remove(pathDest);
      });
    };

    yield test_with_options({tmpPath: tmpPath}, "Renaming, not flushing");
    yield test_with_options({tmpPath: tmpPath, flush: true}, "Renaming, flushing");
    yield test_with_options({}, "Not renaming, not flushing");
    yield test_with_options({flush: true}, "Not renaming, flushing");
  });
});

/**
 * Test file.{getPosition, setPosition}
 */
let test_position = maketest("position", function position(test) {
  return Task.spawn(function() {
    let file = yield OS.File.open(EXISTING_FILE);

    try {
      let stat = yield file.stat();
      test.info("Obtained file length");

      let view = new Uint8Array(stat.size);
      yield file.readTo(view);
      test.info("First batch of content read");

      let CHUNK_SIZE = 178;// An arbitrary number of bytes to read from the file
      let pos = yield file.getPosition();
      test.info("Obtained position");
      test.is(pos, view.byteLength, "getPosition returned the end of the file");
      pos = yield file.setPosition(-CHUNK_SIZE, OS.File.POS_END);
      test.info("Changed position");
      test.is(pos, view.byteLength - CHUNK_SIZE, "setPosition returned the correct position");

      let view2 = new Uint8Array(CHUNK_SIZE);
      yield file.readTo(view2);
      test.info("Read the end of the file");
      for (let i = 0; i < CHUNK_SIZE; ++i) {
        if (view2[i] != view[i + view.byteLength - CHUNK_SIZE]) {
          test.is(view2[i], view[i], "setPosition put us in the right position");
        }
      }
    } finally {
      yield file.close();
    }
  });
});

/**
 * Test OS.File.prototype.{DirectoryIterator}
 */
let test_iter = maketest("iter", function iter(test) {
  return Task.spawn(function() {
    let currentDir = yield OS.File.getCurrentDirectory();

    // Trivial walks through the directory
    test.info("Preparing iteration");
    let iterator = new OS.File.DirectoryIterator(currentDir);
    let temporary_file_name = OS.Path.join(currentDir, "empty-temporary-file.tmp");
    try {
      yield OS.File.remove(temporary_file_name);
    } catch (err) {
      // Ignore errors removing file
    }
    let allFiles1 = yield iterator.nextBatch();
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

    yield iterator.close();
    test.info("Closed iterator");

    test.info("Double closing DirectoryIterator");
    iterator = new OS.File.DirectoryIterator(currentDir);
    yield iterator.close();
    yield iterator.close(); //double closing |DirectoryIterator|
    test.ok(true, "|DirectoryIterator| was closed twice successfully");

    let allFiles2 = [];
    let i = 0;
    iterator = new OS.File.DirectoryIterator(currentDir);
    yield iterator.forEach(function(entry, index) {
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
    yield iterator.close();

    iterator = new OS.File.DirectoryIterator(currentDir);
    let someFiles1 = yield iterator.nextBatch(BATCH_LENGTH);
    let someFiles2 = yield iterator.nextBatch(BATCH_LENGTH);
    yield iterator.close();

    iterator = new OS.File.DirectoryIterator(currentDir);
    yield iterator.forEach(function cb(entry, index, iterator) {
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
    yield iterator.close();

    // Ensuring that we find new files if they appear
    let file = yield OS.File.open(temporary_file_name, { write: true } );
    file.close();
    iterator = new OS.File.DirectoryIterator(currentDir);
    try {
      let files = yield iterator.nextBatch();
      is(files.length, allFiles1.length + 1, "The directory iterator has noticed the new file");
      let exists = yield iterator.exists();
      test.ok(exists, "After nextBatch, iterator detects that the directory exists");
    } finally {
      yield iterator.close();
    }

    // Ensuring that opening a non-existing directory fails consistently
    // once iteration starts.
    try {
      iterator = null;
      iterator = new OS.File.DirectoryIterator("/I do not exist");
      let exists = yield iterator.exists();
      test.ok(!exists, "Before any iteration, iterator detects that the directory doesn't exist");
      let exn = null;
      try {
        yield iterator.next();
      } catch (ex if ex instanceof OS.File.Error && ex.becauseNoSuchFile) {
        exn = ex;
        let exists = yield iterator.exists();
        test.ok(!exists, "After one iteration, iterator detects that the directory doesn't exist");
      }
      test.ok(exn, "Iterating through a directory that does not exist has failed with becauseNoSuchFile");
    } finally {
      if (iterator) {
        iterator.close();
      }
    }
    test.ok(!!iterator, "The directory iterator for a non-existing directory was correctly created");
  });
});

/**
 * Test OS.File.prototype.{exists}
 */
let test_exists = maketest("exists", function exists(test) {
  return Task.spawn(function() {
    let fileExists = yield OS.File.exists(EXISTING_FILE);
    test.ok(fileExists, "file exists");
    fileExists = yield OS.File.exists(EXISTING_FILE + ".tmp");
    test.ok(!fileExists, "file does not exists");
  });
});

/**
 * Test changes to OS.Shared.DEBUG flag.
 */
let test_debug = maketest("debug", function debug(test) {
  return Task.spawn(function() {
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
    let workerDEBUG = yield OS.File.GET_DEBUG();
    test.is(workerDEBUG, true, "Worker's DEBUG is set.");
    testSetDebugPref(false);
    workerDEBUG = yield OS.File.GET_DEBUG();
    test.is(workerDEBUG, false, "Worker's DEBUG is unset.");
  });
});

/**
 * Test logging in the main thread with set OS.Shared.DEBUG and
 * OS.Shared.TEST flags.
 */
let test_debug_test = maketest("debug_test", function debug_test(test) {
  return Task.spawn(function () {
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
    let fileExists = yield OS.File.exists(EXISTING_FILE);
    toggleDebugTest(false, consoleListener);
  });
});


/**
 * Test optional duration reporting that can be used for telemetry.
 */
let test_duration = maketest("duration", function duration(test) {
  return Task.spawn(function() {
    Services.prefs.setBoolPref("toolkit.osfile.log", true);
    // Options structure passed to a OS.File copy method.
    let copyOptions = {
      // This field should be overridden with the actual duration
      // measurement.
      outExecutionDuration: null
    };
    let currentDir = yield OS.File.getCurrentDirectory();
    let pathSource = OS.Path.join(currentDir, EXISTING_FILE);
    let copyFile = pathSource + ".bak";
    let testOptions = function testOptions(options, name) {
      test.info("Checking outExecutionDuration for operation: " + name);
      test.info(name + ": Gathered method duration time: " +
        options.outExecutionDuration + "ms");
      // Making sure that duration was updated.
      test.ok(typeof options.outExecutionDuration === "number",
              name + ": Operation duration is a number");
      test.ok(options.outExecutionDuration >= 0,
              name + ": Operation duration time is non-negative.");
    };
    // Testing duration of OS.File.copy.
    yield OS.File.copy(pathSource, copyFile, copyOptions);
    testOptions(copyOptions, "OS.File.copy");
    yield OS.File.remove(copyFile);

    // Trying an operation where options are cloned.
    let pathDest = OS.Path.join(OS.Constants.Path.tmpDir,
      "osfile async test read writeAtomic.tmp");
    let tmpPath = pathDest + ".tmp";
    let readOptions = {
      outExecutionDuration: null
    };
    let contents = yield OS.File.read(pathSource, undefined, readOptions);
    testOptions(readOptions, "OS.File.read");
    // Options structure passed to a OS.File writeAtomic method.
    let writeAtomicOptions = {
      // This field should be first initialized with the actual
      // duration measurement then progressively incremented.
      outExecutionDuration: null,
      tmpPath: tmpPath
    };
    yield OS.File.writeAtomic(pathDest, contents, writeAtomicOptions);
    testOptions(writeAtomicOptions, "OS.File.writeAtomic");
    yield OS.File.remove(pathDest);

    test.info("Ensuring that we can use outExecutionDuration to accumulate durations");

    let ARBITRARY_BASE_DURATION = 5;
    copyOptions = {
      // This field should now be incremented with the actual duration
      // measurement.
      outExecutionDuration: ARBITRARY_BASE_DURATION
    };
    let backupDuration = ARBITRARY_BASE_DURATION;
    // Testing duration of OS.File.copy.
    yield OS.File.copy(pathSource, copyFile, copyOptions);
    test.ok(copyOptions.outExecutionDuration >= backupDuration, "duration has increased 1");

    backupDuration = copyOptions.outExecutionDuration;
    yield OS.File.remove(copyFile, copyOptions);
    test.ok(copyOptions.outExecutionDuration >= backupDuration, "duration has increased 2");

    // Trying an operation where options are cloned.
    // Options structure passed to a OS.File writeAtomic method.
    writeAtomicOptions = {
      // This field should be overridden with the actual duration
      // measurement.
      outExecutionDuration: copyOptions.outExecutionDuration,
      tmpPath: tmpPath
    };
    backupDuration = writeAtomicOptions.outExecutionDuration;

    yield OS.File.writeAtomic(pathDest, contents, writeAtomicOptions);
    test.ok(copyOptions.outExecutionDuration >= backupDuration, "duration has increased 3");
    OS.File.remove(pathDest);

    // Testing an operation that doesn't take arguments at all
    let file = yield OS.File.open(pathSource);
    yield file.stat();
    yield file.close();
  });
});
