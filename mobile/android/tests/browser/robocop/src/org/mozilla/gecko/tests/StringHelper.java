/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import android.content.res.Resources;

import org.mozilla.gecko.R;

public class StringHelper {
    private static StringHelper instance;

    // This needs to be accessed statically, before an instance of StringHelper can be created.
    public static String STATIC_ABOUT_HOME_URL = "about:home";

    public final String OK;
    public final String CANCEL;
    public final String CLEAR;

    // Note: DEFAULT_BOOKMARKS_TITLES.length == DEFAULT_BOOKMARKS_URLS.length
    public final String[] DEFAULT_BOOKMARKS_TITLES;
    public final String[] DEFAULT_BOOKMARKS_URLS;
    public final int DEFAULT_BOOKMARKS_COUNT;

    // About pages
    public final String ABOUT_BLANK_URL = "about:blank";
    public final String ABOUT_FIREFOX_URL;
    public final String ABOUT_HOME_URL = "about:home";
    public final String ABOUT_ADDONS_URL = "about:addons";
    public final String ABOUT_SCHEME = "about:";

    // About pages' titles
    public final String ABOUT_HOME_TITLE = "";

    // Context Menu item strings
    public final String CONTEXT_MENU_BOOKMARK_LINK = "Bookmark Link";
    public final String CONTEXT_MENU_OPEN_LINK_IN_NEW_TAB = "Open Link in New Tab";
    public final String CONTEXT_MENU_OPEN_IN_NEW_TAB;
    public final String CONTEXT_MENU_OPEN_LINK_IN_PRIVATE_TAB = "Open Link in Private Tab";
    public final String CONTEXT_MENU_OPEN_IN_PRIVATE_TAB;
    public final String CONTEXT_MENU_COPY_LINK = "Copy Link";
    public final String CONTEXT_MENU_SHARE_LINK = "Share Link";
    public final String CONTEXT_MENU_EDIT;
    public final String CONTEXT_MENU_SHARE;
    public final String CONTEXT_MENU_REMOVE;
    public final String CONTEXT_MENU_COPY_ADDRESS;
    public final String CONTEXT_MENU_EDIT_SITE_SETTINGS;
    public final String CONTEXT_MENU_SITE_SETTINGS_SAVE_PASSWORD = "Save Password";
    public final String CONTEXT_MENU_ADD_TO_HOME_SCREEN;
    public final String CONTEXT_MENU_PIN_SITE;
    public final String CONTEXT_MENU_UNPIN_SITE;

    // Context Menu menu items
    public final String[] CONTEXT_MENU_ITEMS_IN_PRIVATE_TAB;

    public final String[] CONTEXT_MENU_ITEMS_IN_NORMAL_TAB;

    public final String[] BOOKMARK_CONTEXT_MENU_ITEMS;

    public final String[] CONTEXT_MENU_ITEMS_IN_URL_BAR;

    public final String TITLE_PLACE_HOLDER;

    // Robocop page urls
    // Note: please use getAbsoluteUrl(String url) on each robocop url to get the correct url
    public final String ROBOCOP_BIG_LINK_URL = "/robocop/robocop_big_link.html";
    public final String ROBOCOP_BIG_MAILTO_URL = "/robocop/robocop_big_mailto.html";
    public final String ROBOCOP_BLANK_PAGE_01_URL = "/robocop/robocop_blank_01.html";
    public final String ROBOCOP_BLANK_PAGE_02_URL = "/robocop/robocop_blank_02.html";
    public final String ROBOCOP_BLANK_PAGE_03_URL = "/robocop/robocop_blank_03.html";
    public final String ROBOCOP_BLANK_PAGE_04_URL = "/robocop/robocop_blank_04.html";
    public final String ROBOCOP_BLANK_PAGE_05_URL = "/robocop/robocop_blank_05.html";
    public final String ROBOCOP_BOXES_URL = "/robocop/robocop_boxes.html";
    public final String ROBOCOP_GEOLOCATION_URL = "/robocop/robocop_geolocation.html";
    public final String ROBOCOP_LOGIN_01_URL= "/robocop/robocop_login_01.html";
    public final String ROBOCOP_LOGIN_02_URL= "/robocop/robocop_login_02.html";
    public final String ROBOCOP_POPUP_URL = "/robocop/robocop_popup.html";
    public final String ROBOCOP_OFFLINE_STORAGE_URL = "/robocop/robocop_offline_storage.html";
    public final String ROBOCOP_PICTURE_LINK_URL = "/robocop/robocop_picture_link.html";
    public final String ROBOCOP_SEARCH_URL = "/robocop/robocop_search.html";
    public final String ROBOCOP_TEXT_PAGE_URL = "/robocop/robocop_text_page.html";
    public final String ROBOCOP_INPUT_URL = "/robocop/robocop_input.html";
    public final String ROBOCOP_READER_MODE_BASIC_ARTICLE = "/robocop/reader_mode_pages/basic_article.html";
    public final String ROBOCOP_LINK_TO_SLOW_LOADING = "/robocop/robocop_link_to_slow_loading.html";
    public final String ROBOCOP_MEDIA_PLAYBACK_JS_URL = "/robocop/robocop_media_playback_js.html";
    public final String ROBOCOP_MEDIA_PLAYBACK_LOOP_URL = "/robocop/robocop_media_playback_loop.html";

    private final String ROBOCOP_JS_HARNESS_URL = "/robocop/robocop_javascript.html";

    // Robocop page images
    public final String ROBOCOP_PICTURE_URL = "/robocop/Firefox.jpg";

    // Robocop page titles
    public final String ROBOCOP_BIG_LINK_TITLE = "Big Link";
    public final String ROBOCOP_BIG_MAILTO_TITLE = "Big Mailto";
    public final String ROBOCOP_BLANK_PAGE_01_TITLE = "Browser Blank Page 01";
    public final String ROBOCOP_BLANK_PAGE_02_TITLE = "Browser Blank Page 02";
    public final String ROBOCOP_GEOLOCATION_TITLE = "Geolocation Test Page";
    public final String ROBOCOP_PICTURE_LINK_TITLE = "Picture Link";
    public final String ROBOCOP_SEARCH_TITLE = "Robocop Search Engine";

    // Distribution tile labels
    public final String DISTRIBUTION1_LABEL = "Distribution 1";
    public final String DISTRIBUTION2_LABEL = "Distribution 2";

    // Settings menu strings
    public final String GENERAL_SECTION_LABEL;
    public final String SEARCH_SECTION_LABEL;
    public final String PRIVACY_SECTION_LABEL;
    public final String ACCESSIBILITY_SECTION_LABEL;
    public final String NOTIFICATIONS_SECTION_LABEL;
    public final String ADVANCED_SECTION_LABEL;
    public final String CLEAR_PRIVATE_DATA_SECTION_LABEL;
    public final String DEFAULT_BROWSER_SECTION_LABEL;
    public final String MOZILLA_SECTION_LABEL;

    // Mozilla
    public final String BRAND_NAME = "(Fennec|Nightly|Firefox Aurora|Firefox Beta|Firefox)";
    public final String ABOUT_LABEL = "About " + BRAND_NAME ;
    public final String LOCATION_SERVICES_LABEL = "Mozilla Location Service";

    // Labels for the about:home tabs
    public final String HISTORY_LABEL;
    public final String TOP_SITES_LABEL;
    public final String BOOKMARKS_LABEL;
    public final String TODAY_LABEL;

    // Desktop default bookmarks folders
    public final String BOOKMARKS_UP_TO;
    public final String BOOKMARKS_ROOT_LABEL;
    public final String DESKTOP_FOLDER_LABEL;
    public final String TOOLBAR_FOLDER_LABEL;
    public final String BOOKMARKS_MENU_FOLDER_LABEL;
    public final String UNSORTED_FOLDER_LABEL;

    // Menu items - some of the items are found only on android 2.3 and lower and some only on android 3.0+
    public final String NEW_TAB_LABEL;
    public final String NEW_PRIVATE_TAB_LABEL;
    public final String SHARE_LABEL;
    public final String FIND_IN_PAGE_LABEL;
    public final String DESKTOP_SITE_LABEL;
    public final String PDF_LABEL;
    public final String DOWNLOADS_LABEL;
    public final String ADDONS_LABEL;
    public final String LOGINS_LABEL;
    public final String SETTINGS_LABEL;
    public final String GUEST_MODE_LABEL;
    public final String TAB_QUEUE_LABEL;
    public final String TAB_QUEUE_SUMMARY;

    // Android 3.0+
    public final String TOOLS_LABEL;
    public final String PAGE_LABEL;

    // Android 2.3 and lower only
    public final String MORE_LABEL = "More";
    public final String RELOAD_LABEL;
    public final String FORWARD_LABEL;
    public final String BOOKMARK_LABEL;

    // Bookmark Toast Notification
    public final String BOOKMARK_ADDED_LABEL;
    public final String BOOKMARK_REMOVED_LABEL;
    public final String BOOKMARK_UPDATED_LABEL;
    public final String BOOKMARK_OPTIONS_LABEL;

    // Edit Bookmark screen
    public final String EDIT_BOOKMARK;

    // Strings used in doorhanger messages and buttons
    public final String GEO_MESSAGE = "Share your location with";
    public final String GEO_ALLOW;
    public final String GEO_DENY = "Don't share";

    public final String OFFLINE_MESSAGE = "to store data on your device for offline use";
    public final String OFFLINE_ALLOW = "Allow";
    public final String OFFLINE_DENY = "Don't allow";

    public final String LOGIN_MESSAGE = "Would you like " + BRAND_NAME + " to remember this login?";
    public final String LOGIN_ALLOW = "Remember";
    public final String LOGIN_DENY = "Never";

    public final String POPUP_MESSAGE = "prevented this site from opening";
    public final String POPUP_ALLOW;
    public final String POPUP_DENY = "Don't show";

    // Strings used as content description, e.g. for ImageButtons
    public final String CONTENT_DESCRIPTION_READER_MODE_BUTTON = "Enter Reader View";

    // Home Panel Settings
    public final String CUSTOMIZE_HOME;
    public final String ENABLED;
    public final String HISTORY;
    public final String PANELS;

    // General Settings
    public final String LANGUAGE_LABEL;

    // Search Settings
    public final String SEARCH_TITLE;
    public final String SEARCH_SUGGESTIONS;
    public final String SEARCH_INSTALLED;
    public final String SHOW_SEARCH_HISTORY_LABEL;

    // Privacy Settings
    public final String DO_NOT_TRACK_LABEL;

    // Accessibility Settings
    public final String ALWAYS_ZOOM_LABEL;

    // Notifications Settings
    public final String NEW_IN_FIREFOX_LABEL;

    // Advanced Settings
    public final String ADVANCED;
    public final String DONT_SHOW_MENU;
    public final String SHOW_MENU;
    public final String HIDE_TITLE_BAR;
    public final String RESTORE_TABS_LABEL;

    // Clear Private Data Section
    public final String SITE_SETTINGS_LABEL;

    // Mozilla Firefox Section
    public final String FAQS_LABEL;


    // Update Settings
    public final String AUTOMATIC_UPDATES;
    public final String OVER_WIFI_OPTION;
    public final String DOWNLOAD_UPDATES_AUTO;
    public final String ALWAYS;
    public final String NEVER;

    // Restore Tabs Settings
    public final String DONT_RESTORE_TABS;
    public final String ALWAYS_RESTORE_TABS;
    public final String DONT_RESTORE_QUIT;

    private StringHelper(final Resources res) {

        OK = res.getString(R.string.button_ok);
        CANCEL = res.getString(R.string.button_cancel);
        CLEAR = res.getString(R.string.button_clear);

        // Note: DEFAULT_BOOKMARKS_TITLES.length == DEFAULT_BOOKMARKS_URLS.length
        DEFAULT_BOOKMARKS_TITLES = new String[] {
                res.getString(R.string.bookmarkdefaults_title_aboutfirefox),
                res.getString(R.string.bookmarkdefaults_title_support),
                res.getString(R.string.bookmarkdefaults_title_addons)
        };
        DEFAULT_BOOKMARKS_URLS = new String[] {
                res.getString(R.string.bookmarkdefaults_url_aboutfirefox),
                res.getString(R.string.bookmarkdefaults_url_support),
                res.getString(R.string.bookmarkdefaults_url_addons)
        };
        DEFAULT_BOOKMARKS_COUNT = DEFAULT_BOOKMARKS_TITLES.length;

        // About pages
        ABOUT_FIREFOX_URL = res.getString(R.string.bookmarkdefaults_url_aboutfirefox);

        // Context Menu item strings
        CONTEXT_MENU_OPEN_IN_NEW_TAB = res.getString(R.string.contextmenu_open_new_tab);
        CONTEXT_MENU_OPEN_IN_PRIVATE_TAB = res.getString(R.string.contextmenu_open_private_tab);
        CONTEXT_MENU_EDIT = res.getString(R.string.contextmenu_top_sites_edit);
        CONTEXT_MENU_SHARE = res.getString(R.string.contextmenu_share);
        CONTEXT_MENU_REMOVE = res.getString(R.string.contextmenu_remove);
        CONTEXT_MENU_COPY_ADDRESS = res.getString(R.string.contextmenu_copyurl);
        CONTEXT_MENU_EDIT_SITE_SETTINGS = res.getString(R.string.contextmenu_site_settings);
        CONTEXT_MENU_ADD_TO_HOME_SCREEN = res.getString(R.string.contextmenu_add_to_launcher);
        CONTEXT_MENU_PIN_SITE = res.getString(R.string.contextmenu_top_sites_pin);
        CONTEXT_MENU_UNPIN_SITE = res.getString(R.string.contextmenu_top_sites_unpin);

        // Context Menu menu items
        CONTEXT_MENU_ITEMS_IN_PRIVATE_TAB = new String[] {
                CONTEXT_MENU_OPEN_LINK_IN_PRIVATE_TAB,
                CONTEXT_MENU_COPY_LINK,
                CONTEXT_MENU_SHARE_LINK,
                CONTEXT_MENU_BOOKMARK_LINK
        };

        CONTEXT_MENU_ITEMS_IN_NORMAL_TAB = new String[] {
                CONTEXT_MENU_OPEN_LINK_IN_NEW_TAB,
                CONTEXT_MENU_OPEN_LINK_IN_PRIVATE_TAB,
                CONTEXT_MENU_COPY_LINK,
                CONTEXT_MENU_SHARE_LINK,
                CONTEXT_MENU_BOOKMARK_LINK
        };

        BOOKMARK_CONTEXT_MENU_ITEMS = new String[] {
                CONTEXT_MENU_OPEN_IN_NEW_TAB,
                CONTEXT_MENU_OPEN_IN_PRIVATE_TAB,
                CONTEXT_MENU_COPY_ADDRESS,
                CONTEXT_MENU_SHARE,
                CONTEXT_MENU_EDIT,
                CONTEXT_MENU_REMOVE,
                CONTEXT_MENU_ADD_TO_HOME_SCREEN
        };

        CONTEXT_MENU_ITEMS_IN_URL_BAR = new String[] {
                CONTEXT_MENU_SHARE,
                CONTEXT_MENU_COPY_ADDRESS,
                CONTEXT_MENU_EDIT_SITE_SETTINGS,
                CONTEXT_MENU_ADD_TO_HOME_SCREEN
        };

        TITLE_PLACE_HOLDER = res.getString(R.string.url_bar_default_text);

        // Settings menu strings
        GENERAL_SECTION_LABEL = res.getString(R.string.pref_category_general);
        SEARCH_SECTION_LABEL = res.getString(R.string.pref_category_search);
        PRIVACY_SECTION_LABEL = res.getString(R.string.pref_category_privacy_short);
        ACCESSIBILITY_SECTION_LABEL = res.getString(R.string.pref_category_accessibility);
        NOTIFICATIONS_SECTION_LABEL = res.getString(R.string.pref_category_notifications);
        ADVANCED_SECTION_LABEL = res.getString(R.string.pref_category_advanced);
        CLEAR_PRIVATE_DATA_SECTION_LABEL = res.getString(R.string.pref_clear_private_data_now);
        DEFAULT_BROWSER_SECTION_LABEL = res.getString(R.string.pref_default_browser);
        MOZILLA_SECTION_LABEL = res.getString(R.string.pref_category_vendor);

        // Labels for the about:home tabs
        HISTORY_LABEL = res.getString(R.string.home_history_title);
        TOP_SITES_LABEL = res.getString(R.string.home_top_sites_title);
        BOOKMARKS_LABEL = res.getString(R.string.bookmarks_title);
        TODAY_LABEL = res.getString(R.string.history_today_section);

        BOOKMARKS_UP_TO = res.getString(R.string.home_move_back_to_filter);
        BOOKMARKS_ROOT_LABEL = res.getString(R.string.bookmarks_title);
        DESKTOP_FOLDER_LABEL = res.getString(R.string.bookmarks_folder_desktop);
        TOOLBAR_FOLDER_LABEL = res.getString(R.string.bookmarks_folder_toolbar);
        BOOKMARKS_MENU_FOLDER_LABEL = res.getString(R.string.bookmarks_folder_menu);
        UNSORTED_FOLDER_LABEL = res.getString(R.string.bookmarks_folder_unfiled);

        // Menu items - some of the items are found only on android 2.3 and lower and some only on android 3.0+
        NEW_TAB_LABEL = res.getString(R.string.new_tab);
        NEW_PRIVATE_TAB_LABEL = res.getString(R.string.new_private_tab);
        SHARE_LABEL = res.getString(R.string.share);
        FIND_IN_PAGE_LABEL = res.getString(R.string.find_in_page);
        DESKTOP_SITE_LABEL = res.getString(R.string.desktop_mode);
        PDF_LABEL = res.getString(R.string.save_as_pdf);
        DOWNLOADS_LABEL = res.getString(R.string.downloads);
        ADDONS_LABEL = res.getString(R.string.addons);
        LOGINS_LABEL = res.getString(R.string.logins);
        SETTINGS_LABEL = res.getString(R.string.settings);
        GUEST_MODE_LABEL = res.getString(R.string.new_guest_session);
        TAB_QUEUE_LABEL = res.getString(R.string.pref_tab_queue_title);
        TAB_QUEUE_SUMMARY = res.getString(R.string.pref_tab_queue_summary);

        // Android 3.0+
        TOOLS_LABEL = res.getString(R.string.tools);
        PAGE_LABEL = res.getString(R.string.page);

        // Android 2.3 and lower only
        RELOAD_LABEL = res.getString(R.string.reload);
        FORWARD_LABEL = res.getString(R.string.forward);
        BOOKMARK_LABEL = res.getString(R.string.bookmark);

        // Bookmark Toast Notification
        BOOKMARK_ADDED_LABEL = res.getString(R.string.bookmark_added);
        BOOKMARK_REMOVED_LABEL = res.getString(R.string.bookmark_removed);
        BOOKMARK_UPDATED_LABEL = res.getString(R.string.bookmark_updated);
        BOOKMARK_OPTIONS_LABEL = res.getString(R.string.bookmark_options);

        // Edit Bookmark screen
        EDIT_BOOKMARK = res.getString(R.string.bookmark_edit_title);

        // Strings used in doorhanger messages and buttons
        GEO_ALLOW = res.getString(R.string.share);

        POPUP_ALLOW = res.getString(R.string.pref_panels_show);

        // Home Settings
        PANELS = res.getString(R.string.pref_category_home_panels);
        CUSTOMIZE_HOME = res.getString(R.string.pref_category_home);
        ENABLED = res.getString(R.string.pref_home_updates_enabled);
        HISTORY = res.getString(R.string.home_history_title);

        // General Settings
        LANGUAGE_LABEL = res.getString(R.string.pref_category_language);

        // Search Settings
        SEARCH_TITLE = res.getString(R.string.search);
        SEARCH_SUGGESTIONS = res.getString(R.string.pref_search_suggestions);
        SEARCH_INSTALLED = res.getString(R.string.pref_category_installed_search_engines);
        SHOW_SEARCH_HISTORY_LABEL = res.getString(R.string.pref_history_search_suggestions);

        // Privacy Settings
        DO_NOT_TRACK_LABEL = res.getString(R.string.pref_donottrack_title);

        // Accessibility Settings
        ALWAYS_ZOOM_LABEL = res.getString(R.string.pref_zoom_force_enabled);

        // Notification Settings
        NEW_IN_FIREFOX_LABEL = res.getString(R.string.pref_whats_new_notification);

        // Advanced Settings
        ADVANCED = res.getString(R.string.pref_category_advanced);
        DONT_SHOW_MENU = res.getString(R.string.pref_char_encoding_off);
        SHOW_MENU = res.getString(R.string.pref_char_encoding_on);
        HIDE_TITLE_BAR = res.getString(R.string.pref_scroll_title_bar_summary );
        RESTORE_TABS_LABEL = res.getString(R.string.pref_restore);

        // Clear Private Data Settings
        SITE_SETTINGS_LABEL = res.getString(R.string.pref_private_data_siteSettings);

        // Mozilla Firefox Settings
        FAQS_LABEL = res.getString(R.string.pref_vendor_faqs);

        // Update Settings
        AUTOMATIC_UPDATES = res.getString(R.string.pref_home_updates);
        OVER_WIFI_OPTION = res.getString(R.string.pref_update_autodownload_wifi);
        DOWNLOAD_UPDATES_AUTO = res.getString(R.string.pref_update_autodownload);
        ALWAYS = res.getString(R.string.pref_update_autodownload_enabled);
        NEVER = res.getString(R.string.pref_update_autodownload_disabled);

        // Restore Tabs Settings
        DONT_RESTORE_TABS = res.getString(R.string.pref_restore_quit);
        ALWAYS_RESTORE_TABS = res.getString(R.string.pref_restore_always);
        DONT_RESTORE_QUIT = res.getString(R.string.pref_restore_quit);
    }

    public static void initialize(Resources res) {
        if (instance != null) {
            throw new IllegalStateException(StringHelper.class.getSimpleName() + " already Initialized");
        }
        instance = new StringHelper(res);
    }

    public static StringHelper get() {
        if (instance == null) {
            throw new IllegalStateException(StringHelper.class.getSimpleName() + " instance is not yet initialized. Use StringHelper.initialize(Resources) first.");
        }
        return instance;
    }

    /**
     * Build a URL for loading a Javascript file in the Robocop Javascript
     * harness.
     * <p>
     * We append a random slug to avoid caching: see
     * <a href="https://developer.mozilla.org/en-US/docs/Web/API/XMLHttpRequest/Using_XMLHttpRequest#Bypassing_the_cache">https://developer.mozilla.org/en-US/docs/Web/API/XMLHttpRequest/Using_XMLHttpRequest#Bypassing_the_cache</a>.
     *
     * @param javascriptUrl to load.
     * @return URL with harness wrapper.
     */
    public String getHarnessUrlForJavascript(String javascriptUrl) {
        // We include a slug to make sure we never cache the harness.
        return ROBOCOP_JS_HARNESS_URL +
                "?slug=" + System.currentTimeMillis() +
                "&path=" + javascriptUrl;
    }
}
