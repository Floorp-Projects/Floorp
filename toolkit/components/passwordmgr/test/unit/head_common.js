// Copied from components/places/tests/unit/head_bookmarks.js

const NS_APP_USER_PROFILE_50_DIR = "ProfD";
const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;

// Various functions common to the tests.
const LoginTest = {

  /*
   * initStorage
   *
   */
  initStorage : function (aOutputPathName, aOutputFileName, aExpectedError) {
    var err = null;

    var newStorage = this.newStorage();

    var outputFile = null;
    if (aOutputFileName) {
        var outputFile = Cc["@mozilla.org/file/local;1"].
                         createInstance(Ci.nsILocalFile);
        outputFile.initWithPath(aOutputPathName);
        outputFile.append(aOutputFileName);

        // Delete any existing output file. This is primarily for Windows,
        // where we can't rely on having deleted files in the last test run.
        if (outputFile.exists())
            outputFile.remove(false);
    }

    try {
        newStorage.initWithFile(outputFile);
    } catch (e) {
        err = e;
    }

    this.checkExpectedError(aExpectedError, err);

    return newStorage;
  },


  /*
   * reloadStorage
   *
   * Reinitialize a storage module with the specified input.
   */
  reloadStorage : function (aInputPathName, aInputFileName, aExpectedError) {
    var err = null;
    var newStorage = this.newStorage();

    var inputFile = null;
    if (aInputFileName) {
        var inputFile  = Cc["@mozilla.org/file/local;1"].
                         createInstance(Ci.nsILocalFile);
        inputFile.initWithPath(aInputPathName);
        inputFile.append(aInputFileName);
    }

    try {
        newStorage.initWithFile(inputFile);
    } catch (e) {
        err = e;
    }

    if (aExpectedError)
        this.checkExpectedError(aExpectedError, err);
    else
        do_check_true(err == null);

    return newStorage;
  },


  /*
   * checkExpectedError
   *
   * Checks to see if a thrown error was expected or not, and if it
   * matches the expected value.
   */
  checkExpectedError : function (aExpectedError, aActualError) {
    if (aExpectedError) {
        if (!aActualError)
            throw "Test didn't throw as expected (" + aExpectedError + ")";

        if (!aExpectedError.test(aActualError))
            throw "Test threw (" + aActualError + "), not (" + aExpectedError;

        // We got the expected error, so make a note in the test log.
        dump("...that error was expected.\n\n");
    } else if (aActualError) {
        throw "Test threw unexpected error: " + aActualError;
    }
  },


  /*
   * checkStorageData
   *
   * Compare info from component to what we expected.
   */
  checkStorageData : function (storage, ref_disabledHosts, ref_logins) {
    this.checkLogins(ref_logins, storage.getAllLogins());
    this.checkDisabledHosts(ref_disabledHosts, storage.getAllDisabledHosts());
  },

  /*
   * checkLogins
   *
   * Check values of the logins list.
   */
  checkLogins : function (expectedLogins, actualLogins) {
    do_check_eq(expectedLogins.length, actualLogins.length);
    for (let i = 0; i < expectedLogins.length; i++) {
        let found = false;
        for (let j = 0; !found && j < actualLogins.length; j++) {
            found = expectedLogins[i].equals(actualLogins[j]);
        }
        do_check_true(found);
    }
  },

  /*
   * checkDisabledHosts
   *
   * Check values of the disabled list.
   */
  checkDisabledHosts : function (expectedHosts, actualHosts) {
    do_check_eq(expectedHosts.length, actualHosts.length);
    for (let i = 0; i < expectedHosts.length; i++) {
        let found = false;
        for (let j = 0; !found && j < actualHosts.length; j++) {
            found = (expectedHosts[i] == actualHosts[j]);
        }
        do_check_true(found);
    }
  },

  /*
   * countLinesInFile
   *
   * Counts the number of lines in the specified file.
   */
  countLinesInFile : function (aPathName,  aFileName) {
    var inputFile  = Cc["@mozilla.org/file/local;1"].
                     createInstance(Ci.nsILocalFile);
    inputFile.initWithPath(aPathName);
    inputFile.append(aFileName);
    if (inputFile.fileSize == 0)
      return 0;

    var inputStream = Cc["@mozilla.org/network/file-input-stream;1"].
                      createInstance(Ci.nsIFileInputStream);
    // init the stream as RD_ONLY, -1 == default permissions.
    inputStream.init(inputFile, 0x01, -1, null);
    var lineStream = inputStream.QueryInterface(Ci.nsILineInputStream);

    var line = { value : null };
    var lineCount = 1; // Empty files were dealt with above.
    while (lineStream.readLine(line))
        lineCount++;

    return lineCount;
  },

  newStorage : function () {

    var storage = Cc["@mozilla.org/login-manager/storage/mozStorage;1"].
                  createInstance(Ci.nsILoginManagerStorage);
    if (!storage)
      throw "Couldn't create storage instance.";
    return storage;
  },

  openDB : function (filename) {
    // nsIFile for the specified filename, in the profile dir.
    var dbfile = PROFDIR.clone();
    dbfile.append(filename);

    var ss = Cc["@mozilla.org/storage/service;1"].
             getService(Ci.mozIStorageService);
    var dbConnection = ss.openDatabase(dbfile);

    return dbConnection;
  },

  deleteFile : function (pathname, filename) {
    var file = Cc["@mozilla.org/file/local;1"].
               createInstance(Ci.nsILocalFile);
    file.initWithPath(pathname);
    file.append(filename);
    // Suppress failures, this happens in the mozstorage tests on Windows
    // because the module may still be holding onto the DB. (We don't
    // have a way to explicitly shutdown/GC the module).
    try {
      if (file.exists())
        file.remove(false);
    } catch (e) {}
  },

  // Copies a file from our test data directory to the unit test profile.
  copyFile : function (filename) {
    var file = DATADIR.clone();
    file.append(filename);

    var profileFile = PROFDIR.clone();
    profileFile.append(filename);

    if (profileFile.exists())
        profileFile.remove(false);

    file.copyTo(PROFDIR, filename);
  },

  // Returns true if the timestamp is within 30 seconds of now.
  is_about_now : function (timestamp) {
    var delta = Math.abs(timestamp - Date.now());
    var seconds = 30 * 1000;
    return delta < seconds;
  }
};

// nsIFiles...
var PROFDIR = do_get_profile();
var DATADIR = do_get_file("data/");
// string versions...
var OUTDIR = PROFDIR.path;

// Copy key3.db into the profile used for the unit tests. Need this so we can
// decrypt the encrypted logins stored in the various tests inputs.
LoginTest.copyFile("key3.db");
