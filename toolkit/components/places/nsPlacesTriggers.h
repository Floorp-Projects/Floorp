/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPlacesTables.h"

#ifndef __nsPlacesTriggers_h__
#  define __nsPlacesTriggers_h__

/**
 * These visit types are excluded from visit_count:
 *  0 - invalid
 *  4 - EMBED
 *  7 - DOWNLOAD
 *  8 - FRAMED_LINK
 *  9 - RELOAD
 **/
#  define VISIT_COUNT_INC(field) \
    "(CASE WHEN " field " IN (0, 4, 7, 8, 9) THEN 0 ELSE 1 END) "

/**
 * This triggers update visit_count and last_visit_date based on historyvisits
 * table changes.
 */
#  define CREATE_HISTORYVISITS_AFTERINSERT_TRIGGER \
    nsLiteralCString(                                                         \
        "CREATE TEMP TRIGGER moz_historyvisits_afterinsert_v2_trigger "       \
        "AFTER INSERT ON moz_historyvisits FOR EACH ROW "                     \
        "BEGIN "                                                              \
        "SELECT invalidate_days_of_history();"                                \
        "SELECT store_last_inserted_id('moz_historyvisits', NEW.id); "        \
        "UPDATE moz_places SET "                                              \
        "visit_count = visit_count + " VISIT_COUNT_INC("NEW.visit_type") ", " \
        "recalc_frecency = 1, "                                               \
        "recalc_alt_frecency = 1, "                                           \
        "last_visit_date = MAX(IFNULL(last_visit_date, 0), NEW.visit_date) "  \
        "WHERE id = NEW.place_id;"                                            \
        "END")

#  define CREATE_HISTORYVISITS_AFTERDELETE_TRIGGER \
    nsLiteralCString(                                                         \
        "CREATE TEMP TRIGGER moz_historyvisits_afterdelete_v2_trigger "       \
        "AFTER DELETE ON moz_historyvisits FOR EACH ROW "                     \
        "BEGIN "                                                              \
        "SELECT invalidate_days_of_history();"                                \
        "UPDATE moz_places SET "                                              \
        "visit_count = visit_count - " VISIT_COUNT_INC("OLD.visit_type") ", " \
        "recalc_frecency = (frecency <> 0), "                                 \
        "recalc_alt_frecency = (frecency <> 0), "                             \
        "last_visit_date = (SELECT visit_date FROM moz_historyvisits "        \
        "WHERE place_id = OLD.place_id "                                      \
        "ORDER BY visit_date DESC LIMIT 1) "                                  \
        "WHERE id = OLD.place_id;"                                            \
        "END")

// This trigger stores the last_inserted_id, and inserts entries in moz_origins
// when pages are added.
#  define CREATE_PLACES_AFTERINSERT_TRIGGER                                 \
    nsLiteralCString(                                                       \
        "CREATE TEMP TRIGGER moz_places_afterinsert_trigger "               \
        "AFTER INSERT ON moz_places FOR EACH ROW "                          \
        "BEGIN "                                                            \
        "SELECT store_last_inserted_id('moz_places', NEW.id); "             \
        "INSERT INTO moz_origins "                                          \
        "  (prefix, host, frecency, recalc_frecency, recalc_alt_frecency) " \
        "VALUES (get_prefix(NEW.url), get_host_and_port(NEW.url), "         \
        "        NEW.frecency, 1, 1)  "                                     \
        "ON CONFLICT(prefix, host) DO UPDATE "                              \
        "  SET recalc_frecency = 1, recalc_alt_frecency = 1 "               \
        "  WHERE EXCLUDED.recalc_frecency = 0 OR "                          \
        "        EXCLUDED.recalc_alt_frecency = 0; "                        \
        "UPDATE moz_places SET origin_id = ( "                              \
        "  SELECT id "                                                      \
        "  FROM moz_origins "                                               \
        "  WHERE prefix = get_prefix(NEW.url) "                             \
        "    AND host = get_host_and_port(NEW.url) "                        \
        ") "                                                                \
        "WHERE id = NEW.id; "                                               \
        "END")

// This trigger is a workaround for the lack of FOR EACH STATEMENT in Sqlite.
// While doing deletes into moz_places, we accumulate the affected origins into
// a temp table. Afterwards, we delete everything from the temp table, causing
// the AFTER DELETE trigger to fire for it, which will then update moz_origins.
//
// Note this way we lose atomicity, crashing between the 2 queries may break the
// tables' coherency. So it's better to run those DELETE queries in the same
// transaction as the original change.
#  define CREATE_PLACES_AFTERDELETE_TRIGGER                                    \
    nsLiteralCString(                                                          \
        "CREATE TEMP TRIGGER moz_places_afterdelete_trigger "                  \
        "AFTER DELETE ON moz_places FOR EACH ROW "                             \
        "BEGIN "                                                               \
        "INSERT OR IGNORE INTO moz_updateoriginsdelete_temp (prefix, host) "   \
        "VALUES (get_prefix(OLD.url), get_host_and_port(OLD.url)); "           \
        "UPDATE moz_origins SET recalc_frecency = 1, recalc_alt_frecency = 1 " \
        "WHERE id = OLD.origin_id; "                                           \
        "END ")

// This is an alternate version of CREATE_PLACES_AFTERDELETE_TRIGGER, with
// support for previews tombstones. Only one of these should be used at the
// same time
#  define CREATE_PLACES_AFTERDELETE_WPREVIEWS_TRIGGER                          \
    nsLiteralCString(                                                          \
        "CREATE TEMP TRIGGER moz_places_afterdelete_wpreviews_trigger "        \
        "AFTER DELETE ON moz_places FOR EACH ROW "                             \
        "BEGIN "                                                               \
        "INSERT OR IGNORE INTO moz_updateoriginsdelete_temp (prefix, host) "   \
        "VALUES (get_prefix(OLD.url), get_host_and_port(OLD.url)); "           \
        "UPDATE moz_origins SET recalc_frecency = 1, recalc_alt_frecency = 1 " \
        "WHERE id = OLD.origin_id; "                                           \
        "INSERT OR IGNORE INTO moz_previews_tombstones VALUES "                \
        "(md5hex(OLD.url));"                                                   \
        "END ")

// This is the supporting table for the "AFTER DELETE ON moz_places" triggers.
#  define CREATE_UPDATEORIGINSDELETE_AFTERDELETE_TRIGGER                   \
    nsLiteralCString(                                                      \
        "CREATE TEMP TRIGGER moz_updateoriginsdelete_afterdelete_trigger " \
        "AFTER DELETE ON moz_updateoriginsdelete_temp FOR EACH ROW "       \
        "BEGIN "                                                           \
        "DELETE FROM moz_origins "                                         \
        "WHERE prefix = OLD.prefix AND host = OLD.host "                   \
        "AND NOT EXISTS ( "                                                \
        "    SELECT id FROM moz_places "                                   \
        "    WHERE origin_id = moz_origins.id "                            \
        "); "                                                              \
        "END")

// This trigger runs on updates to moz_places.frecency.
//
// However, we skip this when frecency changes are due to frecency decay
// since (1) decay updates all frecencies at once, so this trigger would
// run for each moz_place, which would be expensive; and (2) decay does
// not change the ordering of frecencies since all frecencies decay by
// the same percentage.
#  define CREATE_PLACES_AFTERUPDATE_FRECENCY_TRIGGER                           \
    nsLiteralCString(                                                          \
        "CREATE TEMP TRIGGER moz_places_afterupdate_frecency_trigger "         \
        "AFTER UPDATE OF frecency ON moz_places FOR EACH ROW "                 \
        "WHEN NOT is_frecency_decaying() "                                     \
        "BEGIN "                                                               \
        "UPDATE moz_places SET recalc_frecency = 0 WHERE id = NEW.id; "        \
        "UPDATE moz_origins SET recalc_frecency = 1, recalc_alt_frecency = 1 " \
        "WHERE id = NEW.origin_id; "                                           \
        "END ")

// Runs when recalc_frecency is set to 1
#  define CREATE_PLACES_AFTERUPDATE_RECALC_FRECENCY_TRIGGER                   \
    nsLiteralCString(                                                         \
        "CREATE TEMP TRIGGER moz_places_afterupdate_recalc_frecency_trigger " \
        "AFTER UPDATE OF recalc_frecency ON moz_places FOR EACH ROW "         \
        "WHEN NEW.recalc_frecency = 1 "                                       \
        "BEGIN "                                                              \
        "  SELECT set_should_start_frecency_recalculation();"                 \
        "END")
#  define CREATE_ORIGINS_AFTERUPDATE_RECALC_FRECENCY_TRIGGER                   \
    nsLiteralCString(                                                          \
        "CREATE TEMP TRIGGER moz_origins_afterupdate_recalc_frecency_trigger " \
        "AFTER UPDATE OF recalc_frecency ON moz_origins FOR EACH ROW "         \
        "WHEN NEW.recalc_frecency = 1 "                                        \
        "BEGIN "                                                               \
        "  SELECT set_should_start_frecency_recalculation();"                  \
        "END")

// Runs when origin frecency is set to 0.
// This is in addition to moz_updateoriginsdelete_afterdelete_trigger, as a
// sanity check to ensure orphan origins don't stay around. We cannot just rely
// on this because it runs delayed and in the meanwhile the existence of the
// origin may impact user's privacy.
#  define CREATE_ORIGINS_AFTERUPDATE_FRECENCY_TRIGGER                   \
    nsLiteralCString(                                                   \
        "CREATE TEMP TRIGGER moz_origins_afterupdate_frecency_trigger " \
        "AFTER UPDATE OF recalc_frecency ON moz_origins FOR EACH ROW "  \
        "WHEN NEW.frecency = 0 AND OLD.frecency > 0 "                   \
        "BEGIN "                                                        \
        "DELETE FROM moz_origins "                                      \
        "WHERE id = NEW.id AND NOT EXISTS ( "                           \
        "  SELECT id FROM moz_places WHERE origin_id = NEW.id "         \
        "); "                                                           \
        "END")

/**
 * This trigger removes a row from moz_openpages_temp when open_count
 * reaches 0.
 *
 * @note this should be kept up-to-date with the definition in
 *       nsPlacesAutoComplete.js
 */
#  define CREATE_REMOVEOPENPAGE_CLEANUP_TRIGGER                            \
    nsLiteralCString(                                                      \
        "CREATE TEMPORARY TRIGGER moz_openpages_temp_afterupdate_trigger " \
        "AFTER UPDATE OF open_count ON moz_openpages_temp FOR EACH ROW "   \
        "WHEN NEW.open_count = 0 "                                         \
        "BEGIN "                                                           \
        "DELETE FROM moz_openpages_temp "                                  \
        "WHERE url = NEW.url "                                             \
        "AND userContextId = NEW.userContextId;"                           \
        "END")

/**
 * Any bookmark operation should recalculate frecency, apart from place:
 * queries.
 */
#  define IS_PLACE_QUERY                            \
    " url_hash BETWEEN hash('place', 'prefix_lo') " \
    "              AND hash('place', 'prefix_hi') "

#  define CREATE_BOOKMARKS_FOREIGNCOUNT_AFTERDELETE_TRIGGER                    \
    nsLiteralCString(                                                          \
        "CREATE TEMP TRIGGER moz_bookmarks_foreign_count_afterdelete_trigger " \
        "AFTER DELETE ON moz_bookmarks FOR EACH ROW "                          \
        "BEGIN "                                                               \
        "UPDATE moz_places "                                                   \
        "SET foreign_count = foreign_count - 1 "                               \
        ",   recalc_frecency = NOT " IS_PLACE_QUERY                            \
        ",   recalc_alt_frecency = NOT " IS_PLACE_QUERY                        \
        "WHERE id = OLD.fk;"                                                   \
        "END")

/**
 * Currently expiration skips anything with frecency = -1, since that is
 * the default value for new page insertions. Unfortunately adding and
 * immediately removing a bookmark will generate a page with frecency =
 * -1 that would never be expired until visited. As a temporary
 * workaround we set frecency to 1 on bookmark addition if it was set to
 * -1. This is not elegant, but it will be fixed by Bug 1475582 once
 * removing bookmarks will immediately take care of removing orphan
 * pages. Note setting frecency resets recalc_frecency, so do it first.
 */
#  define CREATE_BOOKMARKS_FOREIGNCOUNT_AFTERINSERT_TRIGGER                    \
    nsLiteralCString(                                                          \
        "CREATE TEMP TRIGGER moz_bookmarks_foreign_count_afterinsert_trigger " \
        "AFTER INSERT ON moz_bookmarks FOR EACH ROW "                          \
        "BEGIN "                                                               \
        "SELECT store_last_inserted_id('moz_bookmarks', NEW.id); "             \
        "SELECT note_sync_change() WHERE NEW.syncChangeCounter > 0; "          \
        "UPDATE moz_places "                                                   \
        "SET frecency = (CASE WHEN " IS_PLACE_QUERY                            \
        "                THEN 0 ELSE 1 END) "                                  \
        "WHERE frecency = -1 AND id = NEW.fk;"                                 \
        "UPDATE moz_places "                                                   \
        "SET foreign_count = foreign_count + 1 "                               \
        ",   hidden = " IS_PLACE_QUERY                                         \
        ",   recalc_frecency = NOT " IS_PLACE_QUERY                            \
        ",   recalc_alt_frecency = NOT " IS_PLACE_QUERY                        \
        "WHERE id = NEW.fk;"                                                   \
        "END")

#  define CREATE_BOOKMARKS_FOREIGNCOUNT_AFTERUPDATE_TRIGGER                    \
    nsLiteralCString(                                                          \
        "CREATE TEMP TRIGGER moz_bookmarks_foreign_count_afterupdate_trigger " \
        "AFTER UPDATE OF fk, syncChangeCounter ON moz_bookmarks FOR EACH ROW " \
        "BEGIN "                                                               \
        "SELECT note_sync_change() "                                           \
        "WHERE NEW.syncChangeCounter <> OLD.syncChangeCounter; "               \
        "UPDATE moz_places "                                                   \
        "SET foreign_count = foreign_count + 1 "                               \
        ",   hidden = " IS_PLACE_QUERY                                         \
        ",   recalc_frecency = NOT " IS_PLACE_QUERY                            \
        ",   recalc_alt_frecency = NOT " IS_PLACE_QUERY                        \
        "WHERE OLD.fk <> NEW.fk AND id = NEW.fk;"                              \
        "UPDATE moz_places "                                                   \
        "SET foreign_count = foreign_count - 1 "                               \
        ",   recalc_frecency = NOT " IS_PLACE_QUERY                            \
        ",   recalc_alt_frecency = NOT " IS_PLACE_QUERY                        \
        "WHERE OLD.fk <> NEW.fk AND id = OLD.fk;"                              \
        "END")

#  define CREATE_KEYWORDS_FOREIGNCOUNT_AFTERDELETE_TRIGGER                    \
    nsLiteralCString(                                                         \
        "CREATE TEMP TRIGGER moz_keywords_foreign_count_afterdelete_trigger " \
        "AFTER DELETE ON moz_keywords FOR EACH ROW "                          \
        "BEGIN "                                                              \
        "UPDATE moz_places "                                                  \
        "SET foreign_count = foreign_count - 1 "                              \
        "WHERE id = OLD.place_id;"                                            \
        "END")

#  define CREATE_KEYWORDS_FOREIGNCOUNT_AFTERINSERT_TRIGGER                    \
    nsLiteralCString(                                                         \
        "CREATE TEMP TRIGGER moz_keywords_foreign_count_afterinsert_trigger " \
        "AFTER INSERT ON moz_keywords FOR EACH ROW "                          \
        "BEGIN "                                                              \
        "UPDATE moz_places "                                                  \
        "SET foreign_count = foreign_count + 1 "                              \
        "WHERE id = NEW.place_id;"                                            \
        "END")

#  define CREATE_KEYWORDS_FOREIGNCOUNT_AFTERUPDATE_TRIGGER                    \
    nsLiteralCString(                                                         \
        "CREATE TEMP TRIGGER moz_keywords_foreign_count_afterupdate_trigger " \
        "AFTER UPDATE OF place_id ON moz_keywords FOR EACH ROW "              \
        "BEGIN "                                                              \
        "UPDATE moz_places "                                                  \
        "SET foreign_count = foreign_count + 1 "                              \
        "WHERE id = NEW.place_id; "                                           \
        "UPDATE moz_places "                                                  \
        "SET foreign_count = foreign_count - 1 "                              \
        "WHERE id = OLD.place_id; "                                           \
        "END")

#  define CREATE_ICONS_AFTERINSERT_TRIGGER                      \
    nsLiteralCString(                                           \
        "CREATE TEMP TRIGGER moz_icons_afterinsert_v1_trigger " \
        "AFTER INSERT ON moz_icons FOR EACH ROW "               \
        "BEGIN "                                                \
        "SELECT store_last_inserted_id('moz_icons', NEW.id); "  \
        "END")

#  define CREATE_BOOKMARKS_DELETED_AFTERINSERT_TRIGGER                      \
    nsLiteralCString(                                                       \
        "CREATE TEMP TRIGGER moz_bookmarks_deleted_afterinsert_v1_trigger " \
        "AFTER INSERT ON moz_bookmarks_deleted FOR EACH ROW "               \
        "BEGIN "                                                            \
        "SELECT note_sync_change(); "                                       \
        "END")

#  define CREATE_BOOKMARKS_DELETED_AFTERDELETE_TRIGGER                      \
    nsLiteralCString(                                                       \
        "CREATE TEMP TRIGGER moz_bookmarks_deleted_afterdelete_v1_trigger " \
        "AFTER DELETE ON moz_bookmarks_deleted FOR EACH ROW "               \
        "BEGIN "                                                            \
        "SELECT note_sync_change(); "                                       \
        "END")

// This trigger removes orphan search terms when interactions are
// removed from the metadata table.
#  define CREATE_PLACES_METADATA_AFTERDELETE_TRIGGER                   \
    nsLiteralCString(                                                  \
        "CREATE TEMP TRIGGER moz_places_metadata_afterdelete_trigger " \
        "AFTER DELETE ON moz_places_metadata "                         \
        "FOR EACH ROW "                                                \
        "BEGIN "                                                       \
        "DELETE FROM moz_places_metadata_search_queries "              \
        "WHERE id = OLD.search_query_id AND NOT EXISTS ("              \
        "SELECT id FROM moz_places_metadata "                          \
        "WHERE search_query_id = OLD.search_query_id "                 \
        "); "                                                          \
        "END")

// since moz_places_extra is really just storing json, there could be a
// scenario where we have a valid row but empty json -- we should make sure
// we have triggers to remove any such rows
#  define CREATE_MOZ_PLACES_EXTRA_AFTERUPDATE_TRIGGER                 \
    nsLiteralCString(                                                 \
        "CREATE TEMP TRIGGER moz_places_extra_trigger "               \
        "AFTER UPDATE ON moz_places_extra FOR EACH ROW "              \
        "WHEN (NEW.sync_json = '' OR NEW.sync_json = '{}')"           \
        "BEGIN "                                                      \
        "DELETE FROM moz_places_extra WHERE place_id = NEW.place_id;" \
        "END")

#  define CREATE_MOZ_HISTORYVISITS_AFTERUPDATE_TRIGGER                       \
    nsLiteralCString(                                                        \
        "CREATE TEMP TRIGGER moz_historyvisits_extra_trigger "               \
        "AFTER UPDATE ON moz_historyvisits_extra FOR EACH ROW "              \
        "WHEN (NEW.sync_json = '' OR NEW.sync_json = '{}')"                  \
        "BEGIN "                                                             \
        "DELETE FROM moz_historyvisits_extra WHERE visit_id = NEW.visit_id;" \
        "END")

#endif  // __nsPlacesTriggers_h__
