/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// SYNTAX HINTS:  dashes are delimiters.  Use underscores instead.
//  The first character after a period must be alphabetic.

pref("network.search.url","http://cgi.netscape.com/cgi-bin/url_search.cgi?search=");

pref("keyword.URL", "http://keyword.netscape.com/keyword/");
pref("keyword.enabled", false);
pref("general.useragent.locale", "chrome://navigator/locale/navigator.properties");
pref("general.useragent.misc", "rv:0.9.5+");

pref("general.startup.browser",             true);
pref("general.startup.mail",                false);
pref("general.startup.news",                false);
pref("general.startup.editor",              false);
pref("general.startup.compose",             false);
pref("general.startup.addressbook",         false);

pref("general.open_location.last_url",      "");
pref("general.open_location.last_window_choice", 0);

// 0 = blank, 1 = home (browser.startup.homepage), 2 = last
pref("browser.startup.page",                1);
pref("browser.startup.homepage",	   "chrome://navigator-region/locale/region.properties");
// "browser.startup.homepage_override" was for 4.x
pref("browser.startup.homepage_override.1", true);
pref("browser.startup.autoload_homepage",   true);

pref("browser.cache.enable",                true); // see also network.http.use-cache
pref("browser.cache.disk.enable",           true);
pref("browser.cache.disk.capacity",         50000);
pref("browser.cache.memory.enable",         true);
pref("browser.cache.memory.capacity",       4096);
pref("browser.cache.disk_cache_ssl",        false);
// 0 = once-per-session, 1 = each-time, 2 = never, 3 = when-appropriate/automatically
pref("browser.cache.check_doc_frequency",   3);

pref("browser.display.use_document_fonts",  1);  // 0 = never, 1 = quick, 2 = always
pref("browser.display.use_document_colors", true);
pref("browser.display.use_system_colors",   false);
pref("browser.display.foreground_color",    "#000000");
pref("browser.display.background_color",    "#FFFFFF");
pref("browser.anchor_color",                "#0000EE");
pref("browser.visited_color",               "#551A8B");
pref("browser.underline_anchors",           true);
pref("browser.blink_allowed",               true);

pref("browser.display.use_focus_colors",    false);
pref("browser.display.focus_background_color", "#117722");
pref("browser.display.focus_text_color",     "#ffffff");
pref("browser.display.focus_ring_width",     1);
pref("browser.display.focus_ring_on_anything", false);

pref("browser.urlbar.autoFill", false);
pref("browser.urlbar.showPopup", true);
pref("browser.urlbar.showSearch", true);

pref("browser.chrome.toolbar_tips",         true);
pref("browser.chrome.toolbar_style",        2);

pref("browser.toolbars.showbutton.bookmarks", true);
pref("browser.toolbars.showbutton.go",      false);
pref("browser.toolbars.showbutton.home",    true);
pref("browser.toolbars.showbutton.mynetscape", true);
pref("browser.toolbars.showbutton.net2phone", true);
pref("browser.toolbars.showbutton.print",   true);
pref("browser.toolbars.showbutton.search",  true);

pref("browser.turbo.enabled", false);

pref("browser.helperApps.alwaysAsk.force",  false);
pref("browser.helperApps.neverAsk.saveToDisk", "");
pref("browser.helperApps.neverAsk.openFile", "");

pref("accessibility.browsewithcaret", false);
pref("accessibility.usetexttospeech", "");
pref("accessibility.usebrailledisplay", "");
pref("accessibility.accesskeycausesactivation", true);

// Dialog modality issues
pref("browser.prefWindowModal", true);
pref("browser.show_about_as_stupid_modal_window", false);

pref("browser.download.progressDnldDialog.keepAlive", true); // keep the dnload progress dialog up after dnload is complete

// various default search settings
pref("browser.search.defaulturl", "chrome://navigator-region/locale/region.properties");
pref("browser.search.opensidebarsearchpanel", true);
pref("browser.search.last_search_category", "NC:SearchCategory?category=urn:search:category:1");
pref("browser.search.mode", 0);
pref("browser.search.powermode", 0);
// basic search popup constraint: minimum sherlock plugin version displayed
// (note: must be a string representation of a float or it'll default to 0.0)
pref("browser.search.basic.min_ver", "0.0");
pref("browser.urlbar.autocomplete.enabled", true);
pref("browser.urlbar.clickSelectsAll", true);

pref("browser.history.last_page_visited", "");
pref("browser.history_expire_days", 9);
pref("browser.history.grouping", "day");
pref("browser.sessionhistory.max_entries", 50);

// Translation service
pref("browser.translation.service", "http://www.teletranslator.com:8120/?AlisUI=frames_ex/moz_home&alis_info=moz&AlisTargetURI=");
pref("browser.translation.serviceDomain", "teletranslator.com");
  
// Platform for Internet Content Selection
pref("browser.PICS.ratings_enabled", false);
pref("browser.PICS.pages_must_be_rated", false);
pref("browser.PICS.disable_for_this_session", false);
pref("browser.PICS.reenable_for_this_session", false);
pref("browser.PICS.service.http___home_netscape_com_default_rating.service_enabled", true);
pref("browser.PICS.service.http___home_netscape_com_default_rating.s", 0);

pref("browser.target_new_blocked", false);

// loading and rendering of framesets and iframes
pref("browser.frames.enabled", true);

// form submission
pref("browser.forms.submit.backwards_compatible", true);

// Tab browser preferences.
pref("browser.tabs.autoHide", true);
pref("browser.tabs.loadInBackground", false);
pref("browser.tabs.opentabfor.middleclick", false);
pref("browser.tabs.opentabfor.urlbar", false);
pref("browser.tabs.opentabfor.windowopen", false);
pref("browser.tabs.opentabfor.bookmarks", false);

// view source
pref("view_source.syntax_highlight", true);
pref("view_source.wrap_long_lines", false);

// gfx widgets
pref("nglayout.widget.mode", 2);
pref("nglayout.widget.gfxscrollbars", true);

// use nsViewManager2
pref("nglayout.view.useViewManager2", true);

// css2 hover pref
pref("nglayout.events.showHierarchicalHover", false);

// whether or not to use xbl form controls
pref("nglayout.debug.enable_xbl_forms", false);

// size of scrollbar snapping region
pref("slider.snapMultiplier", 6);

// Smart Browsing prefs
pref("browser.related.enabled", true);
pref("browser.related.autoload", 1);  // 0 = Always, 1 = After first use, 2 = Never
pref("browser.related.provider", "http://www-rl.netscape.com/wtgn?");
pref("browser.related.disabledForDomains", "");
pref("browser.goBrowsing.enabled", true);

//Internet Search
pref("browser.search.defaultenginename", "chrome://navigator/locale/navigator.properties");

// Print header customization
// Use the following codes:
// &T - Title
// &U - Document URL
// &D - Date/Time
// &P - Page Number
// &PT - Page Number "of" Page total
// Set each header to a string containing zero or one of these codes
// and the code will be replaced in that string by the corresponding data
pref("print.print_headerleft", "&T");
pref("print.print_headercenter", "");
pref("print.print_headerright", "&U");
pref("print.print_footerleft", "&P");
pref("print.print_footercenter", "");
pref("print.print_footerright", "&D");

// Default Capability Preferences: Security-Critical! 
// Editing these may create a security risk - be sure you know what you're doing
//pref("capability.policy.default.barprop.visible.set", "UniversalBrowserWrite");

pref("capability.policy.default.DOMException.code", "allAccess");
pref("capability.policy.default.DOMException.message", "allAccess");
pref("capability.policy.default.DOMException.name", "allAccess");
pref("capability.policy.default.DOMException.result", "allAccess");
pref("capability.policy.default.DOMException.toString", "allAccess");

pref("capability.policy.default.History.back", "allAccess");
pref("capability.policy.default.History.current", "UniversalBrowserRead");
pref("capability.policy.default.History.forward", "allAccess");
pref("capability.policy.default.History.go", "allAccess");
pref("capability.policy.default.History.item", "UniversalBrowserRead");
pref("capability.policy.default.History.next", "UniversalBrowserRead");
pref("capability.policy.default.History.previous", "UniversalBrowserRead");
pref("capability.policy.default.History.toString", "UniversalBrowserRead");

pref("capability.policy.default.HTMLDocument.close", "allAccess");
pref("capability.policy.default.HTMLDocument.open", "allAccess");

pref("capability.policy.default.Location.hash.set", "allAccess");
pref("capability.policy.default.Location.href.set", "allAccess");
pref("capability.policy.default.Location.reload", "allAccess");
pref("capability.policy.default.Location.replace", "allAccess");

pref("capability.policy.default.Navigator.preference", "allAccess");
pref("capability.policy.default.Navigator.preferenceinternal.get", "UniversalPreferencesRead");
pref("capability.policy.default.Navigator.preferenceinternal.set", "UniversalPreferencesWrite");

pref("capability.policy.default.Window.blur", "allAccess");
pref("capability.policy.default.Window.close", "allAccess");
pref("capability.policy.default.Window.closed", "allAccess");
pref("capability.policy.default.Window.Components", "allAccess");
pref("capability.policy.default.Window.document", "allAccess");
pref("capability.policy.default.Window.focus", "allAccess");
pref("capability.policy.default.Window.history", "allAccess");
pref("capability.policy.default.Window.location", "allAccess");
// window.openDialog is insecure and must be made inaccessible from web scripts - see bug 56009
pref("capability.policy.default.Window.openDialog", "noAccess");
pref("capability.policy.default.Window.self", "allAccess");
pref("capability.policy.default.Window.window", "allAccess");

// Restrictions on the DOM for mail/news - see bugs 66938 and 84545
pref("capability.policy.mailnews.sites", "mailbox: imap: news:");

pref("capability.policy.mailnews.*.attributes.get", "noAccess");
pref("capability.policy.mailnews.*.baseURI.get", "noAccess");
pref("capability.policy.mailnews.*.data.get", "noAccess");
pref("capability.policy.mailnews.*.getAttribute", "noAccess");
pref("capability.policy.mailnews.*.getNamedItem", "noAccess");
pref("capability.policy.mailnews.*.host.get", "noAccess");
pref("capability.policy.mailnews.*.hostname.get", "noAccess");
pref("capability.policy.mailnews.*.href.get", "noAccess");
pref("capability.policy.mailnews.*.innerHTML.get", "noAccess");
pref("capability.policy.mailnews.*.lowSrc.get", "noAccess");
pref("capability.policy.mailnews.*.nodeValue.get", "noAccess");
pref("capability.policy.mailnews.*.pathname.get", "noAccess");
pref("capability.policy.mailnews.*.protocol.get", "noAccess");
pref("capability.policy.mailnews.*.src.get", "noAccess");
pref("capability.policy.mailnews.*.substringData.get", "noAccess");
pref("capability.policy.mailnews.*.text.get", "noAccess");
pref("capability.policy.mailnews.*.title.get", "noAccess");
pref("capability.policy.mailnews.DOMException.toString", "noAccess");
pref("capability.policy.mailnews.HTMLAnchorElement.toString", "noAccess");
pref("capability.policy.mailnews.HTMLDocument.domain", "noAccess");
pref("capability.policy.mailnews.HTMLDocument.URL", "noAccess");
pref("capability.policy.mailnews.Location.toString", "noAccess");
pref("capability.policy.mailnews.Range.toString", "noAccess");
pref("capability.policy.mailnews.Window.name.set", "noAccess");

////////////////////////////////////////////////////////////
pref("capability.principal.codebase.foo.id", "http://www.netscape.com");
pref("capability.principal.codebase.foo.granted", "UniversalFoo");
//////////////////////////////////////////////////////////

pref("dom.disable_open_during_load", false);

pref("javascript.enabled",                  true);
pref("javascript.allow.mailnews",           false);
pref("javascript.options.strict",           false);

// advanced prefs
pref("advanced.always_load_images",         true);
pref("security.enable_java",                 true);
pref("css.allow",                           true);
pref("advanced.mailftp",                    false);
pref("image.animation_mode",                "normal");

pref("offline.startup_state",            0);
pref("offline.send.unsent_messages",            0);
pref("offline.download.download_messages",  0);
pref("offline.prompt_synch_on_exit",            true);
pref("offline.news.download.use_days",          0);

// If there is ever a security firedrill that requires
// us to block certian ports global, this is the pref 
// to use.  Is is a comma delimited list of port numbers
// for example:
//   pref("network.security.ports.banned", "1,2,3,4,5");
// prevents necko connecting to ports 1-5 unless the protocol
// overrides.

pref("network.hosts.smtp_server",           "mail");
pref("network.hosts.pop_server",            "mail");
pref("network.protocols.useSystemDefaults",   false); // set to true if user links should use system default handlers

// <ruslan>
pref("network.http.version", "1.1");	  // default
// pref("network.http.version", "1.0");   // uncomment this out in case of problems
// pref("network.http.version", "0.9");   // it'll work too if you're crazy
// keep-alive option is effectively obsolete. Nevertheless it'll work with
// some older 1.0 servers:

// enable caching of http documents
pref("network.http.use-cache", true);

pref("network.http.keep-alive", true); // set it to false in case of problems
pref("network.http.proxy.keep-alive", true);
pref("network.http.keep-alive.timeout", 300);

// limit the absolute number of http connections.
pref("network.http.max-connections", 24);

// limit the absolute number of http connections that can be established per
// host.  if a http proxy server is enabled, then the "server" is the proxy
// server.  Otherwise, "server" is the http origin server.
pref("network.http.max-connections-per-server", 8);

// if network.http.keep-alive is true, then a new connection will only be
// attempted if the number of active connections to a host is less then
// network.http.max-persistent-connections-per-server.  if a http proxy server
// is enabled, then the "server" is the proxy server.  Otherwise, "server" is
// the http origin server.
pref("network.http.max-persistent-connections-per-server", 2);

// amount of time (in seconds) to suspend pending requests, before spawning a
// new connection, once the limit on the number of persistent connections per
// host has been reached.  however, a new connection will not be created if the
// limit on the number of connections per host has also been reached.
pref("network.http.request.max-start-delay", 10);

// http specific network timeouts (XXX currently unused)
pref("network.http.connect.timeout",  30);	// in seconds
pref("network.http.request.timeout", 120);	// in seconds

// Headers
pref("network.http.accept.default", "text/xml, application/xml, application/xhtml+xml, text/html;q=0.9, image/png, image/jpeg, image/gif;q=0.2, text/plain;q=0.8, text/css, */*;q=0.1");

pref("network.http.sendRefererHeader",      2); // 0=don't send any, 1=send only on clicks, 2=send on image requests as well

// Enable http compression: comment this out in case of problems with 1.1
pref("network.http.accept-encoding" ,"gzip, deflate, compress;q=0.9");

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

// until the directory view has an owner
// necko will produce html -- dougt
pref("network.dir.generate_html", true);

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
pref("network.proxy.socks_version",         5);
pref("network.proxy.no_proxies_on",         "");
pref("network.online",                      true); //online/offline
pref("network.accept_cookies",              0);     // 0 = Always, 1 = warn, 2 = never
pref("network.foreign_cookies",             0); // 0 = Accept, 1 = Don't accept
pref("network.cookie.cookieBehavior",       0); // 0-Accept, 1-dontAcceptForeign, 2-dontUse
pref("network.cookie.warnAboutCookies",     false);
pref("network.cookie.lifetime.enabled",     false);
pref("network.cookie.lifetime.behavior",    0);
pref("network.cookie.lifetime.days",        90);
pref("network.cookie.p3p",                  "drdraaaa");
pref("network.cookie.p3plevel",             1);
pref("signon.rememberSignons",              true);
pref("network.enablePad",                   false); // Allow client to do proxy autodiscovery
pref("network.enableIDN",                   false); // Turn on/off IDN (Internationalized Domain Name) resolution
pref("converter.html2txt.structs",          true); // Output structured phrases (strong, em, code, sub, sup, b, i, u)
pref("converter.html2txt.header_strategy",  1); // 0 = no indention; 1 = indention, increased with header level; 2 = numbering and slight indention
pref("wallet.captureForms",                 true);
pref("wallet.notified",                     false);
pref("wallet.TutorialFromMenu",             "chrome://navigator/locale/navigator.properties");
pref("wallet.Server",                       "chrome://navigator/locale/navigator.properties");
pref("wallet.Samples",                      "chrome://navigator/locale/navigator.properties");
pref("wallet.version",                      "1");
pref("wallet.enabled",                      true);
pref("wallet.crypto",                       false);
pref("wallet.namePanel.hide",               false);
pref("wallet.addressPanel.hide",            false);
pref("wallet.phonePanel.hide",              false);
pref("wallet.creditPanel.hide",             false);
pref("wallet.employPanel.hide",             false);
pref("wallet.miscPanel.hide",               false);
pref("imageblocker.enabled",                true);
pref("intl.accept_languages",               "chrome://navigator/locale/navigator.properties");
pref("intl.accept_charsets",                "iso-8859-1,*,utf-8");
pref("intl.collationOption",                "chrome://navigator-platform/locale/navigator.properties");
pref("intl.menuitems.alwaysappendacceskeys","chrome://navigator/locale/navigator.properties");
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
pref("intl.charset.detector",               "chrome://navigator/locale/navigator.properties");
pref("intl.charset.default",                "chrome://navigator-platform/locale/navigator.properties");
pref("intl.content.langcode",               "chrome://communicator-region/locale/region.properties");
pref("font.language.group",                 "chrome://navigator/locale/navigator.properties");

// -- folders (Mac: these are binary aliases.)
localDefPref("mail.signature_file",             "");
localDefPref("mail.directory",                  "");

pref("images.dither", "auto");
localDefPref("news.directory",                  "");
localDefPref("security.directory",              "");

pref("autoupdate.enabled",              true);

pref("silentdownload.enabled",    true);
pref("silentdownload.directory",  "");
pref("silentdownload.range",      3000);
pref("silentdownload.interval",  10000);

pref("browser.editor.disabled", false);

pref("spellchecker.dictionary", "");

pref("signed.applets.codebase_principal_support", false);
pref("security.checkloaduri", true);
pref("security.xpconnect.plugin.unrestricted", true);

// Modifier key prefs: default to Windows settings,
// menu access key = alt, accelerator key = control.
pref("ui.key.accelKey", 17);
pref("ui.key.menuAccessKey", 18);
pref("ui.key.menuAccessKeyFocuses", false);
pref("ui.key.saveLink.shift", true); // true = shift, false = meta

// Middle-mouse handling
pref("middlemouse.paste", false);
pref("middlemouse.openNewWindow", true);
pref("middlemouse.contentLoadURL", false);
pref("middlemouse.scrollbarPosition", false);

// Clipboard behavior
pref("clipboard.autocopy", false);

// 0=lines, 1=pages, 2=history , 3=text size
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

pref("profile.confirm_automigration",true);

// Customizable toolbar stuff
pref("custtoolbar.personal_toolbar_folder", "");

pref("sidebar.customize.all_panels.url", "http://sidebar-rdf.netscape.com/%LOCALE%/sidebar-rdf/%SIDEBAR_VERSION%/all-panels.rdf");
pref("sidebar.customize.directory.url", "http://dmoz.org/Netscape/Sidebar/");
pref("sidebar.customize.more_panels.url", "http://dmoz.org/Netscape/Sidebar/");

pref("prefs.converted-to-utf8",false);
// --------------------------------------------------
// IBMBIDI 
// --------------------------------------------------
//
// ------------------
//  Text Direction
// ------------------
// 1 = directionLTRBidi *
// 2 = directionRTLBidi
pref("bidi.direction", 1);
// ------------------
//  Text Type
// ------------------
// 1 = charsettexttypeBidi *
// 2 = logicaltexttypeBidi
// 3 = visualtexttypeBidi
pref("bidi.texttype", 1);
// ------------------
//  Controls Text Mode
// ------------------
// 1 = logicalcontrolstextmodeBidiCmd
// 2 = visiualcontrolstextmodeBidi
// 3 = containercontrolstextmodeBidi *
pref("bidi.controlstextmode", 1);
// ------------------
//  Clipboard Text Mode
// ------------------
//  1 = logicalclipboardtextmodeBidi
// 2 = visiualclipboardtextmodeBidi
// 3 = sourceclipboardtextmodeBidi *
pref("bidi.clipboardtextmode", 3);
// ------------------
//  Numeral Style
// ------------------
// 1 = regularcontextnumeralBidi *
// 2 = hindicontextnumeralBidi
// 3 = arabicnumeralBidi
// 4 = hindinumeralBidi
pref("bidi.numeral", 1);
// ------------------
//  Support Mode
// ------------------
// 1 = mozillaBidisupport *
// 2 = OsBidisupport
// 3 = disableBidisupport
pref("bidi.support", 1);
// ------------------
//  Charset Mode
// ------------------
// 1 = doccharactersetBidi *
// 2 = defaultcharactersetBidi
pref("bidi.characterset", 1);


pref("browser.throbber.url","chrome://navigator-region/locale/region.properties");

// used for double-click word selection behavior. Win will override.
pref("layout.word_select.eat_space_to_next_word", false);
pref("layout.word_select.stop_at_punctuation", true);

