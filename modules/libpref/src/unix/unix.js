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

pref("mail.empty_trash", false);

// Handled differently under Mac/Windows
pref("network.hosts.smtp_server", "localhost");
pref("network.hosts.pop_server", "pop");
pref("mail.check_new_mail", true);
pref("browser.display.screen_resolution", 0); // System setting
pref("browser.startup.license_accepted", "");
pref("browser.cache.memory.capacity", 4096);
pref("browser.cache.disk.capacity", 50000);
pref("browser.ncols", 0);
pref("browser.installcmap", false);
pref("browser.drag_out_of_frame_style", 1);
pref("mail.signature_file", "~/.signature");
pref("mail.default_fcc", "~/nsmail/Sent");
pref("news.default_fcc", "~/nsmail/Sent");
pref("mailnews.reply_with_extra_lines", 0);
pref("security.warn_accept_cookie", false);
pref("editor.disable_spell_checker", false);
pref("editor.dont_lock_spell_files", true);
pref("editor.singleLine.pasteNewlines", 0);

// Middle-mouse handling
pref("middlemouse.paste", true);
pref("middlemouse.contentLoadURL", true);
pref("middlemouse.openNewWindow", true);
pref("middlemouse.scrollbarPosition", true);

// Clipboard behavior
pref("clipboard.autocopy", true);

pref("browser.urlbar.clickSelectsAll", false);

// override double-click word selection behavior.
pref("layout.word_select.stop_at_punctuation", false);

// autocomplete keyboard grab workaround
pref("autocomplete.grab_during_popup", true);
pref("autocomplete.ungrab_during_mode_switch", true);

// Most Unix people think modal pref windows are stupid:
pref("browser.prefWindowModal", false);

// turn off scrollbar snapping
pref("slider.snapMultiplier", 0);

// Unix only
pref("mail.use_movemail", true);
pref("mail.use_builtin_movemail", true);
pref("mail.movemail_program", "");
pref("mail.movemail_warn", false);
pref("mail.sash_geometry", "");
pref("news.cache_xover", false);
pref("news.show_first_unread", false);
pref("news.sash_geometry", "");
pref("helpers.global_mime_types_file", "/etc/mime.types");
pref("helpers.global_mailcap_file", "/etc/mailcap");
pref("helpers.private_mime_types_file", "~/.mime.types");
pref("helpers.private_mailcap_file", "~/.mailcap");
pref("applications.telnet", "xterm -e telnet %h %p");
pref("applications.tn3270", "xterm -e tn3270 %h");
pref("applications.rlogin", "xterm -e rlogin %h");
pref("applications.rlogin_with_user", "xterm -e rlogin %h -l %u");
pref("applications.tmp_dir", "/tmp");
// Choose print module. Default is "auto" which selects Xprint module
// if the XPSERVERLIST env var is set
pref("print.print_method", 0); // 0=auto, 1=Xprint, 2=PostScript, 3=reserved
// On Solaris/IRIX, this should be "lp"
pref("print.print_command", "lpr");
pref("print.print_reversed", false);
pref("print.print_color", true);
pref("print.print_landscape", false);
pref("print.print_paper_size", 0);

pref("font.allow_double_byte_special_chars", true);
// font names

// ar

pref("font.name.serif.el", "misc-fixed-iso8859-7");
pref("font.name.sans-serif.el", "misc-fixed-iso8859-7");
pref("font.name.monospace.el", "misc-fixed-iso8859-7");

pref("font.name.serif.he", "misc-fixed-iso8859-8");
pref("font.name.sans-serif.he", "misc-fixed-iso8859-8");
pref("font.name.monospace.he", "misc-fixed-iso8859-8");

pref("font.name.serif.ja", "jis-fixed-jisx0208.1983-0");
pref("font.name.sans-serif.ja", "jis-fixed-jisx0208.1983-0");
pref("font.name.monospace.ja", "jis-fixed-jisx0208.1983-0");

pref("font.name.serif.ko", "daewoo-mincho-ksc5601.1987-0");
pref("font.name.sans-serif.ko", "daewoo-mincho-ksc5601.1987-0");
pref("font.name.monospace.ko", "daewoo-mincho-ksc5601.1987-0");

// th

pref("font.name.serif.tr", "adobe-times-iso8859-9");
pref("font.name.sans-serif.tr", "adobe-helvetica-iso8859-9");
pref("font.name.monospace.tr", "adobe-courier-iso8859-9");

pref("font.name.serif.x-baltic", "b&h-lucidux serif-iso8859-4");
pref("font.name.sans-serif.x-baltic", "b&h-lucidux sans-iso8859-4");
pref("font.name.monospace.x-baltic", "b&h-lucidux mono-iso8859-4");

pref("font.name.serif.x-central-euro", "adobe-times-iso8859-2");
pref("font.name.sans-serif.x-central-euro", "adobe-helvetica-iso8859-2");
pref("font.name.monospace.x-central-euro", "adobe-courier-iso8859-2");

pref("font.name.serif.x-cyrillic", "cronyx-times-koi8-r");
pref("font.name.sans-serif.x-cyrillic", "cronyx-helvetica-koi8-r");
pref("font.name.monospace.x-cyrillic", "cronyx-courier-koi8-r");

pref("font.name.serif.x-unicode", "adobe-times-iso8859-1");
pref("font.name.sans-serif.x-unicode", "adobe-helvetica-iso8859-1");
pref("font.name.monospace.x-unicode", "adobe-courier-iso8859-1");

pref("font.name.serif.x-user-def", "adobe-times-iso8859-1");
pref("font.name.sans-serif.x-user-def", "adobe-helvetica-iso8859-1");
pref("font.name.monospace.x-user-def", "adobe-courier-iso8859-1");

pref("font.name.serif.x-western", "adobe-times-iso8859-1");
pref("font.name.sans-serif.x-western", "adobe-helvetica-iso8859-1");
pref("font.name.monospace.x-western", "adobe-courier-iso8859-1");

pref("font.name.serif.zh-CN", "isas-song ti-gb2312.1980-0");
pref("font.name.sans-serif.zh-CN", "isas-song ti-gb2312.1980-0");
pref("font.name.monospace.zh-CN", "isas-song ti-gb2312.1980-0");

// zh-TW

pref("font.default", "serif");
pref("font.size.variable.ar", 16);
pref("font.size.fixed.ar", 12);

pref("font.size.variable.el", 16);
pref("font.size.fixed.el", 12);

pref("font.size.variable.he", 16);
pref("font.size.fixed.he", 12);

pref("font.size.variable.ja", 16);
pref("font.size.fixed.ja", 16);

pref("font.size.variable.ko", 16);
pref("font.size.fixed.ko", 16);

pref("font.size.variable.th", 16);
pref("font.size.fixed.th", 12);

pref("font.size.variable.tr", 16);
pref("font.size.fixed.tr", 12);

pref("font.size.variable.x-baltic", 16);
pref("font.size.fixed.x-baltic", 12);

pref("font.size.variable.x-central-euro", 16);
pref("font.size.fixed.x-central-euro", 12);

pref("font.size.variable.x-cyrillic", 16);
pref("font.size.fixed.x-cyrillic", 12);

pref("font.size.variable.x-unicode", 16);
pref("font.size.fixed.x-unicode", 12);

pref("font.size.variable.x-western", 16);
pref("font.size.fixed.x-western", 12);

pref("font.size.variable.zh-CN", 16);
pref("font.size.fixed.zh-CN", 16);

pref("font.size.variable.zh-TW", 16);
pref("font.size.fixed.zh-TW", 16);

// below a certian pixel size scaled fonts produce poor results
pref("font.scale.outline.min",      6);
pref("font.scale.bitmap.min",       12);
pref("font.scale.bitmap.undersize", 80);
pref("font.scale.bitmap.oversize",  120);

pref("font.scale.outline.min.ja",      10);
pref("font.scale.bitmap.min.ja",       16);
pref("font.scale.bitmap.undersize.ja", 80);
pref("font.scale.bitmap.oversize.ja",  120);

pref("font.scale.outline.min.ko",      10);
pref("font.scale.bitmap.min.ko",       16);
pref("font.scale.bitmap.undersize.ko", 80);
pref("font.scale.bitmap.oversize.ko",  120);

pref("font.scale.outline.min.zh-CN",      10);
pref("font.scale.bitmap.min.zh-CN",       16);
pref("font.scale.bitmap.undersize.zh-CN", 80);
pref("font.scale.bitmap.oversize.zh-CN",  120);

pref("font.scale.outline.min.zh-TW",      10);
pref("font.scale.bitmap.min.zh-TW",       16);
pref("font.scale.bitmap.undersize.zh-TW", 80);
pref("font.scale.bitmap.oversize.zh-TW",  120);

// minimum font sizes

pref("font.min-size.variable.ja", 16);
pref("font.min-size.fixed.ja", 16);

pref("font.min-size.variable.ko", 10);
pref("font.min-size.fixed.ko", 10);

pref("font.min-size.variable.zh-CN", 10);
pref("font.min-size.fixed.zh-CN", 10);

pref("font.min-size.variable.zh-TW", 10);
pref("font.min-size.fixed.zh-TW", 10);

// ps font
// this list is used by the postscript font
// to enumerate the list of langGroups
// there should be a call to get the
// langGroups; see bug 75054
pref("print.psnativefont.ar", "");
pref("print.psnativefont.el", "");
pref("print.psnativefont.he", "");
pref("print.psnativefont.ja", "");
pref("print.psnativefont.ko", "");
pref("print.psnativefont.th", "");
pref("print.psnativefont.tr", "");
pref("print.psnativefont.x-baltic", "");
pref("print.psnativefont.x-central-euro", "");
pref("print.psnativefont.x-cyrillic", "");
pref("print.psnativefont.x-unicode", "");
pref("print.psnativefont.x-user-def", "");
pref("print.psnativefont.x-western", "");
pref("print.psnativefont.zh-CN", "");
pref("print.psnativefont.zh-TW", "");

pref("mail.signature_date", 0);
