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

platform.beos = true;

// The other platforms roll this all into "toolbar mode".
pref("browser.chrome.toolbar_tips", true);
pref("browser.chrome.show_menubar", true);

// Instead of "delay_images"
pref("browser.autoload_images", true);

// Not sure what this one does...
pref("browser.fancy_ftp", true);

// Fortezza stuff
pref("fortezza.toggle", 1);
pref("fortezza.timeout", 30);

pref("intl.font_charset", "");
pref("intl.font_spec_list", "");
pref("mail.signature_date", 0);

// Outliner column defaults
pref("mail.threadpane.messagepane_height", 400);

pref("taskbar.x", -1);
pref("taskbar.y", -1);
pref("taskbar.floating", false);
pref("taskbar.horizontal", false);
pref("taskbar.ontop", false);
pref("taskbar.button_style", -1);

config("menu.help.item_1.url", "http://home.netscape.com/eng/mozilla/5.0/");
