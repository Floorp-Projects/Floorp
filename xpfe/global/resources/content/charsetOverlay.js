function MultiplexHandler(event)
{
    var node = event.target;
    var name = node.getAttribute('name');

    if (name == 'detectorGroup') {
        SetForcedDetector(true);
        SelectDetector(event, false);
    } else if (name == 'charsetGroup') {
        var charset = node.getAttribute('id');
        charset = charset.substring('charset.'.length, charset.length)
        SetForcedCharset(charset);
        SetDefaultCharacterSet(charset);
    } else if (name == 'charsetCustomize') {
        // please remove this else statement, once the charset prefs moves to the pref window
        window.openDialog("chrome://communicator/content/pref/pref-charset.xul",
                          "_blank", "chrome,modal,resizable", "browser");
    } else {
        SetForcedCharset(node.getAttribute('id'));
        SetDefaultCharacterSet(node.getAttribute('id'));
    }
}

function MailMultiplexHandler(event)
{
    var node = event.target;
    var name = node.getAttribute('name');

    if (name == 'detectorGroup') {
        SelectDetector(event, true);
    } else if (name == 'charsetGroup') {
        var charset = node.getAttribute('id');
        charset = charset.substring('charset.'.length, charset.length)
        MessengerSetDefaultCharacterSet(charset);
    } else if (name == 'charsetCustomize') {
        // please remove this else statement, once the charset prefs moves to the pref window
        window.openDialog("chrome://communicator/content/pref/pref-charset.xul",
                          "_blank", "chrome,modal,resizable", "browser");
    } else {
        MessengerSetDefaultCharacterSet(node.getAttribute('id'));
    }
}

function ComposerMultiplexHandler(event)
{
    var node = event.target;
    var name = node.getAttribute('name');

    if (name == 'detectorGroup') {
        ComposerSelectDetector(event, true);
    } else if (name == 'charsetGroup') {
        var charset = node.getAttribute('id');
        charset = charset.substring('charset.'.length, charset.length)
        EditorSetDocumentCharacterSet(charset);
    } else if (name == 'charsetCustomize') {
        // please remove this else statement, once the charset prefs moves to the pref window
        window.openDialog("chrome://communicator/content/pref/pref-charset.xul",
                          "_blank", "chrome,modal,resizable", "browser");
    } else {
        SetForcedEditorCharset(node.getAttribute('id'));
    }
}

function MaileditMultiplexHandler(event)
{
    var node = event.target;
    var name = node.getAttribute('name');

    if (name == 'charsetCustomize') {
        // please remove this else statement, once the charset prefs moves to the pref window
        window.openDialog("chrome://communicator/content/pref/pref-charset.xul",
                          "_blank", "chrome,modal,resizable", "mailedit");
    } else {
        SetDocumentCharacterSet(node.id);
    }
}

function SetDefaultCharacterSet(charset)
{
    dump("Charset Overlay menu item pressed: " + charset + "\n");
    BrowserSetDefaultCharacterSet(charset);
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
        if (doReload) window.content.location.reload();
    }
    catch (ex) {
        dump("Failed to set the intl.charset.detector preference.\n");
    }
}

function ComposerSelectDetector(event)
{
    //dump("Charset Detector menu item pressed: " + event.target.getAttribute('id') + "\n");

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
        EditorLoadUrl(GetDocumentUrl());    
    }
    catch (ex) {
        dump("Failed to set the intl.charset.detector preference.\n");
    }
}

function SetForcedDetector(doReload)
{
    BrowserSetForcedDetector(doReload);
}

function SetForcedCharset(charset)
{
    BrowserSetForcedCharacterSet(charset);
}

var gPrevCharset = null;
function UpdateCurrentCharset()
{
    var menuitem = null;

    // exctract the charset from DOM
    var wnd = document.commandDispatcher.focusedWindow;
    if ((window == wnd) || (wnd == null)) wnd = window.content;
    menuitem = document.getElementById('charset.' + wnd.document.characterSet);

    if (menuitem) {
        // uncheck previously checked item to workaround Mac checkmark problem
        // bug 98625
        if (gPrevCharset) {
            var pref_item = document.getElementById('charset.' + gPrevCharset);
            if (pref_item)
              pref_item.setAttribute('checked', 'false');
        }
        menuitem.setAttribute('checked', 'true');
    }
}

function UpdateCurrentMailCharset()
{
    var charset = msgWindow.mailCharacterSet;
    dump("Update current mail charset: " + charset + " \n");

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
    setTimeout("UpdateCurrentCharset()", 0);
    UpdateCharsetDetector();
    setTimeout("UpdateCharsetDetector()", 0);
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
    setTimeout("UpdateCurrentMailCharset()", 0);
    UpdateCharsetDetector();
    setTimeout("UpdateCharsetDetector()", 0);
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
            dump("mailCharsetLoadListener: " + charset + " \n");
        }
    }
}

var wintype = document.firstChild.getAttribute('windowtype');
if (window && (wintype == "navigator:browser"))
{
    var contentArea = window.document.getElementById("appcontent");
    if (contentArea)
        contentArea.addEventListener("load", charsetLoadListener, true);
}
else
{
    var arrayOfStrings = wintype.split(":");
    if (window && arrayOfStrings[0] == "mail") 
    {
        var messageContent = window.document.getElementById("messagepane");
        if (messageContent)
            messageContent.addEventListener("load", mailCharsetLoadListener, true);
    }
    else
    if (window && arrayOfStrings[0] == "composer") 
    {
        contentArea = window.document.getElementById("appcontent");
        if (contentArea)
            contentArea.addEventListener("load", composercharsetLoadListener, true);
    }

}
