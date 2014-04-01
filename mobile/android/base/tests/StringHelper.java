package org.mozilla.gecko.tests;

import org.mozilla.gecko.*;

public class StringHelper {
    // Note: DEFAULT_BOOKMARKS_TITLES.length == DEFAULT_BOOKMARKS_URLS.length
    public static final String[] DEFAULT_BOOKMARKS_TITLES = new String[] {
        "Firefox: About your browser",
        "Firefox: Support",
        "Firefox: Customize with add-ons"
    };
    public static final String[] DEFAULT_BOOKMARKS_URLS = new String[] {
        "about:firefox",
        "http://support.mozilla.org/en-US/products/mobile",
        "https://addons.mozilla.org/en-US/android/"
    };
    public static final int DEFAULT_BOOKMARKS_COUNT = DEFAULT_BOOKMARKS_TITLES.length;

    // About pages
    public static final String ABOUT_BLANK_URL = "about:blank";
    public static final String ABOUT_FIREFOX_URL = "about:firefox";
    public static final String ABOUT_DOWNLOADS_URL = "about:downloads";
    public static final String ABOUT_HOME_URL = "about:home";
    public static final String ABOUT_ADDONS_URL = "about:addons";
    public static final String ABOUT_APPS_URL = "about:apps";

    // About pages' titles
    public static final String ABOUT_HOME_TITLE = "";

    // Context Menu menu items
    public static final String[] CONTEXT_MENU_ITEMS_IN_PRIVATE_TAB = new String[] {
        "Open Link in Private Tab",
        "Copy Link",
        "Share Link",
        "Bookmark Link"
    };

    public static final String[] CONTEXT_MENU_ITEMS_IN_NORMAL_TAB = new String[] {
        "Open Link in New Tab",
        "Open Link in Private Tab",
        "Copy Link",
        "Share Link",
        "Bookmark Link"
    };

    public static final String[] BOOKMARK_CONTEXT_MENU_ITEMS = new String[] {
        "Open in New Tab",
        "Open in Private Tab",
        "Edit",
        "Remove",
        "Share",
        "Add to Home Screen"
    };

    public static final String[] CONTEXT_MENU_ITEMS_IN_URL_BAR = new String[] {
        "Share",
        "Copy Address",
        "Edit Site Settings",
        "Add to Home Screen"
    };

    public static final String TITLE_PLACE_HOLDER = "Enter Search or Address";

    // Robocop page urls
    // Note: please use getAbsoluteUrl(String url) on each robocop url to get the correct url
    public static final String ROBOCOP_BIG_LINK_URL = "/robocop/robocop_big_link.html";
    public static final String ROBOCOP_BIG_MAILTO_URL = "/robocop/robocop_big_mailto.html";
    public static final String ROBOCOP_BLANK_PAGE_01_URL = "/robocop/robocop_blank_01.html";
    public static final String ROBOCOP_BLANK_PAGE_02_URL = "/robocop/robocop_blank_02.html";
    public static final String ROBOCOP_BLANK_PAGE_03_URL = "/robocop/robocop_blank_03.html";
    public static final String ROBOCOP_BOXES_URL = "/robocop/robocop_boxes.html";
    public static final String ROBOCOP_GEOLOCATION_URL = "/robocop/robocop_geolocation.html";
    public static final String ROBOCOP_LOGIN_URL = "/robocop/robocop_login.html";
    public static final String ROBOCOP_OFFLINE_STORAGE_URL = "/robocop/robocop_offline_storage.html";
    public static final String ROBOCOP_PICTURE_LINK_URL = "/robocop/robocop_picture_link.html";
    public static final String ROBOCOP_SEARCH_URL = "/robocop/robocop_search.html";
    public static final String ROBOCOP_TEXT_PAGE_URL = "/robocop/robocop_text_page.html";
    public static final String ROBOCOP_ADOBE_FLASH_URL = "/robocop/robocop_adobe_flash.html";
    public static final String ROBOCOP_INPUT_URL = "/robocop/robocop_input.html";
    public static final String ROBOCOP_JS_HARNESS_URL = "/robocop/robocop_javascript.html";

    // Robocop page titles
    public static final String ROBOCOP_BIG_LINK_TITLE = "Big Link";
    public static final String ROBOCOP_BIG_MAILTO_TITLE = "Big Mailto";
    public static final String ROBOCOP_BLANK_PAGE_01_TITLE = "Browser Blank Page 01";
    public static final String ROBOCOP_BLANK_PAGE_02_TITLE = "Browser Blank Page 02";
    public static final String ROBOCOP_BLANK_PAGE_03_TITLE = "Browser Blank Page 03";
    public static final String ROBOCOP_BOXES_TITLE = "Browser Box test";
    public static final String ROBOCOP_GEOLOCATION_TITLE = "Geolocation Test Page";
    public static final String ROBOCOP_LOGIN_TITLE = "Robocop Login";
    public static final String ROBOCOP_OFFLINE_STORAGE_TITLE = "Robocop offline storage";
    public static final String ROBOCOP_PICTURE_LINK_TITLE = "Picture Link";
    public static final String ROBOCOP_SEARCH_TITLE = "Robocop Search Engine";
    public static final String ROBOCOP_TEXT_PAGE_TITLE = "Robocop Text Page";
    public static final String ROBOCOP_INPUT_TITLE = "Robocop Input";

    // Settings menu strings
    // Section labels - ordered as found in the settings menu
    public static final String CUSTOMIZE_SECTION_LABEL = "Customize";
    public static final String DISPLAY_SECTION_LABEL = "Display";
    public static final String PRIVACY_SECTION_LABEL = "Privacy";
    public static final String MOZILLA_SECTION_LABEL = "Mozilla";
    public static final String DEVELOPER_TOOLS_SECTION_LABEL = "Developer tools";

    // Option labels
    // Customize
    public static final String SYNC_LABEL = "Sync";
    public static final String SEARCH_SETTINGS_LABEL = "Search settings";
    public static final String IMPORT_FROM_ANDROID_LABEL = "Import from Android";
    public static final String TABS_LABEL = "Tabs";

    // Display
    public static final String TEXT_SIZE_LABEL = "Text size";
    public static final String TITLE_BAR_LABEL = "Title bar";
    public static final String TEXT_REFLOW_LABEL = "Text reflow";
    public static final String CHARACTER_ENCODING_LABEL = "Character encoding";
    public static final String PLUGINS_LABEL = "Plugins";
 
    // Title bar
    public static final String SHOW_PAGE_TITLE_LABEL = "Show page title";
    public static final String SHOW_PAGE_ADDRESS_LABEL = "Show page address";

    // Privacy
    public static final String TRACKING_LABEL = "Tracking";
    public static final String COOKIES_LABEL = "Cookies";
    public static final String REMEMBER_PASSWORDS_LABEL = "Remember passwords";
    public static final String MASTER_PASWSWORD_LABEL = "Use master password";
    public static final String CLEAR_PRIVATE_DATA_LABEL = "Clear private data";

    // Mozilla
    public static final String ABOUT_LABEL = "About (Fennec|Nightly|Aurora|Firefox Beta|Firefox)";
    public static final String FAQS_LABEL = "FAQs";
    public static final String FEEDBACK_LABEL = "Give feedback";
    public static final String PRODUCT_ANNOUNCEMENTS_LABEL = "Show product announcements";
    public static final String LOCATION_SERVICES_LABEL = "Mozilla location services";
    public static final String HELTH_REPORT_LABEL = "(Fennec|Nightly|Aurora|Firefox Beta|Firefox) Health Report";
    public static final String MY_HEALTH_REPORT_LABEL = "View my Health Report";

    // Developer tools
    public static final String REMOTE_DEBUGGING_LABEL = "Remote debugging";
    public static final String LEARN_MORE_LABEL = "Learn more";

    // Labels for the about:home tabs
    public static final String HISTORY_LABEL = "HISTORY";
    public static final String TOP_SITES_LABEL = "TOP SITES";
    public static final String BOOKMARKS_LABEL = "BOOKMARKS";
    public static final String READING_LIST_LABEL = "READING LIST";
    public static final String TODAY_LABEL = "Today";
    public static final String TABS_FROM_LAST_TIME_LABEL = "Open all tabs from last time";

    // Desktop default bookmarks folders
    public static final String DESKTOP_FOLDER_LABEL = "Desktop Bookmarks";
    public static final String TOOLBAR_FOLDER_LABEL = "Bookmarks Toolbar";
    public static final String BOOKMARKS_MENU_FOLDER_LABEL = "Bookmarks Menu";
    public static final String UNSORTED_FOLDER_LABEL = "Unsorted Bookmarks";

    // Menu items - some of the items are found only on android 2.3 and lower and some only on android 3.0+
    public static final String NEW_TAB_LABEL = "New Tab";
    public static final String NEW_PRIVATE_TAB_LABEL = "New Private Tab";
    public static final String SHARE_LABEL = "Share";
    public static final String FIND_IN_PAGE_LABEL = "Find in Page";
    public static final String DESKTOP_SITE_LABEL = "Request Desktop Site";
    public static final String PDF_LABEL = "Save as PDF";
    public static final String DOWNLOADS_LABEL = "Downloads";
    public static final String ADDONS_LABEL = "Add-ons";
    public static final String APPS_LABEL = "Apps";
    public static final String SETTINGS_LABEL = "Settings";
    public static final String GUEST_MODE_LABEL = "New Guest Session";

    // Android 3.0+
    public static final String TOOLS_LABEL = "Tools";
    public static final String PAGE_LABEL = "Page";

    // Android 2.3 and lower only
    public static final String MORE_LABEL = "More";
    public static final String RELOAD_LABEL = "Reload";
    public static final String FORWARD_LABEL = "Forward";
    public static final String BOOKMARK_LABEL = "Bookmark";

    // Bookmark Toast Notification
    public static final String BOOKMARK_ADDED_LABEL = "Bookmark added";
    public static final String BOOKMARK_REMOVED_LABEL = "Bookmark removed";
    public static final String BOOKMARK_UPDATED_LABEL = "Bookmark updated";
    public static final String BOOKMARK_OPTIONS_LABEL = "Options";
}
