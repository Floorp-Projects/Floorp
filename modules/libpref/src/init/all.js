/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

// SYNTAX HINTS:  dashes are delimiters.  Use underscores instead.
//  The first character after a period must be alphabetic.

config("timebomb.expiration_time",-1);
config("timebomb.warning_time",-1);
config("timebomb.relative_timebomb_secret_name","general.bproxy_cert_digest");
config("timebomb.relative_timebomb_days",-1);
config("timebomb.relative_timebomb_warning_days",-1);

pref("network.search.url","http://cgi.netscape.com/cgi-bin/url_search.cgi?search=");
pref("general.bproxy_cert_digest",0);

pref("general.startup.browser",             true);
pref("general.startup.mail",                false);
pref("general.startup.news",                false);
pref("general.startup.editor",              false);
pref("general.startup.conference",          false);
pref("general.startup.calendar",            false);
pref("general.startup.netcaster",           false);

pref("general.always_load_images",          true);
pref("general.always_load_movies",          true);
pref("general.always_load_sounds",          true);
pref("general.title_tips",                  true);

pref("general.help_source.site",            1); // 0 = Netscape, 1 = installed, 2 = custom
pref("general.help_source.url",             "");

pref("general.fullcircle_enable",           true);
pref("general.fullcircle_collect_ns_data",  false);

pref("browser.enable_style_sheets",         true);
pref("browser.startup.page",                1);     // 0 = blank, 1 = home, 2 = last
pref("browser.startup.homepage",            "http://www.mozilla.org/");
pref("browser.startup.homepage_override",   true);
pref("browser.startup.autoload_homepage",   true);
pref("browser.startup.agreed_to_licence",   false);
pref("browser.startup.license_version",     0);
pref("browser.startup.default_window",      1); // start up browser
pref("browser.cache.disk_cache_size",       7680);
pref("browser.cache.memory_cache_size",     1024);
pref("browser.cache.disk_cache_ssl",        false);
pref("browser.foreground_color",            "#000000");
pref("browser.background_color",            "#C0C0C0");
pref("browser.anchor_color",                "#0000EE");
pref("browser.visited_color",               "#551A8B");
pref("browser.chrome.show_directory_buttons",   true);
pref("browser.chrome.toolbar_style",        2);
pref("browser.chrome.advanced_toolbar",     false);
pref("browser.chrome.show_toolbar",         true);
pref("browser.chrome.show_status_bar",      true);
pref("browser.chrome.show_url_bar",         true);
pref("browser.chrome.show_security_bar",    true);
pref("browser.chrome.button_style",         0);

pref("browser.background_option",           0); // DEFAULT_BACKGROUND
pref("browser.link_expiration",             9);
pref("browser.cache.check_doc_frequency",   0);

pref("browser.delay_images",                false);
pref("browser.underline_anchors",           true);
pref("browser.never_expire",                false);
pref("browser.display_while_loading",       true);
pref("browser.custom_link_color",           false);
pref("browser.custom_visited_color",        false);
pref("browser.custom_text_color",           false);
pref("browser.use_document_colors",         true);
pref("browser.ldapfile_location",       "");
pref("browser.print_background",            false);
pref("browser.prefs_window_rect",           "-1,-1,-1,-1");
pref("browser.find_window_rect",            "-1,-1,-1,-1");
pref("browser.bookmark_window_rect",        "-1,-1,-1,-1");
pref("browser.download_window_rect",        "-1,-1,-1,-1");
pref("browser.wfe.ignore_def_check",false);
pref("browser.wfe.use_windows_colors",true);
pref("browser.startup_mode",1);
localDefPref("browser.bookmark_location",       "");
localDefPref("browser.addressbook_location",    "");
localDefPref("browser.socksfile_location",      "");
localDefPref("browser.ldapfile_location",       "");

pref("browser.url_history.URL_1", "");
pref("browser.url_history.URL_2", "");
pref("browser.url_history.URL_3", "");
pref("browser.url_history.URL_4", "");
pref("browser.url_history.URL_5", "");
pref("browser.url_history.URL_6", "");
pref("browser.url_history.URL_7", "");
pref("browser.url_history.URL_8", "");
pref("browser.url_history.URL_9", "");
pref("browser.url_history.URL_10", "");
pref("browser.url_history.URL_11", "");
pref("browser.url_history.URL_12", "");
pref("browser.url_history.URL_13", "");
pref("browser.url_history.URL_14", "");
pref("browser.url_history.URL_15", "");

pref("browser.personal_toolbar_button.min_chars", 15);
pref("browser.personal_toolbar_button.max_chars", 30);

pref("browser.PICS.ratings_enabled", false);
pref("browser.PICS.pages_must_be_rated", false);
pref("browser.PICS.disable_for_this_session", false);
pref("browser.PICS.reenable_for_this_session", false);
pref("browser.PICS.service.http___home_netscape_com_default_rating.service_enabled", true);
pref("browser.PICS.service.http___home_netscape_com_default_rating.s", 0);


// Smart Browsing prefs
pref("browser.related.enabled", true);
pref("browser.related.autoload", 1);  // 0 = Always, 1 = After first use, 2 = Never
pref("browser.related.provider", "http://www-rl.netscape.com/wtgn?");
pref("browser.related.detailsProvider", "http://cgi.netscape.com/cgi-bin/rlcgi.cgi?URL=");
pref("browser.related.disabledForDomains", "");
pref("browser.goBrowsing.enabled", true);


// The NavCenter preferences
localDefPref("browser.navcenter.dockstyle", 1); // 1 = left, 2 = right, 3 = top, 4 = bottom
localDefPref("browser.navcenter.docked.tree.visible", false);
localDefPref("browser.navcenter.docked.selector.visible", true);
localDefPref("browser.navcenter.docked.tree.width", 250); // Percent of parent window consumed by docked nav center
localDefPref("browser.navcenter.floating.rect", "20, 20, 400, 600"); // Window dimensions when floating

localDefPref("ghist.expires.pos",          4);
localDefPref("ghist.expires.width",        1400);
localDefPref("ghist.firstvisit.pos",       2);
localDefPref("ghist.firstvisit.width",     1400);
localDefPref("ghist.lastvisit.pos",        3);
localDefPref("ghist.lastvisit.width",      1400);
localDefPref("ghist.location.pos",         1);
localDefPref("ghist.location.width",       2400);
pref("ghist.show_value",           0);
pref("ghist.sort_descending",      false);
pref("ghist.sortby",               3);  // eGH_LastDateSort
localDefPref("ghist.title.pos",            0);
localDefPref("ghist.title.width",          2400);
localDefPref("ghist.visiblecolumns",       6);
localDefPref("ghist.visitcount.pos",       5);
localDefPref("ghist.visitcount.width",     1000);
localDefPref("ghist.window_rect",          "0,0,0,0");

pref("javascript.enabled",                  true);
pref("javascript.allow.mailnews",           true);
pref("javascript.allow.signing",            true);
pref("javascript.reflect_preferences",      false);     // for PE

pref("network.dnsAttempt",              0);
pref("network.tcptimeout",                  0);         // use default
pref("network.tcpbufsize",                  0);         //
pref("network.use_async_dns",               true);
pref("network.dnsCacheExpiration",          900); // in seconds
pref("network.enableUrlMatch",              true);
pref("network.max_connections",             4);
pref("network.speed_over_ui",               true);
pref("network.file_sort_method",            0);     // NAME 0, TYPE 1, SIZE 2, DATE 3
pref("network.ftp.passive",		    true);
pref("network.hosts.smtp_server",           "mail");
pref("network.hosts.pop_server",            "mail");
pref("network.hosts.nntp_server",           "news");
pref("network.hosts.socks_server",          "");
pref("network.hosts.socks_serverport",      1080);
pref("network.hosts.socks_conf",            "");
pref("network.proxy.autoconfig_url",        "");
pref("network.proxy.type",                  3); /* Changed default by req of Jon Kalb */
pref("network.proxy.ftp",                   "");
pref("network.proxy.ftp_port",              0);
pref("network.proxy.gopher",                "");
pref("network.proxy.gopher_port",           0);
pref("network.proxy.news",                  "");
pref("network.proxy.news_port",             0);
pref("network.proxy.http",                  "");
pref("network.proxy.http_port",             0);
pref("network.proxy.wais",                  "");
pref("network.proxy.wais_port",             0);
pref("network.proxy.ssl",                   "");
pref("network.proxy.ssl_port",              0);
pref("network.proxy.no_proxies_on",         "");
pref("network.online",                      true); //online/offline
pref("network.prompt_at_startup",           false);//Ask me
pref("network.accept_cookies",              0);     // 0 = Always, 1 = warn, 2 = never
pref("network.foreign_cookies",             0); // 0 = Accept, 1 = Don't accept
pref("network.cookie.cookieBehavior",       0); // 0-Accept, 1-dontAcceptForeign, 2-dontUse
pref("network.cookie.warnAboutCookies",     false);
pref("network.signon.rememberSignons",      true);
pref("network.cookie.filterName",			"");
pref("network.sendRefererHeader",           true);
pref("network.enablePad",                   false); // Allow client to do proxy autodiscovery
pref("network.padPacURL",                   ""); // The proxy autodiscovery url
pref("wallet.captureForms",                 true);
pref("wallet.useDialogs",                   false);
pref("privacy.warn_no_policy",              false); // Warn when submitting to site without policy

pref("messages.new_window",                 true); // ML obsolete; use mailnews.message_in_thread_window
pref("intl.accept_languages",               "en");
pref("intl.mailcharset.cyrillic",           "koi8-r");
pref("intl.accept_charsets",                "iso-8859-1,*,utf-8");
pref("intl.auto_detect_encoding",           true);
pref("intl.character_set",                  2);     // CS_LATIN1
pref("intl.font_encoding",                  6);     // CS_MAC_ROMAN

pref("browser.enable_webfonts",         true);
pref("browser.use_document_fonts",              2); // 0 = never, 1 = quick, 2 = always

// -- folders (Mac: these are binary aliases.)
localDefPref("browser.download_directory",      "");
localDefPref("browser.cache.directory",         "");
localDefPref("mail.signature_file",             "");
localDefPref("mail.directory",                  "");
localDefPref("mail.cc_file",                    "");
localDefPref("news.cc_file",                    "");

pref("news.fancy_listing",      true);      // obsolete
localDefPref("browser.cache.wfe.directory", null);
pref("browser.wfe.show_value", 1);
pref("browser.blink_allowed", true);
pref("images.dither", "auto");
pref("images.incremental_display", true);
pref("network.wfe.use_async_dns", true);
pref("network.wfe.tcp_connect_timeout",0);
localDefPref("news.directory",                  "");
localDefPref("security.directory",              "");

pref("autoupdate.enabled",              true);
pref("autoupdate.confirm_install",				false);
pref("autoupdate.unsigned_jar_support",  false);

pref("silentdownload.enabled",    true);
pref("silentdownload.directory",  "");
pref("silentdownload.range",      3000);
pref("silentdownload.interval",  10000);


pref("imap.io.mac.logging", false);

pref("browser.editor.disabled", false);

pref("netcaster.containers.count", "2");
pref("netcaster.containers.container1", "-3;'Channel_Finder';'http://netcaster.netscape.com/finder/container/index.html';1440;0");
pref("netcaster.containers.container2", "1,2;'Channels';");
pref("netcaster.channel.count", "0");
pref("netcaster.castanet.count", "0");
pref("netcaster.castanet.acceptCookies", false);
pref("netcaster.castanet.loggingEnabled", true);
pref("netcaster.castanet.profileEnabled", true);
pref("netcaster.admin.startTime", "9");
pref("netcaster.admin.endTime", "17");
pref("netcaster.admin.times", "0");
pref("netcaster.defaultChannel", "netscape_channel");

pref("SpellChecker.DefaultLanguage", 0);
pref("SpellChecker.DefaultDialect", 0);

pref("li.enabled", false);
pref("li.protocol","ldap");

pref("li.client.bookmarks", true);
pref("li.client.cookies", true);
pref("li.client.filters", true);
pref("li.client.addressbook", true);
pref("li.client.globalhistory", true);
pref("li.client.navcntr", true);
pref("li.client.liprefs", true);
pref("li.client.security", true);
pref("li.client.javasecurity", true);

pref("li.client.news", true);
pref("li.client.mail", true);

pref("li.login.name", "");
pref("li.login.password", "");

pref("li.server.http.baseURL", "");
pref("li.server.ldap.url", "");
pref("li.server.ldap.userbase", "");

pref("li.sync.enabled", false);
pref("li.sync.time", 30);

pref("mime.table.allow_add", true);
pref("mime.table.allow_edit", true);
pref("mime.table.allow_remove", true);
