/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * What this is aimed to test:
 *
 * bh.removeAllPages should expire everything but bookmarked pages and valid
 * annos.
 */

let hs = PlacesUtils.history;
let bs = PlacesUtils.bookmarks;
let as = PlacesUtils.annotations;

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
          "WHERE id = ( " +
            "SELECT a.id FROM moz_items_annos a " +
            "JOIN moz_anno_attributes n ON n.id = a.anno_attribute_id " +
            "WHERE a.item_id = :id " +
              "AND n.name = :anno_name " +
            "ORDER BY a.dateAdded DESC LIMIT 1 " +
          ")";
  }
  else if (aIdentifier instanceof Ci.nsIURI){
    // Page annotation.
    as.setPageAnnotation(aIdentifier, aName, aValue, 0, aExpirePolicy);
    // Update dateAdded for the last added annotation.
    sql = "UPDATE moz_annos SET dateAdded = :expire_date, lastModified = :last_modified " +
          "WHERE id = ( " +
            "SELECT a.id FROM moz_annos a " +
            "JOIN moz_anno_attributes n ON n.id = a.anno_attribute_id " +
            "JOIN moz_places h on h.id = a.place_id " +
            "WHERE h.url = :id " +
            "AND n.name = :anno_name " +
            "ORDER BY a.dateAdded DESC LIMIT 1 " +
          ")";
  }
  else
    do_throw("Wrong identifier type");

  let stmt = DBConn().createStatement(sql);
  stmt.params.id = (typeof(aIdentifier) == "number") ? aIdentifier
                                                     : aIdentifier.spec;
  stmt.params.expire_date = expireDate;
  stmt.params.last_modified = lastModifiedDate;
  stmt.params.anno_name = aName;
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

  // Add some bookmarked page with visit and annotations.
  for (let i = 0; i < 5; i++) {
    let pageURI = uri("http://item_anno." + i + ".mozilla.org/");
    // This visit will be expired.
    hs.addVisit(pageURI, now++, null, hs.TRANSITION_TYPED, false, 0);
    let id = bs.insertBookmark(bs.unfiledBookmarksFolder, pageURI,
                               bs.DEFAULT_INDEX, null);
    // Will persist because it's an EXPIRE_NEVER item anno.
    as.setItemAnnotation(id, "persist", "test", 0, as.EXPIRE_NEVER);
    // Will persist because the page is bookmarked.
    as.setPageAnnotation(pageURI, "persist", "test", 0, as.EXPIRE_NEVER);
    // All EXPIRE_SESSION annotations are expected to expire on clear history.
    as.setItemAnnotation(id, "expire_session", "test", 0, as.EXPIRE_SESSION);
    as.setPageAnnotation(pageURI, "expire_session", "test", 0, as.EXPIRE_SESSION);
    // Annotations with timed policy will expire regardless bookmarked status.
    add_old_anno(id, "expire_days", "test", as.EXPIRE_DAYS, 8);
    add_old_anno(id, "expire_weeks", "test", as.EXPIRE_WEEKS, 31);
    add_old_anno(id, "expire_months", "test", as.EXPIRE_MONTHS, 181);
    add_old_anno(pageURI, "expire_days", "test", as.EXPIRE_DAYS, 8);
    add_old_anno(pageURI, "expire_weeks", "test", as.EXPIRE_WEEKS, 31);
    add_old_anno(pageURI, "expire_months", "test", as.EXPIRE_MONTHS, 181);
  }

  // Add some visited page and annotations for each.
  let now = Date.now() * 1000;
  for (let i = 0; i < 5; i++) {
    // All page annotations related to these expired pages are expected to
    // expire as well.
    let pageURI = uri("http://page_anno." + i + ".mozilla.org/");
    hs.addVisit(pageURI, now++, null, hs.TRANSITION_TYPED, false, 0);
    as.setPageAnnotation(pageURI, "expire", "test", 0, as.EXPIRE_NEVER);
    as.setPageAnnotation(pageURI, "expire_session", "test", 0, as.EXPIRE_SESSION);
    add_old_anno(pageURI, "expire_days", "test", as.EXPIRE_DAYS, 8);
    add_old_anno(pageURI, "expire_weeks", "test", as.EXPIRE_WEEKS, 31);
    add_old_anno(pageURI, "expire_months", "test", as.EXPIRE_MONTHS, 181);
  }

  // Observe expirations.
  observer = {
    observe: function(aSubject, aTopic, aData) {
      Services.obs.removeObserver(observer, PlacesUtils.TOPIC_EXPIRATION_FINISHED);

      ["expire_days", "expire_weeks", "expire_months", "expire_session",
       "expire"].forEach(function(aAnno) {
        let pages = as.getPagesWithAnnotation(aAnno);
        do_check_eq(pages.length, 0);
      });

      ["expire_days", "expire_weeks", "expire_months", "expire_session",
       "expire"].forEach(function(aAnno) {
        let items = as.getItemsWithAnnotation(aAnno);
        do_check_eq(items.length, 0);
      });

      ["persist"].forEach(function(aAnno) {
        let pages = as.getPagesWithAnnotation(aAnno);
        do_check_eq(pages.length, 5);
      });

      ["persist"].forEach(function(aAnno) {
        let items = as.getItemsWithAnnotation(aAnno);
        do_check_eq(items.length, 5);
        items.forEach(function(aItemId) {
          // Check item exists.
          bs.getItemIndex(aItemId);
        });
      });

      do_test_finished();
    }
  };
  Services.obs.addObserver(observer, PlacesUtils.TOPIC_EXPIRATION_FINISHED, false);

  // Expire all visits for the bookmarks.
  hs.QueryInterface(Ci.nsIBrowserHistory).removeAllPages();
  do_test_pending();
}
