/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Benjamin Smedberg <bsmedberg@covad.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* The prefs in this file are specific to the seamonkey browser.
 * Generic default prefs that would be useful to embedders belong in
 * modules/libpref/src/init/all.js
 */

pref("startup.homepage_override_url","chrome://navigator-region/locale/region.properties");
pref("browser.chromeURL","chrome://navigator/content/navigator.xul");

pref("general.startup.browser",             true);
pref("general.startup.mail",                false);
pref("general.startup.news",                false);
pref("general.startup.editor",              false);
pref("general.startup.compose",             false);
pref("general.startup.addressbook",         false);

pref("general.open_location.last_url",      "");
pref("general.open_location.last_window_choice", 0);

pref("general.smoothScroll", false);

// 0 = blank, 1 = home (browser.startup.homepage), 2 = last
pref("browser.startup.page",                1);
pref("browser.startup.homepage",	   "chrome://navigator-region/locale/region.properties");
pref("browser.startup.homepage.count", 1);
// "browser.startup.homepage_override" was for 4.x
pref("browser.startup.homepage_override.1", true);
pref("browser.startup.autoload_homepage",   true);

pref("browser.urlbar.autoFill", false);
pref("browser.urlbar.showPopup", true);
pref("browser.urlbar.showSearch", true);
pref("browser.urlbar.matchOnlyTyped", false);

pref("browser.chrome.site_icons", true);
pref("browser.chrome.favicons", false);

// 0 = never, 1 = when in cache, 2 = always
pref("browser.chrome.load_toolbar_icons", 0);

pref("browser.toolbars.showbutton.bookmarks", true);
pref("browser.toolbars.showbutton.go",      false);
pref("browser.toolbars.showbutton.home",    true);
pref("browser.toolbars.showbutton.print",   true);
pref("browser.toolbars.showbutton.search",  true);

// Dialog modality issues
pref("browser.show_about_as_stupid_modal_window", false);

pref("browser.download.progressDnldDialog.keepAlive", true); // keep the dnload progress dialog up after dnload is complete
pref("browser.download.progressDnldDialog.enable_launch_reveal_buttons", true);
pref("browser.download.progressDnlgDialog.dontAskForLaunch", false);
pref("browser.download.finished_sound_url", "");

// various default search settings
pref("browser.search.defaulturl", "chrome://navigator-region/locale/region.properties");
pref("browser.search.opensidebarsearchpanel", true);
pref("browser.search.last_search_category", "NC:SearchCategory?category=urn:search:category:1");
pref("browser.search.mode", 0);
// basic search popup constraint: minimum sherlock plugin version displayed
// (note: must be a string representation of a float or it'll default to 0.0)
pref("browser.search.basic.min_ver", "0.0");
pref("browser.urlbar.autocomplete.enabled", true);
pref("browser.urlbar.clickSelectsAll", true);
// when clickSelectsAll=true, does it also apply when the click is past end of text?
pref("browser.urlbar.clickAtEndSelects", true);

pref("browser.search.param.Google.1.custom", "chrome://navigator/content/searchconfig.properties");
pref("browser.search.param.Google.1.default", "chrome://navigator/content/searchconfig.properties");

pref("browser.history.grouping", "day");
pref("browser.sessionhistory.max_entries", 50);

// Translation service
pref("browser.translation.service", "chrome://navigator-region/locale/region.properties");
pref("browser.translation.serviceDomain", "chrome://navigator-region/locale/region.properties");
  
// Smart Browsing prefs
pref("browser.related.enabled", true);
pref("browser.related.autoload", 1);  // 0 = Always, 1 = After first use, 2 = Never
pref("browser.related.provider", "http://www-rl.netscape.com/wtgn?");
pref("browser.related.disabledForDomains", "");

// Bookmarks prefs
pref("browser.bookmarks.confirm_sorting", true);

//Internet Search
pref("browser.search.defaultenginename", "chrome://communicator-region/locale/region.properties");

pref("javascript.options.showInConsole",    true);

pref("offline.startup_state",            0);
pref("offline.send.unsent_messages",            0);
pref("offline.download.download_messages",  0);
pref("offline.prompt_synch_on_exit",            true);

pref("signon.rememberSignons",              true);
pref("signon.expireMasterPassword",         false);

pref("wallet.captureForms",                 true);
pref("wallet.enabled",                      true);
pref("wallet.crypto",                       false);
pref("wallet.crypto.autocompleteoverride",  false); // Ignore 'autocomplete=off' - available only when wallet.crypto is enabled. 
pref("wallet.namePanel.hide",               false);
pref("wallet.addressPanel.hide",            false);
pref("wallet.phonePanel.hide",              false);
pref("wallet.creditPanel.hide",             false);
pref("wallet.employPanel.hide",             false);
pref("wallet.miscPanel.hide",               false);

// -- folders (Mac: these are binary aliases.)
pref("mail.signature_file",             "");
pref("mail.directory",                  "");
pref("news.directory",                  "");
pref("browser.editor.disabled", false);
pref("spellchecker.dictionary", "");

pref("xpinstall.dialog.confirm",        "chrome://communicator/content/xpinstall/institems.xul");
pref("xpinstall.dialog.progress",       "chrome://communicator/content/xpinstall/xpistatus.xul");
pref("xpinstall.dialog.progress.type",  "");
pref("xpinstall.whitelist.add", "mozilla.org, mozdev.org, texturizer.net");
pref("xpinstall.blacklist.add", "");

// Customizable toolbar stuff
pref("custtoolbar.personal_toolbar_folder", "");

pref("sidebar.customize.all_panels.url", "http://sidebar-rdf.netscape.com/%LOCALE%/sidebar-rdf/%SIDEBAR_VERSION%/all-panels.rdf");
pref("sidebar.customize.directory.url", "http://dmoz.org/Netscape/Sidebar/");
pref("sidebar.customize.more_panels.url", "http://dmoz.org/Netscape/Sidebar/");
pref("sidebar.num_tabs_in_view", 8);

pref("browser.throbber.url","chrome://navigator-region/locale/region.properties");

// pref to control the alert notification 
pref("alerts.slideIncrement", 1);
pref("alerts.slideIncrementTime", 10);
pref("alerts.totalOpenTime", 4000);
pref("alerts.height", 50);

// update notifications prefs
pref("update_notifications.enabled", true);
pref("update_notifications.provider.0.frequency", 7); // number of days
pref("update_notifications.provider.0.datasource", "chrome://communicator-region/locale/region.properties");

// 0 opens the download manager
// 1 opens a progress dialog
// 2 and other values, no download manager, no progress dialog. 
pref("browser.downloadmanager.behavior", 0);

pref("privacy.popups.first_popup",                true);
pref("privacy.popups.sound_enabled",              false);
pref("privacy.popups.sound_url",                  "");
pref("privacy.popups.statusbar_icon_enabled",     true);
pref("privacy.popups.prefill_whitelist",          false);
pref("privacy.popups.remove_blacklist",           true);

#ifdef XP_MACOSX
// determines the behavior upon starting a download.
//  0 - open the download manager
//  1 - open a progress dialog
//  2 - do nothing

pref("browser.downloadmanager.behavior", 1);
#endif

#ifndef XP_MACOSX
#ifdef XP_UNIX
// For the download dialog
pref("browser.download.progressDnldDialog.enable_launch_reveal_buttons", false);
pref("browser.urlbar.clickSelectsAll", false);
#endif
#endif
