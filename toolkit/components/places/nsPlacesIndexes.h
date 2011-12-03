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
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Marco Bonardo <mak77@bonardo.net> (Original Author)
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

#ifndef nsPlacesIndexes_h__
#define nsPlacesIndexes_h__

#define CREATE_PLACES_IDX(__name, __table, __columns, __type) \
  NS_LITERAL_CSTRING( \
    "CREATE " __type " INDEX IF NOT EXISTS " __table "_" __name \
      " ON " __table " (" __columns ")" \
  )

/**
 * moz_places
 */
#define CREATE_IDX_MOZ_PLACES_URL \
  CREATE_PLACES_IDX( \
    "url_uniqueindex", "moz_places", "url", "UNIQUE" \
  )

#define CREATE_IDX_MOZ_PLACES_FAVICON \
  CREATE_PLACES_IDX( \
    "faviconindex", "moz_places", "favicon_id", "" \
  )

#define CREATE_IDX_MOZ_PLACES_REVHOST \
  CREATE_PLACES_IDX( \
    "hostindex", "moz_places", "rev_host", "" \
  )

#define CREATE_IDX_MOZ_PLACES_VISITCOUNT \
  CREATE_PLACES_IDX( \
    "visitcount", "moz_places", "visit_count", "" \
  )

#define CREATE_IDX_MOZ_PLACES_FRECENCY \
  CREATE_PLACES_IDX( \
    "frecencyindex", "moz_places", "frecency", "" \
  )

#define CREATE_IDX_MOZ_PLACES_LASTVISITDATE \
  CREATE_PLACES_IDX( \
    "lastvisitdateindex", "moz_places", "last_visit_date", "" \
  )

#define CREATE_IDX_MOZ_PLACES_GUID \
  CREATE_PLACES_IDX( \
    "guid_uniqueindex", "moz_places", "guid", "UNIQUE" \
  )

/**
 * moz_historyvisits
 */

#define CREATE_IDX_MOZ_HISTORYVISITS_PLACEDATE \
  CREATE_PLACES_IDX( \
    "placedateindex", "moz_historyvisits", "place_id, visit_date", "" \
  )

#define CREATE_IDX_MOZ_HISTORYVISITS_FROMVISIT \
  CREATE_PLACES_IDX( \
    "fromindex", "moz_historyvisits", "from_visit", "" \
  )

#define CREATE_IDX_MOZ_HISTORYVISITS_VISITDATE \
  CREATE_PLACES_IDX( \
    "dateindex", "moz_historyvisits", "visit_date", "" \
  )

/**
 * moz_bookmarks
 */

#define CREATE_IDX_MOZ_BOOKMARKS_PLACETYPE \
  CREATE_PLACES_IDX( \
    "itemindex", "moz_bookmarks", "fk, type", "" \
  )

#define CREATE_IDX_MOZ_BOOKMARKS_PARENTPOSITION \
  CREATE_PLACES_IDX( \
    "parentindex", "moz_bookmarks", "parent, position", "" \
  )

#define CREATE_IDX_MOZ_BOOKMARKS_PLACELASTMODIFIED \
  CREATE_PLACES_IDX( \
    "itemlastmodifiedindex", "moz_bookmarks", "fk, lastModified", "" \
  )

#define CREATE_IDX_MOZ_BOOKMARKS_GUID \
  CREATE_PLACES_IDX( \
    "guid_uniqueindex", "moz_bookmarks", "guid", "UNIQUE" \
  )

/**
 * moz_annos
 */

#define CREATE_IDX_MOZ_ANNOS_PLACEATTRIBUTE \
  CREATE_PLACES_IDX( \
    "placeattributeindex", "moz_annos", "place_id, anno_attribute_id", "UNIQUE" \
  )

/**
 * moz_items_annos
 */

#define CREATE_IDX_MOZ_ITEMSANNOS_PLACEATTRIBUTE \
  CREATE_PLACES_IDX( \
    "itemattributeindex", "moz_items_annos", "item_id, anno_attribute_id", "UNIQUE" \
  )

/**
 * moz_favicons
 */

#define CREATE_IDX_MOZ_FAVICONS_GUID \
  CREATE_PLACES_IDX( \
    "guid_uniqueindex", "moz_favicons", "guid", "UNIQUE" \
  )

#endif // nsPlacesIndexes_h__
