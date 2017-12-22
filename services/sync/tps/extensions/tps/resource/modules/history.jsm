/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 /* This is a JavaScript module (JSM) to be imported via
  * Components.utils.import() and acts as a singleton. Only the following
  * listed symbols will exposed on import, and only when and where imported.
  */

var EXPORTED_SYMBOLS = ["HistoryEntry", "DumpHistory"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/PlacesUtils.jsm");
Cu.import("resource://gre/modules/PlacesSyncUtils.jsm");
Cu.import("resource://tps/logger.jsm");
Cu.import("resource://services-common/async.js");

var DumpHistory = async function TPS_History__DumpHistory() {
  let query = PlacesUtils.history.getNewQuery();
  let options = PlacesUtils.history.getNewQueryOptions();
  let root = PlacesUtils.history.executeQuery(query, options).root;
  root.containerOpen = true;
  Logger.logInfo("\n\ndumping history\n", true);
  for (var i = 0; i < root.childCount; i++) {
    let node = root.getChild(i);
    let uri = node.uri;
    let curvisits = await PlacesSyncUtils.history.fetchVisitsForURL(uri);
    for (var visit of curvisits) {
      Logger.logInfo("URI: " + uri + ", type=" + visit.type + ", date=" + visit.date, true);
    }
  }
  root.containerOpen = false;
  Logger.logInfo("\nend history dump\n", true);
};

/**
 * HistoryEntry object
 *
 * Contains methods for manipulating browser history entries.
 */
var HistoryEntry = {
  /**
   * Add
   *
   * Adds visits for a uri to the history database.  Throws on error.
   *
   * @param item An object representing one or more visits to a specific uri
   * @param usSinceEpoch The number of microseconds from Epoch to
   *        the time the current Crossweave run was started
   * @return nothing
   */
  async Add(item, usSinceEpoch) {
    Logger.AssertTrue("visits" in item && "uri" in item,
      "History entry in test file must have both 'visits' " +
      "and 'uri' properties");
    let uri = Services.io.newURI(item.uri);
    let place = {
      uri,
      visits: []
    };
    for (let visit of item.visits) {
      place.visits.push({
        visitDate: usSinceEpoch + (visit.date * 60 * 60 * 1000 * 1000),
        transitionType: visit.type
      });
    }
    if ("title" in item) {
      place.title = item.title;
    }
    return new Promise((resolve, reject) => {
      PlacesUtils.asyncHistory.updatePlaces(place, {
          handleError() {
            reject(new Error("Error adding history entry"));
          },
          handleResult() {},
          handleCompletion() {
            resolve();
          }
      });
    });
  },

  /**
   * Find
   *
   * Finds visits for a uri to the history database.  Throws on error.
   *
   * @param item An object representing one or more visits to a specific uri
   * @param usSinceEpoch The number of microseconds from Epoch to
   *        the time the current Crossweave run was started
   * @return true if all the visits for the uri are found, otherwise false
   */
  async Find(item, usSinceEpoch) {
    Logger.AssertTrue("visits" in item && "uri" in item,
      "History entry in test file must have both 'visits' " +
      "and 'uri' properties");
    let curvisits = await PlacesSyncUtils.history.fetchVisitsForURL(item.uri);
    for (let visit of curvisits) {
      for (let itemvisit of item.visits) {
        let expectedDate = itemvisit.date * 60 * 60 * 1000 * 1000
            + usSinceEpoch;
        if (visit.type == itemvisit.type && visit.date == expectedDate) {
          itemvisit.found = true;
        }
      }
    }

    let all_items_found = true;
    for (let itemvisit of item.visits) {
      all_items_found = all_items_found && "found" in itemvisit;
      Logger.logInfo(
        `History entry for ${item.uri}, type: ${itemvisit.type}, date: ${itemvisit.date}` +
        `(${itemvisit.date * 60 * 60 * 1000 * 1000}), found = ${!!itemvisit.found}`
      );
    }
    return all_items_found;
  },

  /**
   * Delete
   *
   * Removes visits from the history database. Throws on error.
   *
   * @param item An object representing items to delete
   * @param usSinceEpoch The number of microseconds from Epoch to
   *        the time the current Crossweave run was started
   * @return nothing
   */
  async Delete(item, usSinceEpoch) {
    if ("uri" in item) {
      let removedAny = await PlacesUtils.history.remove(item.uri);
      if (!removedAny) {
        Logger.log("Warning: Removed 0 history visits for uri " + item.uri);
      }
    } else if ("host" in item) {
      await PlacesUtils.history.removePagesFromHost(item.host, false);
    } else if ("begin" in item && "end" in item) {
      let msSinceEpoch = parseInt(usSinceEpoch / 1000);
      let filter = {
        beginDate: new Date(msSinceEpoch + (item.begin * 60 * 60 * 1000)),
        endDate: new Date(msSinceEpoch + (item.end * 60 * 60 * 1000))
      };
      let removedAny = await PlacesUtils.history.removeVisitsByFilter(filter);
      if (!removedAny) {
        Logger.log("Warning: Removed 0 history visits with " + JSON.stringify({ item, filter }));
      }
    } else {
      Logger.AssertTrue(false, "invalid entry in delete history " + JSON.stringify(item));
    }
  },
};
