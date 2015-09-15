Cu.import("resource://gre/modules/PlacesUtils.jsm");
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/engines/bookmarks.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");

Service.engineManager.register(BookmarksEngine);

var engine = Service.engineManager.get("bookmarks");
var store = engine._store;
var tracker = engine._tracker;

// Return a promise resolved when the specified message is written to the
// bookmarks engine log.
function promiseLogMessage(messagePortion) {
  return new Promise(resolve => {
    let appender;
    let log = Log.repository.getLogger("Sync.Engine.Bookmarks");

    function TestAppender() {
      Log.Appender.call(this);
    }
    TestAppender.prototype = Object.create(Log.Appender.prototype);
    TestAppender.prototype.doAppend = function(message) {
      if (message.indexOf(messagePortion) >= 0) {
        log.removeAppender(appender);
        resolve();
      }
    };
    TestAppender.prototype.level = Log.Level.Debug;
    appender = new TestAppender();
    log.addAppender(appender);
  });
}

// Returns a promise that resolves if the specified ID does *not* exist and
// rejects if it does.
function promiseNoItem(itemId) {
  return new Promise((resolve, reject) => {
    try {
      PlacesUtils.bookmarks.getFolderIdForItem(itemId);
      reject("fetching the item didn't fail");
    } catch (ex) {
      if (ex.result == Cr.NS_ERROR_ILLEGAL_VALUE) {
        resolve("item doesn't exist");
      } else {
        reject("unexpected exception: " + ex);
      }
    }
  });
}

add_task(function* test_ignore_invalid_uri() {
  _("Ensure that we don't die with invalid bookmarks.");

  // First create a valid bookmark.
  let bmid = PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                                  Services.io.newURI("http://example.com/", null, null),
                                                  PlacesUtils.bookmarks.DEFAULT_INDEX,
                                                  "the title");

  // Now update moz_places with an invalid url.
  yield PlacesUtils.withConnectionWrapper("test_ignore_invalid_uri", Task.async(function* (db) {
    yield db.execute(
      `UPDATE moz_places SET url = :url
       WHERE id = (SELECT b.fk FROM moz_bookmarks b
       WHERE b.id = :id LIMIT 1)`,
      { id: bmid, url: "<invalid url>" });
  }));

  // DB is now "corrupt" - setup a log appender to capture what we log.
  let promiseMessage = promiseLogMessage('Deleting bookmark with invalid URI. url="<invalid url>"');
  // This should work and log our invalid id.
  engine._buildGUIDMap();
  yield promiseMessage;
  // And we should have deleted the item.
  yield promiseNoItem(bmid);
});

add_task(function* test_ignore_missing_uri() {
  _("Ensure that we don't die with a bookmark referencing an invalid bookmark id.");

  // First create a valid bookmark.
  let bmid = PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                                  Services.io.newURI("http://example.com/", null, null),
                                                  PlacesUtils.bookmarks.DEFAULT_INDEX,
                                                  "the title");

  // Now update moz_bookmarks to reference a non-existing places ID
  yield PlacesUtils.withConnectionWrapper("test_ignore_missing_uri", Task.async(function* (db) {
    yield db.execute(
      `UPDATE moz_bookmarks SET fk = 999999
       WHERE id = :id`
      , { id: bmid });
  }));

  // DB is now "corrupt" - bookmarks will fail to locate a string url to log
  // and use "<not found>" as a placeholder.
  let promiseMessage = promiseLogMessage('Deleting bookmark with invalid URI. url="<not found>"');
  engine._buildGUIDMap();
  yield promiseMessage;
  // And we should have deleted the item.
  yield promiseNoItem(bmid);
});

function run_test() {
  initTestLogging('Trace');
  run_next_test();
}
