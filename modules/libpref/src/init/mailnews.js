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

pref("mail.showMessengerPerformance", false);
pref("sidebar.messenger.open", true);

// Added by mwelch 97 Feb
pref("news.show_pretty_names",              false);
pref("mail.wrap_long_lines",                false);
pref("news.wrap_long_lines",                false);
pref("mail.inline_attachments",             true);

// Mail server preferences, pop by default
pref("mail.server_type",                    0); // 0 pop, 1 imap,
                                                // (Unix only:)
                                                // 2 movemail, 3 inbox

pref("mail.auth_login", true);

pref("mail.use_drafts", true);
pref("mail.default_drafts", "");    // empty string use default Drafts name;
pref("mail.use_templates", true);
pref("mail.default_templates", ""); // empty string use default Templates name

pref("mail.use_x_sender",                   false);
pref("mail.imap.local_copies",              false);
pref("mail.imap.cache_fetch_responses",     true);
pref("mail.imap.server_sub_directory",      "");
pref("mail.imap.delete_is_move_to_trash",   false);
pref("mail.imap.server_ssl",                false);
pref("mail.imap.ssl_port",                  993);
pref("mail.imap.hard.mailbox.depth",        0);
pref("mail.imap.max_cached_connections",    10);
pref("mail.imap.fetch_by_chunks",           true);
pref("mail.imap.auto_subscribe_on_open",    true);
pref("mail.imap.chunk_size",                10240);
pref("mail.imap.min_chunk_size_threshold",  15360);
pref("mail.imap.max_chunk_size",            40960);
pref("mail.imap.chunk_fast",                2);
pref("mail.imap.chunk_ideal",               4);
pref("mail.imap.chunk_add",                 2048);
pref("mail.imap.auto_subscribe",            true);
pref("mail.imap.auto_unsubscrube",          true);
pref("mail.imap.new_mail_get_headers",      true);
pref("mail.imap.strip_personal_namespace",  true);
pref("mail.imap.upgrade.leave_subscriptions_alone", false);
pref("mail.imap.upgrade.auto_subscribe_to_all",     false);
pref("mail.imap.auto_unsubscribe_from_noselect_folders",    true);
pref("mail.imap.allow_multiple_folder_connections",     false);
pref("mail.imap.cleanup_inbox_on_exit",     false);
pref("mail.imap.mime_parts_on_demand",      false);
pref("mail.imap.mime_parts_on_demand_threshold", 15000);
pref("mail.imap.optimize_header_dl",        false);
pref("mail.use_altmail",                    false);
pref("mail.altmail_dll",                    "postal32.dll");
pref("mail.use_altmail_for_news",           true);
pref("mail.thread_mail",                    false);
pref("mail.sort_mail",                      false);
pref("mail.ascend_mail",                    false);
pref("mail.leave_on_server",                false);
pref("mail.default_cc",                     "");
pref("mail.default_fcc",                    ""); // maibox:URL or Imap://Host/OnLineFolderName
pref("mail.check_new_mail",                 false);
pref("mail.pop3_gets_new_mail",             false);
pref("mail.check_time",                     10);
pref("mail.pop_name",                       "");
pref("mail.remember_password",              false);
pref("mail.support_skey",                   false);
pref("mail.pop_password",                   "");
pref("mail.auto_quote",                     true);
pref("mail.fixed_width_messages",           true);
pref("mail.quoted_style",                   2); // 0=plain, 1=bold, 2=italic, 3=bolditalic
pref("mail.quoted_size",                    0); // 0=normal, 1=bigger, 2=smaller
pref("mail.citation_color",                 "#000000"); // quoted color
pref("mail.identity.organization",          "");
pref("mail.identity.reply_to",              "");
pref("mail.identity.username",              "");
pref("mail.identity.useremail",             "");
pref("mail.identity.defaultdomain",         "");
pref("mail.use_fcc",                        true);
pref("mail.cc_self",                        false);
pref("mail.limit_message_size",             false);
pref("mail.max_size",                       50); // download message size limit
pref("mail.deliver_immediately",            true);
pref("mail.strictly_mime",                  false);
pref("mail.strictly_mime_headers",  true);
pref("mail.file_attach_binary",             false);
pref("mail.use_signature_file",             false);
pref("mail.show_headers",                   1); // some
pref("mail.pane_config",                    0);
pref("mail.sort_by",                        1);     // by date
localDefPref("mail.window_rect",                    "-1,-1,-1,-1");
localDefPref("mail.compose_window_rect",            "-1,-1,-1,-1");
pref("mail.addr_book.ldap.disabled", false);
localDefPref("mail.addr_book_window_rect",          "-1,-1,-1,-1");
pref("mail.wfe.addr_book.show_value", 0);
pref("mail.addr_book.lastnamefirst", false);
pref("mail.addr_book.sortby", 2);
pref("mail.addr_book.sort_ascending", true);
pref("mail.addr_book.displayName.autoGeneration", true);
localDefPref("mail.addr_book.name.width", 0);
localDefPref("mail.addr_book.email.width", 0);
localDefPref("mail.addr_book.nickname.width", 0);
localDefPref("mail.addr_book.locality.width", 0);
localDefPref("mail.addr_book.company.width", 0);
localDefPref("mail.addr_book.type.pos", -1);
localDefPref("mail.addr_book.name.pos", -1);
localDefPref("mail.addr_book.email.pos", -1);
localDefPref("mail.addr_book.nickname.pos", -1);
localDefPref("mail.addr_book.locality.pos", -1);
localDefPref("mail.addr_book.company.pos", -1);
pref("mail.attach_vcard",                   false);
pref("mail.html_compose",                   true);
pref("mail.compose.other.header",	    "");
pref("mail.htmldomains", "netscape.com,mcom.com");
pref("mail.play_sound",                     true);
pref("mail.send_html",                      true);
pref("mail.directory_names.first_first",    true);
pref("mail.attach_address_card",            false);
localDefPref("mail.fcc_folder",                     "");
pref("mail.purge_threshhold",               100);
pref("mail.prompt_purge_threshhold",        false); //Ask about compacting folders
pref("mail.encrypt_outgoing_mail",          false);
pref("mail.crypto_sign_outgoing_mail",      false);
pref("mail.crypto_sign_outgoing_news",      false);
pref("mail.warn_forward_encrypted",     true); // forward encrypted message to another warning
pref("mail.warn_reply_unencrypted",     true); // clear reply to encrypted message warning
pref("mail.use_mapi_server",                false);
pref("mail.match_nickname",                 false);
pref("mail.default_html_action", 0);    // 0=ask, 1=plain, 2=html, 3=both
pref("mail.selection.count",0); //default - 0 mail folders selected for download
pref("mail.smtp.ssl",                       0); // 0 = no, 1 = try, 2 = must use SSL
pref("mail.allow_at_sign_in_user_name", false);  //strip off chars following the @ sign in mail user name

pref("mail.mdn.report.enabled", false);       // enable sending MDN report
    // no denial mdns for not_in_to_cc, forward and outside_domain cases
    // this could eliminate potential mail traffics
pref("mail.mdn.report.not_in_to_cc", 0);      // 0: Never 1: Always 2: Ask me
pref("mail.mdn.report.outside_domain", 2);    // 0: Never 1: Always 2: Ask me
pref("mail.mdn.report.other", 2);   // 0: Never 1: Always 2: Ask me 3: Denial
pref("mail.incorporate.return_receipt", 1); // 0: inbox/filter 1: Sent folder
pref("mail.request.return_receipt", 2);     // 1: DSN 2: MDN 3: Both
pref("mail.receipt.request_header_type", 0); // 0: MDN-DNT header  1: RRT header 2: Both (MC)

pref("news.enabled",                        true);
pref("news.default_cc",                     "");
pref("news.default_fcc",                    ""); // mailbox:URL or Imap://Host/OnlineFolderName
pref("news.thread_news",                    true);
pref("news.use_fcc",                        true);
pref("news.cc_self",                        false);
pref("news.show_headers",                   2); // some
pref("news.pane_config",                    0);
pref("news.sort_by",                        1);     // by date
localDefPref("news.window_rect",                    "-1,-1,-1,-1");
pref("news.fcc_folder",                     "");
pref("news.notify.on",                      true);
pref("news.notify.size",                    1000);
// was 500.  this is only until we fix performance in 5.0
pref("news.max_articles",                   50);
pref("news.abbreviate_pretty_names",        0);
pref("news.mark_old_read",                  false);
pref("news.server_is_secure",               false);
pref("news.server_port",                    119);  //default non-secure port number
localDefPref("news.subscribe.name_width",           -1);   // Subscribe UI cols width
localDefPref("news.subscribe.join_width",           -1);
localDefPref("news.subscribe.post_width",           -1);
localDefPref("news.subscribe.name_pos",             0);    // Subscribe UI cols pos
localDefPref("news.subscribe.join_pos",             1);
localDefPref("news.subscribe.post_pos",             2);
// the next pref's value is irrelevant; it gets tweaked when back end needs to be called
// regarding any change to network.hosts.nntp_server, news.server_port,
// and/or news.server_is_secure.
pref("news.server_change_xaction",          0);
pref("news.send.fake_sender",               "");


pref("mailnews.profile_age",                0);
pref("mailnews.profile_age.default_install",    13);
localDefPref("mailnews.folder_window_rect",         "-1,-1,-1,-1");
localDefPref("mailnews.thread_window_rect",         "-1,-1,-1,-1");
localDefPref("mailnews.message_window_rect",        "-1,-1,-1,-1");

localDefPref("mailnews.thread_pane_height",         -1);
localDefPref("mailnews.category_pane_width",        -1);
pref("mailnews.reuse_thread_window",        true);
pref("mailnews.reuse_message_window",       true);
pref("mailnews.message_in_thread_window",   true);
pref("mailnews.javascript_enabled",         true);

pref("mailnews.wraplength",                 72);
pref("mailnews.nicknames_only",             false);
pref("mailnews.nav_crosses_folders",        1); // 0=do it, don't prompt 1=prompt, 2=don't do it, don't prompt

pref("mailnews.reply_on_top",               1); // 0=bottom 1=top 2=select+bottom 3=select+top
pref("mailnews.reply_with_extra_lines",     2);

pref("mailnews.force_ascii_search",         true);
pref("mailnews.autolookup_unknown_mime_types",  true);

pref("mailnews.customHeaders", "");
pref("mailnews.searchSubFolders",           true);
pref("mailnews.searchServer",               true);
pref("mailnews.sort_by_date_received",      false);

pref("offline.startup_mode",                0);     // 0 online work, 1 offline work, 2 ask me
pref("offline.download.size_limit",         50);    // K
pref("offline.news.download.unread_only",   true);
pref("offline.news.download.by_date",       true);
pref("offline.news.download.use_days",      false);
pref("offline.news.download.days",          30);    // days
pref("offline.news.download.increments",    3); // 0-yesterday, 1-1 wk ago, 2-2 wk ago,
                                                // 3-1 month ago, 4-6 month ago, 5-1 year ago
pref("offline.news.discussions_count",      0); // select discussion count

pref("offline.download_discussions",        true);  //default for offline usage is TRUE
pref("offline.download_mail",               true);  //default for offline usage is TRUE
pref("offline.download_messages",           true);  //default for offline usage is TRUE
pref("offline.download_directories",        true);  //default for offline usage is TRUE

pref("news.keep.method",                    0); // 0 = all, 1 = by age, 2 = by message count
pref("news.keep.days",                      30); // days
pref("news.keep.count",                     30); //keep x newest messages
pref("news.keep.only_unread",               false);
pref("news.remove_bodies.by_age",           false);
pref("news.remove_bodies.days",             20);

pref("ldap_1.autoComplete.interval", 1250);
pref("ldap_1.autoComplete.useAddressBooks", true);
pref("ldap_1.autoComplete.useDirectory", false);
pref("ldap_1.autoComplete.showDialogForMultipleMatches", true);
pref("ldap_1.autoComplete.skipDirectoryIfLocalMatchFound", false);

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

pref("ldapList.version", 0);

pref("ldap_2.autoComplete.interval",							650);
pref("ldap_2.autoComplete.enabled",								true);
pref("ldap_2.autoComplete.useAddressBooks",						true);
pref("ldap_2.autoComplete.useDirectory",						false);
pref("ldap_2.autoComplete.showDialogForMultipleMatches",		true);
pref("ldap_2.autoComplete.skipDirectoryIfLocalMatchFound",		false);
pref("ldap_2.autoComplete.nicknameHasPrecedence",               false);
pref("ldap_2.autoComplete.numAddresBooks",						15); // number of address books to search for name completion.

pref("ldap_2.servers.pab.position",								1);
pref("ldap_2.servers.pab.description",							"Personal Address Book");
pref("ldap_2.servers.pab.dirType",								2);
pref("ldap_2.servers.pab.isOffline",							false);

pref("ldap_2.servers.history.position",							2);
pref("ldap_2.servers.history.description",						"Collected Addresses");
pref("ldap_2.servers.history.dirType",							2);
pref("ldap_2.servers.history.isOffline",						false);


pref("ldap_2.servers.netcenter.position",						3);
pref("ldap_2.servers.netcenter.description",					"Netcenter Member Directory");
pref("ldap_2.servers.netcenter.vlvDisabled",					true);
pref("ldap_2.servers.netcenter.serverName",						"memberdir.netscape.com");
pref("ldap_2.servers.netcenter.searchBase",						"ou=member_directory,o=netcenter.com");
pref("ldap_2.servers.netcenter.auth.enabled",					true);
pref("ldap_2.servers.netcenter.auth.dn",						"uid=mozilla,ou=people,o=netcenter.com");
pref("ldap_2.servers.netcenter.auth.savePassword",				true);
pref("ldap_2.servers.netcenter.auth.password",					"BPqvLvWNew==");
pref("ldap_2.servers.netcenter.customDisplayUrl",				"http://dirsearch.netscape.com/cgi-bin/member_lookup.cgi?dn=%s");
pref("ldap_2.servers.netcenter.replication.never",				true);

pref("ldap_2.servers.infospace.position",						4);
pref("ldap_2.servers.infospace.description",					"InfoSpace Directory");
pref("ldap_2.servers.infospace.serverName",						"ldap.infospace.com");
pref("ldap_2.servers.infospace.searchBase",						"c=US");
pref("ldap_2.servers.infospace.vlvDisabled",					true);
pref("ldap_2.servers.infospace.autoComplete.never",				true);
pref("ldap_2.servers.infospace.replication.never",				true);

pref("ldap_2.servers.verisign.position",						5);
pref("ldap_2.servers.verisign.description",						"Verisign Directory");
pref("ldap_2.servers.verisign.serverName",						"directory.verisign.com");
pref("ldap_2.servers.verisign.vlvDisabled",						true);
pref("ldap_2.servers.verisign.autoComplete.never",				true);
pref("ldap_2.servers.verisign.replication.never",				true);

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
pref("ldap_2.version",											1); /* Update kCurrentListVersion in include/dirprefs.h if you change this */

pref("mailnews.start_page.url", "http://messenger.netscape.com/bookmark/4_5/messengerstart.html");
pref("mailnews.start_page.enabled", true);

/* default prefs for Mozilla 5.0 */
pref("mail.identity.default.compose_html", true);

pref("mail.update_compose_title_as_you_type", true);

pref("mail.collect_email_address", true);
