function MultiplexHandler(event)
{
  node = event.target;
  name = node.getAttribute('name');

  if (name == 'detectorGroup') {
    SelectDetector(event);
  } else if (name == 'charsetGroup') {
    charset = node.getAttribute('id');
    charset = charset.substring('charset.'.length, charset.length)
    SetDefaultCharacterSet(charset);
  } else {
    SetDefaultCharacterSet(node.getAttribute('id'));
  }
}

function SetDefaultCharacterSet(charset)
{
	dump("Charset Overlay menu item pressed: " + charset + "\n");
    BrowserSetDefaultCharacterSet(charset);
}

function SelectDetector(event)
{
	dump("Charset Detector menu item pressed: " + event.target.getAttribute('id') + "\n");

    uri =  event.target.getAttribute("id");
    prefvalue = uri.substring('charsetDetector.'.length, uri.length);
    if("off" == prefvalue) { // "off" is special value to turn off the detectors
        prefvalue = "";
    }

    pref = Components.classes['component://netscape/preferences'];
    if (pref) {
        pref = pref.getService();
        pref = pref.QueryInterface(Components.interfaces.nsIPref);
    }
 
    if (pref) {
        pref.SetCharPref("intl.charset.detector", prefvalue);
        window.content.location.reload();
    }
}

function UpdateCurrentCharset()
{
    charset = document.commandDispatcher.focusedWindow.document.characterSet;
    charset = charset.toLowerCase();

    menuitem = document.getElementById('charset.' + charset);

    if (menuitem) {
        menuitem.setAttribute('checked', 'true');
    }
}

function UpdateCharsetDetector()
{
    pref = Components.classes['component://netscape/preferences'];
    if (pref) {
        pref = pref.getService();
        pref = pref.QueryInterface(Components.interfaces.nsIPref);
    }
 
    if (pref) {
        prefvalue = pref.CopyCharPref("intl.charset.detector");
        if (prefvalue == "") prefvalue = "off";
    }

    prefvalue = 'charsetDetector.' + prefvalue;
    menuitem = document.getElementById(prefvalue);

    if (menuitem) {
        menuitem.setAttribute('checked', 'true');
    }
}

function UpdateMenus(event)
{
    UpdateCurrentCharset();
    UpdateCharsetDetector();
}

function charsetLoadListener (event)
{
    menu = Components.classes['component://netscape/rdf/datasource?name=charset-menu'];

    if (menu) {
        menu = menu.getService();
        menu = menu.QueryInterface(Components.interfaces.nsICurrentCharsetListener);
    }

    charset = window.content.document.characterSet;
    charset = charset.toLowerCase();

    if (menu) {
        menu.SetCurrentCharset(charset);
    }

    // XXX you know, here I could also set the checkmark, for the case when a 
    // doc finishes loading after the menu is already diplayed. But I get a
    // weird assertion!
}

contentArea = window.document.getElementById("appcontent");
if (contentArea)
  contentArea.addEventListener("load", charsetLoadListener, true);
