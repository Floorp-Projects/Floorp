function MultiplexHandler(event)
{
  var node = event.target;
  var name = node.getAttribute('name');
  var charset;

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
        window.content.location.reload();
    }
}

function UpdateCurrentCharset()
{
    var charset = document.commandDispatcher.focusedWindow.document.characterSet;
	dump("Update current charset: " + charset + "\n");

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
        prefvalue = pref.CopyCharPref("intl.charset.detector");
        if (prefvalue == "") prefvalue = "off";
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

function charsetLoadListener (event)
{
    var menu = Components.classes['component://netscape/rdf/datasource?name=charset-menu'];

    if (menu) {
        menu = menu.getService();
        menu = menu.QueryInterface(Components.interfaces.nsICurrentCharsetListener);
    }

    var charset = window.content.document.characterSet;

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
