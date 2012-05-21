/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function MultiplexHandler(aEvent)
{
    MultiplexHandlerEx(
        aEvent,
        function Browser_SelectDetector(event) {
            BrowserCharsetReload();
            /* window.content.location.reload() will re-download everything */
            SelectDetector(event, null);
        },
        function Browser_SetForcedCharset(charset, isPredefined) {
            BrowserSetForcedCharacterSet(charset);
        }
    );
}

function MailMultiplexHandler(aEvent)
{
    MultiplexHandlerEx(
        aEvent,
        function Mail_SelectDetector(event) {
            SelectDetector(
                event,
                function Mail_Reload() {
                    messenger.setDocumentCharset(msgWindow.mailCharacterSet);
                }
            );
        },
        function Mail_SetForcedCharset(charset, isPredefined) {
            MessengerSetForcedCharacterSet(charset);
        }
    );
}

function ComposerMultiplexHandler(aEvent)
{
    MultiplexHandlerEx(
        aEvent,
        function Composer_SelectDetector(event) {
            SelectDetector(
                event,
                function Composer_Reload() {
                    EditorLoadUrl(GetDocumentUrl());
                }
            );
        },
        function Composer_SetForcedCharset(charset, isPredefined) {
            if ((!isPredefined) && charset.length > 0) {
                gCharsetMenu.SetCurrentComposerCharset(charset);
            }
            EditorSetDocumentCharacterSet(charset);
        }
    );
}

function MultiplexHandlerEx(aEvent, aSelectDetector, aSetForcedCharset)
{
    try {
        var node = aEvent.target;
        var name = node.getAttribute('name');

        if (name == 'detectorGroup') {
            aSelectDetector(aEvent);
        } else if (name == 'charsetGroup') {
            var charset = node.getAttribute('id');
            charset = charset.substring('charset.'.length, charset.length)
            aSetForcedCharset(charset, true);
        } else if (name == 'charsetCustomize') {
            //do nothing - please remove this else statement, once the charset prefs moves to the pref window
        } else {
            aSetForcedCharset(node.getAttribute('id'), false);
        }
    } catch(ex) {
        alert(ex);
    }
}

function SelectDetector(event, doReload)
{
    dump("Charset Detector menu item pressed: " + event.target.getAttribute('id') + "\n");

    var uri =  event.target.getAttribute("id");
    var prefvalue = uri.substring('chardet.'.length, uri.length);
    if ("off" == prefvalue) { // "off" is special value to turn off the detectors
        prefvalue = "";
    }

    try {
        var pref = Components.classes["@mozilla.org/preferences-service;1"]
                             .getService(Components.interfaces.nsIPrefBranch);
        var str =  Components.classes["@mozilla.org/supports-string;1"]
                             .createInstance(Components.interfaces.nsISupportsString);

        str.data = prefvalue;
        pref.setComplexValue("intl.charset.detector",
                             Components.interfaces.nsISupportsString, str);
        if (typeof doReload == "function") doReload();
    }
    catch (ex) {
        dump("Failed to set the intl.charset.detector preference.\n");
    }
}

var gPrevCharset = null;
function UpdateCurrentCharset()
{
    // extract the charset from DOM
    var wnd = document.commandDispatcher.focusedWindow;
    if ((window == wnd) || (wnd == null)) wnd = window.content;

    // Uncheck previous item
    if (gPrevCharset) {
        var pref_item = document.getElementById('charset.' + gPrevCharset);
        if (pref_item)
          pref_item.setAttribute('checked', 'false');
    }

    var menuitem = document.getElementById('charset.' + wnd.document.characterSet);
    if (menuitem) {
        menuitem.setAttribute('checked', 'true');
    }
}

function UpdateCurrentMailCharset()
{
    var charset = msgWindow.mailCharacterSet;
    var menuitem = document.getElementById('charset.' + charset);

    if (menuitem) {
        menuitem.setAttribute('checked', 'true');
    }
}

function UpdateCharsetDetector()
{
    var prefvalue;

    try {
        var pref = Components.classes["@mozilla.org/preferences-service;1"]
                             .getService(Components.interfaces.nsIPrefBranch);
        prefvalue = pref.getComplexValue("intl.charset.detector",
                                         Components.interfaces.nsIPrefLocalizedString).data;
    }
    catch (ex) {
        prefvalue = "";
    }

    if (prefvalue == "") prefvalue = "off";
    dump("intl.charset.detector = "+ prefvalue + "\n");

    prefvalue = 'chardet.' + prefvalue;
    var menuitem = document.getElementById(prefvalue);

    if (menuitem) {
        menuitem.setAttribute('checked', 'true');
    }
}

function UpdateMenus(event)
{
    // use setTimeout workaround to delay checkmark the menu
    // when onmenucomplete is ready then use it instead of oncreate
    // see bug 78290 for the detail
    UpdateCurrentCharset();
    setTimeout(UpdateCurrentCharset, 0);
    UpdateCharsetDetector();
    setTimeout(UpdateCharsetDetector, 0);
}

function CreateMenu(node)
{
  var observerService = Components.classes["@mozilla.org/observer-service;1"].getService(Components.interfaces.nsIObserverService);
  observerService.notifyObservers(null, "charsetmenu-selected", node);
}

function UpdateMailMenus(event)
{
    // use setTimeout workaround to delay checkmark the menu
    // when onmenucomplete is ready then use it instead of oncreate
    // see bug 78290 for the detail
    UpdateCurrentMailCharset();
    setTimeout(UpdateCurrentMailCharset, 0);
    UpdateCharsetDetector();
    setTimeout(UpdateCharsetDetector, 0);
}

var gCharsetMenu = Components.classes['@mozilla.org/rdf/datasource;1?name=charset-menu'].getService().QueryInterface(Components.interfaces.nsICurrentCharsetListener);
var gLastBrowserCharset = null;

function charsetLoadListener (event)
{
    var charset = window.content.document.characterSet;

    if (charset.length > 0 && (charset != gLastBrowserCharset)) {
        gCharsetMenu.SetCurrentCharset(charset);
        gPrevCharset = gLastBrowserCharset;
        gLastBrowserCharset = charset;
    }
}


function composercharsetLoadListener (event)
{
    var charset = window.content.document.characterSet;

 
    if (charset.length > 0 ) {
       gCharsetMenu.SetCurrentComposerCharset(charset);
    }
 }

function SetForcedEditorCharset(charset)
{
    if (charset.length > 0 ) {
       gCharsetMenu.SetCurrentComposerCharset(charset);
    }
    EditorSetDocumentCharacterSet(charset);
}


var gLastMailCharset = null;

function mailCharsetLoadListener (event)
{
    if (msgWindow) {
        var charset = msgWindow.mailCharacterSet;
        if (charset.length > 0 && (charset != gLastMailCharset)) {
            gCharsetMenu.SetCurrentMailCharset(charset);
            gLastMailCharset = charset;
        }
    }
}

function InitCharsetMenu()
{
    removeEventListener("load", InitCharsetMenu, true);

    var wintype = document.documentElement.getAttribute('windowtype');
    if (window && (wintype == "navigator:browser"))
    {
        var contentArea = window.document.getElementById("appcontent");
        if (contentArea)
            contentArea.addEventListener("pageshow", charsetLoadListener, true);
    }
    else
    {
        var arrayOfStrings = wintype.split(":");
        if (window && arrayOfStrings[0] == "mail")
        {
            var messageContent = window.document.getElementById("messagepane");
            if (messageContent)
                messageContent.addEventListener("pageshow", mailCharsetLoadListener, true);
        }
        else
        if (window && arrayOfStrings[0] == "composer")
        {
            contentArea = window.document.getElementById("appcontent");
            if (contentArea)
                contentArea.addEventListener("pageshow", composercharsetLoadListener, true);
        }
    }
}

addEventListener("load", InitCharsetMenu, true);
