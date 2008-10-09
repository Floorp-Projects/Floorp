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

#ifndef __nsPlacesTriggers_h__
#define __nsPlacesTriggers_h__

/**
 * Trigger increments the visit count by one for each inserted visit that isn't
 * an invalid transition, embedded transition, or a download transition.
 */
#define CREATE_VISIT_COUNT_INSERT_TRIGGER NS_LITERAL_CSTRING( \
  "CREATE TRIGGER moz_historyvisits_afterinsert_v1_trigger " \
  "AFTER INSERT ON moz_historyvisits FOR EACH ROW " \
  "WHEN NEW.visit_type NOT IN (0, 4, 7) " /* invalid, EMBED, DOWNLOAD */ \
  "BEGIN " \
    "UPDATE moz_places " \
    "SET visit_count = visit_count + 1 " \
    "WHERE moz_places.id = NEW.place_id; " \
  "END" \
)

/**
 * Trigger decrements the visit count by one for each removed visit that isn't
 * an invalid transition, embeded transition, or a download transition.  To be
 * safe, we ensure that the visit count will not fall below zero.
 */
#define CREATE_VISIT_COUNT_DELETE_TRIGGER NS_LITERAL_CSTRING( \
  "CREATE TRIGGER moz_historyvisits_afterdelete_v1_trigger " \
  "AFTER DELETE ON moz_historyvisits FOR EACH ROW " \
  "WHEN OLD.visit_type NOT IN (0, 4, 7) " /* invalid, EMBED, DOWNLOAD */ \
  "BEGIN " \
    "UPDATE moz_places " \
    "SET visit_count = visit_count - 1 " \
    "WHERE moz_places.id = OLD.place_id " \
    "AND visit_count > 0; " \
  "END" \
)

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

#endif // __nsPlacesTriggers_h__
