/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that Sync can correctly handle a legacy microsummary record

Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/engines/bookmarks.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-common/log4moz.js");
Cu.import("resource://services-sync/util.js");

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/PlacesUtils.jsm");

const GENERATORURI_ANNO = "microsummary/generatorURI";
const STATICTITLE_ANNO = "bookmarks/staticTitle";

const TEST_URL = "http://micsum.mozilla.org/";
const TEST_TITLE = "A microsummarized bookmark"
const GENERATOR_URL = "http://generate.micsum/"
const STATIC_TITLE = "Static title"

function newMicrosummary(url, title) {
  let id = PlacesUtils.bookmarks.insertBookmark(
    PlacesUtils.unfiledBookmarksFolderId, NetUtil.newURI(url),
    PlacesUtils.bookmarks.DEFAULT_INDEX, title
  );
  PlacesUtils.annotations.setItemAnnotation(id, GENERATORURI_ANNO,
                                            GENERATOR_URL, 0,
                                            PlacesUtils.annotations.EXPIRE_NEVER);
  PlacesUtils.annotations.setItemAnnotation(id, STATICTITLE_ANNO,
                                            "Static title", 0,
                                            PlacesUtils.annotations.EXPIRE_NEVER);
  return id;
}

function run_test() {

  Engines.register(BookmarksEngine);
  let engine = Engines.get("bookmarks");
  let store = engine._store;

  // Clean up.
  store.wipe();

  initTestLogging("Trace");
  Log4Moz.repository.getLogger("Sync.Engine.Bookmarks").level = Log4Moz.Level.Trace;

  _("Create a microsummarized bookmark.");
  let id = newMicrosummary(TEST_URL, TEST_TITLE);
  let guid = store.GUIDForId(id);
  _("GUID: " + guid);
  do_check_true(!!guid);

  _("Create record object and verify that it's sane.");
  let record = store.createRecord(guid);
  do_check_true(record instanceof Bookmark);
  do_check_eq(record.bmkUri, TEST_URL);

  _("Make sure the new record does not carry the microsummaries annotations.");
  do_check_false("staticTitle" in record);
  do_check_false("generatorUri" in record);

  _("Remove the bookmark from Places.");
  PlacesUtils.bookmarks.removeItem(id);

  _("Convert record to the old microsummaries one.");
  record.staticTitle = STATIC_TITLE;
  record.generatorUri = GENERATOR_URL;
  record.type = "microsummary";

  _("Apply the modified record as incoming data.");
  store.applyIncoming(record);

  _("Verify it has been created correctly as a simple Bookmark.");
  id = store.idForGUID(record.id);
  do_check_eq(store.GUIDForId(id), record.id);
  do_check_eq(PlacesUtils.bookmarks.getItemType(id),
              PlacesUtils.bookmarks.TYPE_BOOKMARK);
  do_check_eq(PlacesUtils.bookmarks.getBookmarkURI(id).spec, TEST_URL);
  do_check_eq(PlacesUtils.bookmarks.getItemTitle(id), TEST_TITLE);
  do_check_eq(PlacesUtils.bookmarks.getFolderIdForItem(id),
              PlacesUtils.unfiledBookmarksFolderId);
  do_check_eq(PlacesUtils.bookmarks.getKeywordForBookmark(id), null);

  do_check_throws(
    function () PlacesUtils.annotations.getItemAnnotation(id, GENERATORURI_ANNO),
    Cr.NS_ERROR_NOT_AVAILABLE
  );

  do_check_throws(
    function () PlacesUtils.annotations.getItemAnnotation(id, STATICTITLE_ANNO),
    Cr.NS_ERROR_NOT_AVAILABLE
  );

  // Clean up.
  store.wipe();
}
