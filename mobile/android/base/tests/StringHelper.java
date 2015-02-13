/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;


import android.content.res.Resources;

import org.mozilla.gecko.R;

public class StringHelper {
    private static StringHelper instance = null;

    public final String OK;

    // Note: DEFAULT_BOOKMARKS_TITLES.length == DEFAULT_BOOKMARKS_URLS.length
    public final String[] DEFAULT_BOOKMARKS_TITLES;
    public final String[] DEFAULT_BOOKMARKS_URLS;
    public final int DEFAULT_BOOKMARKS_COUNT;

    // About pages
    public final String ABOUT_BLANK_URL;
    public final String ABOUT_FIREFOX_URL;
    public final String ABOUT_RIGHTS_URL;
    public final String ABOUT_BUILDCONFIG_URL;
    public final String ABOUT_FEEDBACK_URL;
    public final String ABOUT_HEALTHREPORT_URL;
    public final String ABOUT_DOWNLOADS_URL;
    public final String ABOUT_HOME_URL;
    public final String ABOUT_ADDONS_URL;
    public final String ABOUT_APPS_URL;
    public final String ABOUT_ABOUT_URL;
    public final String ABOUT_SCHEME;

    // About pages' titles
    public final String ABOUT_HOME_TITLE;

    // Context Menu item strings
    public final String CONTEXT_MENU_BOOKMARK_LINK;
    public final String CONTEXT_MENU_OPEN_LINK_IN_NEW_TAB;
    public final String CONTEXT_MENU_OPEN_IN_NEW_TAB;
    public final String CONTEXT_MENU_OPEN_LINK_IN_PRIVATE_TAB;
    public final String CONTEXT_MENU_OPEN_IN_PRIVATE_TAB;
    public final String CONTEXT_MENU_COPY_LINK;
    public final String CONTEXT_MENU_SHARE_LINK;
    public final String CONTEXT_MENU_EDIT;
    public final String CONTEXT_MENU_SHARE;
    public final String CONTEXT_MENU_REMOVE;
    public final String CONTEXT_MENU_COPY_ADDRESS;
    public final String CONTEXT_MENU_EDIT_SITE_SETTINGS;
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
    public final String ROBOCOP_BIG_LINK_URL;
    public final String ROBOCOP_BIG_MAILTO_URL;
    public final String ROBOCOP_BLANK_PAGE_01_URL;
    public final String ROBOCOP_BLANK_PAGE_02_URL;
    public final String ROBOCOP_BLANK_PAGE_03_URL;
    public final String ROBOCOP_BLANK_PAGE_04_URL;
    public final String ROBOCOP_BLANK_PAGE_05_URL;
    public final String ROBOCOP_BOXES_URL;
    public final String ROBOCOP_GEOLOCATION_URL;
    public final String ROBOCOP_LOGIN_URL;
    public final String ROBOCOP_POPUP_URL;
    public final String ROBOCOP_OFFLINE_STORAGE_URL;
    public final String ROBOCOP_PICTURE_LINK_URL;
    public final String ROBOCOP_SEARCH_URL;
    public final String ROBOCOP_TEXT_PAGE_URL;
    public final String ROBOCOP_ADOBE_FLASH_URL;
    public final String ROBOCOP_INPUT_URL;

    private final String ROBOCOP_JS_HARNESS_URL;

    // Robocop page titles
    public final String ROBOCOP_BIG_LINK_TITLE;
    public final String ROBOCOP_BIG_MAILTO_TITLE;
    public final String ROBOCOP_BLANK_PAGE_01_TITLE;
    public final String ROBOCOP_BLANK_PAGE_02_TITLE;
    public final String ROBOCOP_BLANK_PAGE_03_TITLE;
    public final String ROBOCOP_BLANK_PAGE_04_TITLE;
    public final String ROBOCOP_BLANK_PAGE_05_TITLE;
    public final String ROBOCOP_BOXES_TITLE;
    public final String ROBOCOP_GEOLOCATION_TITLE;
    public final String ROBOCOP_LOGIN_TITLE;
    public final String ROBOCOP_OFFLINE_STORAGE_TITLE;
    public final String ROBOCOP_PICTURE_LINK_TITLE;
    public final String ROBOCOP_SEARCH_TITLE;
    public final String ROBOCOP_TEXT_PAGE_TITLE;
    public final String ROBOCOP_INPUT_TITLE;

    // Distribution tile labels
    public final String DISTRIBUTION1_LABEL;
    public final String DISTRIBUTION2_LABEL;

    // Settings menu strings
    // Section labels - ordered as found in the settings menu
    public final String CUSTOMIZE_SECTION_LABEL;
    public final String DISPLAY_SECTION_LABEL;
    public final String PRIVACY_SECTION_LABEL;
    public final String MOZILLA_SECTION_LABEL;
    public final String DEVELOPER_TOOLS_SECTION_LABEL;

    // Option labels
    // Customize
    public final String SYNC_LABEL;
    public final String IMPORT_FROM_ANDROID_LABEL;
    public final String TABS_LABEL;

    // Display
    public final String TEXT_SIZE_LABEL;
    public final String TITLE_BAR_LABEL;
    public final String SCROLL_TITLE_BAR_LABEL;
    public final String TEXT_REFLOW_LABEL;
    public final String CHARACTER_ENCODING_LABEL;
    public final String PLUGINS_LABEL;

    // Title bar
    public final String SHOW_PAGE_TITLE_LABEL;
    public final String SHOW_PAGE_ADDRESS_LABEL;

    // Privacy
    public final String TRACKING_PROTECTION_LABEL;
    public final String DNT_LABEL;
    public final String COOKIES_LABEL;
    public final String REMEMBER_PASSWORDS_LABEL;
    public final String MASTER_PASSWORD_LABEL;
    public final String CLEAR_PRIVATE_DATA_LABEL;

    // Mozilla
    public final String BRAND_NAME;
    public final String ABOUT_LABEL;
    public final String FAQS_LABEL;
    public final String FEEDBACK_LABEL;
    public final String LOCATION_SERVICES_LABEL;
    public final String HEALTH_REPORT_LABEL;
    public final String MY_HEALTH_REPORT_LABEL;

    // Developer tools
    public final String PAINT_FLASHING_LABEL;
    public final String REMOTE_DEBUGGING_LABEL;
    public final String LEARN_MORE_LABEL;

    // Labels for the about:home tabs
    public final String HISTORY_LABEL;
    public final String TOP_SITES_LABEL;
    public final String BOOKMARKS_LABEL;
    public final String READING_LIST_LABEL;
    public final String TODAY_LABEL;
    public final String TABS_FROM_LAST_TIME_LABEL;

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
    public final String APPS_LABEL;
    public final String SETTINGS_LABEL;
    public final String GUEST_MODE_LABEL;

    // Android 3.0+
    public final String TOOLS_LABEL;
    public final String PAGE_LABEL;

    // Android 2.3 and lower only
    public final String MORE_LABEL;
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
    public final String GEO_MESSAGE;
    public final String GEO_ALLOW;
    public final String GEO_DENY;

    public final String OFFLINE_MESSAGE;
    public final String OFFLINE_ALLOW;
    public final String OFFLINE_DENY;

    public final String LOGIN_MESSAGE;
    public final String LOGIN_ALLOW;
    public final String LOGIN_DENY;

    public final String POPUP_MESSAGE;
    public final String POPUP_ALLOW;
    public final String POPUP_DENY;

    private StringHelper (final Resources res) {

        OK = res.getString(R.string.button_ok);

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
        ABOUT_BLANK_URL = "about:blank";
        ABOUT_FIREFOX_URL = res.getString(R.string.bookmarkdefaults_url_aboutfirefox);
        ABOUT_RIGHTS_URL = "about:rights";
        ABOUT_BUILDCONFIG_URL = "about:buildconfig";
        ABOUT_FEEDBACK_URL = "about:feedback";
        ABOUT_HEALTHREPORT_URL = "about:healthreport";
        ABOUT_DOWNLOADS_URL = "about:downloads";
        ABOUT_HOME_URL = "about:home";
        ABOUT_ADDONS_URL = "about:addons";
        ABOUT_APPS_URL = "about:apps";
        ABOUT_ABOUT_URL = "about:about";
        ABOUT_SCHEME = "about:";

        // About pages' titles
        ABOUT_HOME_TITLE = "";

        // Context Menu item strings
        CONTEXT_MENU_BOOKMARK_LINK = "Bookmark Link";
        CONTEXT_MENU_OPEN_LINK_IN_NEW_TAB = "Open Link in New Tab";
        CONTEXT_MENU_OPEN_IN_NEW_TAB = res.getString(R.string.contextmenu_open_new_tab);
        CONTEXT_MENU_OPEN_LINK_IN_PRIVATE_TAB = "Open Link in Private Tab";
        CONTEXT_MENU_OPEN_IN_PRIVATE_TAB = res.getString(R.string.contextmenu_open_private_tab);
        CONTEXT_MENU_COPY_LINK = "Copy Link";
        CONTEXT_MENU_SHARE_LINK = "Share Link";
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

        // Robocop page urls
        // Note: please use getAbsoluteUrl(String url) on each robocop url to get the correct url
        ROBOCOP_BIG_LINK_URL = "/robocop/robocop_big_link.html";
        ROBOCOP_BIG_MAILTO_URL = "/robocop/robocop_big_mailto.html";
        ROBOCOP_BLANK_PAGE_01_URL = "/robocop/robocop_blank_01.html";
        ROBOCOP_BLANK_PAGE_02_URL = "/robocop/robocop_blank_02.html";
        ROBOCOP_BLANK_PAGE_03_URL = "/robocop/robocop_blank_03.html";
        ROBOCOP_BLANK_PAGE_04_URL = "/robocop/robocop_blank_04.html";
        ROBOCOP_BLANK_PAGE_05_URL = "/robocop/robocop_blank_05.html";
        ROBOCOP_BOXES_URL = "/robocop/robocop_boxes.html";
        ROBOCOP_GEOLOCATION_URL = "/robocop/robocop_geolocation.html";
        ROBOCOP_LOGIN_URL = "/robocop/robocop_login.html";
        ROBOCOP_POPUP_URL = "/robocop/robocop_popup.html";
        ROBOCOP_OFFLINE_STORAGE_URL = "/robocop/robocop_offline_storage.html";
        ROBOCOP_PICTURE_LINK_URL = "/robocop/robocop_picture_link.html";
        ROBOCOP_SEARCH_URL = "/robocop/robocop_search.html";
        ROBOCOP_TEXT_PAGE_URL = "/robocop/robocop_text_page.html";
        ROBOCOP_ADOBE_FLASH_URL = "/robocop/robocop_adobe_flash.html";
        ROBOCOP_INPUT_URL = "/robocop/robocop_input.html";

        ROBOCOP_JS_HARNESS_URL = "/robocop/robocop_javascript.html";

        // Robocop page titles
        ROBOCOP_BIG_LINK_TITLE = "Big Link";
        ROBOCOP_BIG_MAILTO_TITLE = "Big Mailto";
        ROBOCOP_BLANK_PAGE_01_TITLE = "Browser Blank Page 01";
        ROBOCOP_BLANK_PAGE_02_TITLE = "Browser Blank Page 02";
        ROBOCOP_BLANK_PAGE_03_TITLE = "Browser Blank Page 03";
        ROBOCOP_BLANK_PAGE_04_TITLE = "Browser Blank Page 04";
        ROBOCOP_BLANK_PAGE_05_TITLE = "Browser Blank Page 05";
        ROBOCOP_BOXES_TITLE = "Browser Box test";
        ROBOCOP_GEOLOCATION_TITLE = "Geolocation Test Page";
        ROBOCOP_LOGIN_TITLE = "Robocop Login";
        ROBOCOP_OFFLINE_STORAGE_TITLE = "Robocop offline storage";
        ROBOCOP_PICTURE_LINK_TITLE = "Picture Link";
        ROBOCOP_SEARCH_TITLE = "Robocop Search Engine";
        ROBOCOP_TEXT_PAGE_TITLE = "Robocop Text Page";
        ROBOCOP_INPUT_TITLE = "Robocop Input";

        // Distribution tile labels
        DISTRIBUTION1_LABEL = "Distribution 1";
        DISTRIBUTION2_LABEL = "Distribution 2";

        // Settings menu strings
        // Section labels - ordered as found in the settings menu
        CUSTOMIZE_SECTION_LABEL = res.getString(R.string.pref_category_customize);
        DISPLAY_SECTION_LABEL = res.getString(R.string.pref_category_display);
        PRIVACY_SECTION_LABEL = res.getString(R.string.pref_category_privacy_short);
        MOZILLA_SECTION_LABEL = res.getString(R.string.pref_category_vendor);
        DEVELOPER_TOOLS_SECTION_LABEL = res.getString(R.string.pref_category_devtools);

        // Option labels
        // Customize
        SYNC_LABEL = res.getString(R.string.pref_sync);
        IMPORT_FROM_ANDROID_LABEL = res.getString(R.string.pref_import_android);
        TABS_LABEL = res.getString(R.string.pref_restore);

        // Display
        TEXT_SIZE_LABEL = res.getString(R.string.pref_text_size);
        TITLE_BAR_LABEL = "Title bar";
        SCROLL_TITLE_BAR_LABEL = res.getString(R.string.pref_scroll_title_bar2);
        TEXT_REFLOW_LABEL = res.getString(R.string.pref_reflow_on_zoom);
        CHARACTER_ENCODING_LABEL = res.getString(R.string.pref_char_encoding);
        PLUGINS_LABEL = res.getString(R.string.pref_plugins);

        // Title bar
        SHOW_PAGE_TITLE_LABEL = "Show page title";
        SHOW_PAGE_ADDRESS_LABEL = "Show page address";

        // Privacy
        TRACKING_PROTECTION_LABEL = res.getString(R.string.pref_tracking_protection_title);
        DNT_LABEL = res.getString(R.string.pref_donottrack_title);
        COOKIES_LABEL = res.getString(R.string.pref_cookies_menu);
        REMEMBER_PASSWORDS_LABEL = res.getString(R.string.pref_remember_signons);
        MASTER_PASSWORD_LABEL = res.getString(R.string.pref_use_master_password);
        CLEAR_PRIVATE_DATA_LABEL = res.getString(R.string.pref_clear_private_data);

        // Mozilla
        BRAND_NAME = "(Fennec|Nightly|Aurora|Firefox Beta|Firefox)";
        ABOUT_LABEL = "About " + BRAND_NAME;
        FAQS_LABEL = res.getString(R.string.pref_vendor_faqs);
        FEEDBACK_LABEL = res.getString(R.string.pref_vendor_feedback);
        LOCATION_SERVICES_LABEL = "Mozilla Location Service";
        HEALTH_REPORT_LABEL = BRAND_NAME + " Health Report";
        MY_HEALTH_REPORT_LABEL = res.getString(R.string.datareporting_abouthr_title);

        // Developer tools
        PAINT_FLASHING_LABEL = res.getString(R.string.pref_developer_paint_flashing);
        REMOTE_DEBUGGING_LABEL = res.getString(R.string.pref_developer_remotedebugging);
        LEARN_MORE_LABEL = res.getString(R.string.pref_learn_more);

        // Labels for the about:home tabs
        HISTORY_LABEL = res.getString(R.string.home_history_title);
        TOP_SITES_LABEL = res.getString(R.string.home_top_sites_title);
        BOOKMARKS_LABEL = res.getString(R.string.bookmarks_title);
        READING_LIST_LABEL = res.getString(R.string.reading_list_title);
        TODAY_LABEL = res.getString(R.string.history_today_section);
        TABS_FROM_LAST_TIME_LABEL = "Open all tabs from last time";

        // Desktop default bookmarks folders
        BOOKMARKS_UP_TO = res.getString(R.string.home_move_up_to_filter);
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
        APPS_LABEL = res.getString(R.string.apps);
        SETTINGS_LABEL = res.getString(R.string.settings);
        GUEST_MODE_LABEL = res.getString(R.string.new_guest_session);

        // Android 3.0+
        TOOLS_LABEL = res.getString(R.string.tools);
        PAGE_LABEL = res.getString(R.string.page);

        // Android 2.3 and lower only
        MORE_LABEL = "More";
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
        GEO_MESSAGE = "Share your location with";
        GEO_ALLOW = res.getString(R.string.share);
        GEO_DENY = "Don't share";

        OFFLINE_MESSAGE = "to store data on your device for offline use";
        OFFLINE_ALLOW = "Allow";
        OFFLINE_DENY = "Don't allow";

        LOGIN_MESSAGE = "Save password";
        LOGIN_ALLOW = "Save";
        LOGIN_DENY = "Don't save";

        POPUP_MESSAGE = "prevented this site from opening";
        POPUP_ALLOW = res.getString(R.string.pref_panels_show);
        POPUP_DENY = "Don't show";
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
