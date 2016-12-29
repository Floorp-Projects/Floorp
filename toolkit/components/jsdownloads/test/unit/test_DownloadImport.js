/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the DownloadImport object.
 */

"use strict";

// Globals

XPCOMUtils.defineLazyModuleGetter(this, "Sqlite",
                                  "resource://gre/modules/Sqlite.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DownloadImport",
                                  "resource://gre/modules/DownloadImport.jsm");

// Importable states
const DOWNLOAD_NOTSTARTED = -1;
const DOWNLOAD_DOWNLOADING = 0;
const DOWNLOAD_PAUSED = 4;
const DOWNLOAD_QUEUED = 5;

// Non importable states
const DOWNLOAD_FAILED = 2;
const DOWNLOAD_CANCELED = 3;
const DOWNLOAD_BLOCKED_PARENTAL = 6;
const DOWNLOAD_SCANNING = 7;
const DOWNLOAD_DIRTY = 8;
const DOWNLOAD_BLOCKED_POLICY = 9;

// The TEST_DATA_TAINTED const is a version of TEST_DATA_SHORT in which the
// beginning of the data was changed (with the TEST_DATA_REPLACEMENT value).
// We use this to test that the entityID is properly imported and the download
// can be resumed from where it was paused.
// For simplification purposes, the test requires that TEST_DATA_SHORT and
// TEST_DATA_TAINTED have the same length.
const TEST_DATA_REPLACEMENT = "-changed- ";
const TEST_DATA_TAINTED = TEST_DATA_REPLACEMENT +
                          TEST_DATA_SHORT.substr(TEST_DATA_REPLACEMENT.length);
const TEST_DATA_LENGTH = TEST_DATA_SHORT.length;

// The length of the partial file that we'll write to disk as an existing
// ongoing download.
const TEST_DATA_PARTIAL_LENGTH = TEST_DATA_REPLACEMENT.length;

// The value of the "maxBytes" column stored in the DB about the downloads.
// It's intentionally different than TEST_DATA_LENGTH to test that each value
// is seen when expected.
const MAXBYTES_IN_DB = TEST_DATA_LENGTH - 10;

var gDownloadsRowToImport;
var gDownloadsRowNonImportable;

/**
 * Creates a database with an empty moz_downloads table and leaves an
 * open connection to it.
 *
 * @param aPath
 *        String containing the path of the database file to be created.
 * @param aSchemaVersion
 *        Number with the version of the database schema to set.
 *
 * @return {Promise}
 * @resolves The open connection to the database.
 * @rejects If an error occurred during the database creation.
 */
function promiseEmptyDatabaseConnection({aPath, aSchemaVersion}) {
  return Task.spawn(function* () {
    let connection = yield Sqlite.openConnection({ path: aPath });

    yield connection.execute("CREATE TABLE moz_downloads ("
                             + "id INTEGER PRIMARY KEY,"
                             + "name TEXT,"
                             + "source TEXT,"
                             + "target TEXT,"
                             + "tempPath TEXT,"
                             + "startTime INTEGER,"
                             + "endTime INTEGER,"
                             + "state INTEGER,"
                             + "referrer TEXT,"
                             + "entityID TEXT,"
                             + "currBytes INTEGER NOT NULL DEFAULT 0,"
                             + "maxBytes INTEGER NOT NULL DEFAULT -1,"
                             + "mimeType TEXT,"
                             + "preferredApplication TEXT,"
                             + "preferredAction INTEGER NOT NULL DEFAULT 0,"
                             + "autoResume INTEGER NOT NULL DEFAULT 0,"
                             + "guid TEXT)");

    yield connection.setSchemaVersion(aSchemaVersion);

    return connection;
  });
}

/**
 * Inserts a new entry in the database with the given columns' values.
 *
 * @param aConnection
 *        The database connection.
 * @param aDownloadRow
 *        An object representing the values for each column of the row
 *        being inserted.
 *
 * @return {Promise}
 * @resolves When the operation completes.
 * @rejects If there's an error inserting the row.
 */
function promiseInsertRow(aConnection, aDownloadRow) {
  // We can't use the aDownloadRow obj directly in the execute statement
  // because the obj bind code in Sqlite.jsm doesn't allow objects
  // with extra properties beyond those being binded. So we might as well
  // use an array as it is simpler.
  let values = [
    aDownloadRow.source, aDownloadRow.target, aDownloadRow.tempPath,
    aDownloadRow.startTime.getTime() * 1000, aDownloadRow.state,
    aDownloadRow.referrer, aDownloadRow.entityID, aDownloadRow.maxBytes,
    aDownloadRow.mimeType, aDownloadRow.preferredApplication,
    aDownloadRow.preferredAction, aDownloadRow.autoResume
  ];

  return aConnection.execute("INSERT INTO moz_downloads ("
                            + "name, source, target, tempPath, startTime,"
                            + "endTime, state, referrer, entityID, currBytes,"
                            + "maxBytes, mimeType, preferredApplication,"
                            + "preferredAction, autoResume, guid)"
                            + "VALUES ("
                            + "'', ?, ?, ?, ?, " // name,
                            + "0, ?, ?, ?, 0, "  // endTime, currBytes
                            + " ?, ?, ?, "       //
                            + " ?, ?, '')",      // and guid are not imported
                            values);
}

/**
 * Retrieves the number of rows in the moz_downloads table of the
 * database.
 *
 * @param aConnection
 *        The database connection.
 *
 * @return {Promise}
 * @resolves With the number of rows.
 * @rejects Never.
 */
function promiseTableCount(aConnection) {
  return aConnection.execute("SELECT COUNT(*) FROM moz_downloads")
                    .then(res => res[0].getResultByName("COUNT(*)"))
                    .then(null, Cu.reportError);
}

/**
 * Briefly opens a network channel to a given URL to retrieve
 * the entityID of this url, as generated by the network code.
 *
 * @param aUrl
 *        The URL to retrieve the entityID.
 *
 * @return {Promise}
 * @resolves The EntityID of the given URL.
 * @rejects When there's a problem accessing the URL.
 */
function promiseEntityID(aUrl) {
  let deferred = Promise.defer();
  let entityID = "";
  let channel = NetUtil.newChannel({
    uri: NetUtil.newURI(aUrl),
    loadUsingSystemPrincipal: true
  });

  channel.asyncOpen2({
    onStartRequest: function(aRequest) {
      if (aRequest instanceof Ci.nsIResumableChannel) {
        entityID = aRequest.entityID;
      }
      aRequest.cancel(Cr.NS_BINDING_ABORTED);
    },

    onStopRequest: function(aRequest, aContext, aStatusCode) {
      if (aStatusCode == Cr.NS_BINDING_ABORTED) {
        deferred.resolve(entityID);
      } else {
        deferred.reject("Unexpected status code received");
      }
    },

    onDataAvailable: function() {}
  });

  return deferred.promise;
}

/**
 * Gets a file path to a temporary writeable download target, in the
 * correct format as expected to be stored in the downloads database,
 * which is file:///absolute/path/to/file
 *
 * @param aLeafName
 *        A hint leaf name for the file.
 *
 * @return String The path to the download target.
 */
function getDownloadTarget(aLeafName) {
  return NetUtil.newURI(getTempFile(aLeafName)).spec;
}

/**
 * Generates a temporary partial file to use as an in-progress
 * download. The file is written to disk with a part of the total expected
 * download content pre-written.
 *
 * @param aLeafName
 *        A hint leaf name for the file.
 * @param aTainted
 *        A boolean value. When true, the partial content of the file
 *        will be different from the expected content of the original source
 *        file. See the declaration of TEST_DATA_TAINTED for more information.
 *
 * @return {Promise}
 * @resolves When the operation completes, and returns a string with the path
 *           to the generated file.
 * @rejects If there's an error writing the file.
 */
function getPartialFile(aLeafName, aTainted = false) {
  let tempDownload = getTempFile(aLeafName);
  let partialContent = aTainted
                     ? TEST_DATA_TAINTED.substr(0, TEST_DATA_PARTIAL_LENGTH)
                     : TEST_DATA_SHORT.substr(0, TEST_DATA_PARTIAL_LENGTH);

  return OS.File.writeAtomic(tempDownload.path, partialContent,
                             { tmpPath: tempDownload.path + ".tmp",
                               flush: true })
                .then(() => tempDownload.path);
}

/**
 * Generates a Date object to be used as the startTime for the download rows
 * in the DB. A date that is obviously different from the current time is
 * generated to make sure this stored data and a `new Date()` can't collide.
 *
 * @param aOffset
 *        A offset from the base generated date is used to differentiate each
 *        row in the database.
 *
 * @return A Date object.
 */
function getStartTime(aOffset) {
  return new Date(1000000 + (aOffset * 10000));
}

/**
 * Performs various checks on an imported Download object to make sure
 * all properties are properly set as expected from the import procedure.
 *
 * @param aDownload
 *        The Download object to be checked.
 * @param aDownloadRow
 *        An object that represents a row from the original database table,
 *        with extra properties describing expected values that are not
 *        explictly part of the database.
 *
 * @return {Promise}
 * @resolves When the operation completes
 * @rejects Never
 */
function checkDownload(aDownload, aDownloadRow) {
  return Task.spawn(function*() {
    do_check_eq(aDownload.source.url, aDownloadRow.source);
    do_check_eq(aDownload.source.referrer, aDownloadRow.referrer);

    do_check_eq(aDownload.target.path,
                NetUtil.newURI(aDownloadRow.target)
                       .QueryInterface(Ci.nsIFileURL).file.path);

    do_check_eq(aDownload.target.partFilePath, aDownloadRow.tempPath);

    if (aDownloadRow.expectedResume) {
      do_check_true(!aDownload.stopped || aDownload.succeeded);
      yield promiseDownloadStopped(aDownload);

      do_check_true(aDownload.succeeded);
      do_check_eq(aDownload.progress, 100);
      // If the download has resumed, a new startTime will be set.
      // By calling toJSON we're also testing that startTime is a Date object.
      do_check_neq(aDownload.startTime.toJSON(),
                   aDownloadRow.startTime.toJSON());
    } else {
      do_check_false(aDownload.succeeded);
      do_check_eq(aDownload.startTime.toJSON(),
                  aDownloadRow.startTime.toJSON());
    }

    do_check_eq(aDownload.stopped, true);

    let serializedSaver = aDownload.saver.toSerializable();
    if (typeof(serializedSaver) == "object") {
      do_check_eq(serializedSaver.type, "copy");
    } else {
      do_check_eq(serializedSaver, "copy");
    }

    if (aDownloadRow.entityID) {
      do_check_eq(aDownload.saver.entityID, aDownloadRow.entityID);
    }

    do_check_eq(aDownload.currentBytes, aDownloadRow.expectedCurrentBytes);
    do_check_eq(aDownload.totalBytes, aDownloadRow.expectedTotalBytes);

    if (aDownloadRow.expectedContent) {
      let fileToCheck = aDownloadRow.expectedResume
                        ? aDownload.target.path
                        : aDownload.target.partFilePath;
      yield promiseVerifyContents(fileToCheck, aDownloadRow.expectedContent);
    }

    do_check_eq(aDownload.contentType, aDownloadRow.expectedContentType);
    do_check_eq(aDownload.launcherPath, aDownloadRow.preferredApplication);

    do_check_eq(aDownload.launchWhenSucceeded,
                aDownloadRow.preferredAction != Ci.nsIMIMEInfo.saveToDisk);
  });
}

// Preparation tasks

/**
 * Prepares the list of downloads to be added to the database that should
 * be imported by the import procedure.
 */
add_task(function* prepareDownloadsToImport() {

  let sourceUrl = httpUrl("source.txt");
  let sourceEntityId = yield promiseEntityID(sourceUrl);

  gDownloadsRowToImport = [
    // Paused download with autoResume and a partial file. By
    // setting the correct entityID the download can resume from
    // where it stopped, and to test that this works properly we
    // intentionally set different data in the beginning of the
    // partial file to make sure it was not replaced.
    {
      source: sourceUrl,
      target: getDownloadTarget("inprogress1.txt"),
      tempPath: yield getPartialFile("inprogress1.txt.part", true),
      startTime: getStartTime(1),
      state: DOWNLOAD_PAUSED,
      referrer: httpUrl("referrer1"),
      entityID: sourceEntityId,
      maxBytes: MAXBYTES_IN_DB,
      mimeType: "mimeType1",
      preferredAction: Ci.nsIMIMEInfo.saveToDisk,
      preferredApplication: "prerredApplication1",
      autoResume: 1,

      // Even though the information stored in the DB said
      // maxBytes was MAXBYTES_IN_DB, the download turned out to be
      // a different length. Here we make sure the totalBytes property
      // was correctly set with the actual value. The same consideration
      // applies to the contentType.
      expectedCurrentBytes: TEST_DATA_LENGTH,
      expectedTotalBytes: TEST_DATA_LENGTH,
      expectedResume: true,
      expectedContentType: "text/plain",
      expectedContent: TEST_DATA_TAINTED,
    },

    // Paused download with autoResume and a partial file,
    // but missing entityID. This means that the download will
    // start from beginning, and the entire original content of the
    // source file should replace the different data that was stored
    // in the partial file.
    {
      source: sourceUrl,
      target: getDownloadTarget("inprogress2.txt"),
      tempPath: yield getPartialFile("inprogress2.txt.part", true),
      startTime: getStartTime(2),
      state: DOWNLOAD_PAUSED,
      referrer: httpUrl("referrer2"),
      entityID: "",
      maxBytes: MAXBYTES_IN_DB,
      mimeType: "mimeType2",
      preferredAction: Ci.nsIMIMEInfo.saveToDisk,
      preferredApplication: "prerredApplication2",
      autoResume: 1,

      expectedCurrentBytes: TEST_DATA_LENGTH,
      expectedTotalBytes: TEST_DATA_LENGTH,
      expectedResume: true,
      expectedContentType: "text/plain",
      expectedContent: TEST_DATA_SHORT
    },

    // Paused download with no autoResume and a partial file.
    {
      source: sourceUrl,
      target: getDownloadTarget("inprogress3.txt"),
      tempPath: yield getPartialFile("inprogress3.txt.part"),
      startTime: getStartTime(3),
      state: DOWNLOAD_PAUSED,
      referrer: httpUrl("referrer3"),
      entityID: "",
      maxBytes: MAXBYTES_IN_DB,
      mimeType: "mimeType3",
      preferredAction: Ci.nsIMIMEInfo.saveToDisk,
      preferredApplication: "prerredApplication3",
      autoResume: 0,

      // Since this download has not been resumed, the actual data
      // about its total size and content type is not known.
      // Therefore, we're going by the information imported from the DB.
      expectedCurrentBytes: TEST_DATA_PARTIAL_LENGTH,
      expectedTotalBytes: MAXBYTES_IN_DB,
      expectedResume: false,
      expectedContentType: "mimeType3",
      expectedContent: TEST_DATA_SHORT.substr(0, TEST_DATA_PARTIAL_LENGTH),
    },

    // Paused download with autoResume and no partial file.
    {
      source: sourceUrl,
      target: getDownloadTarget("inprogress4.txt"),
      tempPath: "",
      startTime: getStartTime(4),
      state: DOWNLOAD_PAUSED,
      referrer: httpUrl("referrer4"),
      entityID: "",
      maxBytes: MAXBYTES_IN_DB,
      mimeType: "text/plain",
      preferredAction: Ci.nsIMIMEInfo.useHelperApp,
      preferredApplication: "prerredApplication4",
      autoResume: 1,

      expectedCurrentBytes: TEST_DATA_LENGTH,
      expectedTotalBytes: TEST_DATA_LENGTH,
      expectedResume: true,
      expectedContentType: "text/plain",
      expectedContent: TEST_DATA_SHORT
    },

    // Paused download with no autoResume and no partial file.
    {
      source: sourceUrl,
      target: getDownloadTarget("inprogress5.txt"),
      tempPath: "",
      startTime: getStartTime(5),
      state: DOWNLOAD_PAUSED,
      referrer: httpUrl("referrer4"),
      entityID: "",
      maxBytes: MAXBYTES_IN_DB,
      mimeType: "text/plain",
      preferredAction: Ci.nsIMIMEInfo.useSystemDefault,
      preferredApplication: "prerredApplication5",
      autoResume: 0,

      expectedCurrentBytes: 0,
      expectedTotalBytes: MAXBYTES_IN_DB,
      expectedResume: false,
      expectedContentType: "text/plain",
    },

    // Queued download with no autoResume and no partial file.
    // Even though autoResume=0, queued downloads always autoResume.
    {
      source: sourceUrl,
      target: getDownloadTarget("inprogress6.txt"),
      tempPath: "",
      startTime: getStartTime(6),
      state: DOWNLOAD_QUEUED,
      referrer: httpUrl("referrer6"),
      entityID: "",
      maxBytes: MAXBYTES_IN_DB,
      mimeType: "text/plain",
      preferredAction: Ci.nsIMIMEInfo.useHelperApp,
      preferredApplication: "prerredApplication6",
      autoResume: 0,

      expectedCurrentBytes: TEST_DATA_LENGTH,
      expectedTotalBytes: TEST_DATA_LENGTH,
      expectedResume: true,
      expectedContentType: "text/plain",
      expectedContent: TEST_DATA_SHORT
    },

    // Notstarted download with no autoResume and no partial file.
    // Even though autoResume=0, notstarted downloads always autoResume.
    {
      source: sourceUrl,
      target: getDownloadTarget("inprogress7.txt"),
      tempPath: "",
      startTime: getStartTime(7),
      state: DOWNLOAD_NOTSTARTED,
      referrer: httpUrl("referrer7"),
      entityID: "",
      maxBytes: MAXBYTES_IN_DB,
      mimeType: "text/plain",
      preferredAction: Ci.nsIMIMEInfo.useHelperApp,
      preferredApplication: "prerredApplication7",
      autoResume: 0,

      expectedCurrentBytes: TEST_DATA_LENGTH,
      expectedTotalBytes: TEST_DATA_LENGTH,
      expectedResume: true,
      expectedContentType: "text/plain",
      expectedContent: TEST_DATA_SHORT
    },

    // Downloading download with no autoResume and a partial file.
    // Even though autoResume=0, downloading downloads always autoResume.
    {
      source: sourceUrl,
      target: getDownloadTarget("inprogress8.txt"),
      tempPath: yield getPartialFile("inprogress8.txt.part", true),
      startTime: getStartTime(8),
      state: DOWNLOAD_DOWNLOADING,
      referrer: httpUrl("referrer8"),
      entityID: sourceEntityId,
      maxBytes: MAXBYTES_IN_DB,
      mimeType: "text/plain",
      preferredAction: Ci.nsIMIMEInfo.saveToDisk,
      preferredApplication: "prerredApplication8",
      autoResume: 0,

      expectedCurrentBytes: TEST_DATA_LENGTH,
      expectedTotalBytes: TEST_DATA_LENGTH,
      expectedResume: true,
      expectedContentType: "text/plain",
      expectedContent: TEST_DATA_TAINTED
    },
  ];
});

/**
 * Prepares the list of downloads to be added to the database that should
 * *not* be imported by the import procedure.
 */
add_task(function* prepareNonImportableDownloads()
{
  gDownloadsRowNonImportable = [
    // Download with no source (should never happen in normal circumstances).
    {
      source: "",
      target: "nonimportable1.txt",
      tempPath: "",
      startTime: getStartTime(1),
      state: DOWNLOAD_PAUSED,
      referrer: "",
      entityID: "",
      maxBytes: MAXBYTES_IN_DB,
      mimeType: "mimeType1",
      preferredAction: Ci.nsIMIMEInfo.saveToDisk,
      preferredApplication: "prerredApplication1",
      autoResume: 1
    },

    // state = DOWNLOAD_FAILED
    {
      source: httpUrl("source.txt"),
      target: "nonimportable2.txt",
      tempPath: "",
      startTime: getStartTime(2),
      state: DOWNLOAD_FAILED,
      referrer: "",
      entityID: "",
      maxBytes: MAXBYTES_IN_DB,
      mimeType: "mimeType2",
      preferredAction: Ci.nsIMIMEInfo.saveToDisk,
      preferredApplication: "prerredApplication2",
      autoResume: 1
    },

    // state = DOWNLOAD_CANCELED
    {
      source: httpUrl("source.txt"),
      target: "nonimportable3.txt",
      tempPath: "",
      startTime: getStartTime(3),
      state: DOWNLOAD_CANCELED,
      referrer: "",
      entityID: "",
      maxBytes: MAXBYTES_IN_DB,
      mimeType: "mimeType3",
      preferredAction: Ci.nsIMIMEInfo.saveToDisk,
      preferredApplication: "prerredApplication3",
      autoResume: 1
    },

    // state = DOWNLOAD_BLOCKED_PARENTAL
    {
      source: httpUrl("source.txt"),
      target: "nonimportable4.txt",
      tempPath: "",
      startTime: getStartTime(4),
      state: DOWNLOAD_BLOCKED_PARENTAL,
      referrer: "",
      entityID: "",
      maxBytes: MAXBYTES_IN_DB,
      mimeType: "mimeType4",
      preferredAction: Ci.nsIMIMEInfo.saveToDisk,
      preferredApplication: "prerredApplication4",
      autoResume: 1
    },

    // state = DOWNLOAD_SCANNING
    {
      source: httpUrl("source.txt"),
      target: "nonimportable5.txt",
      tempPath: "",
      startTime: getStartTime(5),
      state: DOWNLOAD_SCANNING,
      referrer: "",
      entityID: "",
      maxBytes: MAXBYTES_IN_DB,
      mimeType: "mimeType5",
      preferredAction: Ci.nsIMIMEInfo.saveToDisk,
      preferredApplication: "prerredApplication5",
      autoResume: 1
    },

    // state = DOWNLOAD_DIRTY
    {
      source: httpUrl("source.txt"),
      target: "nonimportable6.txt",
      tempPath: "",
      startTime: getStartTime(6),
      state: DOWNLOAD_DIRTY,
      referrer: "",
      entityID: "",
      maxBytes: MAXBYTES_IN_DB,
      mimeType: "mimeType6",
      preferredAction: Ci.nsIMIMEInfo.saveToDisk,
      preferredApplication: "prerredApplication6",
      autoResume: 1
    },

    // state = DOWNLOAD_BLOCKED_POLICY
    {
      source: httpUrl("source.txt"),
      target: "nonimportable7.txt",
      tempPath: "",
      startTime: getStartTime(7),
      state: DOWNLOAD_BLOCKED_POLICY,
      referrer: "",
      entityID: "",
      maxBytes: MAXBYTES_IN_DB,
      mimeType: "mimeType7",
      preferredAction: Ci.nsIMIMEInfo.saveToDisk,
      preferredApplication: "prerredApplication7",
      autoResume: 1
    },
  ];
});

// Test

/**
 * Creates a temporary Sqlite database with download data and perform an
 * import of that data to the new Downloads API to verify that the import
 * worked correctly.
 */
add_task(function* test_downloadImport()
{
  let connection = null;
  let downloadsSqlite = getTempFile("downloads.sqlite").path;

  try {
    // Set up the database.
    connection = yield promiseEmptyDatabaseConnection({
      aPath: downloadsSqlite,
      aSchemaVersion: 9
    });

    // Insert both the importable and non-importable
    // downloads together.
    for (let downloadRow of gDownloadsRowToImport) {
      yield promiseInsertRow(connection, downloadRow);
    }

    for (let downloadRow of gDownloadsRowNonImportable) {
      yield promiseInsertRow(connection, downloadRow);
    }

    // Check that every item was inserted.
    do_check_eq((yield promiseTableCount(connection)),
                gDownloadsRowToImport.length +
                gDownloadsRowNonImportable.length);
  } finally {
    // Close the connection so that DownloadImport can open it.
    yield connection.close();
  }

  // Import items.
  let list = yield promiseNewList(false);
  yield new DownloadImport(list, downloadsSqlite).import();
  let items = yield list.getAll();

  do_check_eq(items.length, gDownloadsRowToImport.length);

  for (let i = 0; i < gDownloadsRowToImport.length; i++) {
    yield checkDownload(items[i], gDownloadsRowToImport[i]);
  }
})
