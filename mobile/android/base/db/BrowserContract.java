/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import org.mozilla.gecko.AppConstants;

import android.net.Uri;
import org.mozilla.gecko.mozglue.RobocopTarget;

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

    public static final String PARAM_PROFILE = "profile";
    public static final String PARAM_PROFILE_PATH = "profilePath";
    public static final String PARAM_LIMIT = "limit";
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

        public static final String MOBILE_FOLDER_GUID = "mobile";
        public static final String PLACES_FOLDER_GUID = "places";
        public static final String MENU_FOLDER_GUID = "menu";
        public static final String TAGS_FOLDER_GUID = "tags";
        public static final String TOOLBAR_FOLDER_GUID = "toolbar";
        public static final String UNFILED_FOLDER_GUID = "unfiled";
        public static final String READING_LIST_FOLDER_GUID = "readinglist";
        public static final String FAKE_DESKTOP_FOLDER_GUID = "desktop";
        public static final String PINNED_FOLDER_GUID = "pinned";

        public static final int TYPE_FOLDER = 0;
        public static final int TYPE_BOOKMARK = 1;
        public static final int TYPE_SEPARATOR = 2;
        public static final int TYPE_LIVEMARK = 3;
        public static final int TYPE_QUERY = 4;

        /*
         * These values are returned by getItemFlags. They're not really
         * exclusive to bookmarks, but there's no better place to put them.
         */
        public static final int FLAG_SUCCESS  = 1 << 1;   // The query succeeded.
        public static final int FLAG_BOOKMARK = 1 << 2;
        public static final int FLAG_PINNED   = 1 << 3;
        public static final int FLAG_READING  = 1 << 4;

        public static final Uri FLAGS_URI = Uri.withAppendedPath(AUTHORITY_URI, "flags");

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

        public static final int DISPLAY_NORMAL = 0;
        public static final int DISPLAY_READER = 1;

        public static final String BOOKMARK_ID = "bookmark_id";
        public static final String HISTORY_ID = "history_id";
        public static final String DISPLAY = "display";
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

    public static final class Tabs implements CommonColumns {
        private Tabs() {}
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

    public static final class Clients {
        private Clients() {}
        public static final Uri CONTENT_URI = Uri.withAppendedPath(TABS_AUTHORITY_URI, "clients");
        public static final String CONTENT_TYPE = "vnd.android.cursor.dir/client";
        public static final String CONTENT_ITEM_TYPE = "vnd.android.cursor.item/client";

        // Implicit rowid in SQL table.
        public static final String ROWID = "rowid";

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
        public static final String CREATED = "created";
        public static final String FILTER = "filter";

        public static final String[] DEFAULT_PROJECTION =
            new String[] { _ID, DATASET_ID, URL, TITLE, DESCRIPTION, IMAGE_URL, FILTER };
    }

    /*
     * Contains names and schema definitions for tables and views
     * no longer being used by current ContentProviders. These values are used
     * to make incremental updates to the schema during a database upgrade. Will be
     * removed with bug 947018.
     */
    static final class Obsolete {
        public static final String TABLE_IMAGES = "images";
        public static final String VIEW_BOOKMARKS_WITH_IMAGES = "bookmarks_with_images";
        public static final String VIEW_HISTORY_WITH_IMAGES = "history_with_images";
        public static final String VIEW_COMBINED_WITH_IMAGES = "combined_with_images";

        public static final class Images implements CommonColumns, SyncColumns {
            private Images() {}

            public static final String URL = "url_key";
            public static final String FAVICON_URL = "favicon_url";
            public static final String FAVICON = "favicon";
            public static final String THUMBNAIL = "thumbnail";
            public static final String _ID = "_id";
            public static final String GUID = "guid";
            public static final String DATE_CREATED = "created";
            public static final String DATE_MODIFIED = "modified";
            public static final String IS_DELETED = "deleted";
        }

        public static final class Combined {
            private Combined() {}

            public static final String THUMBNAIL = "thumbnail";
        }

        static final String TABLE_BOOKMARKS_JOIN_IMAGES = Bookmarks.TABLE_NAME + " LEFT OUTER JOIN " +
                Obsolete.TABLE_IMAGES + " ON " + Bookmarks.TABLE_NAME + "." + Bookmarks.URL + " = " +
                Obsolete.TABLE_IMAGES + "." + Obsolete.Images.URL;

        static final String TABLE_HISTORY_JOIN_IMAGES = History.TABLE_NAME + " LEFT OUTER JOIN " +
                Obsolete.TABLE_IMAGES + " ON " + Bookmarks.TABLE_NAME + "." + History.URL + " = " +
                Obsolete.TABLE_IMAGES + "." + Obsolete.Images.URL;

        static final String FAVICON_DB = "favicon_urls.db";
    }

    @RobocopTarget
    public static final class ReadingListItems implements CommonColumns, URLColumns, SyncColumns {
        private ReadingListItems() {}
        public static final Uri CONTENT_URI = Uri.withAppendedPath(READING_LIST_AUTHORITY_URI, "items");

        public static final String CONTENT_TYPE = "vnd.android.cursor.dir/readinglistitem";
        public static final String CONTENT_ITEM_TYPE = "vnd.android.cursor.item/readinglistitem";

        public static final String EXCERPT = "excerpt";
        public static final String READ = "read";
        public static final String LENGTH = "length";
        public static final String DEFAULT_SORT_ORDER = DATE_MODIFIED + " DESC";
        public static final String[] DEFAULT_PROJECTION = new String[] { _ID, URL, TITLE, EXCERPT, LENGTH };

        // Minimum fields required to create a reading list item.
        public static final String[] REQUIRED_FIELDS = { Bookmarks.URL, Bookmarks.TITLE };

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
        public static final String DISPLAY = "display";

        public static final String TYPE = "type";
    }

    @RobocopTarget
    public static final class SuggestedSites implements CommonColumns, URLColumns {
        private SuggestedSites() {}

        public static final Uri CONTENT_URI = Uri.withAppendedPath(AUTHORITY_URI, "suggestedsites");
    }
}
