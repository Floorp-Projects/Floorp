//get prefInt services

var availCharsetList		 = new Array();
var activeCharsetList		 = new Array();
var availCharsetDict		 = new Array();
var ccm						       = null; //Charset Coverter Mgr.
var prefInt					     = null; //Preferences Interface
var charsets_pref_string = new String();
var applicationArea      = new String();

function Init()
{

  dump("*** pref-charset.js, Init()\n");

  try {
    if (window.arguments && window.arguments[0])  {
           applicationArea = window.arguments[0];
    } else {
      dump("*** no window arguments!\n");
    } //if
  } //try

  catch(ex) {
     dump("*** failed reading arguments\n");
  }

	try
	{
		prefInt = Components.classes["component://netscape/preferences"];

		if (prefInt) {
			prefInt = prefInt.getService();
			prefInt = prefInt.QueryInterface(Components.interfaces.nsIPref);
			
      if (applicationArea == 'mailnews') {
        charsets_pref_string = prefInt.CopyCharPref("intl.charsetmenu.mailedit");
        dump("*** intl.charsetmenu.mailedit\n");
      } else {
        //default is the browser
        charsets_pref_string == prefInt.CopyCharPref("intl.charsetmenu.browser.static");
        dump("*** intl.charsetmenu.browser.static\n");
      }
      
      //AddRemoveLatin1('add');
			dump("*** Charset PrefString: " + charsets_pref_string + "\n");
		}
	}


	catch(ex)
	{
		dump("failed to get prefs services!\n");
		prefInt = null;
	}


  try {
		ccm		= Components.classes['component://netscape/charset-converter-manager'];

		if (ccm) {
			ccm = ccm.getService();
			ccm = ccm.QueryInterface(Components.interfaces.nsICharsetConverterManager2);
			availCharsetList = ccm.GetDecoderList();
			availCharsetList = availCharsetList.QueryInterface(Components.interfaces.nsISupportsArray);
			availCharsetList.sort;
		}
  }

  catch(ex)
  {
		dump("failed to get charset mgr. services!\n");
		ccm		= null;
  }


	LoadAvailableCharSets();
	LoadActiveCharSets();
}


function LoadAvailableCharSets()
{
  var available_charsets			= document.getElementById('available_charsets'); 
  var available_charsets_treeroot	= document.getElementById('available_charsets_root'); 
  var invisible						= new String();


		if (availCharsetList) for (i = 0; i < availCharsetList.Count(); i++) {
			
			atom = availCharsetList.GetElementAt(i);
			atom = atom.QueryInterface(Components.interfaces.nsIAtom);

			if (atom) {
				str = atom.GetUnicode();
				try {
				  tit = ccm.GetCharsetTitle(atom);
				} catch (ex) {
				  tit = str; //don't ignore charset detectors without a title
				}
				
				try {                                  
				  visible = ccm.GetCharsetData(atom,'.notForBrowser');
				  visible = false;
				} catch (ex) {
				  visible = true;
				  //dump('Getting invisible for:' + str + ' failed!\n');
				}
			} //atom

			if (str) if (tit) {
  
				availCharsetDict[i] = new Array(2);
				availCharsetDict[i][0]	= tit;	
				availCharsetDict[i][1]	= str;
				availCharsetDict[i][2]	= visible;
				if (tit) {}
				else dump('Not label for :' + str + ', ' + tit+'\n');
					
			
			} //str
		} //for

		availCharsetDict.sort();

		if (availCharsetDict) for (i = 0; i < availCharsetDict.length; i++) {

			try {  //let's beef up our error handling for charsets without label / title

				if (availCharsetDict[i][2]) {

					// Create a treerow for the new charset
					var item = document.createElement('treeitem');
					var row  = document.createElement('treerow');
					var cell = document.createElement('treecell');
  
					tit = availCharsetDict[i][0];	
					str = availCharsetDict[i][1];	


					// Copy over the attributes
					cell.setAttribute('value', tit);
					cell.setAttribute('id', str);
 
					// Add it to the active charsets tree
					item.appendChild(row);
					row.appendChild(cell);
					available_charsets_treeroot.appendChild(item);

					// Select first item
					if (i == 0) {
					}

					dump("*** Added Available Charset: " + tit + "\n");

				} //if visible

			} //try

			catch (ex) {
				dump("*** Failed to add Avail. Charset: " + tit + "\n");
			} //catch

		} //for

		//select first item in list box

		//item = available_charsets_treeroot.firstChild;

		//if (item) try {
		//	available_charsets.selectItem(item);
		//	available_charsets.ensureElementIsVisible(item);
		//}
		
		//catch (ex) {
		//	dump("Not able to select first avail. charset.\n");
		//}

}


function GetCharSetTitle(id) 
{
	
		dump("looking up title for:" + id + "\n");

		if (availCharsetDict) for (j = 0; j < availCharsetDict.length; j++) {

			//dump("ID:" + availCharsetDict[j][1] + " ==> " + availCharsetDict[j][0] + "\n");
			if ( availCharsetDict[j][1] == id) {	
				//title = 
				dump("found title for:" + id + " ==> " + availCharsetDict[j][0] + "\n");
				return availCharsetDict[j][0];
			}	

		}

		return '';
}

function GetCharSetVisibility(id) 
{
	
		dump("looking up visibility for:" + id + "\n");

		if (availCharsetDict) for (j = 0; j < availCharsetDict.length; j++) {

			//dump("ID:" + availCharsetDict[j][1] + " ==> " + availCharsetDict[j][2] + "\n");
			if ( availCharsetDict[j][1] == id) {	
				//title = 
				dump("found visibility for:" + id + " ==> " + availCharsetDict[j][2] + "\n");
				return availCharsetDict[j][2];
			}	

		}

		return false;
}


function AddRemoveLatin1(action) 
{
	
  try {
	arrayOfPrefs = charsets_pref_string.split(', ');
  } 
  
  catch (ex) {
	dump("failed to split the preference string!\n");
  }

		if (arrayOfPrefs) for (i = 0; i < arrayOfPrefs.length; i++) {

			str = arrayOfPrefs[i];

			if (str == 'iso-8859-1') {

				if (action == 'remove') {
					arrayOfPrefs[i]=arrayOfPrefs[arrayOfPrefs.length-1];
					arrayOfPrefs.length = arrayOfPrefs.length - 1;
				}
				
				charsets_pref_string = arrayOfPrefs.join(', ');
				return;
				
			}

		} //for
	
	if (action == 'add')	{

		arrayOfPrefs[arrayOfPrefs.length]= 'iso-8859-1';
		charsets_pref_string = arrayOfPrefs.join(', ');

	}
	
}


function LoadActiveCharSets()
{
  var active_charsets		   = document.getElementById('active_charsets'); 
  var active_charsets_treeroot = document.getElementById('active_charsets_root'); 
  var visible;

  
  try {
	arrayOfPrefs = charsets_pref_string.split(', ');
  } 
  
  catch (ex) {
	dump("failed to split the preference string!\n");
  }

	
		if (arrayOfPrefs) for (i = 0; i < arrayOfPrefs.length; i++) {

			str = arrayOfPrefs[i];

			try {	
			atom = atom.QueryInterface(Components.interfaces.nsIAtom);
			} catch (ex) {
				dump("failed to load atom interface...\n" );
			}

			try {	
			    atom.ToString(str); } catch (ex) {
				dump("failed to create an atom from:" + str + "\n" );
			}

			if (atom) {
			
				try {
					tit = ccm.GetCharsetTitle(NS_NewAtom(str));
				}

				catch (ex) {
			
					tit     = GetCharSetTitle(str);
					visible = GetCharSetVisibility(str);

					if (tit == '')	tit = str; 

					//if (title != '') tit = title;
					dump("failed to get title for:" + str + "\n" );

				}
			
			} //atom


			if (str) if (tit) if (visible) {
				dump("Adding Active Charset: " + str + " ==> " + tit + "\n");

				// Create a treerow for the new charset
				var item = document.createElement('treeitem');
				var row  = document.createElement('treerow');
				var cell = document.createElement('treecell');
  
				// Copy over the attributes
				cell.setAttribute('value', tit);
				cell.setAttribute('id', str);
 
				// Add it to the active charsets tree
				item.appendChild(row);
				row.appendChild(cell);
				active_charsets_treeroot.appendChild(item);
				
				dump("*** Added Active Charset: " + tit + "\n");

			} //if 

		} //for

}



function SelectAvailableCharset()
{ 
  //Remove the selection in the active charsets list
  var active_charsets = document.getElementById('active_charsets');
	
  if (active_charsets.selectedCells.length > 0)
    active_charsets.clearCellSelection();

  if (active_charsets.selectedItems.length > 0)
    active_charsets.clearItemSelection();

  enable_add_button();
} //SelectAvailableCharset



function SelectActiveCharset()
{ 
  //Remove the selection in the available charsets list
  var available_charsets = document.getElementById('available_charsets');

  if (available_charsets.selectedCells.length > 0)
      available_charsets.clearCellSelection();

  if (available_charsets.selectedItems.length > 0)
      available_charsets.clearItemSelection();

  enable_remove_button();
} //SelectActiveCharset



function enable_remove_button()
{
  var remove_button = document.getElementById('remove_button');

  remove_button.setAttribute('disabled','false');
}

  
function enable_save()
{
  var save_button = document.getElementById('save_button');
	
  save_button.setAttribute('disabled','false');
}



function enable_add_button()
{
  var add_button = document.getElementById('add_button');
	
  add_button.setAttribute('disabled','false');
}



function AddAvailableCharset()
{

  var active_charsets		   = document.getElementById('active_charsets'); 
  var active_charsets_treeroot = document.getElementById('active_charsets_root'); 
  var available_charsets	   = document.getElementById('available_charsets'); 
  
  for (var nodeIndex=0; nodeIndex < available_charsets.selectedItems.length; nodeIndex++) {
  
    var selItem =  available_charsets.selectedItems[nodeIndex];
    var selRow  =  selItem.firstChild;
    var selCell =  selRow.firstChild;

	var charsetname		= selCell.getAttribute('value');
	var charsetid	    = selCell.getAttribute('id');
	var already_active	= false;	


	for (var item = active_charsets_treeroot.firstChild; item != null; item = item.nextSibling) {

	    var row  =  item.firstChild;
	    var cell =  row.firstChild;
	    var active_charsetid = cell.getAttribute('id');

		if (active_charsetid == charsetid) {
			already_active = true;
			break;
		}
	}

	if (already_active == false) {

		// Create a treerow for the new charset
		var item = document.createElement('treeitem');
		var row  = document.createElement('treerow');
		var cell = document.createElement('treecell');
  
		// Copy over the attributes
		cell.setAttribute('value', charsetname);
		cell.setAttribute('id', charsetid);
 
		// Add it to the active charsets tree
		item.appendChild(row);
		row.appendChild(cell);
		active_charsets_treeroot.appendChild(item);

		// Select is only if the caller wants to.
		active_charsets.selectItem(item);
		active_charsets.ensureElementIsVisible(item);

	}//add new item

  } //loop selected charsets

  available_charsets.clearItemSelection();
  enable_save();

} //AddAvailableCharset



function RemoveActiveCharset()
{
  
  var active_charsets_treeroot = document.getElementById('active_charsets_root'); 
  var tree = document.getElementById('active_charsets');
  var nextNode = null;
  var numSelected = tree.selectedItems.length;
  var deleted_all = false;

  while (tree.selectedItems.length > 0) {
    
	var selectedNode = tree.selectedItems[0];
    nextNode = selectedNode.nextSibling;
    
	if (!nextNode) 
	  if (selectedNode.previousSibling) 
		nextNode = selectedNode.previousSibling;
    
    var row  =  selectedNode.firstChild;
    var cell =  row.firstChild;

	row.removeChild(cell);
	selectedNode.removeChild(row);
    active_charsets_treeroot.removeChild(selectedNode);

   } //while
  
  if (nextNode) {
    tree.selectItem(nextNode)
  } else {
    //tree.clearItemSelection();
  }
  
  enable_save();
} //RemoveActiveCharset



function Save()
{

  // Iterate through the 'active charsets  tree to collect the charsets
  // that the user has chosen. 

  dump('Entering Save() function.\n');

  var active_charsets		   = document.getElementById('active_charsets'); 
  var active_charsets_treeroot = document.getElementById('active_charsets_root'); 
  
  var row          = null;
  var cell         = null;
  var charsetid    = new String();
  var num_charsets = 0;
  charsets_pref_string = '';

  for (var item = active_charsets_treeroot.firstChild; item != null; item = item.nextSibling) {

    row  =  item.firstChild;
    cell =  row.firstChild;
    charsetid = cell.getAttribute('id');

	if (charsetid.length > 1) {
	
        num_charsets++;

		//separate >1 charsets by commas
		if (num_charsets > 1) {
			charsets_pref_string = charsets_pref_string + "," + " " + charsetid;
		} else {
			charsets_pref_string = charsetid;
		}

	}
  }
	
	  try
	  {
		if (prefInt)
		{
			
			//AddRemoveLatin1('remove');
			prefInt.SetCharPref("intl.charset_menu.static", charsets_pref_string);
			//prefInt.SetCharPref("browser.startup.homepage", charsets_pref_string);
			//prefInt.CopyCharPref("intl.charset_menu.static", charsets_pref_string);
 			//prefInt.SetCharPref("browser.startup.homepage", "www.betak.net");

			confirm_text = document.getElementById('confirm_text');
			dump('intl.charset_menu.static set to ' + charsets_pref_string + '.\n');
			window.close();
			confirm(confirm_text.getAttribute('value'));
		}
	  }

	  catch(ex)
	  {
		  confirm('exception' + ex);
 		  //prefInt.SetDefaultCharPref("intl.charset_menu.static", "iso-8859-1, iso-2022-jp, shift_jis, euc-jp");
	  }

} //Save


function MoveUp() {
  var tree = document.getElementById('active_charsets'); 
  if (tree.selectedItems.length == 1) {
    var selected = tree.selectedItems[0];
    var before = selected.previousSibling
    if (before) {
      before.parentNode.insertBefore(selected, before);
      tree.selectItem(selected);
      tree.ensureElementIsVisible(selected);
    }
  }

  enable_save();
} //MoveUp


   
function MoveDown() {
  var tree = document.getElementById('active_charsets');
  if (tree.selectedItems.length == 1) {
    var selected = tree.selectedItems[0];
    if (selected.nextSibling) {
      if (selected.nextSibling.nextSibling) {
        selected.parentNode.insertBefore(selected, selected.nextSibling.nextSibling);
      }
      else {
        selected.parentNode.appendChild(selected);
      }
      tree.selectItem(selected);
    }
  }

  enable_save();
} //MoveDown

