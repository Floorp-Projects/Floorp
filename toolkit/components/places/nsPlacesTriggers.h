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
 *  7 - FRAMED_LINK
 **/
#define EXCLUDED_VISIT_TYPES "0, 4, 7, 8"

/**
 * This triggers update visit_count and last_visit_date based on historyvisits
 * table changes.
 */
#define CREATE_HISTORYVISITS_AFTERINSERT_TRIGGER NS_LITERAL_CSTRING( \
  "CREATE TEMP TRIGGER moz_historyvisits_afterinsert_v2_trigger " \
  "AFTER INSERT ON moz_historyvisits FOR EACH ROW " \
  "BEGIN " \
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

/**
 * These triggers update the hostnames table whenever moz_places changes.
 */
#define CREATE_PLACES_AFTERINSERT_TRIGGER NS_LITERAL_CSTRING( \
  "CREATE TEMP TRIGGER moz_places_afterinsert_trigger " \
  "AFTER INSERT ON moz_places FOR EACH ROW " \
  "WHEN LENGTH(NEW.rev_host) > 1 " \
  "BEGIN " \
    "INSERT OR REPLACE INTO moz_hosts (id, host, frecency, typed) " \
    "VALUES (" \
      "(SELECT id FROM moz_hosts WHERE host = fixup_url(get_unreversed_host(NEW.rev_host))), " \
      "fixup_url(get_unreversed_host(NEW.rev_host)), " \
      "MAX(IFNULL((SELECT frecency FROM moz_hosts WHERE host = fixup_url(get_unreversed_host(NEW.rev_host))), -1), NEW.frecency), " \
      "MAX(IFNULL((SELECT typed FROM moz_hosts WHERE host = fixup_url(get_unreversed_host(NEW.rev_host))), 0), NEW.typed) " \
    "); " \
  "END" \
)

#define CREATE_PLACES_AFTERDELETE_TRIGGER NS_LITERAL_CSTRING( \
  "CREATE TEMP TRIGGER moz_places_afterdelete_trigger " \
  "AFTER DELETE ON moz_places FOR EACH ROW " \
  "BEGIN " \
    "DELETE FROM moz_hosts " \
    "WHERE host = fixup_url(get_unreversed_host(OLD.rev_host)) " \
      "AND NOT EXISTS(" \
        "SELECT 1 FROM moz_places " \
          "WHERE rev_host = get_unreversed_host(host || '.') || '.' " \
             "OR rev_host = get_unreversed_host(host || '.') || '.www.' " \
      "); " \
  "END" \
)

// For performance reasons the host frecency is updated only when the page
// frecency changes by a meaningful percentage.  This is because the frecency
// decay algorithm requires to update all the frecencies at once, causing a
// too high overhead, while leaving the ordering unchanged.
#define CREATE_PLACES_AFTERUPDATE_FRECENCY_TRIGGER NS_LITERAL_CSTRING( \
  "CREATE TEMP TRIGGER moz_places_afterupdate_frecency_trigger " \
  "AFTER UPDATE OF frecency ON moz_places FOR EACH ROW " \
  "WHEN NEW.frecency >= 0 " \
    "AND ABS(" \
      "IFNULL((NEW.frecency - OLD.frecency) / CAST(NEW.frecency AS REAL), " \
             "(NEW.frecency - OLD.frecency))" \
    ") > .05 " \
  "BEGIN " \
    "UPDATE moz_hosts " \
    "SET frecency = (SELECT MAX(frecency) FROM moz_places " \
                    "WHERE rev_host = get_unreversed_host(host || '.') || '.' " \
                       "OR rev_host = get_unreversed_host(host || '.') || '.www.') " \
    "WHERE host = fixup_url(get_unreversed_host(NEW.rev_host)); " \
  "END" \
)

#define CREATE_PLACES_AFTERUPDATE_TYPED_TRIGGER NS_LITERAL_CSTRING( \
  "CREATE TEMP TRIGGER moz_places_afterupdate_typed_trigger " \
  "AFTER UPDATE OF typed ON moz_places FOR EACH ROW " \
  "WHEN NEW.typed = 1 " \
  "BEGIN " \
    "UPDATE moz_hosts " \
    "SET typed = 1 " \
    "WHERE host = fixup_url(get_unreversed_host(NEW.rev_host)); " \
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
    "WHERE url = NEW.url;" \
  "END" \
)

#endif // __nsPlacesTriggers_h__
