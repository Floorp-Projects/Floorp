/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Crossweave.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonathan Griffin <jgriffin@mozilla.com>
 *   Philipp von Weitershausen <philipp@weitershausen.de>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

 /* This is a JavaScript module (JSM) to be imported via 
  * Components.utils.import() and acts as a singleton. Only the following 
  * listed symbols will exposed on import, and only when and where imported.
  */

var EXPORTED_SYMBOLS = ["HistoryEntry", "DumpHistory"];

const CC = Components.classes;
const CI = Components.interfaces;
const CU = Components.utils;

CU.import("resource://gre/modules/Services.jsm");
CU.import("resource://gre/modules/PlacesUtils.jsm");
CU.import("resource://tps/logger.jsm");
CU.import("resource://services-sync/async.js");

var DumpHistory = function TPS_History__DumpHistory() {
  let writer = {
    value: "",
    write: function PlacesItem__dump__write(aStr, aLen) {
      this.value += aStr;
    }
  };

  let query = PlacesUtils.history.getNewQuery();
  let options = PlacesUtils.history.getNewQueryOptions();
  let root = PlacesUtils.history.executeQuery(query, options).root;
  root.containerOpen = true;
  Logger.logInfo("\n\ndumping history\n", true);
  for (var i = 0; i < root.childCount; i++) {
    let node = root.getChild(i);
    let uri = node.uri;
    let curvisits = HistoryEntry._getVisits(uri);
    for each (var visit in curvisits) {
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
   * _db
   *
   * Returns the DBConnection object for the history service.
   */
  get _db() {
    return PlacesUtils.history.QueryInterface(CI.nsPIPlacesDatabase).DBConnection;
  },

  /**
   * _visitStm
   *
   * Return the SQL statement for getting history visit information
   * from the moz_historyvisits table.  Borrowed from Weave's
   * history.js.
   */
  get _visitStm() {
    let stm = this._db.createStatement(
      "SELECT visit_type type, visit_date date " +
      "FROM moz_historyvisits " +
      "WHERE place_id = (" +
        "SELECT id " +
        "FROM moz_places " +
        "WHERE url = :url) " +
      "ORDER BY date DESC LIMIT 10");
    this.__defineGetter__("_visitStm", function() stm);
    return stm;
  },

  /**
   * _getVisits
   *
   * Gets history information about visits to a given uri.
   *
   * @param uri The uri to get visits for
   * @return an array of objects with 'date' and 'type' properties,
   * corresponding to the visits in the history database for the
   * given uri
   */
  _getVisits: function HistStore__getVisits(uri) {
    this._visitStm.params.url = uri;
    return Async.querySpinningly(this._visitStm, ["date", "type"]);
  },

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
  Add: function(item, usSinceEpoch) {
    Logger.AssertTrue("visits" in item && "uri" in item,
      "History entry in test file must have both 'visits' " +
      "and 'uri' properties");
    let uri = Services.io.newURI(item.uri, null, null);
    for each (visit in item.visits) {
      let visitId = PlacesUtils.history.addVisit(
                    uri, 
                    usSinceEpoch + (visit.date * 60 * 60 * 1000 * 1000), 
                    null, visit.type, 
                    visit.type == 5 || visit.type == 6, 0);
      Logger.AssertTrue(visitId, "Error adding history entry");
      if ("title" in item)
        PlacesUtils.history.setPageTitle(uri, item.title);
    }
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
  Find: function(item, usSinceEpoch) {
    Logger.AssertTrue("visits" in item && "uri" in item,
      "History entry in test file must have both 'visits' " +
      "and 'uri' properties");
    let curvisits = this._getVisits(item.uri);
    for each (visit in curvisits) {
      for each (itemvisit in item.visits) {
        let expectedDate = itemvisit.date * 60 * 60 * 1000 * 1000 
            + usSinceEpoch;
        if (visit.type == itemvisit.type && visit.date == expectedDate) {
          itemvisit.found = true;
        }
      }
    }
    
    let all_items_found = true;
    for each (itemvisit in item.visits) {
      all_items_found = all_items_found && "found" in itemvisit;
      Logger.logInfo("History entry for " + item.uri + ", type:" + 
              itemvisit.type + ", date:" + itemvisit.date + 
              ("found" in itemvisit ? " is present" : " is not present"));
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
  Delete: function(item, usSinceEpoch) {
    if ("uri" in item) {
      let uri = Services.io.newURI(item.uri, null, null);
      PlacesUtils.history.removePage(uri);
    }
    else if ("host" in item) {
      PlacesUtils.history.removePagesFromHost(item.host, false);
    }
    else if ("begin" in item && "end" in item) {
      PlacesUtils.history.removeVisitsByTimeframe(
          usSinceEpoch + (item.begin * 60 * 60 * 1000 * 1000), 
          usSinceEpoch + (item.end * 60 * 60 * 1000 * 1000));
    }
    else {
      Logger.AssertTrue(false, "invalid entry in delete history");
    }
  },
};

