/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["HistoryEngine", "HistoryRec"];

const HISTORY_TTL = 5184000; // 60 days in milliseconds
const THIRTY_DAYS_IN_MS = 2592000000; // 30 days in milliseconds

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://services-common/async.js");
ChromeUtils.import("resource://services-common/utils.js");
ChromeUtils.import("resource://services-sync/constants.js");
ChromeUtils.import("resource://services-sync/engines.js");
ChromeUtils.import("resource://services-sync/record.js");
ChromeUtils.import("resource://services-sync/util.js");

ChromeUtils.defineModuleGetter(this, "PlacesUtils",
                               "resource://gre/modules/PlacesUtils.jsm");

ChromeUtils.defineModuleGetter(this, "PlacesSyncUtils",
                               "resource://gre/modules/PlacesSyncUtils.jsm");

function HistoryRec(collection, id) {
  CryptoWrapper.call(this, collection, id);
}
HistoryRec.prototype = {
  __proto__: CryptoWrapper.prototype,
  _logName: "Sync.Record.History",
  ttl: HISTORY_TTL
};

Utils.deferGetSet(HistoryRec, "cleartext", ["histUri", "title", "visits"]);


function HistoryEngine(service) {
  SyncEngine.call(this, "History", service);
}
HistoryEngine.prototype = {
  __proto__: SyncEngine.prototype,
  _recordObj: HistoryRec,
  _storeObj: HistoryStore,
  _trackerObj: HistoryTracker,
  downloadLimit: MAX_HISTORY_DOWNLOAD,

  syncPriority: 7,

  _migratedSyncMetadata: false,
  async _migrateSyncMetadata() {
    if (this._migratedSyncMetadata) {
      return;
    }
    // Migrate the history sync ID and last sync time from prefs, to avoid
    // triggering a full sync on upgrade. This can be removed in bug 1443021.
    let existingSyncID = await super.getSyncID();
    if (existingSyncID) {
      this._log.debug("Migrating existing sync ID ${existingSyncID} from prefs",
                      { existingSyncID });
      await PlacesSyncUtils.history.ensureCurrentSyncId(existingSyncID);
    }
    let existingLastSync = await super.getLastSync();
    if (existingLastSync) {
      this._log.debug("Migrating existing last sync time ${existingLastSync} " +
                      "from prefs", { existingLastSync });
      await PlacesSyncUtils.history.setLastSync(existingLastSync);
    }
    this._migratedSyncMetadata = true;
  },

  async getSyncID() {
    return PlacesSyncUtils.history.getSyncId();
  },

  async ensureCurrentSyncID(newSyncID) {
    this._log.debug("Checking if server sync ID ${newSyncID} matches existing",
                    { newSyncID });
    await PlacesSyncUtils.history.ensureCurrentSyncId(newSyncID);
    await super.ensureCurrentSyncID(newSyncID); // Remove in bug 1443021.
    return newSyncID;
  },

  async resetSyncID() {
    // First, delete the collection on the server. It's fine if we're
    // interrupted here: on the next sync, we'll detect that our old sync ID is
    // now stale, and start over as a first sync.
    await this._deleteServerCollection();
    // Then, reset our local sync ID.
    return this.resetLocalSyncID();
  },

  async resetLocalSyncID() {
    let newSyncID = await PlacesSyncUtils.history.resetSyncId();
    this._log.debug("Assigned new sync ID ${newSyncID}", { newSyncID });
    await super.ensureCurrentSyncID(newSyncID); // Remove in bug 1443021.
    return newSyncID;
  },

  async getLastSync() {
    let lastSync = await PlacesSyncUtils.history.getLastSync();
    return lastSync;
  },

  async setLastSync(lastSync) {
    await PlacesSyncUtils.history.setLastSync(lastSync);
    await super.setLastSync(lastSync); // Remove in bug 1443021.
  },

  async _syncStartup() {
    await this._migrateSyncMetadata();
    await super._syncStartup();
  },

  shouldSyncURL(url) {
    return !url.startsWith("file:");
  },

  async pullNewChanges() {
    const changedIDs = await this._tracker.getChangedIDs();
    let modifiedGUIDs = Object.keys(changedIDs);
    if (!modifiedGUIDs.length) {
      return {};
    }

    let guidsToRemove = await PlacesSyncUtils.history.determineNonSyncableGuids(modifiedGUIDs);
    await this._tracker.removeChangedID(...guidsToRemove);
    return changedIDs;
  },

  async _resetClient() {
    await super._resetClient();
    await PlacesSyncUtils.history.reset();
  },
};

function HistoryStore(name, engine) {
  Store.call(this, name, engine);
}

HistoryStore.prototype = {
  __proto__: Store.prototype,

  // We try and only update this many visits at one time.
  MAX_VISITS_PER_INSERT: 500,

  // Some helper functions to handle GUIDs
  async setGUID(uri, guid) {

    if (!guid) {
      guid = Utils.makeGUID();
    }

    try {
      await PlacesSyncUtils.history.changeGuid(uri, guid);
    } catch (e) {
      this._log.error("Error setting GUID ${guid} for URI ${uri}", guid, uri);
    }

    return guid;
  },

  async GUIDForUri(uri, create) {

    // Use the existing GUID if it exists
    let guid;
    try {
      guid = await PlacesSyncUtils.history.fetchGuidForURL(uri);
    } catch (e) {
      this._log.error("Error fetching GUID for URL ${uri}", uri);
    }

    // If the URI has an existing GUID, return it.
    if (guid) {
      return guid;
    }

    // If the URI doesn't have a GUID and we were indicated to create one.
    if (create) {
      return this.setGUID(uri);
    }

    // If the URI doesn't have a GUID and we didn't create one for it.
    return null;
  },

  async changeItemID(oldID, newID) {
    let info = await PlacesSyncUtils.history.fetchURLInfoForGuid(oldID);
    if (!info) {
      throw new Error(`Can't change ID for nonexistent history entry ${oldID}`);
    }
    this.setGUID(info.url, newID);
  },

  async getAllIDs() {
    let urls = await PlacesSyncUtils.history.getAllURLs({ since: new Date((Date.now() - THIRTY_DAYS_IN_MS)), limit: MAX_HISTORY_UPLOAD });

    let urlsByGUID = {};
    for (let url of urls) {
      if (!this.engine.shouldSyncURL(url)) {
        continue;
      }
      let guid = await this.GUIDForUri(url, true);
      urlsByGUID[guid] = url;
    }
    return urlsByGUID;
  },

  async applyIncomingBatch(records) {
    // Convert incoming records to mozIPlaceInfo objects which are applied as
    // either history additions or removals.
    let failed = [];
    let toAdd = [];
    let toRemove = [];
    for await (let record of Async.yieldingIterator(records)) {
      if (record.deleted) {
        toRemove.push(record);
      } else {
        try {
          if (await this._recordToPlaceInfo(record)) {
            toAdd.push(record);
          }
        } catch (ex) {
          if (Async.isShutdownException(ex)) {
            throw ex;
          }
          this._log.error("Failed to create a place info", ex);
          this._log.trace("The record that failed", record);
          failed.push(record.id);
        }
      }
    }
    if (toAdd.length || toRemove.length) {
      // We want to notify history observers that a batch operation is underway
      // so they don't do lots of work for each incoming record.
      let observers = PlacesUtils.history.getObservers();
      const notifyHistoryObservers = (notification) => {
        for (let observer of observers) {
          try {
            observer[notification]();
          } catch (ex) {
            // don't log an error - it's not our code that failed and we don't
            // want an error log written just for this.
            this._log.info("history observer failed", ex);
          }
        }
      };
      notifyHistoryObservers("onBeginUpdateBatch");
      try {
        if (toRemove.length) {
          // PlacesUtils.history.remove takes an array of visits to remove,
          // but the error semantics are tricky - a single "bad" entry will cause
          // an exception before anything is removed. So we do remove them one at
          // a time.
          for await (let record of Async.yieldingIterator(toRemove)) {
            try {
              await this.remove(record);
            } catch (ex) {
              if (Async.isShutdownException(ex)) {
                throw ex;
              }
              this._log.error("Failed to delete a place info", ex);
              this._log.trace("The record that failed", record);
              failed.push(record.id);
            }
          }
        }
        for (let chunk of this._generateChunks(toAdd)) {
          // Per bug 1415560, we ignore any exceptions returned by insertMany
          // as they are likely to be spurious. We do supply an onError handler
          // and log the exceptions seen there as they are likely to be
          // informative, but we still never abort the sync based on them.
          try {
            await PlacesUtils.history.insertMany(chunk, null, failedVisit => {
              this._log.info("Failed to insert a history record", failedVisit.guid);
              this._log.trace("The record that failed", failedVisit);
              failed.push(failedVisit.guid);
            });
          } catch (ex) {
            this._log.info("Failed to insert history records", ex);
          }
        }
      } finally {
        notifyHistoryObservers("onEndUpdateBatch");
      }
    }

    return failed;
  },

  /**
   * Returns a generator that splits records into sanely sized chunks suitable
   * for passing to places to prevent places doing bad things at shutdown.
   */
  * _generateChunks(records) {
    // We chunk based on the number of *visits* inside each record. However,
    // we do not split a single record into multiple records, because at some
    // time in the future, we intend to ensure these records are ordered by
    // lastModified, and advance the engine's timestamp as we process them,
    // meaning we can resume exactly where we left off next sync - although
    // currently that's not done, so we will retry the entire batch next sync
    // if interrupted.
    // ie, this means that if a single record has more than MAX_VISITS_PER_INSERT
    // visits, we will call insertMany() with exactly 1 record, but with
    // more than MAX_VISITS_PER_INSERT visits.
    let curIndex = 0;
    this._log.debug(`adding ${records.length} records to history`);
    while (curIndex < records.length) {
      Async.checkAppReady(); // may throw if we are shutting down.
      let toAdd = []; // what we are going to insert.
      let count = 0; // a counter which tells us when toAdd is full.
      do {
        let record = records[curIndex];
        curIndex += 1;
        toAdd.push(record);
        count += record.visits.length;
      } while (curIndex < records.length &&
               count + records[curIndex].visits.length <= this.MAX_VISITS_PER_INSERT);
      this._log.trace(`adding ${toAdd.length} items in this chunk`);
      yield toAdd;
    }
  },

  /* An internal helper to determine if we can add an entry to places.
     Exists primarily so tests can override it.
   */
  _canAddURI(uri) {
    return PlacesUtils.history.canAddURI(uri);
  },

  /**
   * Converts a Sync history record to a mozIPlaceInfo.
   *
   * Throws if an invalid record is encountered (invalid URI, etc.),
   * returns true if the record is to be applied, false otherwise
   * (no visits to add, etc.),
   */
  async _recordToPlaceInfo(record) {
    // Sort out invalid URIs and ones Places just simply doesn't want.
    record.url = PlacesUtils.normalizeToURLOrGUID(record.histUri);
    record.uri = CommonUtils.makeURI(record.histUri);

    if (!Utils.checkGUID(record.id)) {
      this._log.warn("Encountered record with invalid GUID: " + record.id);
      return false;
    }
    record.guid = record.id;

    if (!this._canAddURI(record.uri) ||
        !this.engine.shouldSyncURL(record.uri.spec)) {
      this._log.trace("Ignoring record " + record.id + " with URI "
                      + record.uri.spec + ": can't add this URI.");
      return false;
    }

    // We dupe visits by date and type. So an incoming visit that has
    // the same timestamp and type as a local one won't get applied.
    // To avoid creating new objects, we rewrite the query result so we
    // can simply check for containment below.
    let curVisitsAsArray = [];
    let curVisits = new Set();
    try {
      curVisitsAsArray = await PlacesSyncUtils.history.fetchVisitsForURL(record.histUri);
    } catch (e) {
      this._log.error("Error while fetching visits for URL ${record.histUri}", record.histUri);
    }
    let oldestAllowed = PlacesSyncUtils.bookmarks.EARLIEST_BOOKMARK_TIMESTAMP;
    if (curVisitsAsArray.length == 20) {
      let oldestVisit = curVisitsAsArray[curVisitsAsArray.length - 1];
      oldestAllowed = PlacesSyncUtils.history.clampVisitDate(
        PlacesUtils.toDate(oldestVisit.date).getTime());
    }

    let i, k;
    for (i = 0; i < curVisitsAsArray.length; i++) {
      // Same logic as used in the loop below to generate visitKey.
      let {date, type} = curVisitsAsArray[i];
      let dateObj = PlacesUtils.toDate(date);
      let millis = PlacesSyncUtils.history.clampVisitDate(dateObj).getTime();
      curVisits.add(`${millis},${type}`);
    }

    // Walk through the visits, make sure we have sound data, and eliminate
    // dupes. The latter is done by rewriting the array in-place.
    for (i = 0, k = 0; i < record.visits.length; i++) {
      let visit = record.visits[k] = record.visits[i];

      if (!visit.date || typeof visit.date != "number" || !Number.isInteger(visit.date)) {
        this._log.warn("Encountered record with invalid visit date: "
                       + visit.date);
        continue;
      }

      if (!visit.type ||
          !Object.values(PlacesUtils.history.TRANSITIONS).includes(visit.type)) {
        this._log.warn("Encountered record with invalid visit type: " +
                       visit.type + "; ignoring.");
        continue;
      }

      // Dates need to be integers. Future and far past dates are clamped to the
      // current date and earliest sensible date, respectively.
      let originalVisitDate = PlacesUtils.toDate(Math.round(visit.date));
      visit.date = PlacesSyncUtils.history.clampVisitDate(originalVisitDate);

      if (visit.date.getTime() < oldestAllowed) {
        // Visit is older than the oldest visit we have, and we have so many
        // visits for this uri that we hit our limit when inserting.
        continue;
      }
      let visitKey = `${visit.date.getTime()},${visit.type}`;
      if (curVisits.has(visitKey)) {
        // Visit is a dupe, don't increment 'k' so the element will be
        // overwritten.
        continue;
      }

      // Note the visit key, so that we don't add duplicate visits with
      // clamped timestamps.
      curVisits.add(visitKey);

      visit.transition = visit.type;
      k += 1;
    }
    record.visits.length = k; // truncate array

    // No update if there aren't any visits to apply.
    // History wants at least one visit.
    // In any case, the only thing we could change would be the title
    // and that shouldn't change without a visit.
    if (!record.visits.length) {
      this._log.trace("Ignoring record " + record.id + " with URI "
                      + record.uri.spec + ": no visits to add.");
      return false;
    }

    return true;
  },

  async remove(record) {
    this._log.trace("Removing page: " + record.id);
    let removed = await PlacesUtils.history.remove(record.id);
    if (removed) {
      this._log.trace("Removed page: " + record.id);
    } else {
      this._log.debug("Page already removed: " + record.id);
    }
  },

  async itemExists(id) {
    return !!(await PlacesSyncUtils.history.fetchURLInfoForGuid(id));
  },

  async createRecord(id, collection) {
    let foo = await PlacesSyncUtils.history.fetchURLInfoForGuid(id);
    let record = new HistoryRec(collection, id);
    if (foo) {
      record.histUri = foo.url;
      record.title = foo.title;
      record.sortindex = foo.frecency;
      try {
        record.visits = await PlacesSyncUtils.history.fetchVisitsForURL(record.histUri);
      } catch (e) {
        this._log.error("Error while fetching visits for URL ${record.histUri}", record.histUri);
        record.visits = [];
      }
    } else {
      record.deleted = true;
    }

    return record;
  },

  async wipe() {
    return PlacesSyncUtils.history.wipe();
  }
};

function HistoryTracker(name, engine) {
  Tracker.call(this, name, engine);
}
HistoryTracker.prototype = {
  __proto__: Tracker.prototype,

  onStart() {
    this._log.info("Adding Places observer.");
    PlacesUtils.history.addObserver(this, true);
    this._placesObserver =
      new PlacesWeakCallbackWrapper(this.handlePlacesEvents.bind(this));
    PlacesObservers.addListener(["page-visited"], this._placesObserver);
  },

  onStop() {
    this._log.info("Removing Places observer.");
    PlacesUtils.history.removeObserver(this);
    if (this._placesObserver) {
      PlacesObservers.removeListener(["page-visited"], this._placesObserver);
    }
  },

  QueryInterface: ChromeUtils.generateQI([
    Ci.nsINavHistoryObserver,
    Ci.nsISupportsWeakReference
  ]),

  async onDeleteAffectsGUID(uri, guid, reason, source, increment) {
    if (this.ignoreAll || reason == Ci.nsINavHistoryObserver.REASON_EXPIRED) {
      return;
    }
    this._log.trace(source + ": " + uri.spec + ", reason " + reason);
    const added = await this.addChangedID(guid);
    if (added) {
      this.score += increment;
    }
  },

  onDeleteVisits(uri, visitTime, guid, reason) {
    this.asyncObserver.enqueueCall(() =>
      this.onDeleteAffectsGUID(uri, guid, reason, "onDeleteVisits", SCORE_INCREMENT_SMALL)
    );
  },

  onDeleteURI(uri, guid, reason) {
    this.asyncObserver.enqueueCall(() =>
      this.onDeleteAffectsGUID(uri, guid, reason, "onDeleteURI", SCORE_INCREMENT_XLARGE)
    );
  },

  handlePlacesEvents(aEvents) {
    this.asyncObserver.enqueueCall(() => this._handlePlacesEvents(aEvents));
  },

  async _handlePlacesEvents(aEvents) {
    if (this.ignoreAll) {
      this._log.trace("ignoreAll: ignoring visits [" +
                      aEvents.map(v => v.guid).join(",") + "]");
      return;
    }
    for (let event of aEvents) {
      this._log.trace("'page-visited': " + event.url);
      if (this.engine.shouldSyncURL(event.url) &&
          await this.addChangedID(event.pageGuid)) {
        this.score += SCORE_INCREMENT_SMALL;
      }
    }
  },

  onClearHistory() {
    this._log.trace("onClearHistory");
    // Note that we're going to trigger a sync, but none of the cleared
    // pages are tracked, so the deletions will not be propagated.
    // See Bug 578694.
    this.score += SCORE_INCREMENT_XLARGE;
  },

  onBeginUpdateBatch() {},
  onEndUpdateBatch() {},
  onPageChanged() {},
  onTitleChanged() {},
  onBeforeDeleteURI() {},
};
