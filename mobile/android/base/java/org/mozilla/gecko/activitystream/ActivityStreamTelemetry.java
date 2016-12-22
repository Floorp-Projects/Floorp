/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.activitystream;

import android.support.annotation.NonNull;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.R;
import org.mozilla.gecko.db.BrowserContract;

import java.util.HashMap;

/**
 * Telemetry constants and an 'extras' builder specific to Activity Stream.
 */
public class ActivityStreamTelemetry {
    public static class Contract {
        // Keys
        public final static String FX_ACCOUNT_PRESENT = "fx_account_present";
        public final static String ITEM = "item";
        public final static String SOURCE_TYPE = "source_type";
        public final static String SOURCE_SUBTYPE = "source_subtype";
        public final static String ACTION_POSITION = "action_position";
        public final static String COUNT = "count";

        // Values
        public final static String TYPE_TOPSITES = "topsites";
        public final static String TYPE_HIGHLIGHTS = "highlights";
        public final static String SUBTYPE_PINNED = "pinned";
        public final static String SUBTYPE_SUGGESTED = "suggested";
        public final static String SUBTYPE_TOP = "top";
        public final static String SUBTYPE_VISITED = "visited";
        public final static String SUBTYPE_BOOKMARKED = "bookmarked";
        public final static String ITEM_SHARE = "share";
        public final static String ITEM_ADD_BOOKMARK = "add_bookmark";
        public final static String ITEM_REMOVE_BOOKMARK = "remove_bookmark";
        public final static String ITEM_PIN = "pin";
        public final static String ITEM_UNPIN = "unpin";
        public final static String ITEM_COPY = "copy";
        public final static String ITEM_ADD_TO_HOMESCREEN = "homescreen";
        public final static String ITEM_NEW_TAB = "newtab";
        public final static String ITEM_PRIVATE_TAB = "privatetab";
        public final static String ITEM_DISMISS = "dismiss";
        public final static String ITEM_DELETE_HISTORY = "delete";
    }

    /**
     * A helper class used for composing an 'extras' field. It encapsulates a holder of "global"
     * key/value pairs which will be present in every 'extras' constructed by this class, and a
     * static builder which is aware of Activity Stream telemetry needs.
     */
    public final static class Extras {
        private static final HashMap<String, Object> globals = new HashMap<>();

        public static void setGlobal(String key, Object value) {
            globals.put(key, value);
        }

        public static Builder builder() {
            return new Builder();
        }

        /**
         * Allows composing a JSON extras blob, which is then "built" into a string representation.
         */
        public final static class Builder {
            private final JSONObject data;

            private Builder() {
                data = new JSONObject(globals);
            }

            /**
             * @param value a {@link JSONObject}, {@link JSONArray}, String, Boolean,
             *     Integer, Long, Double, {@link JSONObject#NULL}, or {@code null}. May not be
             *     {@link Double#isNaN() NaNs} or {@link Double#isInfinite()
             *     infinities}.
             * @return this object.
             */
            public Builder set(@NonNull String key, Object value) {
                try {
                    data.put(key, value);
                } catch (JSONException e) {
                    throw new IllegalArgumentException("Key can't be null");
                }
                return this;
            }

            /**
             * Sets extras values describing a context menu interaction based on the menu item ID.
             *
             * @param itemId ID of a menu item, which is transformed into a corresponding item
             *               key/value pair and passed off to {@link this#set(String, Object)}.
             * @return this object.
             */
            public Builder fromMenuItemId(int itemId) {
                switch (itemId) {
                    case R.id.share:
                        this.set(Contract.ITEM, Contract.ITEM_SHARE);
                        break;

                    case R.id.copy_url:
                        this.set(Contract.ITEM, Contract.ITEM_COPY);
                        break;

                    case R.id.add_homescreen:
                        this.set(Contract.ITEM, Contract.ITEM_ADD_TO_HOMESCREEN);
                        break;

                    case R.id.open_new_tab:
                        this.set(Contract.ITEM, Contract.ITEM_NEW_TAB);
                        break;

                    case R.id.open_new_private_tab:
                        this.set(Contract.ITEM, Contract.ITEM_PRIVATE_TAB);
                        break;

                    case R.id.dismiss:
                        this.set(Contract.ITEM, Contract.ITEM_DISMISS);
                        break;

                    case R.id.delete:
                        this.set(Contract.ITEM, Contract.ITEM_DELETE_HISTORY);
                        break;
                }
                return this;
            }

            public Builder forHighlightSource(Utils.HighlightSource source) {
                switch (source) {
                    case VISITED:
                        this.set(Contract.SOURCE_SUBTYPE, Contract.SUBTYPE_VISITED);
                        break;
                    case BOOKMARKED:
                        this.set(Contract.SOURCE_SUBTYPE, Contract.SUBTYPE_BOOKMARKED);
                        break;
                    default:
                        throw new IllegalStateException("Unknown highlight source: " + source);
                }
                return this;
            }

            public Builder forTopSiteType(int type) {
                switch (type) {
                    case BrowserContract.TopSites.TYPE_PINNED:
                        this.set(Contract.SOURCE_SUBTYPE, Contract.SUBTYPE_PINNED);
                        break;
                    case BrowserContract.TopSites.TYPE_SUGGESTED:
                        this.set(Contract.SOURCE_SUBTYPE, Contract.SUBTYPE_SUGGESTED);
                        break;
                    case BrowserContract.TopSites.TYPE_TOP:
                        this.set(Contract.SOURCE_SUBTYPE, Contract.SUBTYPE_TOP);
                        break;
                    // While we also have a "blank" type, it is not used by Activity Stream.
                    default:
                        throw new IllegalStateException("Unknown top site type: " + type);
                }
                return this;
            }

            public String build() {
                return data.toString();
            }
        }
    }
}
