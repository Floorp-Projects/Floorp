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

// The other platforms roll this all into "toolbar mode".
pref("browser.chrome.toolbar_tips", true);
pref("browser.chrome.show_menubar", true);

pref("mail.empty_trash", false);

// Handled differently under Mac/Windows
pref("network.hosts.smtp_server", "localhost");
pref("network.hosts.pop_server", "pop");
pref("mail.check_new_mail", true);
pref("mail.sort_by", 0);
pref("news.sort_by", 0);
pref("browser.startup.license_accepted", "");
pref("browser.cache.memory_cache_size", 3000);
pref("browser.cache.disk_cache_size", 5000);
pref("browser.ncols", 0);
pref("browser.installcmap", false);
pref("mail.signature_file", "~/.signature");
pref("mail.default_fcc", "~/nsmail/Sent");
pref("news.default_fcc", "~/nsmail/Sent");
pref("mailnews.reply_on_top", 0);
pref("mailnews.reply_with_extra_lines", 0);
pref("browser.startup.default_window", 0);
pref("security.warn_accept_cookie", false);
pref("editor.disable_spell_checker", false);
pref("editor.dont_lock_spell_files", true);

// Instead of "delay_images"
pref("browser.autoload_images", true);

// Unix only
pref("mail.use_movemail", true);
pref("mail.use_builtin_movemail", true);
pref("mail.movemail_program", "");
pref("mail.movemail_warn", false);
pref("mail.sash_geometry", "");
pref("news.cache_xover", false);
pref("news.show_first_unread", false);
pref("news.sash_geometry", "");
pref("helpers.global_mime_types_file", "/usr/local/lib/netscape/mime.types");
pref("helpers.global_mailcap_file", "/usr/local/lib/netscape/mailcap");
pref("helpers.private_mime_types_file", "~/.mime.types");
pref("helpers.private_mailcap_file", "~/.mailcap");
pref("applications.telnet", "xterm -e telnet %h %p");
pref("applications.tn3270", "xterm -e tn3270 %h");
pref("applications.rlogin", "xterm -e rlogin %h");
pref("applications.rlogin_with_user", "xterm -e rlogin %h -l %u");
pref("applications.tmp_dir", "/tmp");
// On Solaris/IRIX, this should be "lp"
pref("print.print_command", "lpr");
pref("print.print_reversed", false);
pref("print.print_color", true);
pref("print.print_landscape", false);
pref("print.print_paper_size", 0);

// Not sure what this one does...
pref("browser.fancy_ftp", true);

// Fortezza stuff
pref("fortezza.toggle", 1);
pref("fortezza.timeout", 30);

pref("intl.font_charset", "");
pref("intl.font_spec_list", "");
pref("mail.signature_date", 0);

// Outliner column defaults
pref("addressbook.searchpane.outliner_geometry", "");
pref("addressbook.addresseepane.outliner_geometry", "");
pref("addressbook.listpane.outliner_geometry", "");
pref("addressbook.outliner_geometry", "");
pref("mail.composition.addresspane.outliner_geometry", "");
pref("bookmarks.outliner_geometry", "");
pref("mail.categorypane.outliner_geometry", "");
pref("mail.folderpane.outliner_geometry", "");
pref("history.outliner_geometry", "");
pref("mail.ldapsearchpane.outliner_geometry", "");
pref("mail.searchpane.outliner_geometry", "");
pref("mail.mailfilter.outliner_geometry", "");
pref("preferences.lang.outliner_geometry", "");
pref("preferences.dir.outliner_geometry", "");
pref("preferences.font.outliner_geometry", "");
pref("mail.subscribepane.all_groups.outliner_geometry", "");
pref("mail.subscribepane.new_groups.outliner_geometry", "");
pref("mail.subscribepane.search_groups.outliner_geometry", "");
pref("mail.threadpane.outliner_geometry", "");
pref("mail.threadpane.messagepane_height", 400);

// Customizable toolbar stuff
pref("custtoolbar.personal_toolbar_folder", "Personal Toolbar Folder");
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

pref("taskbar.x", -1);
pref("taskbar.y", -1);
pref("taskbar.floating", false);
pref("taskbar.horizontal", false);
pref("taskbar.ontop", false);
pref("taskbar.button_style", -1);

config("menu.help.item_1.url", "http://home.netscape.com/eng/mozilla/5.0/relnotes/unix-5.0.html");
