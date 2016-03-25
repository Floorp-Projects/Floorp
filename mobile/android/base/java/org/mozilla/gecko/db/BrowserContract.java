/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import org.mozilla.gecko.AppConstants;

import android.net.Uri;
import org.mozilla.gecko.annotation.RobocopTarget;

@RobocopTarget
public class BrowserContract {
    public static final String AUTHORITY = AppConstants.ANDROID_PACKAGE_NAME + ".db.browser";
    public static final Uri AUTHORITY_URI = Uri.parse("content://" + AUTHORITY);

    public static final String PASSWORDS_AUTHORITY = AppConstants.ANDROID_PACKAGE_NAME + ".db.passwords";
    public static final Uri PASSWORDS_AUTHORITY_URI = Uri.parse("content://" + PASSWORDS_AUTHORITY);

    public static final String FORM_HISTORY_AUTHORITY = AppConstants.ANDROID_PACKAGE_NAME + ".db.formhistory";
    public static final Uri FORM_HISTORY_AUTHORITY_URI = Uri.parse("content://" + FORM_HISTORY_AUTHORITY);

    public static final String TABS_AUTHORITY = AppConstants.ANDROID_PACKAGE_NAME + ".db.tabs";
    public static final Uri TABS_AUTHORITY_URI = Uri.parse("content://" + TABS_AUTHORITY);

    public static final String HOME_AUTHORITY = AppConstants.ANDROID_PACKAGE_NAME + ".db.home";
    public static final Uri HOME_AUTHORITY_URI = Uri.parse("content://" + HOME_AUTHORITY);

    public static final String PROFILES_AUTHORITY = AppConstants.ANDROID_PACKAGE_NAME + ".profiles";
    public static final Uri PROFILES_AUTHORITY_URI = Uri.parse("content://" + PROFILES_AUTHORITY);

    public static final String READING_LIST_AUTHORITY = AppConstants.ANDROID_PACKAGE_NAME + ".db.readinglist";
    public static final Uri READING_LIST_AUTHORITY_URI = Uri.parse("content://" + READING_LIST_AUTHORITY);

    public static final String SEARCH_HISTORY_AUTHORITY = AppConstants.ANDROID_PACKAGE_NAME + ".db.searchhistory";
    public static final Uri SEARCH_HISTORY_AUTHORITY_URI = Uri.parse("content://" + SEARCH_HISTORY_AUTHORITY);

    public static final String LOGINS_AUTHORITY = AppConstants.ANDROID_PACKAGE_NAME + ".db.logins";
    public static final Uri LOGINS_AUTHORITY_URI = Uri.parse("content://" + LOGINS_AUTHORITY);

    public static final String PARAM_PROFILE = "profile";
    public static final String PARAM_PROFILE_PATH = "profilePath";
    public static final String PARAM_LIMIT = "limit";
    public static final String PARAM_SUGGESTEDSITES_LIMIT = "suggestedsites_limit";
    public static final String PARAM_IS_SYNC = "sync";
    public static final String PARAM_SHOW_DELETED = "show_deleted";
    public static final String PARAM_IS_TEST = "test";
    public static final String PARAM_INSERT_IF_NEEDED = "insert_if_needed";
    public static final String PARAM_INCREMENT_VISITS = "increment_visits";
    public static final String PARAM_EXPIRE_PRIORITY = "priority";
    public static final String PARAM_DATASET_ID = "dataset_id";

    static public enum ExpirePriority {
        NORMAL,
        AGGRESSIVE
    }

    static public String getFrecencySortOrder(boolean includesBookmarks, boolean asc) {
        final String age = "(" + Combined.DATE_LAST_VISITED + " - " + System.currentTimeMillis() + ") / 86400000";

        StringBuilder order = new StringBuilder(Combined.VISITS + " * MAX(1, 100 * 225 / (" + age + "*" + age + " + 225)) ");

        if (includesBookmarks) {
            order.insert(0, "(CASE WHEN " + Combined.BOOKMARK_ID + " > -1 THEN 100 ELSE 0 END) + ");
        }

        order.append(asc ? " ASC" : " DESC");
        return order.toString();
    }

    @RobocopTarget
    public interface CommonColumns {
        public static final String _ID = "_id";
    }

    @RobocopTarget
    public interface DateSyncColumns {
        public static final String DATE_CREATED = "created";
        public static final String DATE_MODIFIED = "modified";
    }

    @RobocopTarget
    public interface SyncColumns extends DateSyncColumns {
        public static final String GUID = "guid";
        public static final String IS_DELETED = "deleted";
    }

    @RobocopTarget
    public interface URLColumns {
        public static final String URL = "url";
        public static final String TITLE = "title";
    }

    @RobocopTarget
    public interface FaviconColumns {
        public static final String FAVICON = "favicon";
        public static final String FAVICON_ID = "favicon_id";
        public static final String FAVICON_URL = "favicon_url";
    }

    @RobocopTarget
    public interface HistoryColumns {
        public static final String DATE_LAST_VISITED = "date";
        public static final String VISITS = "visits";
    }

    public interface DeletedColumns {
        public static final String ID = "id";
        public static final String GUID = "guid";
        public static final String TIME_DELETED = "timeDeleted";
    }

    @RobocopTarget
    public static final class Favicons implements CommonColumns, DateSyncColumns {
        private Favicons() {}

        public static final String TABLE_NAME = "favicons";

        public static final Uri CONTENT_URI = Uri.withAppendedPath(AUTHORITY_URI, "favicons");

        public static final String URL = "url";
        public static final String DATA = "data";
        public static final String PAGE_URL = "page_url";
    }

    @RobocopTarget
    public static final class Thumbnails implements CommonColumns {
        private Thumbnails() {}

        public static final String TABLE_NAME = "thumbnails";

        public static final Uri CONTENT_URI = Uri.withAppendedPath(AUTHORITY_URI, "thumbnails");

        public static final String URL = "url";
        public static final String DATA = "data";
    }

    public static final class Profiles {
        private Profiles() {}
        public static final String NAME = "name";
        public static final String PATH = "path";
    }

    @RobocopTarget
    public static final class Bookmarks implements CommonColumns, URLColumns, FaviconColumns, SyncColumns {
        private Bookmarks() {}

        public static final String TABLE_NAME = "bookmarks";

        public static final String VIEW_WITH_FAVICONS = "bookmarks_with_favicons";

        public static final int FIXED_ROOT_ID = 0;
        public static final int FAKE_DESKTOP_FOLDER_ID = -1;
        public static final int FIXED_READING_LIST_ID = -2;
        public static final int FIXED_PINNED_LIST_ID = -3;
        public static final int FIXED_SCREENSHOT_FOLDER_ID = -4;

        public static final String MOBILE_FOLDER_GUID = "mobile";
        public static final String PLACES_FOLDER_GUID = "places";
        public static final String MENU_FOLDER_GUID = "menu";
        public static final String TAGS_FOLDER_GUID = "tags";
        public static final String TOOLBAR_FOLDER_GUID = "toolbar";
        public static final String UNFILED_FOLDER_GUID = "unfiled";
        public static final String FAKE_DESKTOP_FOLDER_GUID = "desktop";
        public static final String PINNED_FOLDER_GUID = "pinned";
        public static final String SCREENSHOT_FOLDER_GUID = "screenshots";

        public static final int TYPE_FOLDER = 0;
        public static final int TYPE_BOOKMARK = 1;
        public static final int TYPE_SEPARATOR = 2;
        public static final int TYPE_LIVEMARK = 3;
        public static final int TYPE_QUERY = 4;

        public static final Uri CONTENT_URI = Uri.withAppendedPath(AUTHORITY_URI, "bookmarks");
        public static final Uri PARENTS_CONTENT_URI = Uri.withAppendedPath(CONTENT_URI, "parents");
        // Hacky API for bulk-updating positions. Bug 728783.
        public static final Uri POSITIONS_CONTENT_URI = Uri.withAppendedPath(CONTENT_URI, "positions");
        public static final long DEFAULT_POSITION = Long.MIN_VALUE;

        public static final String CONTENT_TYPE = "vnd.android.cursor.dir/bookmark";
        public static final String CONTENT_ITEM_TYPE = "vnd.android.cursor.item/bookmark";
        public static final String TYPE = "type";
        public static final String PARENT = "parent";
        public static final String POSITION = "position";
        public static final String TAGS = "tags";
        public static final String DESCRIPTION = "description";
        public static final String KEYWORD = "keyword";
    }

    @RobocopTarget
    public static final class History implements CommonColumns, URLColumns, HistoryColumns, FaviconColumns, SyncColumns {
        private History() {}

        public static final String TABLE_NAME = "history";

        public static final String VIEW_WITH_FAVICONS = "history_with_favicons";

        public static final Uri CONTENT_URI = Uri.withAppendedPath(AUTHORITY_URI, "history");
        public static final Uri CONTENT_OLD_URI = Uri.withAppendedPath(AUTHORITY_URI, "history/old");
        public static final String CONTENT_TYPE = "vnd.android.cursor.dir/browser-history";
        public static final String CONTENT_ITEM_TYPE = "vnd.android.cursor.item/browser-history";
    }

    // Combined bookmarks and history
    @RobocopTarget
    public static final class Combined implements CommonColumns, URLColumns, HistoryColumns, FaviconColumns  {
        private Combined() {}

        public static final String VIEW_NAME = "combined";

        public static final String VIEW_WITH_FAVICONS = "combined_with_favicons";

        public static final Uri CONTENT_URI = Uri.withAppendedPath(AUTHORITY_URI, "combined");

        public static final String BOOKMARK_ID = "bookmark_id";
        public static final String HISTORY_ID = "history_id";
    }

    public static final class Schema {
        private Schema() {}
        public static final Uri CONTENT_URI = Uri.withAppendedPath(AUTHORITY_URI, "schema");

        public static final String VERSION = "version";
    }

    public static final class Passwords {
        private Passwords() {}
        public static final Uri CONTENT_URI = Uri.withAppendedPath(PASSWORDS_AUTHORITY_URI, "passwords");
        public static final String CONTENT_TYPE = "vnd.android.cursor.dir/passwords";

        public static final String ID = "id";
        public static final String HOSTNAME = "hostname";
        public static final String HTTP_REALM = "httpRealm";
        public static final String FORM_SUBMIT_URL = "formSubmitURL";
        public static final String USERNAME_FIELD = "usernameField";
        public static final String PASSWORD_FIELD = "passwordField";
        public static final String ENCRYPTED_USERNAME = "encryptedUsername";
        public static final String ENCRYPTED_PASSWORD = "encryptedPassword";
        public static final String ENC_TYPE = "encType";
        public static final String TIME_CREATED = "timeCreated";
        public static final String TIME_LAST_USED = "timeLastUsed";
        public static final String TIME_PASSWORD_CHANGED = "timePasswordChanged";
        public static final String TIMES_USED = "timesUsed";
        public static final String GUID = "guid";

        // This needs to be kept in sync with the types defined in toolkit/components/passwordmgr/nsILoginManagerCrypto.idl#45
        public static final int ENCTYPE_SDR = 1;
    }

    public static final class DeletedPasswords implements DeletedColumns {
        private DeletedPasswords() {}
        public static final String CONTENT_TYPE = "vnd.android.cursor.dir/deleted-passwords";
        public static final Uri CONTENT_URI = Uri.withAppendedPath(PASSWORDS_AUTHORITY_URI, "deleted-passwords");
    }

    @RobocopTarget
    public static final class GeckoDisabledHosts {
        private GeckoDisabledHosts() {}
        public static final String CONTENT_TYPE = "vnd.android.cursor.dir/disabled-hosts";
        public static final Uri CONTENT_URI = Uri.withAppendedPath(PASSWORDS_AUTHORITY_URI, "disabled-hosts");

        public static final String HOSTNAME = "hostname";
    }

    public static final class FormHistory {
        private FormHistory() {}
        public static final Uri CONTENT_URI = Uri.withAppendedPath(FORM_HISTORY_AUTHORITY_URI, "formhistory");
        public static final String CONTENT_TYPE = "vnd.android.cursor.dir/formhistory";

        public static final String ID = "id";
        public static final String FIELD_NAME = "fieldname";
        public static final String VALUE = "value";
        public static final String TIMES_USED = "timesUsed";
        public static final String FIRST_USED = "firstUsed";
        public static final String LAST_USED = "lastUsed";
        public static final String GUID = "guid";
    }

    public static final class DeletedFormHistory implements DeletedColumns {
        private DeletedFormHistory() {}
        public static final Uri CONTENT_URI = Uri.withAppendedPath(FORM_HISTORY_AUTHORITY_URI, "deleted-formhistory");
        public static final String CONTENT_TYPE = "vnd.android.cursor.dir/deleted-formhistory";
    }

    @RobocopTarget
    public static final class Tabs implements CommonColumns {
        private Tabs() {}
        public static final String TABLE_NAME = "tabs";

        public static final Uri CONTENT_URI = Uri.withAppendedPath(TABS_AUTHORITY_URI, "tabs");
        public static final String CONTENT_TYPE = "vnd.android.cursor.dir/tab";
        public static final String CONTENT_ITEM_TYPE = "vnd.android.cursor.item/tab";

        // Title of the tab.
        public static final String TITLE = "title";

        // Topmost URL from the history array. Allows processing of this tab without
        // parsing that array.
        public static final String URL = "url";

        // Sync-assigned GUID for client device. NULL for local tabs.
        public static final String CLIENT_GUID = "client_guid";

        // JSON-encoded array of history URL strings, from most recent to least recent.
        public static final String HISTORY = "history";

        // Favicon URL for the tab's topmost history entry.
        public static final String FAVICON = "favicon";

        // Last used time of the tab.
        public static final String LAST_USED = "last_used";

        // Position of the tab. 0 represents foreground.
        public static final String POSITION = "position";
    }

    public static final class Clients implements CommonColumns {
        private Clients() {}
        public static final Uri CONTENT_RECENCY_URI = Uri.withAppendedPath(TABS_AUTHORITY_URI, "clients_recency");
        public static final Uri CONTENT_URI = Uri.withAppendedPath(TABS_AUTHORITY_URI, "clients");
        public static final String CONTENT_TYPE = "vnd.android.cursor.dir/client";
        public static final String CONTENT_ITEM_TYPE = "vnd.android.cursor.item/client";

        // Client-provided name string. Could conceivably be null.
        public static final String NAME = "name";

        // Sync-assigned GUID for client device. NULL for local tabs.
        public static final String GUID = "guid";

        // Last modified time for the client's tab record. For remote records, a server
        // timestamp provided by Sync during insertion.
        public static final String LAST_MODIFIED = "last_modified";

        public static final String DEVICE_TYPE = "device_type";
    }

    // Data storage for dynamic panels on about:home
    @RobocopTarget
    public static final class HomeItems implements CommonColumns {
        private HomeItems() {}
        public static final Uri CONTENT_FAKE_URI = Uri.withAppendedPath(HOME_AUTHORITY_URI, "items/fake");
        public static final Uri CONTENT_URI = Uri.withAppendedPath(HOME_AUTHORITY_URI, "items");

        public static final String CONTENT_TYPE = "vnd.android.cursor.dir/homeitem";
        public static final String CONTENT_ITEM_TYPE = "vnd.android.cursor.item/homeitem";

        public static final String DATASET_ID = "dataset_id";
        public static final String URL = "url";
        public static final String TITLE = "title";
        public static final String DESCRIPTION = "description";
        public static final String IMAGE_URL = "image_url";
        public static final String BACKGROUND_COLOR = "background_color";
        public static final String BACKGROUND_URL = "background_url";
        public static final String CREATED = "created";
        public static final String FILTER = "filter";

        public static final String[] DEFAULT_PROJECTION =
            new String[] { _ID, DATASET_ID, URL, TITLE, DESCRIPTION, IMAGE_URL, BACKGROUND_COLOR, BACKGROUND_URL, FILTER };
    }

    @RobocopTarget
    public static final class ReadingListItems implements CommonColumns, URLColumns {
        public static final String EXCERPT = "excerpt";
        public static final String CLIENT_LAST_MODIFIED = "client_last_modified";
        public static final String GUID = "guid";
        public static final String SERVER_LAST_MODIFIED = "last_modified";
        public static final String SERVER_STORED_ON = "stored_on";
        public static final String ADDED_ON = "added_on";
        public static final String MARKED_READ_ON = "marked_read_on";
        public static final String IS_DELETED = "is_deleted";
        public static final String IS_ARCHIVED = "is_archived";
        public static final String IS_UNREAD = "is_unread";
        public static final String IS_ARTICLE = "is_article";
        public static final String IS_FAVORITE = "is_favorite";
        public static final String RESOLVED_URL = "resolved_url";
        public static final String RESOLVED_TITLE = "resolved_title";
        public static final String ADDED_BY = "added_by";
        public static final String MARKED_READ_BY = "marked_read_by";
        public static final String WORD_COUNT = "word_count";
        public static final String READ_POSITION = "read_position";
        public static final String CONTENT_STATUS = "content_status";

        public static final String SYNC_STATUS = "sync_status";
        public static final String SYNC_CHANGE_FLAGS = "sync_change_flags";

        private ReadingListItems() {}
        public static final Uri CONTENT_URI = Uri.withAppendedPath(READING_LIST_AUTHORITY_URI, "items");

        public static final String CONTENT_TYPE = "vnd.android.cursor.dir/readinglistitem";
        public static final String CONTENT_ITEM_TYPE = "vnd.android.cursor.item/readinglistitem";

        // CONTENT_STATUS represents the result of an attempt to fetch content for the reading list item.
        public static final int STATUS_UNFETCHED = 0;
        public static final int STATUS_FETCH_FAILED_TEMPORARY = 1;
        public static final int STATUS_FETCH_FAILED_PERMANENT = 2;
        public static final int STATUS_FETCH_FAILED_UNSUPPORTED_FORMAT = 3;
        public static final int STATUS_FETCHED_ARTICLE = 4;

        // See https://github.com/mozilla-services/readinglist/wiki/Client-phases for how this is expected to work.
        //
        // If an item is SYNCED, it doesn't need to be uploaded.
        //
        // If its status is NEW, the entire record should be uploaded.
        //
        // If DELETED, the record should be deleted. A record can only move into this state from SYNCED; NEW records
        // are deleted immediately.
        //

        public static final int SYNC_STATUS_SYNCED = 0;
        public static final int SYNC_STATUS_NEW = 1;                      // Upload everything.
        public static final int SYNC_STATUS_DELETED = 2;                  // Delete the record from the server.
        public static final int SYNC_STATUS_MODIFIED = 3;                 // Consult SYNC_CHANGE_FLAGS.

        // SYNC_CHANGE_FLAG represents the sets of fields that need to be uploaded.
        // If its status is only UNREAD_CHANGED (and maybe FAVORITE_CHANGED?), then it can easily be uploaded
        // in a fire-and-forget manner. This change can never conflict.
        //
        // If its status is RESOLVED, then one or more of the content-oriented fields has changed, and a full
        // upload of those fields should occur. These can result in conflicts.
        //
        // Note that these are flags; they should be considered together when deciding on a course of action.
        //
        // These flags are meaningless for records in any state other than SYNCED. They can be safely altered in
        // other states (to avoid having to query to pre-fill a ContentValues), but should be ignored.
        public static final int SYNC_CHANGE_NONE = 0;
        public static final int SYNC_CHANGE_UNREAD_CHANGED   = 1 << 0;    // => marked_read_{on,by}, is_unread
        public static final int SYNC_CHANGE_FAVORITE_CHANGED = 1 << 1;    // => is_favorite
        public static final int SYNC_CHANGE_RESOLVED = 1 << 2;            // => is_article, resolved_{url,title}, excerpt, word_count


        public static final String DEFAULT_SORT_ORDER = CLIENT_LAST_MODIFIED + " DESC";
        public static final String[] DEFAULT_PROJECTION = new String[] { _ID, URL, TITLE, EXCERPT, WORD_COUNT, IS_UNREAD };

        // Minimum fields required to create a reading list item.
        public static final String[] REQUIRED_FIELDS = { ReadingListItems.URL, ReadingListItems.TITLE };

        // All fields that might be mapped from the DB into a record object.
        public static final String[] ALL_FIELDS = {
                CommonColumns._ID,
                URLColumns.URL,
                URLColumns.TITLE,
                EXCERPT,
                CLIENT_LAST_MODIFIED,
                GUID,
                SERVER_LAST_MODIFIED,
                SERVER_STORED_ON,
                ADDED_ON,
                MARKED_READ_ON,
                IS_DELETED,
                IS_ARCHIVED,
                IS_UNREAD,
                IS_ARTICLE,
                IS_FAVORITE,
                RESOLVED_URL,
                RESOLVED_TITLE,
                ADDED_BY,
                MARKED_READ_BY,
                WORD_COUNT,
                READ_POSITION,
                CONTENT_STATUS,

                SYNC_STATUS,
                SYNC_CHANGE_FLAGS,
        };

        public static final String TABLE_NAME = "reading_list";
    }

    @RobocopTarget
    public static final class TopSites implements CommonColumns, URLColumns {
        private TopSites() {}

        public static final int TYPE_BLANK = 0;
        public static final int TYPE_TOP = 1;
        public static final int TYPE_PINNED = 2;
        public static final int TYPE_SUGGESTED = 3;

        public static final String BOOKMARK_ID = "bookmark_id";
        public static final String HISTORY_ID = "history_id";
        public static final String TYPE = "type";

        public static final Uri CONTENT_URI = Uri.withAppendedPath(AUTHORITY_URI, "topsites");
    }

    @RobocopTarget
    public static final class SearchHistory implements CommonColumns, HistoryColumns {
        private SearchHistory() {}

        public static final String CONTENT_TYPE = "vnd.android.cursor.dir/searchhistory";
        public static final String QUERY = "query";
        public static final String DATE = "date";
        public static final String TABLE_NAME = "searchhistory";

        public static final Uri CONTENT_URI = Uri.withAppendedPath(SEARCH_HISTORY_AUTHORITY_URI, "searchhistory");
    }

    @RobocopTarget
    public static final class SuggestedSites implements CommonColumns, URLColumns {
        private SuggestedSites() {}

        public static final Uri CONTENT_URI = Uri.withAppendedPath(AUTHORITY_URI, "suggestedsites");
    }

    @RobocopTarget
    public static final class UrlAnnotations implements CommonColumns, DateSyncColumns {
        private UrlAnnotations() {}

        public static final String TABLE_NAME = "urlannotations";
        public static final Uri CONTENT_URI = Uri.withAppendedPath(AUTHORITY_URI, TABLE_NAME);

        public static final String URL = "url";
        public static final String KEY = "key";
        public static final String VALUE = "value";
        public static final String SYNC_STATUS = "sync_status";

        public enum Key {
            // We use a parameter, rather than name(), as defensive coding: we can't let the
            // enum name change because we've already stored values into the DB.
            SCREENSHOT ("screenshot"),

            /**
             * This key maps URLs to its feeds.
             *
             * Key:   feed
             * Value: URL of feed
             */
            FEED("feed"),

            /**
             * This key maps URLs of feeds to an object describing the feed.
             *
             * Key:   feed_subscription
             * Value: JSON object describing feed
             */
            FEED_SUBSCRIPTION("feed_subscription");

            private final String dbValue;

            Key(final String dbValue) { this.dbValue = dbValue; }
            public String getDbValue() { return dbValue; }
        }

        public enum SyncStatus {
            // We use a parameter, rather than ordinal(), as defensive coding: we can't let the
            // ordinal values change because we've already stored values into the DB.
            NEW (0);

            // Value stored into the database for this column.
            private final int dbValue;

            SyncStatus(final int dbValue) {
                this.dbValue = dbValue;
            }

            public int getDBValue() { return dbValue; }
        }
    }

    public static final class Numbers {
        private Numbers() {}

        public static final String TABLE_NAME = "numbers";

        public static final String POSITION = "position";

        public static final int MAX_VALUE = 50;
    }

    @RobocopTarget
    public static final class Logins implements CommonColumns {
        private Logins() {}

        public static final Uri CONTENT_URI = Uri.withAppendedPath(LOGINS_AUTHORITY_URI, "logins");
        public static final String CONTENT_TYPE = "vnd.android.cursor.dir/logins";
        public static final String CONTENT_ITEM_TYPE = "vnd.android.cursor.item/logins";
        public static final String TABLE_LOGINS = "logins";

        public static final String HOSTNAME = "hostname";
        public static final String HTTP_REALM = "httpRealm";
        public static final String FORM_SUBMIT_URL = "formSubmitURL";
        public static final String USERNAME_FIELD = "usernameField";
        public static final String PASSWORD_FIELD = "passwordField";
        public static final String ENCRYPTED_USERNAME = "encryptedUsername";
        public static final String ENCRYPTED_PASSWORD = "encryptedPassword";
        public static final String ENC_TYPE = "encType";
        public static final String TIME_CREATED = "timeCreated";
        public static final String TIME_LAST_USED = "timeLastUsed";
        public static final String TIME_PASSWORD_CHANGED = "timePasswordChanged";
        public static final String TIMES_USED = "timesUsed";
        public static final String GUID = "guid";
    }

    @RobocopTarget
    public static final class DeletedLogins implements CommonColumns {
        private DeletedLogins() {}

        public static final Uri CONTENT_URI = Uri.withAppendedPath(LOGINS_AUTHORITY_URI, "deleted-logins");
        public static final String CONTENT_TYPE = "vnd.android.cursor.dir/deleted-logins";
        public static final String CONTENT_ITEM_TYPE = "vnd.android.cursor.item/deleted-logins";
        public static final String TABLE_DELETED_LOGINS = "deleted_logins";

        public static final String GUID = "guid";
        public static final String TIME_DELETED = "timeDeleted";
    }

    @RobocopTarget
    public static final class LoginsDisabledHosts implements CommonColumns {
        private LoginsDisabledHosts() {}

        public static final Uri CONTENT_URI = Uri.withAppendedPath(LOGINS_AUTHORITY_URI, "logins-disabled-hosts");
        public static final String CONTENT_TYPE = "vnd.android.cursor.dir/logins-disabled-hosts";
        public static final String CONTENT_ITEM_TYPE = "vnd.android.cursor.item/logins-disabled-hosts";
        public static final String TABLE_DISABLED_HOSTS = "logins_disabled_hosts";

        public static final String HOSTNAME = "hostname";
    }

    // We refer to the service by name to decouple services from the rest of the code base.
    public static final String TAB_RECEIVED_SERVICE_CLASS_NAME = "org.mozilla.gecko.tabqueue.TabReceivedService";

    public static final String SKIP_TAB_QUEUE_FLAG = "skip_tab_queue";

    public static final String EXTRA_CLIENT_GUID = "org.mozilla.gecko.extra.CLIENT_ID";
}
