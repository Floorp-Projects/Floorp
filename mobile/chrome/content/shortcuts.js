// -*- Mode: js2; tab-width: 4; indent-tabs-mode: nil; js2-basic-offset: 4; js2-skip-preprocessor-directives: t; -*-
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
 * The Original Code is Mozilla Mobile Browser.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Brooks <db48x@yahoo.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

// TODO: need to make the listbox editable, and to save the changes to prefs
// TODO: read the prefs when the window is opened, and make the required changes

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

function ShortcutEditor()
{
    var prefs = Components.classes["@mozilla.org/preferences-service;1"]
                          .getService(Components.interfaces.nsIPrefBranch2);

    // first, we need to be able to manipulate the keys and commands themselves
    function getCommandNames()
    {
        return Array.map(document.getElementsByTagNameNS(XUL_NS, "command"), function(c) { return c.getAttribute("id"); });
    }

    function findKeyForCommand(command)
    {
        // looks for a key the calls the named command, or null if there aren't any
        // TODO: cache the list of keys, invalidating the cache when we change it
        // TODO: the chache should be a hash of command names to key elements,
        //       other wise fillShortcutList will be O(n^2) on the number of
        //       commands.
        var keys = document.getElementsByTagNameNS(XUL_NS, "key");
        var l = keys.length;
        for (var i = 0; i < l; i++)
            if (keys[i].getAttribute("command") == command)
                return keys[i];
    }

    function findCommandForKey(modifiers, key, keycode)
    {
        // TODO: This is a bit simplistic as yet. For example, we should match
        //       a key with an optional modifier even if that modifier isn't
        //       specified in our arguments. Also, we need to differentiate
        //       between a key element with an attribute that is an empty string
        //       and one without that attribute at all.
        var keys = document.getElementsByTagNameNS(XUL_NS, "key");
        var l = keys.length;
        for (var i = 0; i < l; i++)
            if (keys[i].getAttribute("modifiers") == modifiers &&
                keys[i].getAttribute("key") == key &&
                keys[i].getAttribute("keycode") == keycode)
                return keys[i];
    }

    function addKey(command, modifiers, key, keycode)
    {
        // generally adds a new key to the document that runs command,
        // but if a key for command already exists it instead modifies
        // that key. If a key already exists that matches the
        // arguments, no modifications are made and null is
        // returned. Otherwise, the new key is returned.

        if (findCommandForKey(modifiers, key, keycode))
            return null;

        var k;
        if ((k = findKeyForCommand(command)))
        {
            k.modifiers = modifiers;
            k.key = key;
            k.keycode = keycode;
        }
        else
        {
            k = document.createElementNS(XUL_NS, "key");
            k.modifiers = modifiers;
            k.key = key;
            k.keycode = keycode;
            k.command = command;
            document.getElementById("mainKeyset").appendChild(k);
        }

        return k;
    }

    // This code is all about converting key elements into human-readable
    // descriptions of the keys they match. Copied essentially verbatim from
    // nsMenuFrame::BuildAcceleratorText
    // TODO: write some tests

    // first, we need to look up the right names of the various modifier keys.
    var platformBundle = document.getElementById("bundle-platformKeys");
    var platformKeys = {
        shift: platformBundle.getString("VK_SHIFT"),
        meta: platformBundle.getString("VK_META"),
        alt: platformBundle.getString("VK_ALT"),
        control: platformBundle.getString("VK_CONTROL")
    };
    var modifierSeparator = platformBundle.getString("MODIFIER_SEPARATOR");

#ifdef XP_MACOSX
    var accelKey = Components.interfaces.nsIDOMKeyEvent.DOM_VK_META;
#else
    var accelKey = Components.interfaces.nsIDOMKeyEvent.DOM_VK_CONTROL;
#endif

    try {
        accelKey = prefs.getCharPref("ui.key.accelKey");
    } catch (e) { }

    // convert from the accel keycode to the right string
    var platformAccel = { };
    platformAccel[Components.interfaces.nsIDOMKeyEvent.DOM_VK_META] = platformKeys.meta;
    platformAccel[Components.interfaces.nsIDOMKeyEvent.DOM_VK_ALT] = platformKeys.alt;
    platformAccel[Components.interfaces.nsIDOMKeyEvent.DOM_VK_CONTROL] = platformKeys.control;
    if (accelKey in platformAccel)
        platformKeys.accel = platformAccel[accelKey];
    else
        platformKeys.accel = platformKeys.control;

    function getKeyName(key) {
        // convert a key element into a string describing what keys to push.
        // "Control-C" or "Control-Meta-Hyper-Shift-Q" or whatever
        if (!key)
            return "";

        var accel = [];
        var keybundle = document.getElementById("bundle-keys");
        var keyName = key.getAttribute("keytext") || key.getAttribute("key") || keybundle.getString(key.getAttribute("keycode"));
        var modifiers = key.getAttribute("modifiers").split(" ");
        for each (m in modifiers)
            if (m in platformKeys)
                accel.push(platformKeys[m]);
        accel.push(keyName);

        return accel.join(modifierSeparator);
    }

    // show the window
    this.edit = function()
    {
        var nodes = document.getElementById("ui-stack").childNodes;
        Array.forEach(nodes, function(n) { if (n.getAttribute("id") != "browser-container") { n.hidden = true; }});
        document.getElementById("shortcuts-container").hidden = false;
        fillShortcutList();
    };

    this.dismiss = function()
    {
        document.getElementById("shortcuts-container").hidden = true;
    };

    // also, updating the UI is helpful
    function fillShortcutList()
    {
        var listbox = document.getElementById("shortcuts");
        var commands = getCommandNames();
        var sb = document.getElementById("bundle-shortcuts");

        function doAppend(name, key)
        {
            // TODO: alter the listbox xbl binding so that if appendItem is
            //       given more than 2 arguments, it interprets the additional
            //       arguments as labels for additional cells.
            var cell1 = document.createElementNS(XUL_NS, "listcell");
            cell1.setAttribute("label", name);
            var cell2 = document.createElementNS(XUL_NS, "listcell");
            cell2.setAttribute("label", key);
            var item = document.createElementNS(XUL_NS, "listitem");
            item.appendChild(cell1);
            item.appendChild(cell2);
            listbox.appendChild(item);
        }

        function doGetString(name)
        {
            try
            {
                return sb.getString(name);
            }
            catch (e) { }
        }

        function clear()
        {
            // TODO: correct this oversight in the listbox binding
            var c;
            while ((c = listbox.getRowCount()))
                listbox.removeItemAt(c - 1);
        }

        clear();
        commands.forEach(function(c) { doAppend(doGetString(c +".name") || c, getKeyName(findKeyForCommand(c)) || "—"); });
    }
}

var Shortcuts = new ShortcutEditor();
