function MultiplexHandler(event)
{
  var node = event.target;
  var name = node.getAttribute('name');
  var charset;

  if (name == 'detectorGroup') {
    SetForcedDetector();
    SelectDetector(event, true);
  } else if (name == 'charsetGroup') {
    charset = node.getAttribute('id');
    charset = charset.substring('charset.'.length, charset.length)
    SetForcedCharset(charset);
    SetDefaultCharacterSet(charset);
  } else if (name == 'charsetCustomize') {
    //do nothing - please remove this else statement, once the charset prefs moves to the pref window
  } else {
    SetForcedCharset(node.getAttribute('id'));
    SetDefaultCharacterSet(node.getAttribute('id'));
  }
}

function MailMultiplexHandler(event)
{
  var node = event.target;
  var name = node.getAttribute('name');
  var charset;

  if (name == 'detectorGroup') {
    SelectDetector(event, false);
  } else if (name == 'charsetGroup') {
    charset = node.getAttribute('id');
    charset = charset.substring('charset.'.length, charset.length)
    MessengerSetDefaultCharacterSet(charset);
  } else if (name == 'charsetCustomize') {
    //do nothing - please remove this else statement, once the charset prefs moves to the pref window
  } else {
    MessengerSetDefaultCharacterSet(node.getAttribute('id'));
  }
}

function ComposerMultiplexHandler(event)
{
  var node = event.target;
  var name = node.getAttribute('name');
  var charset;

  if (name == 'detectorGroup') {
    ComposerSelectDetector(event, true);
  } else if (name == 'charsetGroup') {
    charset = node.getAttribute('id');
    charset = charset.substring('charset.'.length, charset.length)
    EditorSetDocumentCharacterSet(charset);
  } else if (name == 'charsetCustomize') {
    //do nothing - please remove this else statement, once the charset prefs moves to the pref window
  } else {
    EditorSetDocumentCharacterSet(node.getAttribute('id'));
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
    if("off" == prefvalue) { // "off" is special value to turn off the detectors
        prefvalue = "";
    }

    var pref = Components.classes['component://netscape/preferences'];
    if (pref) {
        pref = pref.getService();
        pref = pref.QueryInterface(Components.interfaces.nsIPref);
    }
 
    if (pref) {
        pref.SetCharPref("intl.charset.detector", prefvalue);
        if (doReload) window._content.location.reload();
    }
}

function ComposerSelectDetector(event)
{
    //dump("Charset Detector menu item pressed: " + event.target.getAttribute('id') + "\n");

    var uri =  event.target.getAttribute("id");
    var prefvalue = uri.substring('chardet.'.length, uri.length);
    if("off" == prefvalue) { // "off" is special value to turn off the detectors
        prefvalue = "";
    }

    var pref = Components.classes['component://netscape/preferences'];
    if (pref) {
        pref = pref.getService();
        pref = pref.QueryInterface(Components.interfaces.nsIPref);
    }

    if (pref) {
        pref.SetCharPref("intl.charset.detector", prefvalue);
          editorShell.LoadUrl(editorShell.editorDocument.location);    
    }
}

function SetForcedDetector()
{
  BrowserSetForcedDetector();
}

function SetForcedCharset(charset)
{
  BrowserSetForcedCharacterSet(charset);
}

function UpdateCurrentCharset()
{
    var wnd = document.commandDispatcher.focusedWindow;
    if (window == wnd) wnd = window._content;

    var charset = wnd.document.characterSet;
	dump("Update current charset: " + charset + "\n");

    var menuitem = document.getElementById('charset.' + charset);

    if (menuitem) {
        menuitem.setAttribute('checked', 'true');
    }
}

function UpdateCurrentMailCharset()
{
    var charset = msgWindow.mailCharacterSet;
    dump("UPdate current mail charset: " + charset + " \n");

    var menuitem = document.getElementById('charset.' + charset);

    if (menuitem) {
        menuitem.setAttribute('checked', 'true');
    }
}

function UpdateCharsetDetector()
{
    var pref = Components.classes['component://netscape/preferences'];
    if (pref) {
        pref = pref.getService();
        pref = pref.QueryInterface(Components.interfaces.nsIPref);
    }
 
    if (pref) {
        prefvalue = pref.getLocalizedUnicharPref("intl.charset.detector");
        if (prefvalue == "") prefvalue = "off";
dump("intl.charset.detector = "+ prefvalue + "\n");
    }

    var prefvalue = 'chardet.' + prefvalue;
    var menuitem = document.getElementById(prefvalue);

    if (menuitem) {
        menuitem.setAttribute('checked', 'true');
    }
}

function UpdateMenus(event)
{
    UpdateCurrentCharset();
    UpdateCharsetDetector();
}

function UpdateMailMenus(event)
{
    UpdateCurrentMailCharset();
    UpdateCharsetDetector();
}

function charsetLoadListener (event)
{
    var menu = Components.classes['component://netscape/rdf/datasource?name=charset-menu'];

    if (menu) {
        menu = menu.getService();
        menu = menu.QueryInterface(Components.interfaces.nsICurrentCharsetListener);
    }

    var charset = window._content.document.characterSet;

    if (menu) {
        menu.SetCurrentCharset(charset);
    }

    // XXX you know, here I could also set the checkmark, for the case when a 
    // doc finishes loading after the menu is already diplayed. But I get a
    // weird assertion!
}

function mailCharsetLoadListener (event)
{
    var menu = Components.classes['component://netscape/rdf/datasource?name=charset-menu'];

    if (menu) {
        menu = menu.getService();
        menu = menu.QueryInterface(Components.interfaces.nsICurrentCharsetListener);
        
        if (msgWindow) {
            var charset = msgWindow.mailCharacterSet;
            if (charset.length > 0) {
                menu.SetCurrentMailCharset(charset);
                dump("mailCharsetLoadListener: " + charset + " \n");
            }
        }
    }
}

var wintype = document.firstChild.getAttribute('windowtype');
if (window && (wintype == "navigator:browser"))
{
  contentArea = window.document.getElementById("appcontent");
  if (contentArea)
    contentArea.addEventListener("load", charsetLoadListener, true);
}
else
{
  var arrayOfStrings = wintype.split(":");
  if (window && arrayOfStrings[0] == "mail") 
  {
    contentArea = window.document.getElementById("messagepane");
    if (contentArea)
      contentArea.addEventListener("load", mailCharsetLoadListener, true);
  }
}
