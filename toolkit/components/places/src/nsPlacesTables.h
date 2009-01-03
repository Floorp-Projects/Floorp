/* vim: sw=2 ts=2 sts=2 expandtab
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

#ifndef __nsPlacesTables_h__
#define __nsPlacesTables_h__

#define CREATE_MOZ_PLACES_BASE(__name, __temporary) NS_LITERAL_CSTRING( \
  "CREATE " __temporary " TABLE " __name " ( " \
    "  id INTEGER PRIMARY KEY" \
    ", url LONGVARCHAR" \
    ", title LONGVARCHAR" \
    ", rev_host LONGVARCHAR" \
    ", visit_count INTEGER DEFAULT 0" \
    ", hidden INTEGER DEFAULT 0 NOT NULL" \
    ", typed INTEGER DEFAULT 0 NOT NULL" \
    ", favicon_id INTEGER" \
    ", frecency INTEGER DEFAULT -1 NOT NULL" \
  ")" \
)
#define CREATE_MOZ_PLACES CREATE_MOZ_PLACES_BASE("moz_places", "")
#define CREATE_MOZ_PLACES_TEMP CREATE_MOZ_PLACES_BASE("moz_places_temp", "TEMP")
#define CREATE_MOZ_PLACES_VIEW NS_LITERAL_CSTRING( \
  "CREATE TEMPORARY VIEW moz_places_view AS " \
  "SELECT * FROM moz_places_temp " \
  "UNION ALL " \
  "SELECT * FROM moz_places " \
  "WHERE id NOT IN (SELECT id FROM moz_places_temp) " \
)

#define CREATE_MOZ_HISTORYVISITS_BASE(__name, __temporary) NS_LITERAL_CSTRING( \
  "CREATE " __temporary " TABLE " __name " (" \
    "  id INTEGER PRIMARY KEY" \
    ", from_visit INTEGER" \
    ", place_id INTEGER" \
    ", visit_date INTEGER" \
    ", visit_type INTEGER" \
    ", session INTEGER" \
  ")" \
)
#define CREATE_MOZ_HISTORYVISITS \
  CREATE_MOZ_HISTORYVISITS_BASE("moz_historyvisits", "")
#define CREATE_MOZ_HISTORYVISITS_TEMP \
  CREATE_MOZ_HISTORYVISITS_BASE("moz_historyvisits_temp", "TEMP")
#define CREATE_MOZ_HISTORYVISITS_VIEW NS_LITERAL_CSTRING( \
  "CREATE TEMPORARY VIEW moz_historyvisits_view AS " \
  "SELECT * FROM moz_historyvisits_temp " \
  "UNION ALL " \
  "SELECT * FROM moz_historyvisits " \
  "WHERE id NOT IN (SELECT id FROM moz_historyvisits_temp) " \
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
  ")" \
)

#define CREATE_MOZ_BOOKMARKS_ROOTS NS_LITERAL_CSTRING( \
  "CREATE TABLE moz_bookmarks_roots (" \
    "  root_name VARCHAR(16) UNIQUE" \
    ", folder_id INTEGER" \
  ")" \
)

#define CREATE_MOZ_KEYWORDS NS_LITERAL_CSTRING( \
  "CREATE TABLE moz_keywords (" \
    "  id INTEGER PRIMARY KEY AUTOINCREMENT" \
    ", keyword TEXT UNIQUE" \
  ")" \
)

#endif // __nsPlacesTables_h__
