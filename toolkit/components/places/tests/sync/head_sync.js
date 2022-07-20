/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Import common head.
{
  /* import-globals-from ../head_common.js */
  let commonFile = do_get_file("../head_common.js", false);
  let uri = Services.io.newFileURI(commonFile);
  Services.scriptloader.loadSubScript(uri.spec, this);
}

// Put any other stuff relative to this test folder below.

var { CanonicalJSON } = ChromeUtils.import(
  "resource://gre/modules/CanonicalJSON.jsm"
);
var { Log } = ChromeUtils.import("resource://gre/modules/Log.jsm");
var { ObjectUtils } = ChromeUtils.import(
  "resource://gre/modules/ObjectUtils.jsm"
);
var { PlacesSyncUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/PlacesSyncUtils.sys.mjs"
);
var { SyncedBookmarksMirror } = ChromeUtils.importESModule(
  "resource://gre/modules/SyncedBookmarksMirror.sys.mjs"
);
var { CommonUtils } = ChromeUtils.import("resource://services-common/utils.js");
var { FileTestUtils } = ChromeUtils.import(
  "resource://testing-common/FileTestUtils.jsm"
);
var {
  HTTP_400,
  HTTP_401,
  HTTP_402,
  HTTP_403,
  HTTP_404,
  HTTP_405,
  HTTP_406,
  HTTP_407,
  HTTP_408,
  HTTP_409,
  HTTP_410,
  HTTP_411,
  HTTP_412,
  HTTP_413,
  HTTP_414,
  HTTP_415,
  HTTP_417,
  HTTP_500,
  HTTP_501,
  HTTP_502,
  HTTP_503,
  HTTP_504,
  HTTP_505,
  HttpError,
  HttpServer,
} = ChromeUtils.import("resource://testing-common/httpd.js");

// These titles are defined in Database::CreateBookmarkRoots
const BookmarksMenuTitle = "menu";
const BookmarksToolbarTitle = "toolbar";
const UnfiledBookmarksTitle = "unfiled";
const MobileBookmarksTitle = "mobile";

function run_test() {
  let bufLog = Log.repository.getLogger("Sync.Engine.Bookmarks.Mirror");
  bufLog.level = Log.Level.All;

  let sqliteLog = Log.repository.getLogger("Sqlite");
  sqliteLog.level = Log.Level.Error;

  let formatter = new Log.BasicFormatter();
  let appender = new Log.DumpAppender(formatter);
  appender.level = Log.Level.All;

  for (let log of [bufLog, sqliteLog]) {
    log.addAppender(appender);
  }

  do_get_profile();
  run_next_test();
}

// A test helper to insert local roots directly into Places, since the public
// bookmarks APIs no longer support custom roots.
async function insertLocalRoot({ guid, title }) {
  await PlacesUtils.withConnectionWrapper("insertLocalRoot", async function(
    db
  ) {
    let dateAdded = PlacesUtils.toPRTime(new Date());
    await db.execute(
      `
        INSERT INTO moz_bookmarks(guid, type, parent, position, title,
                                  dateAdded, lastModified)
        VALUES(:guid, :type, (SELECT id FROM moz_bookmarks
                              WHERE guid = :parentGuid),
               (SELECT COUNT(*) FROM moz_bookmarks
                WHERE parent = (SELECT id FROM moz_bookmarks
                                WHERE guid = :parentGuid)),
               :title, :dateAdded, :dateAdded)`,
      {
        guid,
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        parentGuid: PlacesUtils.bookmarks.rootGuid,
        title,
        dateAdded,
      }
    );
  });
}

// Returns a `CryptoWrapper`-like object that wraps the Sync record cleartext.
// This exists to avoid importing `record.js` from Sync.
function makeRecord(cleartext) {
  return new Proxy(
    { cleartext },
    {
      get(target, property, receiver) {
        if (property == "cleartext") {
          return target.cleartext;
        }
        if (property == "cleartextToString") {
          return () => JSON.stringify(target.cleartext);
        }
        return target.cleartext[property];
      },
      set(target, property, value, receiver) {
        if (property == "cleartext") {
          target.cleartext = value;
        } else if (property != "cleartextToString") {
          target.cleartext[property] = value;
        }
      },
      has(target, property) {
        return property == "cleartext" || property in target.cleartext;
      },
      deleteProperty(target, property) {},
      ownKeys(target) {
        return ["cleartext", ...Reflect.ownKeys(target)];
      },
    }
  );
}

async function storeRecords(buf, records, options) {
  await buf.store(records.map(makeRecord), options);
}

async function storeChangesInMirror(buf, changesToUpload) {
  let cleartexts = [];
  for (let recordId in changesToUpload) {
    changesToUpload[recordId].synced = true;
    cleartexts.push(changesToUpload[recordId].cleartext);
  }
  await storeRecords(buf, cleartexts, { needsMerge: false });
  await PlacesSyncUtils.bookmarks.pushChanges(changesToUpload);
}

function inspectChangeRecords(changeRecords) {
  let results = { updated: [], deleted: [] };
  for (let [id, record] of Object.entries(changeRecords)) {
    (record.tombstone ? results.deleted : results.updated).push(id);
  }
  results.updated.sort();
  results.deleted.sort();
  return results;
}

async function promiseManyDatesAdded(guids) {
  let datesAdded = new Map();
  let db = await PlacesUtils.promiseDBConnection();
  for (let chunk of PlacesUtils.chunkArray(guids, 100)) {
    let rows = await db.executeCached(
      `
      SELECT guid, dateAdded FROM moz_bookmarks
      WHERE guid IN (${new Array(chunk.length).fill("?").join(",")})`,
      chunk
    );
    if (rows.length != chunk.length) {
      throw new TypeError("Can't fetch date added for nonexistent items");
    }
    for (let row of rows) {
      let dateAdded = row.getResultByName("dateAdded") / 1000;
      datesAdded.set(row.getResultByName("guid"), dateAdded);
    }
  }
  return datesAdded;
}

async function fetchLocalTree(rootGuid) {
  function bookmarkNodeToInfo(node) {
    let { guid, index, title, typeCode: type } = node;
    let itemInfo = { guid, index, title, type };
    if (node.annos) {
      let syncableAnnos = node.annos.filter(anno =>
        [PlacesUtils.LMANNO_FEEDURI, PlacesUtils.LMANNO_SITEURI].includes(
          anno.name
        )
      );
      if (syncableAnnos.length) {
        itemInfo.annos = syncableAnnos;
      }
    }
    if (node.uri) {
      itemInfo.url = node.uri;
    }
    if (node.keyword) {
      itemInfo.keyword = node.keyword;
    }
    if (node.children) {
      itemInfo.children = node.children.map(bookmarkNodeToInfo);
    }
    if (node.tags) {
      itemInfo.tags = node.tags.split(",").sort();
    }
    return itemInfo;
  }
  let root = await PlacesUtils.promiseBookmarksTree(rootGuid);
  return bookmarkNodeToInfo(root);
}

async function assertLocalTree(rootGuid, expected, message) {
  let actual = await fetchLocalTree(rootGuid);
  if (!ObjectUtils.deepEqual(actual, expected)) {
    info(
      `Expected structure for ${rootGuid}: ${CanonicalJSON.stringify(expected)}`
    );
    info(
      `Actual structure for ${rootGuid}:   ${CanonicalJSON.stringify(actual)}`
    );
    throw new Assert.constructor.AssertionError({ actual, expected, message });
  }
}

function makeLivemarkServer() {
  let server = new HttpServer();
  server.registerPrefixHandler("/feed/", do_get_file("./livemark.xml"));
  server.start(-1);
  return {
    server,
    get site() {
      let { identity } = server;
      let host = identity.primaryHost.includes(":")
        ? `[${identity.primaryHost}]`
        : identity.primaryHost;
      return `${identity.primaryScheme}://${host}:${identity.primaryPort}`;
    },
    stopServer() {
      return new Promise(resolve => server.stop(resolve));
    },
  };
}

function shuffle(array) {
  let results = [];
  for (let i = 0; i < array.length; ++i) {
    let randomIndex = Math.floor(Math.random() * (i + 1));
    results[i] = results[randomIndex];
    results[randomIndex] = array[i];
  }
  return results;
}

async function fetchAllKeywords(info) {
  let entries = [];
  await PlacesUtils.keywords.fetch(info, entry => entries.push(entry));
  return entries;
}

async function openMirror(name, options = {}) {
  let buf = await SyncedBookmarksMirror.open({
    path: `${name}_buf.sqlite`,
    recordStepTelemetry(...args) {
      if (options.recordStepTelemetry) {
        options.recordStepTelemetry.call(this, ...args);
      }
    },
    recordValidationTelemetry(...args) {
      if (options.recordValidationTelemetry) {
        options.recordValidationTelemetry.call(this, ...args);
      }
    },
  });
  return buf;
}

function BookmarkObserver({ ignoreDates = true, skipTags = false } = {}) {
  this.notifications = [];
  this.ignoreDates = ignoreDates;
  this.skipTags = skipTags;
  this.handlePlacesEvents = this.handlePlacesEvents.bind(this);
}

BookmarkObserver.prototype = {
  handlePlacesEvents(events) {
    for (let event of events) {
      switch (event.type) {
        case "bookmark-added": {
          if (this.skipTags && event.isTagging) {
            continue;
          }
          let params = {
            itemId: event.id,
            parentId: event.parentId,
            index: event.index,
            type: event.itemType,
            urlHref: event.url,
            title: event.title,
            guid: event.guid,
            parentGuid: event.parentGuid,
            source: event.source,
          };
          if (!this.ignoreDates) {
            params.dateAdded = event.dateAdded;
          }
          this.notifications.push({ name: "bookmark-added", params });
          break;
        }
        case "bookmark-removed": {
          if (this.skipTags && event.isTagging) {
            continue;
          }
          // Since we are now skipping tags on the listener side we don't
          // prevent unTagging notifications from going out. These events cause empty
          // tags folders to be removed which creates another bookmark-removed notification
          if (
            this.skipTags &&
            event.parentGuid == PlacesUtils.bookmarks.tagsGuid
          ) {
            continue;
          }
          let params = {
            itemId: event.id,
            parentId: event.parentId,
            index: event.index,
            type: event.itemType,
            urlHref: event.url || null,
            title: event.title,
            guid: event.guid,
            parentGuid: event.parentGuid,
            source: event.source,
          };
          this.notifications.push({ name: "bookmark-removed", params });
          break;
        }
        case "bookmark-moved": {
          const params = {
            itemId: event.id,
            type: event.itemType,
            urlHref: event.url,
            source: event.source,
            guid: event.guid,
            newIndex: event.index,
            newParentGuid: event.parentGuid,
            oldIndex: event.oldIndex,
            oldParentGuid: event.oldParentGuid,
            isTagging: event.isTagging,
          };
          this.notifications.push({ name: "bookmark-moved", params });
          break;
        }
        case "bookmark-guid-changed": {
          const params = {
            itemId: event.id,
            type: event.itemType,
            urlHref: event.url,
            guid: event.guid,
            parentGuid: event.parentGuid,
            source: event.source,
            isTagging: event.isTagging,
          };
          this.notifications.push({ name: "bookmark-guid-changed", params });
          break;
        }
        case "bookmark-title-changed": {
          const params = {
            itemId: event.id,
            guid: event.guid,
            title: event.title,
            parentGuid: event.parentGuid,
          };
          this.notifications.push({ name: "bookmark-title-changed", params });
          break;
        }
        case "bookmark-url-changed": {
          const params = {
            itemId: event.id,
            type: event.itemType,
            urlHref: event.url,
            guid: event.guid,
            parentGuid: event.parentGuid,
            source: event.source,
            isTagging: event.isTagging,
          };
          this.notifications.push({ name: "bookmark-url-changed", params });
          break;
        }
      }
    }
  },
  onItemChanged(
    itemId,
    property,
    isAnnoProperty,
    newValue,
    lastModified,
    type,
    parentId,
    guid,
    parentGuid,
    oldValue,
    source
  ) {
    let params = {
      itemId,
      property,
      isAnnoProperty,
      newValue,
      type,
      parentId,
      guid,
      parentGuid,
      oldValue,
      source,
    };
    if (!this.ignoreDates) {
      params.lastModified = lastModified;
    }
    this.notifications.push({ name: "onItemChanged", params });
  },

  QueryInterface: ChromeUtils.generateQI(["nsINavBookmarkObserver"]),

  check(expectedNotifications) {
    PlacesUtils.bookmarks.removeObserver(this);
    PlacesUtils.observers.removeListener(
      [
        "bookmark-added",
        "bookmark-removed",
        "bookmark-moved",
        "bookmark-guid-changed",
        "bookmark-title-changed",
        "bookmark-url-changed",
      ],
      this.handlePlacesEvents
    );
    if (!ObjectUtils.deepEqual(this.notifications, expectedNotifications)) {
      info(`Expected notifications: ${JSON.stringify(expectedNotifications)}`);
      info(`Actual notifications: ${JSON.stringify(this.notifications)}`);
      throw new Assert.constructor.AssertionError({
        actual: this.notifications,
        expected: expectedNotifications,
      });
    }
  },
};

function expectBookmarkChangeNotifications(options) {
  let observer = new BookmarkObserver(options);
  PlacesUtils.bookmarks.addObserver(observer);
  PlacesUtils.observers.addListener(
    [
      "bookmark-added",
      "bookmark-removed",
      "bookmark-moved",
      "bookmark-guid-changed",
      "bookmark-title-changed",
      "bookmark-url-changed",
    ],
    observer.handlePlacesEvents
  );
  return observer;
}

// Copies a support file to a temporary fixture file, allowing the support
// file to be reused for multiple tests.
async function setupFixtureFile(fixturePath) {
  let fixtureFile = do_get_file(fixturePath);
  let tempFile = FileTestUtils.getTempFile(fixturePath);
  await IOUtils.copy(fixtureFile.path, tempFile.path);
  return tempFile;
}
