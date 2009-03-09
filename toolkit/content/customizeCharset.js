//get prefInt services

var availCharsetDict     = [];
var prefBranch           = null; //Preferences Interface
var pref_string_title    = "";
var pref_string_content  = "";
var pref_string_object   = null;

function Init()
{
  var applicationArea = "";

  if ("arguments" in window && window.arguments[0])
    applicationArea = window.arguments[0];

  prefBranch = Components.classes["@mozilla.org/preferences-service;1"];

  if (prefBranch) {
    prefBranch = prefBranch.getService(Components.interfaces.nsIPrefBranch);

    if (applicationArea.indexOf("mail") != -1) {
      pref_string_title = "intl.charsetmenu.mailedit";
    } else {
    //default is the browser
      pref_string_title = "intl.charsetmenu.browser.static";
    }

    pref_string_object = prefBranch.getComplexValue(pref_string_title, Components.interfaces.nsIPrefLocalizedString)
    pref_string_content = pref_string_object.data;

    AddRemoveLatin1('add');
  }

  LoadAvailableCharSets();
  LoadActiveCharSets();
}


function readRDFString(aDS,aRes,aProp) 
{
  var n = aDS.GetTarget(aRes, aProp, true);
  if (n)
    return n.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
  else 
    return "";
}


function LoadAvailableCharSets()
{
  try {
    var available_charsets_listbox = document.getElementById('available_charsets');
    var rdf=Components.classes["@mozilla.org/rdf/rdf-service;1"].getService(Components.interfaces.nsIRDFService); 
    var kNC_Root = rdf.GetResource("NC:DecodersRoot");
    var kNC_name = rdf.GetResource("http://home.netscape.com/NC-rdf#Name");
    var rdfDataSource = rdf.GetDataSource("rdf:charset-menu"); 
    var rdfContainer =
      Components.classes["@mozilla.org/rdf/container;1"]
                .createInstance(Components.interfaces.nsIRDFContainer);

    rdfContainer.Init(rdfDataSource, kNC_Root);
    var availableCharsets = rdfContainer.GetElements();
    var charset;

    for (var i = 0; i < rdfContainer.GetCount(); i++) {
      charset = availableCharsets.getNext().QueryInterface(Components.interfaces.nsIRDFResource);
      availCharsetDict[i] = new Array(2);
      availCharsetDict[i][0] = readRDFString(rdfDataSource, charset, kNC_name);
      availCharsetDict[i][1] = charset.Value;

      AddListItem(document,
                  available_charsets_listbox,
                  availCharsetDict[i][1],
                  availCharsetDict[i][0]);
    }
  }
  catch (e) {}
}


function GetCharSetTitle(id)
{
  if (availCharsetDict) {
    for (var j = 0; j < availCharsetDict.length; j++) {
      if (availCharsetDict[j][1] == id) {
        return availCharsetDict[j][0];
      }
    }
  }
  return '';
}

function AddRemoveLatin1(action)
{
  var arrayOfPrefs = [];
  arrayOfPrefs = pref_string_content.split(', ');

  if (arrayOfPrefs.length > 0) {
    for (var i = 0; i < arrayOfPrefs.length; i++) {
      if (arrayOfPrefs[i] == 'ISO-8859-1') {
        if (action == 'remove') {
          arrayOfPrefs[i] = arrayOfPrefs[arrayOfPrefs.length-1];
          arrayOfPrefs.length = arrayOfPrefs.length - 1;
        }

        pref_string_content = arrayOfPrefs.join(', ');
        return;
      }
    }

    if (action == 'add') {
      arrayOfPrefs[arrayOfPrefs.length] = 'ISO-8859-1';
      pref_string_content = arrayOfPrefs.join(', ');
    }
  }
}


function LoadActiveCharSets()
{
  var active_charsets = document.getElementById('active_charsets');
  var arrayOfPrefs = [];
  var str;
  var tit;

  arrayOfPrefs = pref_string_content.split(', ');

  if (arrayOfPrefs.length > 0) {
    for (var i = 0; i < arrayOfPrefs.length; i++) {
      str = arrayOfPrefs[i];
      tit = GetCharSetTitle(str);
      if (str && tit)
        AddListItem(document, active_charsets, str, tit);
    }
  }
}

function enable_save()
{
  var save_button = document.documentElement.getButton("accept");
  save_button.removeAttribute('disabled');
}


function update_buttons()
{
  var available_charsets = document.getElementById('available_charsets');
  var active_charsets = document.getElementById('active_charsets');
  var remove_button = document.getElementById('remove_button');
  var add_button = document.getElementById('add_button');
  var up_button = document.getElementById('up_button');
  var down_button = document.getElementById('down_button');

  var activeCharsetSelected = (active_charsets.selectedItems.length > 0);
  remove_button.disabled = !activeCharsetSelected;

  if (activeCharsetSelected) {
    up_button.disabled = !(active_charsets.selectedItems[0].previousSibling);
    down_button.disabled = !(active_charsets.selectedItems[0].nextSibling);
  }
  else {
    up_button.disabled = true;
    down_button.disabled = true;
  }

  add_button.disabled = (available_charsets.selectedItems.length == 0);
}



function AddAvailableCharset()
{
  var active_charsets = document.getElementById('active_charsets');
  var available_charsets = document.getElementById('available_charsets');

  for (var nodeIndex=0; nodeIndex < available_charsets.selectedItems.length;  nodeIndex++)
  {
    var selItem =  available_charsets.selectedItems[nodeIndex];
    
    var charsetname  = selItem.label;
    var charsetid = selItem.id;
    var already_active = false;

    for (var item = active_charsets.firstChild; item != null; item = item.nextSibling) {
      if (charsetid == item.id) {
        already_active = true;
        break;
      }//if

    }//for

    if (already_active == false) {
      AddListItem(document, active_charsets, charsetid, charsetname);
    }//if

  }//for

  available_charsets.clearSelection();
  enable_save();

} //AddAvailableCharset



function RemoveActiveCharset()
{
  var listbox = document.getElementById('active_charsets');
  var numSelectedItems = listbox.selectedItems.length;

  for (var count = 0; count < numSelectedItems; count ++) {
    listbox.removeChild(listbox.selectedItems[0]);
  }

  listbox.clearSelection();

  enable_save();
} //RemoveActiveCharset



function Save()
{
  // Iterate through the 'active charsets  tree to collect the charsets
  // that the user has chosen.

  var active_charsets = document.getElementById('active_charsets');

  var charsetid    = "";
  var num_charsets = 0;
  var pref_string_content = '';

  for (var item = active_charsets.firstChild; item != null; item = item.nextSibling) {
    charsetid = item.id;

    if (charsetid.length > 1) {
      num_charsets++;

      //separate >1 charsets by commas
      if (num_charsets > 1)
        pref_string_content = pref_string_content + ", " + charsetid;
      else
        pref_string_content = charsetid;
    }
  }

  try
  {
    if (prefBranch) {
      pref_string_object.data = pref_string_content;
      prefBranch.setComplexValue(pref_string_title, Components.interfaces.nsIPrefLocalizedString, pref_string_object);
      window.close();
    }
  }
  catch(ex) {
    confirm('exception' + ex);
  }
  return true;
} //Save


function MoveUp() {
  var listbox = document.getElementById('active_charsets');
  if (listbox.selectedItems.length == 1) {
    var selected = listbox.selectedItems[0];
    var before = selected.previousSibling
    if (before) {
      listbox.insertBefore(selected, before);
      listbox.selectItem(selected);
      listbox.ensureElementIsVisible(selected);
    }
  }

  enable_save();
} //MoveUp



function MoveDown() {
  var listbox = document.getElementById('active_charsets');
  if (listbox.selectedItems.length == 1) {
    var selected = listbox.selectedItems[0];
    if (selected.nextSibling) {
      if (selected.nextSibling.nextSibling)
        listbox.insertBefore(selected, selected.nextSibling.nextSibling);
      else
        selected.parentNode.appendChild(selected);
      listbox.selectItem(selected);
    }
  }

  enable_save();
} //MoveDown

function AddListItem(doc, listbox, ID, UIstring)
{
  // Create a treerow for the new item
  var item = doc.createElement('listitem');

  // Copy over the attributes
  item.setAttribute('label', UIstring);
  item.setAttribute('id', ID);

  listbox.appendChild(item);
}
