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

pref("mailnews.logComposePerformance", false);

pref("mail.wrap_long_lines",                true);
pref("news.wrap_long_lines",                true);
pref("mail.inline_attachments",             true);
// hidden pref for controlling if the user agent string
// is displayed in the message pane or not...
pref("mailnews.headers.showUserAgent",       false);

// Mail server preferences, pop by default
pref("mail.server_type",	0); 	// 0 pop, 1 imap,
					// (Unix only:)
					// 2 movemail, 3 inbox          
pref("mail.auth_login", true);

pref("mail.default_drafts", "");    // empty string use default Drafts name;
pref("mail.default_templates", ""); // empty string use default Templates name

pref("mail.imap.server_sub_directory",      "");
pref("mail.imap.max_cached_connections",    10);
pref("mail.imap.fetch_by_chunks",           true);
pref("mail.imap.chunk_size",                10240);
pref("mail.imap.min_chunk_size_threshold",  15360);
pref("mail.imap.max_chunk_size",            40960);
pref("mail.imap.chunk_fast",                2);
pref("mail.imap.chunk_ideal",               4);
pref("mail.imap.chunk_add",                 2048);
pref("mail.imap.hide_other_users",          false);
pref("mail.imap.hide_unused_namespaces",    true);
pref("mail.imap.new_mail_get_headers",      true);
pref("mail.imap.auto_unsubscribe_from_noselect_folders",    true);
pref("mail.imap.cleanup_inbox_on_exit",     false);
pref("mail.imap.mime_parts_on_demand",      true);
pref("mail.imap.mime_parts_on_demand_max_depth", 15);
pref("mail.imap.mime_parts_on_demand_threshold", 30000);
pref("mail.thread_without_re",	            true);
pref("mail.leave_on_server",                false);
pref("mail.default_cc",                     "");
pref("mail.default_fcc",                    ""); // maibox:URL or Imap://Host/OnLineFolderName
pref("mail.check_new_mail",                 false);
pref("mail.pop3_gets_new_mail",             false);
pref("mail.check_time",                     10);
pref("mail.pop_name",                       "");
pref("mail.remember_password",              false);
pref("mail.pop_password",                   "");
pref("mail.auto_quote",                     true);
pref("mail.fixed_width_messages",           true);
pref("mail.citation_color",                 ""); // quoted color
pref("mail.quoted_style",                   0); // 0=plain, 1=bold, 2=italic, 3=bolditalic
pref("mail.quoted_size",                    0); // 0=normal, 1=big, 2=small
pref("mail.quoted_graphical",               true); // use HTML-style quoting for displaying plain text
pref("mail.quoteasblock",                   true); // use HTML-style quoting for quoting plain text
pref("mail.identity.organization",          "");
pref("mail.identity.reply_to",              "");
pref("mail.identity.username",              "");
pref("mail.identity.useremail",             "");
pref("mail.use_fcc",                        true);
pref("mail.cc_self",                        false);
pref("mail.strictly_mime",                  false);
pref("mail.strictly_mime_headers",          true);
pref("mail.file_attach_binary",             false);
pref("mail.show_headers",                   1); // some
pref("mail.pane_config",                    0);
pref("mail.addr_book.lastnamefirst", 0); //0=displayname, 1=lastname first, 2=firstname first
pref("mail.addr_book.displayName.autoGeneration", true);
pref("mail.addr_book.displayName.lastnamefirst", false); // generate display names in last first order
pref("mail.attach_vcard",                   false);
pref("mail.html_compose",                   true);
pref("mail.compose.other.header",	    "");
localDefPref("mail.fcc_folder",                     "");
pref("mail.encrypt_outgoing_mail",          false);
pref("mail.crypto_sign_outgoing_mail",      false);
pref("mail.default_html_action", 0);    // 0=ask, 1=plain, 2=html, 3=both
pref("mail.smtp.ssl",                       0); // 0 = no, 1 = try, 2 = must use SSL
pref("mail.mdn.report.not_in_to_cc", 0);      // 0: Never 1: Always 2: Ask me
pref("mail.mdn.report.outside_domain", 2);    // 0: Never 1: Always 2: Ask me
pref("mail.mdn.report.other", 2);   // 0: Never 1: Always 2: Ask me 3: Denial
pref("mail.incorporate.return_receipt", 1); // 0: inbox/filter 1: Sent folder
pref("mail.request.return_receipt", 2);     // 1: DSN 2: MDN 3: Both
pref("mail.receipt.request_header_type", 0); // 0: MDN-DNT header  1: RRT header 2: Both (MC)

pref("news.default_cc",                     "");
pref("news.default_fcc",                    ""); // mailbox:URL or Imap://Host/OnlineFolderName
pref("news.use_fcc",                        true);
pref("news.cc_self",                        false);
pref("news.fcc_folder",                     "");
pref("news.notify.on",                      true);
pref("news.max_articles",                   500);
pref("news.mark_old_read",                  false);

pref("mailnews.wraplength",                 72);

pref("mailnews.reply_on_top",               0); // 0=bottom 1=top 2=select+bottom 3=select+top

// 0=no header, 1="<author> wrote:", 2="On <date> <author> wrote:", 3="<author> wrote On <date>:", 4=user specified
pref("mailnews.reply_header_type",          1);
// locale which affects date format, set empty string to use application default locale
pref("mailnews.reply_header_locale",        "en-US");
pref("mailnews.reply_header_authorwrote",   "%s wrote");
pref("mailnews.reply_header_ondate",        "On %s");
// separator to separate between date and author
pref("mailnews.reply_header_separator",     ", ");
pref("mailnews.reply_header_colon",         ":");
pref("mailnews.reply_header_originalmessage",   "--- Original Message ---");

pref("mail.purge_threshhold",                100);
pref("mail.prompt_purge_threshhold",             false);   

pref("mailnews.offline_sync_mail",         false);
pref("mailnews.offline_sync_news",         false);
pref("mailnews.offline_sync_send_unsent",  true);
pref("mailnews.offline_sync_work_offline", false);   
pref("mailnews.force_ascii_search",         false);
pref("mailnews.autolookup_unknown_mime_types",  true);

pref("mailnews.send_default_charset",       "chrome://messenger/locale/messenger.properties");
pref("mailnews.view_default_charset",       "chrome://messenger/locale/messenger.properties");
pref("mailnews.force_charset_override",     false);

pref("mailnews.language_sensitive_font",    true);

pref("offline.news.download.unread_only",   true);
pref("offline.news.download.by_date",       true);
pref("offline.news.download.use_days",      false);
pref("offline.news.download.days",          30);    // days
pref("offline.news.download.increments",    3); // 0-yesterday, 1-1 wk ago, 2-2 wk ago,
                                                // 3-1 month ago, 4-6 month ago, 5-1 year ago

pref("ldap_1.number_of_directories", 6);

pref("ldap_1.directory1.description", "Personal Address Book");
pref("ldap_1.directory1.dirType", 2);
pref("ldap_1.directory1.isOffline", false);

pref("ldap_1.directory2.description", "Four11 Directory");
pref("ldap_1.directory2.serverName", "ldap.four11.com");

pref("ldap_1.directory3.description", "InfoSpace Directory");
pref("ldap_1.directory3.serverName", "ldap.infospace.com");

pref("ldap_1.directory4.description", "WhoWhere Directory");
pref("ldap_1.directory4.serverName", "ldap.whowhere.com");

pref("ldap_1.directory5.description", "Bigfoot Directory");
pref("ldap_1.directory5.serverName", "ldap.bigfoot.com");

pref("ldap_1.directory6.description", "Switchboard Directory");
pref("ldap_1.directory6.serverName", "ldap.switchboard.com");
pref("ldap_1.directory6.searchBase", "c=US");
pref("ldap_1.directory6.attributes.telephoneNumber", "Phone Number:homephone");
pref("ldap_1.directory6.attributes.street", "State:st");
pref("ldap_1.directory6.filter1.repeatFilterForWords", false);

pref("ldap_2.autoComplete.interval",							650);
pref("ldap_2.autoComplete.enabled",								true);
pref("ldap_2.autoComplete.useDirectory", false);
pref("ldap_2.autoComplete.skipDirectoryIfLocalMatchFound", true);
pref("ldap_2.autoComplete.directoryServer", "");

pref("ldap_2.servers.pab.position",								1);
pref("ldap_2.servers.pab.description",							"chrome://messenger/locale/addressbook/addressBook.properties");
pref("ldap_2.servers.pab.dirType",								2);
pref("ldap_2.servers.pab.isOffline",							false);

pref("ldap_2.servers.history.position",							2);
pref("ldap_2.servers.history.description",						"chrome://messenger/locale/addressbook/addressBook.properties");
pref("ldap_2.servers.history.dirType",							2);
pref("ldap_2.servers.history.isOffline",						false);


// A position of zero is a special value that indicates the directory is deleted.
// These entries are provided to keep the (obsolete) Four11 directory and the
// WhoWhere, Bigfoot and Switchboard directories from being migrated.
pref("ldap_2.servers.four11.position",						0);
pref("ldap_2.servers.four11.description",						"Four11 Directory");
pref("ldap_2.servers.four11.serverName",						"ldap.four11.com");

pref("ldap_2.servers.whowhere.position",						0);             
pref("ldap_2.servers.whowhere.description",						"WhoWhere Directory");
pref("ldap_2.servers.whowhere.serverName",						"ldap.whowhere.com");

pref("ldap_2.servers.bigfoot.position",							0);             
pref("ldap_2.servers.bigfoot.description",						"Bigfoot Directory");
pref("ldap_2.servers.bigfoot.serverName",                       "ldap.bigfoot.com");
                                                                                 
pref("ldap_2.servers.switchboard.position",						0);             
pref("ldap_2.servers.switchboard.description",					"Switchboard Directory");
pref("ldap_2.servers.switchboard.serverName",					"ldap.switchboard.com");

pref("ldap_2.user_id",											0);
pref("ldap_2.version",											3); /* Update kCurrentListVersion in include/dirprefs.h if you change this */
pref("ldap_2.prefs_migrated",      false);

pref("mailnews.confirm.moveFoldersToTrash", true);

pref("mailnews.start_page.url", "chrome://messenger-region/locale/region.properties");
pref("mailnews.start_page.enabled", true);

pref("mailnews.account_central_page.url", "chrome://messenger/locale/messenger.properties");

/* default prefs for Mozilla 5.0 */
pref("mail.identity.default.compose_html", true);
pref("mail.identity.default.valid", true);
pref("mail.identity.default.fcc",true);
pref("mail.identity.default.fcc_folder","mailbox://nobody@Local%20Folders/Sent");
pref("mail.identity.default.bcc_self",false);
pref("mail.identity.default.bcc_others",false);
pref("mail.identity.default.bcc_list","");
pref("mail.identity.default.draft_folder","mailbox://nobody@Local%20Folders/Drafts");
pref("mail.identity.default.stationery_folder","mailbox://nobody@Local%20Folders/Templates");
pref("mail.identity.default.directoryServer","");
pref("mail.identity.default.overrideGlobal_Pref", false);


pref("mail.collect_email_address_incoming", true);
pref("mail.collect_email_address_outgoing", true);
pref("mail.collect_email_address_newsgroup", false);
pref("mail.collect_email_address_enable_size_limit", true);
pref("mail.collect_email_address_size_limit", 700);
pref("mail.default_sendlater_uri","mailbox://nobody@Local%20Folders/Unsent%20Messages");

pref("mail.server.default.port", -1);
pref("mail.server.default.offline_support_level", -1);
pref("mail.server.default.leave_on_server", false);
pref("mail.server.default.download_on_biff", false);
pref("mail.server.default.check_time", 10);
// "mail.server.default.check_new_mail" now lives in the protocol info
pref("mail.server.default.dot_fix", true);
pref("mail.server.default.limit_offline_message_size", false);
pref("mail.server.default.max_size", 50);
pref("mail.server.default.auth_login", true);
pref("mail.server.default.delete_mail_left_on_server", false);
pref("mail.server.default.valid", true);
pref("mail.server.default.abbreviate",true);
pref("mail.server.default.isSecure", false);
pref("mail.server.default.override_namespaces", true);

pref("mail.server.default.delete_model", 1);
pref("mail.server.default.fetch_by_chunks", true);
pref("mail.server.default.mime_parts_on_demand", true);

pref("mail.server.default.always_authenticate",false);
pref("mail.server.default.max_articles", 500);
pref("mail.server.default.notify.on", true);
pref("mail.server.default.mark_old_read", false);
pref("mail.server.default.empty_trash_on_exit", false);

pref("mail.server.default.using_subscription", true);
pref("mail.server.default.dual_use_folders", true);
pref("mail.server.default.canDelete", false);
pref("mail.server.default.login_at_startup", false);
pref("mail.server.default.allows_specialfolders_usage", true);
pref("mail.server.default.canCreateFolders", true);
pref("mail.server.default.canFileMessages", true);

pref("mail.smtpserver.default.auth_method", 1); // auth any
pref("mail.smtpserver.default.try_ssl", 0);

pref("mail.display_glyph", true);   // see <http://www.bucksch.org/1/projects/mozilla/16507>
pref("mail.display_struct", true);  // ditto
pref("mail.send_struct", false);   // ditto

pref("mail.forward_message_mode", 0); // 0=default as attachment 2=forward as inline with attachments, (obsolete 4.x value)1=forward as quoted (mapped to 2 in mozilla)

pref("mail.startup.enabledMailCheckOnce", false);

pref("mailnews.max_header_display_length",3); // number of addresses to show

pref("messenger.throbber.url","chrome://messenger-region/locale/region.properties");
pref("compose.throbber.url","chrome://messenger-region/locale/region.properties");
pref("addressbook.throbber.url","chrome://messenger-region/locale/region.properties");

pref("mailnews.send_plaintext_flowed", true); // RFC 2646=======
pref("mailnews.display.disable_format_flowed_support", false);
pref("mailnews.nav_crosses_folders", 1); // prompt user when crossing folders

// these two news.cancel.* prefs are for use by QA for automated testing.  see bug #31057
pref("news.cancel.confirm",true);
pref("news.cancel.alert_on_success",true);
pref("mail.SpellCheckBeforeSend",false);
pref("mail.warn_on_send_accel_key", true);
pref("mail.enable_autocomplete",true);
pref("mailnews.html_domains","");
pref("mailnews.plaintext_domains","");

pref("mail.biff.play_sound",true);

pref("mail.content_disposition_type", 0);

pref("mailnews.show_send_progress", true); //Will show a progress dialog when saving or sending a message
pref("mail.server.default.retainBy", 1);

// for manual upgrades of certain UI features.
// 1 -> 2 is for the folder pane outliner landing, to hide the
// unread and total columns, see msgMail3PaneWindow.js
pref("mail.ui.folderpane.version", 1);                                          

//If set to a number greater than 0, msg compose windows will be recycled in order to open them quickly
pref("mail.compose.max_recycled_windows", 0); 

// true makes it so we persist the open state of news server when starting up the 3 pane
// this is costly, as it might result in network activity.
// false makes it so we act like 4.x
// see bug #103010 for details
pref("news.persist_server_open_state_in_folderpane",false);
