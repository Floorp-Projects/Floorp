/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

// SYNTAX HINTS:  dashes are delimiters.  Use underscores instead. 
//  The first character after a period must be alphabetic.

pref("network.search.url","http://cgi.netscape.com/cgi-bin/url_search.cgi?search=");

pref("keyword.URL", "http://keyword.netscape.com/keyword/");
pref("keyword.enabled", false);
pref("general.useragent.locale", "chrome://navigator/locale/navigator.properties");
pref("general.useragent.misc", "m18");

pref("general.startup.browser",             true);
pref("general.startup.mail",                false);
pref("general.startup.news",                false);
pref("general.startup.editor",              false);
pref("general.startup.compose",             false);
pref("general.startup.addressbook",         false);

pref("general.always_load_images",          true);
pref("general.always_load_movies",          true);
pref("general.always_load_sounds",          true);
pref("general.title_tips",                  true);

pref("general.help_source.site",            1); // 0 = Netscape, 1 = installed, 2 = custom
pref("general.help_source.url",             "");

pref("general.fullcircle_enable",           true);
pref("general.fullcircle_collect_ns_data",  false);

pref("browser.enable_style_sheets",         true);
// 0 = blank, 1 = home (browser.startup.homepage), 2 = last
pref("browser.startup.page",                1);     
pref("browser.startup.homepage",	   "chrome://navigator/locale/navigator.properties");
// "browser.startup.homepage_override" was for 4.x
pref("browser.startup.homepage_override.1", true);
pref("browser.startup.autoload_homepage",   true);
pref("browser.startup.agreed_to_licence",   false);
pref("browser.startup.license_version",     0);
pref("browser.startup.default_window",      1); // start up browser
pref("browser.cache.disk_cache_size",       7680);
pref("browser.cache.enable",                true);
pref("browser.cache.disk.enable",                true);
pref("browser.cache.memory_cache_size",     1024);
pref("browser.cache.disk_cache_ssl",        false);
pref("browser.display.foreground_color",    "#000000");
pref("browser.display.background_color",    "#C0C0C0");
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

pref("browser.toolbars.showbutton.bookmarks", false);
pref("browser.toolbars.showbutton.go",      false);
pref("browser.toolbars.showbutton.home",    true);
pref("browser.toolbars.showbutton.mynetscape", true);
pref("browser.toolbars.showbutton.net2phone", true);
pref("browser.toolbars.showbutton.print",   true);
pref("browser.toolbars.showbutton.search",  true);

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
pref("browser.display.use_document_colors", true);
pref("browser.ldapfile_location",       "");
pref("browser.print_background",            false);
pref("browser.prefs_window.modeless",       false);
pref("browser.prefs_window_rect",           "-1,-1,-1,-1");
pref("browser.find_window_rect",            "-1,-1,-1,-1");
pref("browser.bookmark_window_rect",        "-1,-1,-1,-1");
pref("browser.download_window_rect",        "-1,-1,-1,-1");
pref("browser.wfe.ignore_def_check",false);
pref("browser.display.wfe.use_windows_colors",true);
pref("browser.startup_mode",1);

// Dialog modality issues
pref("browser.prefWindowModal", true);
pref("browser.show_about_as_stupid_modal_window", false);

// various default search settings
pref("browser.search.defaulturl", "chrome://navigator/locale/navigator.properties");
pref("browser.search.opensidebarsearchpanel", true);
pref("browser.search.last_search_category", "NC:SearchCategory?category=urn:search:category:1");
pref("browser.search.mode", 0);
pref("browser.search.powermode", 0);
pref("browser.search.use_double_clicks", false);
pref("browser.urlbar.autocomplete.enabled", true);

localDefPref("browser.bookmark_location",       "");
localDefPref("browser.addressbook_location",    "");
localDefPref("browser.socksfile_location",      "");
localDefPref("browser.ldapfile_location",       "");

pref("browser.history.last_page_visited", "");

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

pref("browser.uriloader", true); // turn new uri loading on by default

pref("browser.personal_toolbar_button.min_chars", 15);
pref("browser.personal_toolbar_button.max_chars", 30);

pref("browser.PICS.ratings_enabled", false);
pref("browser.PICS.pages_must_be_rated", false);
pref("browser.PICS.disable_for_this_session", false);
pref("browser.PICS.reenable_for_this_session", false);
pref("browser.PICS.service.http___home_netscape_com_default_rating.service_enabled", true);
pref("browser.PICS.service.http___home_netscape_com_default_rating.s", 0);

// gfx widgets
pref("nglayout.widget.mode", 2);
pref("nglayout.widget.gfxscrollbars", true);

// use nsViewManager2
pref("nglayout.view.useViewManager2", true);

// css2 hover pref
pref("nglayout.events.showHierarchicalHover", true);

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

pref("capability.policy.default.barprop.visible.write", "UniversalBrowserWrite");

pref("capability.policy.default.history.current.read", "UniversalBrowserRead");
pref("capability.policy.default.history.next.read", "UniversalBrowserRead");
pref("capability.policy.default.history.previous.read", "UniversalBrowserRead");
pref("capability.policy.default.history.item.read", "UniversalBrowserRead");

pref("capability.policy.default.navigator.preference.read", "UniversalPreferencesRead");
pref("capability.policy.default.navigator.preference.write", "UniversalPreferencesWrite");
pref("capability.policy.default.windowinternal.location.write", "allAccess");


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
pref("javascript.allow.mailnews",           false);
pref("javascript.allow.signing",            true);
pref("javascript.reflect_preferences",      false);     // for PE
//pref("javascript.options.strict",           true);

// advanced prefs
pref("advanced.always_load_images",         true);
pref("security.enable_java",                 true);
pref("css.allow",                           true);
pref("advanced.mailftp",                    true);

pref("offline.startup_state",            0);
pref("offline.send.unsent_messages",            0);
pref("offline.prompt_synch_on_exit",            true);
pref("offline.news.download.use_days",          0);

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
pref("network.protocols.useSystemDefaults",   false); // set to true if user links should use system default handlers

// <ruslan>
pref("network.http.version", "1.1");	  // default
// pref("network.http.version", "1.0");   // uncomment this out in case of problems
// pref("network.http.version", "0.9");   // it'll work too if you're crazy
// keep-alive option is effectively obsolete. Nevertheless it'll work with
// some older 1.0 servers:

pref("network.http.keep-alive", true); // set it to false in case of problems
pref("network.http.proxy.keep-alive", true );
pref("network.http.keep-alive.timeout", 300);

pref("network.http.max-connections",  8);
pref("network.http.keep-alive.max-connections", 20); // max connections to be kept alive
pref("network.http.keep-alive.max-connections-per-server", 8);

pref("network.http.connect.timeout",  30);	// in seconds
pref("network.http.request.timeout", 120);	// in seconds

// Enable http compression: comment this out in case of problems with 1.1
pref("network.http.accept-encoding" ,"gzip,deflate,compress,identity");

pref("network.http.pipelining"      , false);
pref("network.http.proxy.pipelining", false);

// Always pipeling the very first request:  this will only work when you are
// absolutely sure the the site or proxy you are browsing to/through support
// pipelining; the default behavior will be that the browser will first make
// a normal, non-pipelined request, then  examine  and remember the responce
// and only the subsequent requests to that site will be pipeline
pref("network.http.pipelining.firstrequest", false);

// Max number of requests in the pipeline
pref("network.http.pipelining.maxrequests" , 4);

pref("network.http.proxy.ssl.connect",true);
// </ruslan>

// sspitzer:  change this back to "news" when we get to beta.
// for now, set this to news.mozilla.org because you can only
// post to the server specified by this pref.
pref("network.hosts.nntp_server",           "news.mozilla.org");

pref("network.hosts.socks_server",          "");
pref("network.hosts.socks_serverport",      1080);
pref("network.hosts.socks_conf",            "");
pref("network.image.imageBehavior",         0); // 0-Accept, 1-dontAcceptForeign, 2-dontUse
pref("network.image.warnAboutImages",       false);
pref("network.proxy.autoconfig_url",        "");
pref("network.proxy.type",                  0);
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
pref("network.proxy.socks",                 "");
pref("network.proxy.socks_port",            0);
pref("network.proxy.no_proxies_on",         "");
pref("network.online",                      true); //online/offline
pref("network.prompt_at_startup",           false);//Ask me
pref("network.accept_cookies",              0);     // 0 = Always, 1 = warn, 2 = never
pref("network.foreign_cookies",             0); // 0 = Accept, 1 = Don't accept
pref("network.cookie.cookieBehavior",       0); // 0-Accept, 1-dontAcceptForeign, 2-dontUse
pref("network.cookie.warnAboutCookies",     false);
pref("signon.rememberSignons",              true);
pref("network.sendRefererHeader",           2); // 0=don't send any, 1=send only on clicks, 2=send on image requests as well
pref("network.enablePad",                   false); // Allow client to do proxy autodiscovery
pref("network.padPacURL",                   ""); // The proxy autodiscovery url
pref("converter.html2txt.structs",          true); // Output structured phrases (strong, em, code, sub, sup, b, i, u)
pref("converter.html2txt.header_strategy",  1); // 0 = no indention; 1 = indention, increased with header level; 2 = numbering and slight indention
pref("wallet.captureForms",                 true);
pref("wallet.notified",                     false);
pref("wallet.fetchPatches",                 false);
pref("wallet.Server",                       "chrome://navigator/locale/navigator.properties");
pref("wallet.Samples",                      "chrome://navigator/locale/navigator.properties");
pref("wallet.version",                      "1");
pref("wallet.enabled",                      true);
pref("wallet.crypto",                       false);
pref("imageblocker.enabled",                true);
pref("messages.new_window",                 true); // ML obsolete; use mailnews.message_in_thread_window
pref("intl.accept_languages",               "chrome://navigator/locale/navigator.properties");
pref("intl.accept_charsets",                "iso-8859-1,*,utf-8");
pref("intl.collationKeyAsCodePoint",        false);

pref("intl.charsetmenu.browser.static",     "chrome://navigator/locale/navigator.properties");
pref("intl.charsetmenu.browser.more1",      "chrome://navigator/locale/navigator.properties");
pref("intl.charsetmenu.browser.more2",      "chrome://navigator/locale/navigator.properties");
pref("intl.charsetmenu.browser.more3",      "chrome://navigator/locale/navigator.properties");
pref("intl.charsetmenu.browser.more4",      "chrome://navigator/locale/navigator.properties");
pref("intl.charsetmenu.browser.more5",      "chrome://navigator/locale/navigator.properties");
pref("intl.charsetmenu.mailedit",           "chrome://navigator/locale/navigator.properties");
pref("intl.charsetmenu.browser.cache",      "");
pref("intl.charsetmenu.mailview.cache",     "");
pref("intl.charsetmenu.composer.cache",     "");
pref("intl.charsetmenu.browser.cache.size", 5);
pref("intl.charset.detector",                "chrome://navigator/locale/navigator.properties");
pref("intl.charset.default",                "chrome://navigator/locale/navigator.properties");

pref("font.default", "serif");
pref("font.size.variable.ar", 16);
pref("font.size.fixed.ar", 13);

pref("font.size.variable.el", 16);
pref("font.size.fixed.el", 13);

pref("font.size.variable.he", 16);
pref("font.size.fixed.he", 13);

pref("font.size.variable.ja", 16);
pref("font.size.fixed.ja", 16);

pref("font.size.variable.ko", 16);
pref("font.size.fixed.ko", 16);

pref("font.size.variable.th", 16);
pref("font.size.fixed.th", 13);

pref("font.size.variable.tr", 16);
pref("font.size.fixed.tr", 13);

pref("font.size.variable.x-baltic", 16);
pref("font.size.fixed.x-baltic", 13);

pref("font.size.variable.x-central-euro", 16);
pref("font.size.fixed.x-central-euro", 13);

pref("font.size.variable.x-cyrillic", 16);
pref("font.size.fixed.x-cyrillic", 13);

pref("font.size.variable.x-unicode", 16);
pref("font.size.fixed.x-unicode", 13);

pref("font.size.variable.x-western", 16);
pref("font.size.fixed.x-western", 13);

pref("font.size.variable.zh-CN", 16);
pref("font.size.fixed.zh-CN", 16);

pref("font.size.variable.zh-TW", 16);
pref("font.size.fixed.zh-TW", 16);

pref("browser.enable_webfonts",                 true);
pref("browser.display.use_document_fonts",      1); // 0 = never, 1 = quick, 2 = always

// -- folders (Mac: these are binary aliases.)
localDefPref("browser.download_directory",      "");
localDefPref("mail.signature_file",             "");
localDefPref("mail.directory",                  "");
localDefPref("mail.cc_file",                    "");
localDefPref("news.cc_file",                    "");

pref("news.fancy_listing",      true);      // obsolete
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

pref("SpellChecker.DefaultLanguage", 0);
pref("SpellChecker.DefaultDialect", 0);

pref("mime.table.allow_add", true);
pref("mime.table.allow_edit", true);
pref("mime.table.allow_remove", true);

pref("signed.applets.codebase_principal_support", false);
pref("security.checkloaduri", true);
pref("security.xpconnect.plugin.unrestricted", true);

// Modifier key prefs: default to Windows settings,
// menu access key = alt, accelerator key = control.
pref("ui.key.acceleratorKey", 17);
pref("ui.key.menuAccessKey", 18);
pref("ui.key.menuAccessKeyFocuses", false);

// Middle-mouse handling
pref("middlemouse.paste", false);
pref("middlemouse.openNewWindow", false);

// Clipboard behavior
pref("clipboard.autocopy", false);

/* 0=lines, 1=pages, 2=history , 3=text size */
pref("mousewheel.withnokey.action",0);
pref("mousewheel.withnokey.numlines",1);	
pref("mousewheel.withnokey.sysnumlines",true);
pref("mousewheel.withcontrolkey.action",0);
pref("mousewheel.withcontrolkey.numlines",1);
pref("mousewheel.withcontrolkey.sysnumlines",true);
pref("mousewheel.withshiftkey.action",0);
pref("mousewheel.withshiftkey.numlines",1);
pref("mousewheel.withshiftkey.sysnumlines",false);
pref("mousewheel.withaltkey.action",2);
pref("mousewheel.withaltkey.numlines",1);
pref("mousewheel.withaltkey.sysnumlines",false);

pref("middlemouse.scrollbarPosition", false);

pref("profile.confirm_automigration",true);

// Customizable toolbar stuff
pref("custtoolbar.personal_toolbar_folder", "");
pref("custtoolbar.has_toolbar_folder", true);
pref("custtoolbar.Browser.Navigation_Toolbar.position", 0);
pref("custtoolbar.Browser.Navigation_Toolbar.showing", true);
pref("custtoolbar.Browser.Navigation_Toolbar.open", true);
pref("custtoolbar.Browser.Location_Toolbar.position", 1);
pref("custtoolbar.Browser.Location_Toolbar.showing", true);
pref("custtoolbar.Browser.Location_Toolbar.open", true);
pref("custtoolbar.Browser.Personal_Toolbar.position", 2);
pref("custtoolbar.Browser.Personal_Toolbar.showing", true);
pref("custtoolbar.Browser.Personal_Toolbar.open", true);
pref("custtoolbar.Messenger.Navigation_Toolbar.position", 0);
pref("custtoolbar.Messenger.Navigation_Toolbar.showing", true);
pref("custtoolbar.Messenger.Navigation_Toolbar.open", true);
pref("custtoolbar.Messenger.Location_Toolbar.position", 1);
pref("custtoolbar.Messenger.Location_Toolbar.showing", true);
pref("custtoolbar.Messenger.Location_Toolbar.open", true);
pref("custtoolbar.Messages.Navigation_Toolbar.position", 0);
pref("custtoolbar.Messages.Navigation_Toolbar.showing", true);
pref("custtoolbar.Messages.Navigation_Toolbar.open", true);
pref("custtoolbar.Messages.Location_Toolbar.position", 1);
pref("custtoolbar.Messages.Location_Toolbar.showing", true);
pref("custtoolbar.Messages.Location_Toolbar.open", true);
pref("custtoolbar.Folders.Navigation_Toolbar.position", 0);
pref("custtoolbar.Folders.Navigation_Toolbar.showing", true);
pref("custtoolbar.Folders.Navigation_Toolbar.open", true);
pref("custtoolbar.Folders.Location_Toolbar.position", 1);
pref("custtoolbar.Folders.Location_Toolbar.showing", true);
pref("custtoolbar.Folders.Location_Toolbar.open", true);
pref("custtoolbar.Address_Book.Address_Book_Toolbar.position", 0);
pref("custtoolbar.Address_Book.Address_Book_Toolbar.showing", true);
pref("custtoolbar.Address_Book.Address_Book_Toolbar.open", true);
pref("custtoolbar.Compose_Message.Message_Toolbar.position", 0);
pref("custtoolbar.Compose_Message.Message_Toolbar.showing", true);
pref("custtoolbar.Compose_Message.Message_Toolbar.open", true);
pref("custtoolbar.Composer.Composition_Toolbar.position", 0);
pref("custtoolbar.Composer.Composition_Toolbar.showing", true);
pref("custtoolbar.Composer.Composition_Toolbar.open", true);
pref("custtoolbar.Composer.Formatting_Toolbar.position", 1);
pref("custtoolbar.Composer.Formatting_Toolbar.showing", true);
pref("custtoolbar.Composer.Formatting_Toolbar.open", true);

pref("sidebar.customize.all_panels.url", "http://sidebar-rdf.netscape.com/%LOCALE%/sidebar-rdf/%SIDEBAR_VERSION%/all-panels.rdf");
pref("sidebar.customize.more_panels.url", "http://dmoz.org/Netscape/Sidebar/");

pref("prefs.converted-to-utf8",false);

pref("browser.throbber.url","chrome://navigator/locale/navigator.properties");
