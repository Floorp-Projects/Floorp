/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const HISTORY_TTL = 5184000; // 60 days in milliseconds
const THIRTY_DAYS_IN_MS = 2592000000; // 30 days in milliseconds
// Sync may bring new fields from other clients, not yet understood by our engine.
// Unknown fields outside these fields are aggregated into 'unknownFields' and
// safely synced to prevent data loss.
const VALID_HISTORY_FIELDS = ["id", "title", "histUri", "visits"];
const VALID_VISIT_FIELDS = ["date", "type", "transition"];

import { Async } from "resource://services-common/async.sys.mjs";
import { CommonUtils } from "resource://services-common/utils.sys.mjs";

import {
  MAX_HISTORY_DOWNLOAD,
  MAX_HISTORY_UPLOAD,
  SCORE_INCREMENT_SMALL,
  SCORE_INCREMENT_XLARGE,
} from "resource://services-sync/constants.sys.mjs";

import {
  Store,
  SyncEngine,
  LegacyTracker,
} from "resource://services-sync/engines.sys.mjs";
import { CryptoWrapper } from "resource://services-sync/record.sys.mjs";
import { Utils } from "resource://services-sync/util.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  PlacesSyncUtils: "resource://gre/modules/PlacesSyncUtils.sys.mjs",
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
});

export function HistoryRec(collection, id) {
  CryptoWrapper.call(this, collection, id);
}

HistoryRec.prototype = {
  _logName: "Sync.Record.History",
  ttl: HISTORY_TTL,
};
Object.setPrototypeOf(HistoryRec.prototype, CryptoWrapper.prototype);

Utils.deferGetSet(HistoryRec, "cleartext", ["histUri", "title", "visits"]);

export function HistoryEngine(service) {
  SyncEngine.call(this, "History", service);
}

HistoryEngine.prototype = {
  _recordObj: HistoryRec,
  _storeObj: HistoryStore,
  _trackerObj: HistoryTracker,
  downloadLimit: MAX_HISTORY_DOWNLOAD,

  syncPriority: 7,

  async getSyncID() {
    return lazy.PlacesSyncUtils.history.getSyncId();
  },

  async ensureCurrentSyncID(newSyncID) {
    this._log.debug(
      "Checking if server sync ID ${newSyncID} matches existing",
      { newSyncID }
    );
    await lazy.PlacesSyncUtils.history.ensureCurrentSyncId(newSyncID);
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
    let newSyncID = await lazy.PlacesSyncUtils.history.resetSyncId();
    this._log.debug("Assigned new sync ID ${newSyncID}", { newSyncID });
    return newSyncID;
  },

  async getLastSync() {
    let lastSync = await lazy.PlacesSyncUtils.history.getLastSync();
    return lastSync;
  },

  async setLastSync(lastSync) {
    await lazy.PlacesSyncUtils.history.setLastSync(lastSync);
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

    let guidsToRemove =
      await lazy.PlacesSyncUtils.history.determineNonSyncableGuids(
        modifiedGUIDs
      );
    await this._tracker.removeChangedID(...guidsToRemove);
    return changedIDs;
  },

  async _resetClient() {
    await super._resetClient();
    await lazy.PlacesSyncUtils.history.reset();
  },
};
Object.setPrototypeOf(HistoryEngine.prototype, SyncEngine.prototype);

function HistoryStore(name, engine) {
  Store.call(this, name, engine);
}

HistoryStore.prototype = {
  // We try and only update this many visits at one time.
  MAX_VISITS_PER_INSERT: 500,

  // Some helper functions to handle GUIDs
  async setGUID(uri, guid) {
    if (!guid) {
      guid = Utils.makeGUID();
    }

    try {
      await lazy.PlacesSyncUtils.history.changeGuid(uri, guid);
    } catch (e) {
      this._log.error("Error setting GUID ${guid} for URI ${uri}", guid, uri);
    }

    return guid;
  },

  async GUIDForUri(uri, create) {
    // Use the existing GUID if it exists
    let guid;
    try {
      guid = await lazy.PlacesSyncUtils.history.fetchGuidForURL(uri);
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
    let info = await lazy.PlacesSyncUtils.history.fetchURLInfoForGuid(oldID);
    if (!info) {
      throw new Error(`Can't change ID for nonexistent history entry ${oldID}`);
    }
    this.setGUID(info.url, newID);
  },

  async getAllIDs() {
    let urls = await lazy.PlacesSyncUtils.history.getAllURLs({
      since: new Date(Date.now() - THIRTY_DAYS_IN_MS),
      limit: MAX_HISTORY_UPLOAD,
    });

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

  async applyIncomingBatch(records, countTelemetry) {
    // Convert incoming records to mozIPlaceInfo objects which are applied as
    // either history additions or removals.
    let failed = [];
    let toAdd = [];
    let toRemove = [];
    let pageGuidsWithUnknownFields = new Map();
    let visitTimesWithUnknownFields = new Map();
    await Async.yieldingForEach(records, async record => {
      if (record.deleted) {
        toRemove.push(record);
      } else {
        try {
          let pageInfo = await this._recordToPlaceInfo(record);
          if (pageInfo) {
            toAdd.push(pageInfo);

            // Pull any unknown fields that may have come from other clients
            let unknownFields = lazy.PlacesSyncUtils.extractUnknownFields(
              record.cleartext,
              VALID_HISTORY_FIELDS
            );
            if (unknownFields) {
              pageGuidsWithUnknownFields.set(pageInfo.guid, { unknownFields });
            }

            // Visits themselves could also contain unknown fields
            for (const visit of pageInfo.visits) {
              let unknownVisitFields =
                lazy.PlacesSyncUtils.extractUnknownFields(
                  visit,
                  VALID_VISIT_FIELDS
                );
              if (unknownVisitFields) {
                // Visits don't have an id at the time of sync so we'll need
                // to use the time instead until it's inserted in the DB
                visitTimesWithUnknownFields.set(visit.date.getTime(), {
                  unknownVisitFields,
                });
              }
            }
          }
        } catch (ex) {
          if (Async.isShutdownException(ex)) {
            throw ex;
          }
          this._log.error("Failed to create a place info", ex);
          this._log.trace("The record that failed", record);
          failed.push(record.id);
          countTelemetry.addIncomingFailedReason(ex.message);
        }
      }
    });
    if (toAdd.length || toRemove.length) {
      if (toRemove.length) {
        // PlacesUtils.history.remove takes an array of visits to remove,
        // but the error semantics are tricky - a single "bad" entry will cause
        // an exception before anything is removed. So we do remove them one at
        // a time.
        await Async.yieldingForEach(toRemove, async record => {
          try {
            await this.remove(record);
          } catch (ex) {
            if (Async.isShutdownException(ex)) {
              throw ex;
            }
            this._log.error("Failed to delete a place info", ex);
            this._log.trace("The record that failed", record);
            failed.push(record.id);
            countTelemetry.addIncomingFailedReason(ex.message);
          }
        });
      }
      for (let chunk of this._generateChunks(toAdd)) {
        // Per bug 1415560, we ignore any exceptions returned by insertMany
        // as they are likely to be spurious. We do supply an onError handler
        // and log the exceptions seen there as they are likely to be
        // informative, but we still never abort the sync based on them.
        let unknownFieldsToInsert = [];
        try {
          await lazy.PlacesUtils.history.insertMany(
            chunk,
            result => {
              const placeToUpdate = pageGuidsWithUnknownFields.get(result.guid);
              // Extract the placeId from this result so we can add the unknownFields
              // to the proper table
              if (placeToUpdate) {
                unknownFieldsToInsert.push({
                  placeId: result.placeId,
                  unknownFields: placeToUpdate.unknownFields,
                });
              }
              // same for visits
              result.visits.forEach(visit => {
                let visitToUpdate = visitTimesWithUnknownFields.get(
                  visit.date.getTime()
                );
                if (visitToUpdate) {
                  unknownFieldsToInsert.push({
                    visitId: visit.visitId,
                    unknownFields: visitToUpdate.unknownVisitFields,
                  });
                }
              });
            },
            failedVisit => {
              this._log.info(
                "Failed to insert a history record",
                failedVisit.guid
              );
              this._log.trace("The record that failed", failedVisit);
              failed.push(failedVisit.guid);
            }
          );
        } catch (ex) {
          this._log.info("Failed to insert history records", ex);
          countTelemetry.addIncomingFailedReason(ex.message);
        }

        // All the top level places or visits that had unknown fields are sent
        // to be added to the appropiate tables
        await lazy.PlacesSyncUtils.history.updateUnknownFieldsBatch(
          unknownFieldsToInsert
        );
      }
    }

    return failed;
  },

  /**
   * Returns a generator that splits records into sanely sized chunks suitable
   * for passing to places to prevent places doing bad things at shutdown.
   */
  *_generateChunks(records) {
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
      } while (
        curIndex < records.length &&
        count + records[curIndex].visits.length <= this.MAX_VISITS_PER_INSERT
      );
      this._log.trace(`adding ${toAdd.length} items in this chunk`);
      yield toAdd;
    }
  },

  /* An internal helper to determine if we can add an entry to places.
     Exists primarily so tests can override it.
   */
  _canAddURI(uri) {
    return lazy.PlacesUtils.history.canAddURI(uri);
  },

  /**
   * Converts a Sync history record to a mozIPlaceInfo.
   *
   * Throws if an invalid record is encountered (invalid URI, etc.),
   * returns a new PageInfo object if the record is to be applied, null
   * otherwise (no visits to add, etc.),
   */
  async _recordToPlaceInfo(record) {
    // Sort out invalid URIs and ones Places just simply doesn't want.
    record.url = lazy.PlacesUtils.normalizeToURLOrGUID(record.histUri);
    record.uri = CommonUtils.makeURI(record.histUri);

    if (!Utils.checkGUID(record.id)) {
      this._log.warn("Encountered record with invalid GUID: " + record.id);
      return null;
    }
    record.guid = record.id;

    if (
      !this._canAddURI(record.uri) ||
      !this.engine.shouldSyncURL(record.uri.spec)
    ) {
      this._log.trace(
        "Ignoring record " +
          record.id +
          " with URI " +
          record.uri.spec +
          ": can't add this URI."
      );
      return null;
    }

    // We dupe visits by date and type. So an incoming visit that has
    // the same timestamp and type as a local one won't get applied.
    // To avoid creating new objects, we rewrite the query result so we
    // can simply check for containment below.
    let curVisitsAsArray = [];
    let curVisits = new Set();
    try {
      curVisitsAsArray = await lazy.PlacesSyncUtils.history.fetchVisitsForURL(
        record.histUri
      );
    } catch (e) {
      this._log.error(
        "Error while fetching visits for URL ${record.histUri}",
        record.histUri
      );
    }
    let oldestAllowed =
      lazy.PlacesSyncUtils.bookmarks.EARLIEST_BOOKMARK_TIMESTAMP;
    if (curVisitsAsArray.length == 20) {
      let oldestVisit = curVisitsAsArray[curVisitsAsArray.length - 1];
      oldestAllowed = lazy.PlacesSyncUtils.history.clampVisitDate(
        lazy.PlacesUtils.toDate(oldestVisit.date).getTime()
      );
    }

    let i, k;
    for (i = 0; i < curVisitsAsArray.length; i++) {
      // Same logic as used in the loop below to generate visitKey.
      let { date, type } = curVisitsAsArray[i];
      let dateObj = lazy.PlacesUtils.toDate(date);
      let millis = lazy.PlacesSyncUtils.history
        .clampVisitDate(dateObj)
        .getTime();
      curVisits.add(`${millis},${type}`);
    }

    // Walk through the visits, make sure we have sound data, and eliminate
    // dupes. The latter is done by rewriting the array in-place.
    for (i = 0, k = 0; i < record.visits.length; i++) {
      let visit = (record.visits[k] = record.visits[i]);

      if (
        !visit.date ||
        typeof visit.date != "number" ||
        !Number.isInteger(visit.date)
      ) {
        this._log.warn(
          "Encountered record with invalid visit date: " + visit.date
        );
        continue;
      }

      if (
        !visit.type ||
        !Object.values(lazy.PlacesUtils.history.TRANSITIONS).includes(
          visit.type
        )
      ) {
        this._log.warn(
          "Encountered record with invalid visit type: " +
            visit.type +
            "; ignoring."
        );
        continue;
      }

      // Dates need to be integers. Future and far past dates are clamped to the
      // current date and earliest sensible date, respectively.
      let originalVisitDate = lazy.PlacesUtils.toDate(Math.round(visit.date));
      visit.date =
        lazy.PlacesSyncUtils.history.clampVisitDate(originalVisitDate);

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
      this._log.trace(
        "Ignoring record " +
          record.id +
          " with URI " +
          record.uri.spec +
          ": no visits to add."
      );
      return null;
    }

    // PageInfo is validated using validateItemProperties which does a shallow
    // copy of the properties. Since record uses getters some of the properties
    // are not copied over. Thus we create and return a new object.
    let pageInfo = {
      title: record.title,
      url: record.url,
      guid: record.guid,
      visits: record.visits,
    };

    return pageInfo;
  },

  async remove(record) {
    this._log.trace("Removing page: " + record.id);
    let removed = await lazy.PlacesUtils.history.remove(record.id);
    if (removed) {
      this._log.trace("Removed page: " + record.id);
    } else {
      this._log.debug("Page already removed: " + record.id);
    }
  },

  async itemExists(id) {
    return !!(await lazy.PlacesSyncUtils.history.fetchURLInfoForGuid(id));
  },

  async createRecord(id, collection) {
    let foo = await lazy.PlacesSyncUtils.history.fetchURLInfoForGuid(id);
    let record = new HistoryRec(collection, id);
    if (foo) {
      record.histUri = foo.url;
      record.title = foo.title;
      record.sortindex = foo.frecency;

      // If we had any unknown fields, ensure we put it back on the
      // top-level record
      if (foo.unknownFields) {
        let unknownFields = JSON.parse(foo.unknownFields);
        Object.assign(record.cleartext, unknownFields);
      }

      try {
        record.visits = await lazy.PlacesSyncUtils.history.fetchVisitsForURL(
          record.histUri
        );
      } catch (e) {
        this._log.error(
          "Error while fetching visits for URL ${record.histUri}",
          record.histUri
        );
        record.visits = [];
      }
    } else {
      record.deleted = true;
    }

    return record;
  },

  async wipe() {
    return lazy.PlacesSyncUtils.history.wipe();
  },
};
Object.setPrototypeOf(HistoryStore.prototype, Store.prototype);

function HistoryTracker(name, engine) {
  LegacyTracker.call(this, name, engine);
}
HistoryTracker.prototype = {
  onStart() {
    this._log.info("Adding Places observer.");
    this._placesObserver = new PlacesWeakCallbackWrapper(
      this.handlePlacesEvents.bind(this)
    );
    PlacesObservers.addListener(
      ["page-visited", "history-cleared", "page-removed"],
      this._placesObserver
    );
  },

  onStop() {
    this._log.info("Removing Places observer.");
    if (this._placesObserver) {
      PlacesObservers.removeListener(
        ["page-visited", "history-cleared", "page-removed"],
        this._placesObserver
      );
    }
  },

  QueryInterface: ChromeUtils.generateQI(["nsISupportsWeakReference"]),

  handlePlacesEvents(aEvents) {
    this.asyncObserver.enqueueCall(() => this._handlePlacesEvents(aEvents));
  },

  async _handlePlacesEvents(aEvents) {
    if (this.ignoreAll) {
      this._log.trace(
        "ignoreAll: ignoring visits [" +
          aEvents.map(v => v.guid).join(",") +
          "]"
      );
      return;
    }
    for (let event of aEvents) {
      switch (event.type) {
        case "page-visited": {
          this._log.trace("'page-visited': " + event.url);
          if (
            this.engine.shouldSyncURL(event.url) &&
            (await this.addChangedID(event.pageGuid))
          ) {
            this.score += SCORE_INCREMENT_SMALL;
          }
          break;
        }
        case "history-cleared": {
          this._log.trace("history-cleared");
          // Note that we're going to trigger a sync, but none of the cleared
          // pages are tracked, so the deletions will not be propagated.
          // See Bug 578694.
          this.score += SCORE_INCREMENT_XLARGE;
          break;
        }
        case "page-removed": {
          if (event.reason === PlacesVisitRemoved.REASON_EXPIRED) {
            return;
          }

          this._log.trace(
            "page-removed: " + event.url + ", reason " + event.reason
          );
          const added = await this.addChangedID(event.pageGuid);
          if (added) {
            this.score += event.isRemovedFromStore
              ? SCORE_INCREMENT_XLARGE
              : SCORE_INCREMENT_SMALL;
          }
          break;
        }
      }
    }
  },
};
Object.setPrototypeOf(HistoryTracker.prototype, LegacyTracker.prototype);
