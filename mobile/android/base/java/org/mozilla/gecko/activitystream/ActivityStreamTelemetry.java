/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.activitystream;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.Resources;
import android.support.annotation.NonNull;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.R;
import org.mozilla.gecko.activitystream.homepanel.ActivityStreamConfiguration;
import org.mozilla.gecko.activitystream.homepanel.ActivityStreamPanel;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.activitystream.homepanel.model.TopSite;

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
        public final static String PAGE_NUMBER = "page_number";
        public final static String INTERACTION = "interaction";
        public final static String AS_USER_PREFERENCES = "as-user-preferences";

        // Values
        public final static String TYPE_TOPSITES = "topsites";
        public final static String TYPE_HIGHLIGHTS = "highlights";
        public final static String TYPE_POCKET = "pocket";
        public final static String TYPE_LEARN_MORE = "learn_more";
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
        public final static String ITEM_DISMISS = "dismiss";
        public final static String ITEM_DELETE_HISTORY = "delete";
        public final static String ITEM_LINK_MORE = "link_more";
        public final static String INTERACTION_MENU_BUTTON = "menu_button";
        public final static String INTERACTION_LONG_CLICK = "long_click";
    }

    /**
     * AS_USER_PREFERENCES
     *
     * NB: Additional pref values should be (unique) powers of 2
     * We skip 1 and 2 to be consistent with Desktop, because those prefs are not used in Firefox for Android.
     **/
    private final static int POCKET_ENABLED_VALUE = 4;
    private final static int VISITED_ENABLED_VALUE = 8;
    private final static int BOOKMARKED_ENABLED_VALUE = 16;
    private final static int POCKET_ENABLED_BY_LOCALE = 32;

    /**
     * Calculates the bit-packed value of the user's Activity Stream preferences (e.g. enabled/disabled sections).
     *
     * @param sharedPreferences SharedPreferences of this profile
     * @return bit-packed value of the user's AS preferences, which is the sum of the values of the enabled preferences.
     */
    public static int getASUserPreferencesValue(final Context context, final SharedPreferences sharedPreferences) {
        final Resources res = context.getResources();
        int bitPackedPrefValue = 0;

        if (ActivityStreamConfiguration.isPocketEnabledByLocale(context)) {
            bitPackedPrefValue += POCKET_ENABLED_BY_LOCALE;
        }

        if (sharedPreferences.getBoolean(ActivityStreamPanel.PREF_POCKET_ENABLED, res.getBoolean(R.bool.pref_activitystream_pocket_enabled_default))) {
            bitPackedPrefValue += POCKET_ENABLED_VALUE;
        }

        if (sharedPreferences.getBoolean(ActivityStreamPanel.PREF_VISITED_ENABLED, res.getBoolean(R.bool.pref_activitystream_visited_enabled_default))) {
            bitPackedPrefValue += VISITED_ENABLED_VALUE;
        }

        if (sharedPreferences.getBoolean(ActivityStreamPanel.PREF_BOOKMARKS_ENABLED, res.getBoolean(R.bool.pref_activitystream_recentbookmarks_enabled_default))) {
            bitPackedPrefValue += BOOKMARKED_ENABLED_VALUE;
        }

        return bitPackedPrefValue;
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

                    // Our current privacy guidelines do not allow us to write to disk
                    // Private Browsing-only telemetry that could indicate that PB mode is used.
                    // See Bug 1325323 for context.
                    case R.id.open_new_private_tab:
                    case R.id.open_new_tab:
                        this.set(Contract.ITEM, Contract.ITEM_NEW_TAB);
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

            public Builder forTopSite(final TopSite topSite) {
                this.set(
                        ActivityStreamTelemetry.Contract.SOURCE_TYPE,
                        ActivityStreamTelemetry.Contract.TYPE_TOPSITES
                );

                switch (topSite.getType()) {
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
                    case BrowserContract.TopSites.TYPE_BLANK:
                    default:
                        throw new IllegalStateException("Unknown top site type: " + topSite.getType());
                }

                return this;
            }

            public String build() {
                return data.toString();
            }
        }
    }
}
