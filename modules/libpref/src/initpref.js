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

// Style note 3/3:
// internal objects & functions are in under_score form.
// public functions are in interCaps.

// Remove this -- but called in winpref.js :
// platform.windows = true;
function plat() {
    this.windows=false;
    this.mac=false;
    this.unix=false;
};
platform = new plat();

/* --- Preference initialization functions ---

    Moved to native functions:
    pref        -> pref_NativeDefaultPref
    defaultPref -> "
    userPref    -> pref_NativeUserPref
    lockPref    -> pref_NativeLockPref
    unlockPref  -> pref_NativeUnlockPref
    getPref     -> pref_NativeGetPref
    config      -> pref_NativeDefaultPref   (?)
*/

// stubs for compatability
var default_pref = defaultPref;
var lock_pref = lockPref;
var unlock_pref = unlockPref;
var userPref = user_pref;

// -------------------------

function mime_type(root, mimetype, extension, load_action, appname, appsig, filetype)
{
    // changed for prefbert
    pref(root + ".mimetype", mimetype);
    pref(root + ".extension", extension);
    pref(root + ".load_action", load_action);
    pref(root + ".mac_appname", appname);
    pref(root + ".mac_appsig", appsig);
    pref(root + ".mac_filetype", filetype);
    pref(root + ".description", "");
    pref(root + ".latent_plug_in", false);
}

// LDAP
// Searches for "key=value" in the given string and returns value.
function getLDAPValue(str, key)
{
    if (str == null || key == null)
        return null;

    var search_key = "\n" + key + "=";

    var start_pos = str.indexOf(search_key);
    if (start_pos == -1)
        return null;

    start_pos += search_key.length;

    var end_pos = str.indexOf("\n", start_pos);
    if (end_pos == -1)
        end_pos = str.length;

    return str.substring(start_pos, end_pos);
}

function begin_mime_def()
{
}

function end_mime_def()
{
   var now = new Date();
   pref("mime.types.all_defined", now.toString());
}

