/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPlacesTables.h"

#ifndef __nsPlacesTriggers_h__
#define __nsPlacesTriggers_h__

/**
 * Exclude these visit types:
 *  0 - invalid
 *  4 - EMBED
 *  7 - DOWNLOAD
 *  8 - FRAMED_LINK
 *  9 - RELOAD
 **/
#define EXCLUDED_VISIT_TYPES "0, 4, 7, 8, 9"

/**
 * This triggers update visit_count and last_visit_date based on historyvisits
 * table changes.
 */
#define CREATE_HISTORYVISITS_AFTERINSERT_TRIGGER NS_LITERAL_CSTRING( \
  "CREATE TEMP TRIGGER moz_historyvisits_afterinsert_v2_trigger " \
  "AFTER INSERT ON moz_historyvisits FOR EACH ROW " \
  "BEGIN " \
    "SELECT store_last_inserted_id('moz_historyvisits', NEW.id); " \
    "UPDATE moz_places SET " \
      "visit_count = visit_count + (SELECT NEW.visit_type NOT IN (" EXCLUDED_VISIT_TYPES ")), "\
      "last_visit_date = MAX(IFNULL(last_visit_date, 0), NEW.visit_date) " \
    "WHERE id = NEW.place_id;" \
  "END" \
)

#define CREATE_HISTORYVISITS_AFTERDELETE_TRIGGER NS_LITERAL_CSTRING( \
  "CREATE TEMP TRIGGER moz_historyvisits_afterdelete_v2_trigger " \
  "AFTER DELETE ON moz_historyvisits FOR EACH ROW " \
  "BEGIN " \
    "UPDATE moz_places SET " \
      "visit_count = visit_count - (SELECT OLD.visit_type NOT IN (" EXCLUDED_VISIT_TYPES ")), "\
      "last_visit_date = (SELECT visit_date FROM moz_historyvisits " \
                         "WHERE place_id = OLD.place_id " \
                         "ORDER BY visit_date DESC LIMIT 1) " \
    "WHERE id = OLD.place_id;" \
  "END" \
)

// This macro is a helper for the next several triggers.  It updates the origin
// frecency stats.  Use it as follows.  Before changing an origin's frecency,
// call the macro and pass "-" (subtraction) as the argument.  That will update
// the stats by deducting the origin's current contribution to them.  And then
// after you change the origin's frecency, call the macro again, this time
// passing "+" (addition) as the argument.  That will update the stats by adding
// the origin's new contribution to them.
#define UPDATE_ORIGIN_FRECENCY_STATS(op) \
  "INSERT OR REPLACE INTO moz_meta(key, value) " \
  "SELECT '" MOZ_META_KEY_ORIGIN_FRECENCY_COUNT "', " \
         "IFNULL((SELECT value FROM moz_meta WHERE key = '" \
                    MOZ_META_KEY_ORIGIN_FRECENCY_COUNT "'), 0) " \
         op " CAST(frecency > 0 AS INT) " \
  "FROM moz_origins WHERE prefix = OLD.prefix AND host = OLD.host " \
  "UNION " \
  "SELECT '" MOZ_META_KEY_ORIGIN_FRECENCY_SUM "', " \
         "IFNULL((SELECT value FROM moz_meta WHERE key = '" \
                    MOZ_META_KEY_ORIGIN_FRECENCY_SUM "'), 0) " \
         op " MAX(frecency, 0) " \
  "FROM moz_origins WHERE prefix = OLD.prefix AND host = OLD.host " \
  "UNION " \
  "SELECT '" MOZ_META_KEY_ORIGIN_FRECENCY_SUM_OF_SQUARES "', " \
         "IFNULL((SELECT value FROM moz_meta WHERE key = '" \
                    MOZ_META_KEY_ORIGIN_FRECENCY_SUM_OF_SQUARES "'), 0) " \
         op " (MAX(frecency, 0) * MAX(frecency, 0)) " \
  "FROM moz_origins WHERE prefix = OLD.prefix AND host = OLD.host "

// The next several triggers are a workaround for the lack of FOR EACH STATEMENT
// in Sqlite, until bug 871908 can be fixed properly.
//
// While doing inserts or deletes into moz_places, we accumulate the affected
// origins into a temp table. Afterwards, we delete everything from the temp
// table, causing the AFTER DELETE trigger to fire for it, which will then
// update moz_origins and the origin frecency stats. As a consequence, we also
// do this for updates to moz_places.frecency in order to make sure that changes
// to origins are serialized.
//
// Note this way we lose atomicity, crashing between the 2 queries may break the
// tables' coherency. So it's better to run those DELETE queries in a single
// transaction. Regardless, this is still better than hanging the browser for
// several minutes on a fast machine.

// This trigger runs on inserts into moz_places.
#define CREATE_PLACES_AFTERINSERT_TRIGGER NS_LITERAL_CSTRING( \
  "CREATE TEMP TRIGGER moz_places_afterinsert_trigger " \
  "AFTER INSERT ON moz_places FOR EACH ROW " \
  "BEGIN " \
    "SELECT store_last_inserted_id('moz_places', NEW.id); " \
    "INSERT OR IGNORE INTO moz_updateoriginsinsert_temp (place_id, prefix, host, frecency) " \
    "VALUES (NEW.id, get_prefix(NEW.url), get_host_and_port(NEW.url), NEW.frecency); " \
  "END" \
)
// This trigger corresponds to the previous trigger.  It runs on deletes on
// moz_updateoriginsinsert_temp -- logically, after inserts on moz_places.
#define CREATE_UPDATEORIGINSINSERT_AFTERDELETE_TRIGGER NS_LITERAL_CSTRING( \
  "CREATE TEMP TRIGGER moz_updateoriginsinsert_afterdelete_trigger " \
  "AFTER DELETE ON moz_updateoriginsinsert_temp FOR EACH ROW " \
  "BEGIN " \
    /* Deduct the origin's current contribution to frecency stats */ \
    UPDATE_ORIGIN_FRECENCY_STATS("-") "; " \
    "INSERT INTO moz_origins (prefix, host, frecency) " \
    "VALUES (OLD.prefix, OLD.host, MAX(OLD.frecency, 0)) " \
    "ON CONFLICT(prefix, host) DO UPDATE " \
    "SET frecency = frecency + OLD.frecency " \
    "WHERE OLD.frecency > 0; " \
    /* Add the origin's new contribution to frecency stats */ \
    UPDATE_ORIGIN_FRECENCY_STATS("+") "; " \
    "UPDATE moz_places SET origin_id = ( " \
      "SELECT id " \
      "FROM moz_origins " \
      "WHERE prefix = OLD.prefix AND host = OLD.host " \
    ") " \
    "WHERE id = OLD.place_id; " \
  "END" \
)

// This trigger runs on deletes on moz_places.
#define CREATE_PLACES_AFTERDELETE_TRIGGER NS_LITERAL_CSTRING( \
  "CREATE TEMP TRIGGER moz_places_afterdelete_trigger " \
  "AFTER DELETE ON moz_places FOR EACH ROW " \
  "BEGIN " \
    "INSERT INTO moz_updateoriginsdelete_temp (prefix, host, frecency_delta) " \
    "VALUES (get_prefix(OLD.url), get_host_and_port(OLD.url), -MAX(OLD.frecency, 0)) " \
    "ON CONFLICT(prefix, host) DO UPDATE " \
    "SET frecency_delta = frecency_delta - OLD.frecency " \
    "WHERE OLD.frecency > 0; " \
  "END " \
)
// This trigger corresponds to the previous trigger.  It runs on deletes on
// moz_updateoriginsdelete_temp -- logically, after deletes on moz_places.
#define CREATE_UPDATEORIGINSDELETE_AFTERDELETE_TRIGGER NS_LITERAL_CSTRING( \
  "CREATE TEMP TRIGGER moz_updateoriginsdelete_afterdelete_trigger " \
  "AFTER DELETE ON moz_updateoriginsdelete_temp FOR EACH ROW " \
  "BEGIN " \
    /* Deduct the origin's current contribution to frecency stats */ \
    UPDATE_ORIGIN_FRECENCY_STATS("-") "; " \
    "UPDATE moz_origins SET frecency = frecency + OLD.frecency_delta " \
    "WHERE prefix = OLD.prefix AND host = OLD.host; " \
    "DELETE FROM moz_origins " \
    "WHERE prefix = OLD.prefix AND host = OLD.host AND NOT EXISTS ( " \
      "SELECT id FROM moz_places " \
      "WHERE origin_id = moz_origins.id " \
      "LIMIT 1 " \
    "); " \
    /* Add the origin's new contribution to frecency stats */ \
    UPDATE_ORIGIN_FRECENCY_STATS("+") "; " \
    "DELETE FROM moz_icons " \
    "WHERE fixed_icon_url_hash = hash(fixup_url(OLD.host || '/favicon.ico')) " \
      "AND fixup_url(icon_url) = fixup_url(OLD.host || '/favicon.ico') " \
      "AND NOT EXISTS (SELECT 1 FROM moz_origins WHERE host = OLD.host " \
                                                   "OR host = fixup_url(OLD.host)); " \
  "END" \
)

// This trigger runs on updates to moz_places.frecency.
//
// However, we skip this when frecency changes are due to frecency decay since
// (1) decay updates all frecencies at once, so this trigger would run for each
// moz_place, which would be expensive; and (2) decay does not change the
// ordering of frecencies since all frecencies decay by the same percentage.
#define CREATE_PLACES_AFTERUPDATE_FRECENCY_TRIGGER NS_LITERAL_CSTRING( \
  "CREATE TEMP TRIGGER moz_places_afterupdate_frecency_trigger " \
  "AFTER UPDATE OF frecency ON moz_places FOR EACH ROW " \
  "WHEN NOT is_frecency_decaying() " \
  "BEGIN " \
    "INSERT INTO moz_updateoriginsupdate_temp (prefix, host, frecency_delta) " \
    "VALUES (get_prefix(NEW.url), get_host_and_port(NEW.url), MAX(NEW.frecency, 0) - MAX(OLD.frecency, 0)) " \
    "ON CONFLICT(prefix, host) DO UPDATE " \
    "SET frecency_delta = frecency_delta + EXCLUDED.frecency_delta; " \
  "END " \
)
// This trigger corresponds to the previous trigger.  It runs on deletes on
// moz_updateoriginsupdate_temp -- logically, after updates to
// moz_places.frecency.
#define CREATE_UPDATEORIGINSUPDATE_AFTERDELETE_TRIGGER NS_LITERAL_CSTRING( \
  "CREATE TEMP TRIGGER moz_updateoriginsupdate_afterdelete_trigger " \
  "AFTER DELETE ON moz_updateoriginsupdate_temp FOR EACH ROW " \
  "BEGIN " \
    /* Deduct the origin's current contribution to frecency stats */ \
    UPDATE_ORIGIN_FRECENCY_STATS("-") "; " \
    "UPDATE moz_origins " \
    "SET frecency = frecency + OLD.frecency_delta " \
    "WHERE prefix = OLD.prefix AND host = OLD.host; " \
    /* Add the origin's new contribution to frecency stats */ \
    UPDATE_ORIGIN_FRECENCY_STATS("+") "; " \
  "END" \
)

/**
 * This trigger removes a row from moz_openpages_temp when open_count reaches 0.
 *
 * @note this should be kept up-to-date with the definition in
 *       nsPlacesAutoComplete.js
 */
#define CREATE_REMOVEOPENPAGE_CLEANUP_TRIGGER NS_LITERAL_CSTRING( \
  "CREATE TEMPORARY TRIGGER moz_openpages_temp_afterupdate_trigger " \
  "AFTER UPDATE OF open_count ON moz_openpages_temp FOR EACH ROW " \
  "WHEN NEW.open_count = 0 " \
  "BEGIN " \
    "DELETE FROM moz_openpages_temp " \
    "WHERE url = NEW.url " \
      "AND userContextId = NEW.userContextId;" \
  "END" \
)

#define CREATE_BOOKMARKS_FOREIGNCOUNT_AFTERDELETE_TRIGGER NS_LITERAL_CSTRING( \
  "CREATE TEMP TRIGGER moz_bookmarks_foreign_count_afterdelete_trigger " \
  "AFTER DELETE ON moz_bookmarks FOR EACH ROW " \
  "BEGIN " \
    "UPDATE moz_places " \
    "SET foreign_count = foreign_count - 1 " \
    "WHERE id = OLD.fk;" \
  "END" \
)

#define CREATE_BOOKMARKS_FOREIGNCOUNT_AFTERINSERT_TRIGGER NS_LITERAL_CSTRING( \
  "CREATE TEMP TRIGGER moz_bookmarks_foreign_count_afterinsert_trigger " \
  "AFTER INSERT ON moz_bookmarks FOR EACH ROW " \
  "BEGIN " \
    "SELECT store_last_inserted_id('moz_bookmarks', NEW.id); " \
    "UPDATE moz_places " \
    "SET foreign_count = foreign_count + 1 " \
    "WHERE id = NEW.fk;" \
  "END" \
)

#define CREATE_BOOKMARKS_FOREIGNCOUNT_AFTERUPDATE_TRIGGER NS_LITERAL_CSTRING( \
  "CREATE TEMP TRIGGER moz_bookmarks_foreign_count_afterupdate_trigger " \
  "AFTER UPDATE OF fk ON moz_bookmarks FOR EACH ROW " \
  "BEGIN " \
    "UPDATE moz_places " \
    "SET foreign_count = foreign_count + 1 " \
    "WHERE id = NEW.fk;" \
    "UPDATE moz_places " \
    "SET foreign_count = foreign_count - 1 " \
    "WHERE id = OLD.fk;" \
  "END" \
)

#define CREATE_KEYWORDS_FOREIGNCOUNT_AFTERDELETE_TRIGGER NS_LITERAL_CSTRING( \
  "CREATE TEMP TRIGGER moz_keywords_foreign_count_afterdelete_trigger " \
  "AFTER DELETE ON moz_keywords FOR EACH ROW " \
  "BEGIN " \
    "UPDATE moz_places " \
    "SET foreign_count = foreign_count - 1 " \
    "WHERE id = OLD.place_id;" \
  "END" \
)

#define CREATE_KEYWORDS_FOREIGNCOUNT_AFTERINSERT_TRIGGER NS_LITERAL_CSTRING( \
  "CREATE TEMP TRIGGER moz_keyords_foreign_count_afterinsert_trigger " \
  "AFTER INSERT ON moz_keywords FOR EACH ROW " \
  "BEGIN " \
    "UPDATE moz_places " \
    "SET foreign_count = foreign_count + 1 " \
    "WHERE id = NEW.place_id;" \
  "END" \
)

#define CREATE_KEYWORDS_FOREIGNCOUNT_AFTERUPDATE_TRIGGER NS_LITERAL_CSTRING( \
  "CREATE TEMP TRIGGER moz_keywords_foreign_count_afterupdate_trigger " \
  "AFTER UPDATE OF place_id ON moz_keywords FOR EACH ROW " \
  "BEGIN " \
    "UPDATE moz_places " \
    "SET foreign_count = foreign_count + 1 " \
    "WHERE id = NEW.place_id; " \
    "UPDATE moz_places " \
    "SET foreign_count = foreign_count - 1 " \
    "WHERE id = OLD.place_id; " \
  "END" \
)

#define CREATE_ICONS_AFTERINSERT_TRIGGER NS_LITERAL_CSTRING( \
  "CREATE TEMP TRIGGER moz_icons_afterinsert_v1_trigger " \
  "AFTER INSERT ON moz_icons FOR EACH ROW " \
  "BEGIN " \
    "SELECT store_last_inserted_id('moz_icons', NEW.id); " \
  "END" \
)

#endif // __nsPlacesTriggers_h__
