/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * What this is aimed to test:
 *
 * Annotations can be set with a timed expiration policy.
 * Supported policies are:
 * - EXPIRE_DAYS: annotation would be expired after 7 days
 * - EXPIRE_WEEKS: annotation would be expired after 30 days
 * - EXPIRE_MONTHS: annotation would be expired after 180 days
 */

let os = Cc["@mozilla.org/observer-service;1"].
         getService(Ci.nsIObserverService);
let hs = Cc["@mozilla.org/browser/nav-history-service;1"].
         getService(Ci.nsINavHistoryService);
let bs = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
         getService(Ci.nsINavBookmarksService);
let as = Cc["@mozilla.org/browser/annotation-service;1"].
         getService(Ci.nsIAnnotationService);

/**
 * Creates an aged annotation.
 *
 * @param aIdentifier Either a page url or an item id.
 * @param aIdentifier Name of the annotation.
 * @param aValue Value for the annotation.
 * @param aExpirePolicy Expiration policy of the annotation.
 * @param aAgeInDays Age in days of the annotation.
 * @param [optional] aLastModifiedAgeInDays Age in days of the annotation, for lastModified.
 */
let now = Date.now();
function add_old_anno(aIdentifier, aName, aValue, aExpirePolicy,
                      aAgeInDays, aLastModifiedAgeInDays) {
  let expireDate = (now - (aAgeInDays * 86400 * 1000)) * 1000;
  let lastModifiedDate = 0;
  if (aLastModifiedAgeInDays)
    lastModifiedDate = (now - (aLastModifiedAgeInDays * 86400 * 1000)) * 1000;

  let sql;
  if (typeof(aIdentifier) == "number") {
    // Item annotation.
    as.setItemAnnotation(aIdentifier, aName, aValue, 0, aExpirePolicy);
    // Update dateAdded for the last added annotation.
    sql = "UPDATE moz_items_annos SET dateAdded = :expire_date, lastModified = :last_modified " +
          "WHERE id = (SELECT id FROM moz_items_annos " +
                      "WHERE item_id = :id " +
                      "ORDER BY dateAdded DESC LIMIT 1)";
  }
  else if (aIdentifier instanceof Ci.nsIURI){
    // Page annotation.
    as.setPageAnnotation(aIdentifier, aName, aValue, 0, aExpirePolicy);
    // Update dateAdded for the last added annotation.
    sql = "UPDATE moz_annos SET dateAdded = :expire_date, lastModified = :last_modified " +
          "WHERE id = (SELECT a.id FROM moz_annos a " +
                      "LEFT JOIN moz_places h on h.id = a.place_id " +
                      "WHERE h.url = :id " +
                      "ORDER BY a.dateAdded DESC LIMIT 1)";
  }
  else
    do_throw("Wrong identifier type");

  let stmt = DBConn().createStatement(sql);
  stmt.params.id = (typeof(aIdentifier) == "number") ? aIdentifier
                                                     : aIdentifier.spec;
  stmt.params.expire_date = expireDate;
  stmt.params.last_modified = lastModifiedDate;
  try {
    stmt.executeStep();
  }
  finally {
    stmt.finalize();
  }
}

function run_test() {
  // Set interval to a large value so we don't expire on it.
  setInterval(3600); // 1h

  // Expire all expirable pages.
  setMaxPages(0);

  let now = getExpirablePRTime();
  // Add some bookmarked page and timed annotations for each.
  for (let i = 0; i < 5; i++) {
    let pageURI = uri("http://item_anno." + i + ".mozilla.org/");
    hs.addVisit(pageURI, now++, null, hs.TRANSITION_TYPED, false, 0);
    let id = bs.insertBookmark(bs.unfiledBookmarksFolder, pageURI,
                               bs.DEFAULT_INDEX, null);
    // Add a 6 days old anno.
    add_old_anno(id, "persist_days", "test", as.EXPIRE_DAYS, 6);
    // Add a 8 days old anno, modified 5 days ago.
    add_old_anno(id, "persist_lm_days", "test", as.EXPIRE_DAYS, 8, 6);
    // Add a 8 days old anno.
    add_old_anno(id, "expire_days", "test", as.EXPIRE_DAYS, 8);

    // Add a 29 days old anno.
    add_old_anno(id, "persist_weeks", "test", as.EXPIRE_WEEKS, 29);
    // Add a 31 days old anno, modified 29 days ago.
    add_old_anno(id, "persist_lm_weeks", "test", as.EXPIRE_WEEKS, 31, 29);
    // Add a 31 days old anno.
    add_old_anno(id, "expire_weeks", "test", as.EXPIRE_WEEKS, 31);

    // Add a 179 days old anno.
    add_old_anno(id, "persist_months", "test", as.EXPIRE_MONTHS, 179);
    // Add a 181 days old anno, modified 179 days ago.
    add_old_anno(id, "persist_lm_months", "test", as.EXPIRE_MONTHS, 181, 179);
    // Add a 181 days old anno.
    add_old_anno(id, "expire_months", "test", as.EXPIRE_MONTHS, 181);

    // Add a 6 days old anno.
    add_old_anno(pageURI, "persist_days", "test", as.EXPIRE_DAYS, 6);
    // Add a 8 days old anno, modified 5 days ago.
    add_old_anno(pageURI, "persist_lm_days", "test", as.EXPIRE_DAYS, 8, 6);
    // Add a 8 days old anno.
    add_old_anno(pageURI, "expire_days", "test", as.EXPIRE_DAYS, 8);

    // Add a 29 days old anno.
    add_old_anno(pageURI, "persist_weeks", "test", as.EXPIRE_WEEKS, 29);
    // Add a 31 days old anno, modified 29 days ago.
    add_old_anno(pageURI, "persist_lm_weeks", "test", as.EXPIRE_WEEKS, 31, 29);
    // Add a 31 days old anno.
    add_old_anno(pageURI, "expire_weeks", "test", as.EXPIRE_WEEKS, 31);

    // Add a 179 days old anno.
    add_old_anno(pageURI, "persist_months", "test", as.EXPIRE_MONTHS, 179);
    // Add a 181 days old anno, modified 179 days ago.
    add_old_anno(pageURI, "persist_lm_months", "test", as.EXPIRE_MONTHS, 181, 179);
    // Add a 181 days old anno.
    add_old_anno(pageURI, "expire_months", "test", as.EXPIRE_MONTHS, 181);
  }

  // Add some visited page and timed annotations for each.
  for (let i = 0; i < 5; i++) {
    let pageURI = uri("http://page_anno." + i + ".mozilla.org/");
    hs.addVisit(pageURI, now++, null, hs.TRANSITION_TYPED, false, 0);
    // Add a 6 days old anno.
    add_old_anno(pageURI, "persist_days", "test", as.EXPIRE_DAYS, 6);
    // Add a 8 days old anno, modified 5 days ago.
    add_old_anno(pageURI, "persist_lm_days", "test", as.EXPIRE_DAYS, 8, 6);
    // Add a 8 days old anno.
    add_old_anno(pageURI, "expire_days", "test", as.EXPIRE_DAYS, 8);

    // Add a 29 days old anno.
    add_old_anno(pageURI, "persist_weeks", "test", as.EXPIRE_WEEKS, 29);
    // Add a 31 days old anno, modified 29 days ago.
    add_old_anno(pageURI, "persist_lm_weeks", "test", as.EXPIRE_WEEKS, 31, 29);
    // Add a 31 days old anno.
    add_old_anno(pageURI, "expire_weeks", "test", as.EXPIRE_WEEKS, 31);

    // Add a 179 days old anno.
    add_old_anno(pageURI, "persist_months", "test", as.EXPIRE_MONTHS, 179);
    // Add a 181 days old anno, modified 179 days ago.
    add_old_anno(pageURI, "persist_lm_months", "test", as.EXPIRE_MONTHS, 181, 179);
    // Add a 181 days old anno.
    add_old_anno(pageURI, "expire_months", "test", as.EXPIRE_MONTHS, 181);
  }

  // Observe expirations.
  observer = {
    observe: function(aSubject, aTopic, aData) {
      os.removeObserver(observer, PlacesUtils.TOPIC_EXPIRATION_FINISHED);

      ["expire_days", "expire_weeks", "expire_months"].forEach(function(aAnno) {
        let pages = as.getPagesWithAnnotation(aAnno);
        do_check_eq(pages.length, 0);
      });

      ["expire_days", "expire_weeks", "expire_months"].forEach(function(aAnno) {
        let items = as.getItemsWithAnnotation(aAnno);
        do_check_eq(items.length, 0);
      });

      ["persist_days", "persist_lm_days", "persist_weeks", "persist_lm_weeks",
       "persist_months", "persist_lm_months"].forEach(function(aAnno) {
        let pages = as.getPagesWithAnnotation(aAnno);
        do_check_eq(pages.length, 10);
      });

      ["persist_days", "persist_lm_days", "persist_weeks", "persist_lm_weeks",
       "persist_months", "persist_lm_months"].forEach(function(aAnno) {
        let items = as.getItemsWithAnnotation(aAnno);
        do_check_eq(items.length, 5);
      });

      do_test_finished();
    }
  };
  os.addObserver(observer, PlacesUtils.TOPIC_EXPIRATION_FINISHED, false);

  // Expire all visits for the bookmarks.
  force_expiration_step(5);
  do_test_pending();
}
