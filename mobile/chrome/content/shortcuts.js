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
// TODO: need ui to clear a shortcut; at present all keys typed while editing a
//       key will be interpreted as an indication of what the user wants the
//       shortcut to be. I think a little X button on the textbox will suffice.
// TODO: see about grouping the keys into categories
// TODO: move the shortcut editor to the prefs, if the prefs exist

Components.utils.import("resource://gre/modules/JSON.jsm");

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

function ShortcutEditor()
{
    var prefsvc = Components.classes["@mozilla.org/preferences-service;1"]
                            .getService(Components.interfaces.nsIPrefService);
    var prefs = Components.classes["@mozilla.org/preferences-service;1"]
                          .getService(Components.interfaces.nsIPrefBranch2);
    var keyPrefs = prefsvc.getBranch("shortcut.");
    var keyCache;

    // first, we need to be able to manipulate the keys and commands themselves
    function getCommandNames()
    {
        return Array.map(document.getElementsByTagNameNS(XUL_NS, "command"), function(c) { return c.getAttribute("id"); });
    }

    function getKeys()
    {
        if (keyCache)
            return keyCache;

        keyCache = { };
        Array.map(document.getElementsByTagNameNS(XUL_NS, "key"), function(k) { keyCache[k.getAttribute("command")] = k; });
        return keyCache;
    }

    function findKeyForCommand(command)
    {
        // returns the key the calls the named command, or null if there isn't one
        return getKeys()[command];
    }

    function findCommandForKey(keySpec)
    {
        // TODO: This is a bit simplistic as yet. For example, we should match
        //       a key with an optional modifier even if that modifier isn't
        //       specified in our arguments. Also, we need to differentiate
        //       between a key element with an attribute that is an empty string
        //       and one without that attribute at all.
        var keys = document.getElementsByTagNameNS(XUL_NS, "key");
        var l = keys.length;
        for (var i = 0; i < l; i++)
            if (keys[i].getAttribute("modifiers") == keySpec.modifiers &&
                keys[i].getAttribute("key") == keySpec.key &&
                keys[i].getAttribute("keycode") == keySpec.keycode)
                return keys[i];
    }

    function addKey(command, keySpec)
    {
        // generally adds a new key to the document that runs command,
        // but if a key for command already exists it instead modifies
        // that key. If a key already exists that matches the
        // arguments, no modifications are made and null is
        // returned. Otherwise, the new key is returned.

        var key = findKeyForCommand(command);
        if (keySpec.exists)
        {
            if (findCommandForKey(keySpec))
                return null;

            if (key)
            {
                key.setAttribute("modifiers") = keySpec.modifiers;
                key.setAttribute("key") = keySpec.key;
                key.setAttribute("keycode") = keySpec.keycode;
            }
            else
            {
                key = document.createElementNS(XUL_NS, "key");
                key.setAttribute("modifiers") = keySpec.modifiers;
                key.setAttribute("key") = keySpec.key;
                key.setAttribute("keycode") = keySpec.keycode;
                key.setAttribute("command") = command;
                document.getElementById("mainKeyset").appendChild(k);
            }

            return k;
        }

        if (key)
            key.parentNode.removeChild(key);
        return null;
    }

    function makeKeySpec(modifiers, key, keycode)
    {
        // TODO: make this check more specific, once key elements implement a unique interface
        if (modifiers instanceof Components.interfaces.nsIDOMElement)
            return {
                exists: true,
                modifiers: getFlagsForModifiers(modifiers.getAttribute("modifiers")),
                key: modifiers.getAttribute("key"),
                keycode: modifiers.getAttribute("keycode")
            };
        if (modifiers instanceof Components.interfaces.nsIDOMKeyEvent)
            return {
                exists: true,
                modifiers: getEventModifiers(modifiers),
                key: getEventKey(modifiers),
                keycode: getEventKeyCode(modifiers)
            };
        return {
            exists: !!(modifiers || key || keycode),
            modifiers: getFlagsForModifiers(modifiers),
            key: key,
            keycode: keycode
        };
    }

    var modifierFlags = { alt: 1, control: 2, meta: 4, shift: 8 };
    function getFlagsForModifiers(modifiers)
    {
        if (!modifiers)
            return 0;

        var result;
        for each (m in modifiers.split(" "))
            result |= modifierFlags[m];
        return result;
    }

    function getEventModifiers(event)
    {
        var result, i = 1;
        for each (m in [event.altKey, event.ctrlKey, event.metaKey, event.shiftKey])
        {
            result |= (m && i);
            i += i;
        }
        return result;
    }

    function getEventKey(event)
    {
        if (event.charCode)
            return String.fromCharCode(event.charCode).toUpperCase();
    }

    function getEventKeyCode(event)
    {
        var keyCodeMap = { };
        var nsIDOMKeyEvent = Components.interfaces.nsIDOMKeyEvent;
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_CANCEL] = "VK_CANCEL";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_HELP] = "VK_HELP";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_BACK_SPACE] = "VK_BACK";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_TAB] = "VK_TAB";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_CLEAR] = "VK_CLEAR";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_RETURN] = "VK_RETURN";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_ENTER] = "VK_ENTER";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_SHIFT] = "VK_SHIFT";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_CONTROL] = "VK_CONTROL";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_ALT] = "VK_ALT";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_PAUSE] = "VK_PAUSE";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_CAPS_LOCK] = "VK_CAPS_LOCK";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_ESCAPE] = "VK_ESCAPE";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_SPACE] = "VK_SPACE";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_PAGE_UP] = "VK_PAGE_UP";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_PAGE_DOWN] = "VK_PAGE_DOWN";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_END] = "VK_END";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_HOME] = "VK_HOME";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_LEFT] = "VK_LEFT";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_UP] = "VK_UP";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_RIGHT] = "VK_RIGHT";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_DOWN] = "VK_DOWN";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_PRINTSCREEN] = "VK_PRINTSCREEN";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_INSERT] = "VK_INSERT";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_DELETE] = "VK_DELETE";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_SEMICOLON] = "VK_SEMICOLON";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_EQUALS] = "VK_EQUALS";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_CONTEXT_MENU] = "VK_CONTEXT_MENU";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_MULTIPLY] = "VK_MULTIPLY";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_ADD] = "VK_ADD";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_SEPARATOR] = "VK_SEPARATOR";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_SUBTRACT] = "VK_SUBTRACT";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_DECIMAL] = "VK_DECIMAL";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_DIVIDE] = "VK_DIVIDE";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_F1] = "VK_F1";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_F2] = "VK_F2";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_F3] = "VK_F3";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_F4] = "VK_F4";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_F5] = "VK_F5";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_F6] = "VK_F6";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_F7] = "VK_F7";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_F8] = "VK_F8";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_F9] = "VK_F9";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_F10] = "VK_F10";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_F11] = "VK_F11";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_F12] = "VK_F12";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_F13] = "VK_F13";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_F14] = "VK_F14";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_F15] = "VK_F15";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_F16] = "VK_F16";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_F17] = "VK_F17";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_F18] = "VK_F18";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_F19] = "VK_F19";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_F20] = "VK_F20";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_F21] = "VK_F21";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_F22] = "VK_F22";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_F23] = "VK_F23";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_F24] = "VK_F24";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_NUM_LOCK] = "VK_NUM_LOCK";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_SCROLL_LOCK] = "VK_SCROLL_LOCK";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_COMMA] = "VK_COMMA";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_PERIOD] = "VK_PERIOD";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_SLASH] = "VK_SLASH";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_BACK_QUOTE] = "VK_BACK_QUOTE";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_OPEN_BRACKET] = "VK_OPEN_BRACKET";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_BACK_SLASH] = "VK_BACK_SLASH";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_CLOSE_BRACKET] = "VK_CLOSE_BRACKET";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_QUOTE] = "VK_QUOTE";
        keyCodeMap[nsIDOMKeyEvent.DOM_VK_META] = "VK_META";

        return keyCodeMap[event.keyCode];
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
    platformKeys.accel = platformAccel[accelKey] || platformKeys.control;

    function getKeyName(keySpec) {
        // convert a key element into a string describing what keys to push.
        // "Control-C" or "Control-Meta-Hyper-Shift-Q" or whatever
        if (!keySpec)
            return "";
        if (keySpec instanceof Components.interfaces.nsIDOMElement)
            keySpec = makeKeySpec(keySpec);

        var accel = [];
        var keybundle = document.getElementById("bundle-keys");

        // this is sorta dumb, but whatever
        var modifiers = [], i = 1;
        for each (m in ["alt", "control", "meta", "shift"])
        {
            if (keySpec.modifiers & i)
                modifiers.push(m);
            i += i;
        }

        for each (m in modifiers)
            if (m in platformKeys)
                accel.push(platformKeys[m]);
        accel.push(keySpec.keytext || keySpec.key || keybundle.getString(keySpec.keycode));

        return accel.join(modifierSeparator);
    }

    // this listens to keyup events and converts them into the proper display name for the textbox
    function keyListener(event)
    {
        if (!event instanceof Components.interfaces.nsIDOMKeyEvent)
            return;
        document.getElementById("test").value = getKeyName(makeKeySpec(event));
        event.preventDefault();
    }

    // show the window
    this.edit = function()
    {
        var nodes = document.getElementById("ui-stack").childNodes;
        Array.forEach(nodes, function(n) { if (n.getAttribute("id") != "browser-container") { n.hidden = true; }});
        document.getElementById("shortcuts-container").hidden = false;
        fillShortcutList();

        document.getElementById("test").addEventListener("keypress", keyListener, true);
    };

    function hack()
    {
        // TODO: this is a hack, so I want to remove it. to do so, key elements
        // will have to respond to direct dom manipulation.
        Array.map(document.getElementsByTagNameNS(XUL_NS, "keyset"),
                  function(e) { document.removeChild(e); return document.cloneNode(e, true); })
             .forEach(function(e) { document.appendChild(e); });
    }

    this.dismiss = function()
    {
	hack();
        document.getElementById("test").removeEventListener("keypress", keyListener, true);
        document.getElementById("shortcuts-container").hidden = true;
    };

    // also, updating the UI is helpful
    function fillShortcutList()
    {
        var commands = getCommandNames();
        var sb = document.getElementById("shortcut-bundles").childNodes;

        function doAppend(command)
        {
            // TODO: alter the listbox xbl binding so that if appendItem is
            //       given more than 2 arguments, it interprets the additional
            //       arguments as labels for additional cells.
            var key = findKeyForCommand(command)
            var cell1 = document.createElementNS(XUL_NS, "treecell");
            cell1.setAttribute("label", doGetString(command +".name") || command);
            cell1.setAttribute("value", command);
            var cell2 = document.createElementNS(XUL_NS, "treecell");
            cell2.setAttribute("label", getKeyName(key));
            cell2.setAttribute("value", makeKeySpec(key));
            var row = document.createElementNS(XUL_NS, "treerow");
            row.appendChild(cell1);
            row.appendChild(cell2);
            var item = document.createElementNS(XUL_NS, "treeitem");
            item.appendChild(row);
            children.appendChild(item);
        }

        function doGetString(name)
        {
            var l = sb.length;
            for (var i = 0; i < l; i++)
                try
                {
                    return sb[i].getString(name);
                }
                catch (e) { }
        }

        var tree = document.getElementById("shortcuts");
        var children = document.getElementById("shortcuts-children");
        tree.removeChild(children);
        children = document.createElementNS(XUL_NS, "treechildren");
        children.setAttribute("id", "shortcuts-children");
        tree.appendChild(children);

        commands.forEach(doAppend);
    }

    // saving and restoring a key assignment to the prefs
    function save(command, keySpec)
    {
        keyPrefs.setCharPref(command, JSON.toString(keySpec));
    }

    function load(command)
    {
        try
        {
            return JSON.fromString(keyPrefs.getCharPref(command));
        }
        catch (ex)
        {
            return makeKeySpec();
        }
    }

    // and of course, none of this would be any use unless at some point we made
    // the proper changes to the document based on the user's choices.
    function restore()
    {
        getCommandNames().forEach(function(c) { addKey(cmd, load(cmd)); });
        hack();
    }
}

var Shortcuts = new ShortcutEditor();
