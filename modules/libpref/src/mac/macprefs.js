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

// Mac specific preference defaults
pref("browser.drag_out_of_frame_style", 1);
pref("ui.key.saveLink.shift", false); // true = shift, false = meta

pref("editor.use_html_editor",              false);
pref("editor.use_image_editor",             false);

// should a GURL event open a new window or re-use (4.x compat)
pref("browser.always_reuse_window", false);

pref("mail.notification.sound",             "");
pref("mail.close_message_window.on_delete", true);
pref("mail.close_message_window.on_file", true);

pref("mail.server_type_on_restart",         -1);

// default font name (in UTF8)

pref("font.name.serif.ar", "ثلث‮ ‬أبيض");
pref("font.name.sans-serif.ar", "البيان");
pref("font.name.monospace.ar", "جيزة");
pref("font.name.cursive.ar", "XXX.cursive");
pref("font.name.fantasy.ar", "XXX.fantasy");

pref("font.name.serif.el", "Times GR");
pref("font.name.sans-serif.el", "Helvetica GR");
pref("font.name.monospace.el", "Courier GR");
pref("font.name.cursive.el", "XXX.cursive");
pref("font.name.fantasy.el", "XXX.fantasy");

pref("font.name.serif.he", "פנינים‮ ‬חדש");
pref("font.name.sans-serif.he", "אריאל");
pref("font.name.monospace.he", "אריאל");
pref("font.name.cursive.he", "XXX.cursive");
pref("font.name.fantasy.he", "XXX.fantasy");

pref("font.name.serif.ja", "平成明朝"); 
pref("font.name.sans-serif.ja", "平成角ゴシック"); 
pref("font.name.monospace.ja", "Osaka−等幅"); 
pref("font.name.cursive.ja", "XXX.cursive");
pref("font.name.fantasy.ja", "XXX.fantasy");

pref("font.name.serif.ko", "AppleMyungjo"); 
pref("font.name.sans-serif.ko", "AppleGothic"); 
pref("font.name.monospace.ko", "AppleGothic"); 
pref("font.name.cursive.ko", "XXX.cursive");
pref("font.name.fantasy.ko", "XXX.fantasy");

pref("font.name.serif.th", "Thonburi");
pref("font.name.sans-serif.th", "Krungthep");
pref("font.name.monospace.th", "Ayuthaya");
pref("font.name.cursive.th", "XXX.cursive");
pref("font.name.fantasy.th", "XXX.fantasy");

pref("font.name.serif.tr", "Times");
pref("font.name.sans-serif.tr", "Arial");
pref("font.name.monospace.tr", "CourierR");
pref("font.name.cursive.tr", "XXX.cursive");
pref("font.name.fantasy.tr", "XXX.fantasy");

pref("font.name.serif.x-baltic", "Times CE");
pref("font.name.sans-serif.x-baltic", "Helvetica CE");
pref("font.name.monospace.x-baltic", "Courier CE");
pref("font.name.cursive.x-baltic", "XXX.cursive");
pref("font.name.fantasy.x-baltic", "XXX.fantasy");

pref("font.name.serif.x-central-euro", "Times CE");
pref("font.name.sans-serif.x-central-euro", "Helvetica CE");
pref("font.name.monospace.x-central-euro", "Courier CE");
pref("font.name.cursive.x-central-euro", "XXX.cursive");
pref("font.name.fantasy.x-central-euro", "XXX.fantasy");

pref("font.name.serif.x-cyrillic", "Latinski");
pref("font.name.sans-serif.x-cyrillic", "Pryamoy Prop");
pref("font.name.monospace.x-cyrillic", "APC Courier");
pref("font.name.cursive.x-cyrillic", "XXX.cursive");
pref("font.name.fantasy.x-cyrillic", "XXX.fantasy");

pref("font.name.serif.x-unicode", "Times");
pref("font.name.sans-serif.x-unicode", "Helvetica");
pref("font.name.monospace.x-unicode", "Courier");
pref("font.name.cursive.x-unicode", "Apple Chancery");
pref("font.name.fantasy.x-unicode", "Gadget");

pref("font.name.serif.x-western", "Times");
pref("font.name.sans-serif.x-western", "Helvetica");
pref("font.name.monospace.x-western", "Courier");
pref("font.name.cursive.x-western", "Apple Chancery");
pref("font.name.fantasy.x-western", "Gadget");

pref("font.name.serif.zh-CN", "Song");
pref("font.name.sans-serif.zh-CN", "Hei");
pref("font.name.monospace.zh-CN", "Hei");
pref("font.name.cursive.zh-CN", "XXX.cursive");
pref("font.name.fantasy.zh-CN", "XXX.fantasy");

pref("font.name.serif.zh-TW", "Apple LiSung Light"); 
pref("font.name.sans-serif.zh-TW", "Apple LiGothic Medium");  
pref("font.name.monospace.zh-TW", "Apple LiGothic Medium");  
pref("font.name.cursive.zh-TW", "XXX.cursive");
pref("font.name.fantasy.zh-TW", "XXX.fantasy");

pref("font.default", "serif");
pref("font.size.variable.ar", 16);
pref("font.size.fixed.ar", 13);

pref("font.size.variable.el", 16);
pref("font.size.fixed.el", 13);

pref("font.size.variable.he", 16);
pref("font.size.fixed.he", 13);

pref("font.size.variable.ja", 14);
pref("font.size.fixed.ja", 14);

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

pref("font.size.variable.zh-CN", 15);
pref("font.size.fixed.zh-CN", 16);

pref("font.size.variable.zh-TW", 15);
pref("font.size.fixed.zh-TW", 16);

// Tab focus model bit field:
// 1 focuses text controls, 2 focuses other form elements, 4 adds links.
pref("accessibility.tabfocus", 1);

pref("accessibility.typeaheadfind.soundURL", "beep");

// Override the Windows settings: no menu key, meta accelerator key. ctrl for general access key in HTML/XUL
// Use 17 for Ctrl, 18 for Option, 224 for Cmd, 0 for none
pref("ui.key.menuAccessKey", 0);
pref("ui.key.accelKey", 224);
// (pinkerton, joki, saari) IE5 for mac uses Control for access keys. The HTML4 spec
// suggests to use command on mac, but this really sucks (imagine someone having a "q"
// as an access key and not letting you quit the app!). As a result, we've made a 
// command decision 1 day before tree lockdown to change it to the control key.
pref("ui.key.generalAccessKey", 17);

// print_extra_margin enables platforms to specify an extra gap or margin
// around the content of the page for Print Preview only
pref("print.print_extra_margin", 90); // twips (90 twips is an eigth of an inch)

// This indicates whether it should use the native dialog or the XP Dialog
pref("print.use_native_print_dialog", true);

// determines the behavior upon starting a download.
//  0 - open the download manager
//  1 - open a progress dialog
//  2 - do nothing

pref("browser.downloadmanager.behavior", 1);
