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
 * The Original Code is the Mozilla Penelope project.
 *
 * The Initial Developer of the Original Code is
 * QUALCOMM incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *     Mark Charlebois <mcharleb@qualcomm.com> original author
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

var PenelopeGlobal = { 
    onLoad: function() 
    { 
        // quit if this function has already been called
        if (arguments.callee.done) return;

        // flag this function so we don't do the same thing twice
        arguments.callee.done = true;

        dump("in PenelopeGlobal.onLoad\n");
        updateSelectedTextUrlLabels();

        setKeymap();
    } 
}; 

//load event handler
window.addEventListener("load", function(e) { PenelopeGlobal.onLoad(e); }, false); 

function updateSelectedTextUrlLabels()
{
    // Get the preferences object
    var prefs = Components.classes["@mozilla.org/preferences-service;1"].
                getService(Components.interfaces.nsIPrefBranch);

    // Get the URL prefs
    var menu;
    var url;
    for (var urlnum=1; urlnum<=6; urlnum++)
    {
        menu = document.getElementById('penelopeDoSelectedTextUrl'+urlnum);
        try
        {
            url = prefs.getCharPref("penelope.selectedTextURL"+urlnum);
            name = url.split(",")[0];
            if (name)
            {
                menu.setAttribute('label', name);
                menu.setAttribute('hidden', 'false');
            }
            else
            {
                menu.setAttribute('hidden', 'true');
            }
        }
        catch(e)
        {
            menu.setAttribute('hidden', 'true');
        }
    }
}

function openURL(urlToOpen) 
{
    var uri = Components.classes["@mozilla.org/network/io-service;1"]
              .getService(Components.interfaces.nsIIOService)
              .newURI(urlToOpen, null, null);

    var protocolSvc = Components.classes["@mozilla.org/uriloader/external-protocol-service;1"]
                      .getService(Components.interfaces.nsIExternalProtocolService);
    protocolSvc.loadUrl(uri);
}

function penelopeGetSelectedText()
{
    var selection = "";

    try 
    {
        var focusedWindow = document.commandDispatcher.focusedWindow;
        selection = focusedWindow.getSelection().toString();
    }
    catch (e)
    {
        dump("No text selected\n");
    }

    return selection;
}


function doSelectedTextUrl(number)
{
    // Get the selected text
    // Get the preferences object
    var prefs = Components.classes["@mozilla.org/preferences-service;1"].
                getService(Components.interfaces.nsIPrefBranch);

    var str = "penelope.selectedTextURL"+number;
    try 
    {
        entry = prefs.getCharPref(str);
        if (entry)
        {
            selectedText = penelopeGetSelectedText();
            sep = entry.indexOf(",");
            name = entry.substring(0,sep-1);
            url = entry.substring(sep+1, entry.length);
            url = url.replace("%s", selectedText);
            openURL(url);
        }
    }
    catch(e) 
    { 
        alert(str+" is not set or is invalid");
    }
}

function openDirectoryServices()
{
    // uses "toOpenWindowByType" function provided by utilityOverlay.js
    // which is included by most clients.
    toOpenWindowByType("mail:addressbook", "chrome://messenger/content/addressbook/addressbook.xul");
}

function directoryServicesQuery()
{
    // uses "toOpenWindowByType" function provided by utilityOverlay.js
    // which is included by most clients.
    toOpenWindowByType("mail:addressbook", "chrome://messenger/content/addressbook/addressbook.xul");
}


function penelopeGetPlatform() 
{
    var tmp = navigator.platform.toLowerCase();
    var platform = "windows";

    if (tmp.indexOf("linux") != -1) 
    {
        platform = "linux";
    }
    else if (tmp.indexOf("mac") != -1) 
    {
        platform = "mac";
    }

    return platform;
}

function setKeymap()
{
    dump("Setting keymap...\n");
    var prefs = Components.classes["@mozilla.org/preferences-service;1"].
                getService(Components.interfaces.nsIPrefBranch);

    // If the user wants the penelope keymap, get the name
    var usePenelopeKeyset = prefs.getBoolPref("penelope.useKeymap");
    var selectedKeyset = "thunderbird";
    if (usePenelopeKeyset)
    {
        selectedKeyset = prefs.getCharPref("penelope.keymap").toLowerCase();
    }

    var strbundle;

    if (selectedKeyset == "thunderbird")
    {
        strbundle = document.getElementById("penelopeThunderbirdKeys");
    }
    else
    {
        var platform = penelopeGetPlatform();
        dump("Loading keyset for "+platform+"\n");

        switch(platform)
        {
        case "mac":
            strbundle = document.getElementById("penelopeMacKeys");
            break;
        case "windows":
        case "linux":
        default:
            strbundle = document.getElementById("penelopeWindowsKeys");
            break;
        }
    }

    // The particular window may not have the global overlay applied
    // so the strbundles may not be defined
    if (strbundle)
    {
        loadKeySet(strbundle.strings);
    }
}

function loadKeySet(strings)
{
    while (strings.hasMoreElements())
    {
        var node = strings.getNext().QueryInterface(Components.interfaces.nsIPropertyElement);;
        var key = document.getElementById(node.key);
        if (key)
        {
            setKey(key, node.value);
        }
    }
}

function setKey(key, value)
{
    properties = value.split("][");

    if(properties[0] == "!")
    {
        key.setAttribute("disabled", true);
    }
    else 
    {
        if (properties[0])
        {
            key.setAttribute("modifiers", properties[0]);
        }

        if(properties[1]) 
        {
            key.setAttribute("key", properties[1]);
        }
        else if(properties[2]) 
        {
            key.setAttribute("keycode", properties[2]);
        }
        else
        {
            alert("Error: No key specified for "+key.id+". Disabling entry.");
            key.setAttribute("disabled", true);
        }
    }
}
