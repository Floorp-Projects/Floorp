/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Places code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
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

#include "nsPlacesTables.h"

#ifndef __nsPlacesTriggers_h__
#define __nsPlacesTriggers_h__

/**
 * Exclude these visit types:
 *  0 - invalid
 *  4 - EMBED
 *  7 - DOWNLOAD
 **/
#define EXCLUDED_VISIT_TYPES "0, 4, 7"

/**
 * Trigger checks to ensure that at least one bookmark is still using a keyword
 * when any bookmark is deleted.  If there are no more bookmarks using it, the
 * keyword is deleted.
 */
#define CREATE_KEYWORD_VALIDITY_TRIGGER NS_LITERAL_CSTRING( \
  "CREATE TRIGGER moz_bookmarks_beforedelete_v1_trigger " \
  "BEFORE DELETE ON moz_bookmarks FOR EACH ROW " \
  "WHEN OLD.keyword_id NOT NULL " \
  "BEGIN " \
    "DELETE FROM moz_keywords " \
    "WHERE id = OLD.keyword_id " \
    "AND NOT EXISTS ( " \
      "SELECT id " \
      "FROM moz_bookmarks " \
      "WHERE keyword_id = OLD.keyword_id " \
      "AND id <> OLD.id " \
      "LIMIT 1 " \
    ");" \
  "END" \
)

/**
 * This trigger allows for an insertion into moz_places_view.  It enters the new
 * data into the temporary table, ensuring that the new id is one greater than
 * the largest id value found.
 */
#define CREATE_PLACES_VIEW_INSERT_TRIGGER NS_LITERAL_CSTRING( \
  "CREATE TEMPORARY TRIGGER moz_places_view_insert_trigger " \
  "INSTEAD OF INSERT " \
  "ON moz_places_view " \
  "BEGIN " \
    "INSERT INTO moz_places_temp (" MOZ_PLACES_COLUMNS ") " \
    "VALUES (MAX(IFNULL((SELECT MAX(id) FROM moz_places_temp), 0), " \
                "IFNULL((SELECT MAX(id) FROM moz_places), 0)) + 1," \
            "NEW.url, NEW.title, NEW.rev_host, " \
            "IFNULL(NEW.visit_count, 0), " /* enforce having a value */ \
            "NEW.hidden, NEW.typed, NEW.favicon_id, NEW.frecency, " \
            "NEW.last_visit_date);" \
  "END" \
)

/**
 * This trigger allows for the deletion of a record in moz_places_view.  It
 * removes any entry in the temporary table, and any entry in the permanent
 * table as well.
 */
#define CREATE_PLACES_VIEW_DELETE_TRIGGER NS_LITERAL_CSTRING( \
  "CREATE TEMPORARY TRIGGER moz_places_view_delete_trigger " \
  "INSTEAD OF DELETE " \
  "ON moz_places_view " \
  "BEGIN " \
    "DELETE FROM moz_places_temp " \
    "WHERE id = OLD.id; " \
    "DELETE FROM moz_places " \
    "WHERE id = OLD.id; " \
  "END" \
)

/**
 * This trigger allows for updates to a record in moz_places_view.  It first
 * copies the row from the permanent table over to the temp table if it does not
 * exist in the temporary table.  Then, it will update the temporary table with
 * the new data.
 * We use INSERT OR IGNORE to avoid looking if the place already exists in the
 * temp table.
 */
#define CREATE_PLACES_VIEW_UPDATE_TRIGGER NS_LITERAL_CSTRING( \
  "CREATE TEMPORARY TRIGGER moz_places_view_update_trigger " \
  "INSTEAD OF UPDATE " \
  "ON moz_places_view " \
  "BEGIN " \
    "INSERT OR IGNORE INTO moz_places_temp (" MOZ_PLACES_COLUMNS ") " \
    "SELECT " MOZ_PLACES_COLUMNS " FROM moz_places " \
    "WHERE id = OLD.id; " \
    "UPDATE moz_places_temp " \
    "SET url = IFNULL(NEW.url, OLD.url), " \
        "title = IFNULL(NEW.title, OLD.title), " \
        "rev_host = IFNULL(NEW.rev_host, OLD.rev_host), " \
        "visit_count = IFNULL(NEW.visit_count, OLD.visit_count), " \
        "hidden = IFNULL(NEW.hidden, OLD.hidden), " \
        "typed = IFNULL(NEW.typed, OLD.typed), " \
        "favicon_id = IFNULL(NEW.favicon_id, OLD.favicon_id), " \
        "frecency = IFNULL(NEW.frecency, OLD.frecency), " \
        "last_visit_date = IFNULL(NEW.last_visit_date, OLD.last_visit_date) " \
    "WHERE id = OLD.id; " \
  "END" \
)

/**
 * This trigger allows for an insertion into  moz_historyvisits_view.  It enters
 * the new data into the temporary table, ensuring that the new id is one
 * greater than the largest id value found.  It then updates moz_places_view
 * with the new visit count.
 * We use INSERT OR IGNORE to avoid looking if the place already exists in the
 * temp table.
 * In this case when updating last_visit_date we can simply check the maximum
 * between new visit date and the actual cached value (or 0 if it is NULL).
 */
#define CREATE_HISTORYVISITS_VIEW_INSERT_TRIGGER NS_LITERAL_CSTRING( \
  "CREATE TEMPORARY TRIGGER moz_historyvisits_view_insert_trigger " \
  "INSTEAD OF INSERT " \
  "ON moz_historyvisits_view " \
  "BEGIN " \
    "INSERT INTO moz_historyvisits_temp (" MOZ_HISTORYVISITS_COLUMNS ") " \
    "VALUES (MAX(IFNULL((SELECT MAX(id) FROM moz_historyvisits_temp), 0), " \
                "IFNULL((SELECT MAX(id) FROM moz_historyvisits), 0)) + 1, " \
            "NEW.from_visit, NEW.place_id, NEW.visit_date, NEW.visit_type, " \
            "NEW.session); " \
    "INSERT OR IGNORE INTO moz_places_temp (" MOZ_PLACES_COLUMNS ") " \
    "SELECT " MOZ_PLACES_COLUMNS " FROM moz_places " \
    "WHERE id = NEW.place_id " \
    "AND NEW.visit_type NOT IN (" EXCLUDED_VISIT_TYPES "); " \
    "UPDATE moz_places_temp " \
    "SET visit_count = visit_count + 1 " \
    "WHERE id = NEW.place_id " \
    "AND NEW.visit_type NOT IN (" EXCLUDED_VISIT_TYPES "); " \
    "UPDATE moz_places_temp " \
    "SET last_visit_date = MAX(IFNULL(last_visit_date, 0), NEW.visit_date)" \
    "WHERE id = NEW.place_id;" \
  "END" \
)

/**
 * This trigger allows for the deletion of a record in moz_historyvisits_view.
 * It removes any entry in the temporary table, and removes any entry in the
 * permanent table as well.  It then updates moz_places_view with the new visit
 * count.
 * We use INSERT OR IGNORE to avoid looking if the place already exists in the
 * temp table.
 */
#define CREATE_HISTORYVISITS_VIEW_DELETE_TRIGGER NS_LITERAL_CSTRING( \
  "CREATE TEMPORARY TRIGGER moz_historyvisits_view_delete_trigger " \
  "INSTEAD OF DELETE " \
  "ON moz_historyvisits_view " \
  "BEGIN " \
    "DELETE FROM moz_historyvisits_temp " \
    "WHERE id = OLD.id; " \
    "DELETE FROM moz_historyvisits " \
    "WHERE id = OLD.id; " \
    "INSERT OR IGNORE INTO moz_places_temp (" MOZ_PLACES_COLUMNS ") " \
    "SELECT " MOZ_PLACES_COLUMNS " FROM moz_places " \
    "WHERE id = OLD.place_id " \
    "AND OLD.visit_type NOT IN (" EXCLUDED_VISIT_TYPES "); " \
    "UPDATE moz_places_temp " \
    "SET visit_count = visit_count - 1 " \
    "WHERE id = OLD.place_id " \
    "AND OLD.visit_type NOT IN (" EXCLUDED_VISIT_TYPES "); " \
    "UPDATE moz_places_temp " \
    "SET last_visit_date = " \
      "(SELECT visit_date FROM moz_historyvisits_temp " \
       "WHERE place_id = OLD.place_id " \
       "UNION ALL " \
       "SELECT visit_date FROM moz_historyvisits " \
       "WHERE place_id = OLD.place_id " \
       "ORDER BY visit_date DESC LIMIT 1) " \
    "WHERE id = OLD.place_id; " \
  "END" \
)

/**
 * This trigger allows for updates to a record in moz_historyvisits_view.  It
 * first copies the row from the permanent table over to the temp table if it
 * does not exist in the temporary table.  Then it will update the temporary
 * table with the new data.
 * We use INSERT OR IGNORE to avoid looking if the visit already exists in the
 * temp table.
 */
#define CREATE_HISTORYVISITS_VIEW_UPDATE_TRIGGER NS_LITERAL_CSTRING( \
  "CREATE TEMPORARY TRIGGER moz_historyvisits_view_update_trigger " \
  "INSTEAD OF UPDATE " \
  "ON moz_historyvisits_view " \
  "BEGIN " \
    "INSERT OR IGNORE INTO moz_historyvisits_temp (" MOZ_HISTORYVISITS_COLUMNS ") " \
    "SELECT " MOZ_HISTORYVISITS_COLUMNS " FROM moz_historyvisits " \
    "WHERE id = OLD.id; " \
    "UPDATE moz_historyvisits_temp " \
    "SET from_visit = IFNULL(NEW.from_visit, OLD.from_visit), " \
        "place_id = IFNULL(NEW.place_id, OLD.place_id), " \
        "visit_date = IFNULL(NEW.visit_date, OLD.visit_date), " \
        "visit_type = IFNULL(NEW.visit_type, OLD.visit_type), " \
        "session = IFNULL(NEW.session, OLD.session) " \
    "WHERE id = OLD.id; " \
  "END" \
)

/**
 * This trigger moves the data out of a temporary table into the permanent one
 * before deleting from the temporary table.
 *
 * Note - it's OK to use an INSERT OR REPLACE here because the only conflict
 * that will happen is the primary key.  As a result, the row will be deleted,
 * and the replacement will be inserted with the same id.
 */
#define CREATE_TEMP_SYNC_TRIGGER_BASE(__table, __columns) NS_LITERAL_CSTRING( \
  "CREATE TEMPORARY TRIGGER " __table "_beforedelete_trigger " \
  "BEFORE DELETE ON " __table "_temp FOR EACH ROW " \
  "BEGIN " \
    "INSERT OR REPLACE INTO " __table " (" __columns ") " \
    "SELECT " __columns " FROM " __table "_temp " \
    "WHERE id = OLD.id;" \
  "END" \
)
#define CREATE_MOZ_PLACES_SYNC_TRIGGER \
  CREATE_TEMP_SYNC_TRIGGER_BASE("moz_places", MOZ_PLACES_COLUMNS)
#define CREATE_MOZ_HISTORYVISITS_SYNC_TRIGGER \
  CREATE_TEMP_SYNC_TRIGGER_BASE("moz_historyvisits", MOZ_HISTORYVISITS_COLUMNS)

#endif // __nsPlacesTriggers_h__
