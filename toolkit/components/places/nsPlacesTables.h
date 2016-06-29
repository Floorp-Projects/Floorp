/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsPlacesTables_h__
#define __nsPlacesTables_h__


#define CREATE_MOZ_PLACES NS_LITERAL_CSTRING( \
    "CREATE TABLE moz_places ( " \
    "  id INTEGER PRIMARY KEY" \
    ", url LONGVARCHAR" \
    ", title LONGVARCHAR" \
    ", rev_host LONGVARCHAR" \
    ", visit_count INTEGER DEFAULT 0" \
    ", hidden INTEGER DEFAULT 0 NOT NULL" \
    ", typed INTEGER DEFAULT 0 NOT NULL" \
    ", favicon_id INTEGER" \
    ", frecency INTEGER DEFAULT -1 NOT NULL" \
    ", last_visit_date INTEGER " \
    ", guid TEXT" \
    ", foreign_count INTEGER DEFAULT 0 NOT NULL" \
    ", url_hash INTEGER DEFAULT 0 NOT NULL " \
  ")" \
)

#define CREATE_MOZ_HISTORYVISITS NS_LITERAL_CSTRING( \
  "CREATE TABLE moz_historyvisits (" \
    "  id INTEGER PRIMARY KEY" \
    ", from_visit INTEGER" \
    ", place_id INTEGER" \
    ", visit_date INTEGER" \
    ", visit_type INTEGER" \
    ", session INTEGER" \
  ")" \
)


#define CREATE_MOZ_INPUTHISTORY NS_LITERAL_CSTRING( \
  "CREATE TABLE moz_inputhistory (" \
    "  place_id INTEGER NOT NULL" \
    ", input LONGVARCHAR NOT NULL" \
    ", use_count INTEGER" \
    ", PRIMARY KEY (place_id, input)" \
  ")" \
)

#define CREATE_MOZ_ANNOS NS_LITERAL_CSTRING( \
  "CREATE TABLE moz_annos (" \
    "  id INTEGER PRIMARY KEY" \
    ", place_id INTEGER NOT NULL" \
    ", anno_attribute_id INTEGER" \
    ", mime_type VARCHAR(32) DEFAULT NULL" \
    ", content LONGVARCHAR" \
    ", flags INTEGER DEFAULT 0" \
    ", expiration INTEGER DEFAULT 0" \
    ", type INTEGER DEFAULT 0" \
    ", dateAdded INTEGER DEFAULT 0" \
    ", lastModified INTEGER DEFAULT 0" \
  ")" \
)

#define CREATE_MOZ_ANNO_ATTRIBUTES NS_LITERAL_CSTRING( \
  "CREATE TABLE moz_anno_attributes (" \
    "  id INTEGER PRIMARY KEY" \
    ", name VARCHAR(32) UNIQUE NOT NULL" \
  ")" \
)

#define CREATE_MOZ_ITEMS_ANNOS NS_LITERAL_CSTRING( \
  "CREATE TABLE moz_items_annos (" \
    "  id INTEGER PRIMARY KEY" \
    ", item_id INTEGER NOT NULL" \
    ", anno_attribute_id INTEGER" \
    ", mime_type VARCHAR(32) DEFAULT NULL" \
    ", content LONGVARCHAR" \
    ", flags INTEGER DEFAULT 0" \
    ", expiration INTEGER DEFAULT 0" \
    ", type INTEGER DEFAULT 0" \
    ", dateAdded INTEGER DEFAULT 0" \
    ", lastModified INTEGER DEFAULT 0" \
  ")" \
)

#define CREATE_MOZ_FAVICONS NS_LITERAL_CSTRING( \
  "CREATE TABLE moz_favicons (" \
    "  id INTEGER PRIMARY KEY" \
    ", url LONGVARCHAR UNIQUE" \
    ", data BLOB" \
    ", mime_type VARCHAR(32)" \
    ", expiration LONG" \
  ")" \
)

#define CREATE_MOZ_BOOKMARKS NS_LITERAL_CSTRING( \
  "CREATE TABLE moz_bookmarks (" \
    "  id INTEGER PRIMARY KEY" \
    ", type INTEGER" \
    ", fk INTEGER DEFAULT NULL" /* place_id */ \
    ", parent INTEGER" \
    ", position INTEGER" \
    ", title LONGVARCHAR" \
    ", keyword_id INTEGER" \
    ", folder_type TEXT" \
    ", dateAdded INTEGER" \
    ", lastModified INTEGER" \
    ", guid TEXT" \
  ")" \
)

#define CREATE_MOZ_KEYWORDS NS_LITERAL_CSTRING( \
  "CREATE TABLE moz_keywords (" \
    "  id INTEGER PRIMARY KEY AUTOINCREMENT" \
    ", keyword TEXT UNIQUE" \
    ", place_id INTEGER" \
    ", post_data TEXT" \
  ")" \
)

#define CREATE_MOZ_HOSTS NS_LITERAL_CSTRING( \
  "CREATE TABLE moz_hosts (" \
    "  id INTEGER PRIMARY KEY" \
    ", host TEXT NOT NULL UNIQUE" \
    ", frecency INTEGER" \
    ", typed INTEGER NOT NULL DEFAULT 0" \
    ", prefix TEXT" \
  ")" \
)

// Note: this should be kept up-to-date with the definition in
//       nsPlacesAutoComplete.js.
#define CREATE_MOZ_OPENPAGES_TEMP NS_LITERAL_CSTRING( \
  "CREATE TEMP TABLE moz_openpages_temp (" \
    "  url TEXT PRIMARY KEY" \
    ", open_count INTEGER" \
  ")" \
)

// This table is used, along with moz_places_afterdelete_trigger, to update
// hosts after places removals. During a DELETE FROM moz_places, hosts are
// accumulated into this table, then a DELETE FROM moz_updatehosts_temp will
// take care of updating the moz_hosts table for every modified host.
// See CREATE_PLACES_AFTERDELETE_TRIGGER in nsPlacestriggers.h for details.
#define CREATE_UPDATEHOSTS_TEMP NS_LITERAL_CSTRING( \
  "CREATE TEMP TABLE moz_updatehosts_temp (" \
    "  host TEXT PRIMARY KEY " \
  ") WITHOUT ROWID " \
)

#endif // __nsPlacesTables_h__
