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

pref("ui.key.menuAccessKeyFocuses", true);
pref("browser.display.screen_resolution", 0); // System setting

/* Fonts only needs lists if we have a default that might not be available. */
/* Tms Rmn, Helv and Courier are ALWAYS available on OS/2 */

pref("font.name.serif.ar", "Tms Rmn");
pref("font.name.sans-serif.ar", "Helv");
pref("font.name.monospace.ar", "Courier");

pref("font.name.serif.el", "Tms Rmn");
pref("font.name.sans-serif.el", "Helv");
pref("font.name.monospace.el", "Courier");

pref("font.name.serif.he", "Tms Rmn");
pref("font.name.sans-serif.he", "Helv");
pref("font.name.monospace.he", "Courier");

pref("font.name.serif.ja", "Times New Roman WT J");
pref("font.name-list.serif.ja", "Times New Roman WT J, Times New Roman MT 30, Tms Rmn");
pref("font.name.sans-serif.ja", "Helv");
pref("font.name.monospace.ja", "Courier");

pref("font.name.serif.ko", "Times New Roman WT K");
pref("font.name-list.serif.ko", "Times New Roman WT K, Times New Roman MT 30, Tms Rmn");
pref("font.name.sans-serif.ko", "Helv");
pref("font.name.monospace.ko", "Courier");

pref("font.name.serif.th", "Tms Rmn");
pref("font.name.sans-serif.th", "Helv");
pref("font.name.monospace.th", "Courier");

pref("font.name.serif.tr", "Tms Rmn");
pref("font.name.sans-serif.tr", "Helv");
pref("font.name.monospace.tr", "Courier");

pref("font.name.serif.x-baltic", "Tms Rmn");
pref("font.name.sans-serif.x-baltic", "Helv");
pref("font.name.monospace.x-baltic", "Courier");

pref("font.name.serif.x-central-euro", "Tms Rmn");
pref("font.name.sans-serif.x-central-euro", "Tms Rmn");
pref("font.name.monospace.x-central-euro", "Courier");

pref("font.name.serif.x-cyrillic", "Tms Rmn");
pref("font.name.sans-serif.x-cyrillic", "Tms Rmn");
pref("font.name.monospace.x-cyrillic", "Courier");

/* The unicode fonts must ALWAYS have a list with one valid font */
pref("font.name.serif.x-unicode", "Times New Roman MT 30");
pref("font.name-list.serif.x-unicode", "Times New Roman MT 30, Tms Rmn");
pref("font.name.sans-serif.x-unicode", "Times New Roman MT 30");
pref("font.name-list.sans-serif.x-unicode", "Times New Roman MT 30, Helv");
pref("font.name.monospace.x-unicode", "Times New Roman MT 30");
pref("font.name-list.monospace.x-unicode", "Times New Roman MT 30, Courier");
pref("font.name.fantasy.x-unicode", "Times New Roman MT 30");
pref("font.name-list.fantasy.x-unicode", "Times New Roman MT 30, Helv");
pref("font.name.cursive.x-unicode", "Times New Roman MT 30");
pref("font.name-list.cursive.x-unicode", "Times New Roman MT 30, Helv");

pref("font.name.serif.x-western", "Tms Rmn");
pref("font.name.sans-serif.x-western", "Helv");
pref("font.name.monospace.x-western", "Courier");

pref("font.name.serif.zh-CN", "Times New Roman WT SC");
pref("font.name-list.serif.zh_CN", "Times New Roman WT SC, Times New Roman MT 30, Tms Rmn");
pref("font.name.sans-serif.zh-CN", "Helv");
pref("font.name.monospace.zh-CN", "Courier");

pref("font.name.serif.zh-TW", "Times New Roman WT TC");
pref("font.name-list.serif.zh-TW", "Times New Roman WT TC, Times New Roman MT 30, Tms Rmn");
pref("font.name.sans-serif.zh-TW", "Helv");
pref("font.name.monospace.zh-TW", "Courier");

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

pref("font.size.nav4rounding", false);

pref("netinst.profile.show_profile_wizard", true); 

pref("middlemouse.paste", true);

// turn off scrollbar snapping
pref("slider.snapMultiplier", 0);

//The following pref is internal to Communicator. Please
//do *not* place it in the docs...
pref("netinst.profile.show_dir_overwrite_msg",  true); 

// override double-click word selection behavior.
pref("layout.word_select.eat_space_to_next_word", true);
pref("layout.word_select.stop_at_punctuation", false);

// If false, will always use closest matching size for a given
// image font.  If true, will substitute a vector font for a given
// image font if the given size is smaller/larger than can be handled
// by the image font.
pref("font.substitute_vector_fonts", true);

// print_extra_margin enables platforms to specify an extra gap or margin
// around the content of the page for Print Preview only
pref("print.print_extra_margin", 90); // twips (90 twips is an eigth of an inch)

pref("applications.telnet", "telnetpm.exe");
pref("applications.telnet.host", "%host%");
pref("applications.telnet.port", "-p %port%");

pref("mail.compose.max_recycled_windows", 0);

// Use IBM943 compatible map for JIS X 0208
pref("intl.jis0208.map", "IBM943");

