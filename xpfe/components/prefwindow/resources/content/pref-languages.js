var availLanguageList		= new Array();
var activeLanguageList		= new Array();
var availLanguageDict		= new Array();
var ccm						= null; //Language Coverter Mgr.
var prefInt					= null; //Preferences Interface
var languages_pref_string   = new String();
var strBundleService = null;
var localeService = null;
var bundle;
var test1 = null;
var curitem;


function InitAvailable()
{

  dump("********** InitAvailable()\n");    

	try
	{
		prefInt = Components.classes["component://netscape/preferences"];
		ccm		= Components.classes['component://netscape/charset-converter-manager'];

		if (ccm) {
			ccm = ccm.getService();
			ccm = ccm.QueryInterface(Components.interfaces.nsICharsetConverterManager2);
			availLanguageList = ccm.GetDecoderList();
			availLanguageList = availLanguageList.QueryInterface(Components.interfaces.nsISupportsArray);
			availLanguageList.sort;

		}

		if (prefInt) {
			prefInt = prefInt.getService();
			prefInt = prefInt.QueryInterface(Components.interfaces.nsIPref);
			languages_pref_string = prefInt.CopyCharPref("intl.charset_menu.static");
			AddRemoveLatin1('add');
			dump("*** Language PrefString: " + languages_pref_string + "\n");
		}
  }
  
	catch(ex)
	{
		dump("failed to get prefs or Language mgr. services!\n");
		ccm		= null;
		prefInt = null;
	}

	//ReadAvailableLanguages();
	LoadAvailableLanguages();
}

function InitActive()
{

	try
	{
		prefInt = Components.classes["component://netscape/preferences"];
		ccm		= Components.classes['component://netscape/charset-converter-manager'];

		if (ccm) {
			ccm = ccm.getService();
			ccm = ccm.QueryInterface(Components.interfaces.nsICharsetConverterManager2);
			availLanguageList = ccm.GetDecoderList();
			availLanguageList = availLanguageList.QueryInterface(Components.interfaces.nsISupportsArray);
			availLanguageList.sort;

		}

		if (prefInt) {
			prefInt = prefInt.getService();
			prefInt = prefInt.QueryInterface(Components.interfaces.nsIPref);
			languages_pref_string = prefInt.CopyCharPref("intl.charset_menu.static");
			AddRemoveLatin1('add');
			dump("*** Language PrefString: " + languages_pref_string + "\n");
		}
	}
  
	catch(ex)
	{
		dump("failed to get prefs or charset mgr. services!\n");
		ccm		= null;
		prefInt = null;
	}

	ReadAvailableLanguages();
	//LoadAvailableLanguages();
}



function AddLanguage() 
{
    dump("********** AddLanguage()\n");    
    var SelArray = window.openDialog("chrome://communicator/content/pref/pref-languages-add.xul","","modal=yes,chrome,resizable=no");
    dump("********** addLanguage receiving " + SelArray.document + "\n");    
    dump("********** addLanguage receiving " + SelArray.returnValue + "\n");    
}

function ReadAvailableLanguages()
{
		if (availLanguageList) for (i = 0; i < availLanguageList.Count(); i++) {
			
			atom = availLanguageList.GetElementAt(i);
			atom = atom.QueryInterface(Components.interfaces.nsIAtom);

			if (atom) {
				str = atom.GetUnicode();
				try {
				  tit = ccm.GetCharsetTitle(atom);
				} catch (ex) {
				  tit = str; //don't ignore Language detectors without a title
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
  
				availLanguageDict[i] = new Array(2);
				availLanguageDict[i][0]	= tit;	
				availLanguageDict[i][1]	= str;
				availLanguageDict[i][2]	= true;
				if (tit) {}
				else dump('Not label for :' + str + ', ' + tit+'\n');
			
			} //str
		} //for

		availLanguageDict.sort();

}


function LoadAvailableLanguages()
{
  var available_languages			= document.getElementById('available_languages'); 
  var available_languages_treeroot	= document.getElementById('available_languages_root'); 
  var invisible						= new String();

		if (availLanguageDict) for (i = 0; i < availLanguageDict.length; i++) {

		 	try {  //let's beef up our error handling for languages without label / title

				if (availLanguageDict[i][2]) {

					// Create a treerow for the new Language
					var item = document.createElement('treeitem');
					var row  = document.createElement('treerow');
					var cell = document.createElement('treecell');
  
					tit = availLanguageDict[i][0];	
					str = availLanguageDict[i][1];	


					// Copy over the attributes
					cell.setAttribute('value', tit);
					cell.setAttribute('id', str);
 
					// Add it to the active languages tree
					item.appendChild(row);
					row.appendChild(cell);
					available_languages_treeroot.appendChild(item);

					// Select first item
					if (i == 0) {
					}

					dump("*** Added Available Language: " + tit + "\n");

				} //if visible

			} //try

			catch (ex) {
				dump("*** Failed to add Avail. Language: " + tit + "\n");
			} //catch 

		} //for

}



function GetLanguageTitle(id) 
{
	
		dump("looking up title for:" + id + "\n");

		if (availLanguageDict) for (j = 0; j < availLanguageDict.length; j++) {

			//dump("ID:" + availLanguageDict[j][1] + " ==> " + availLanguageDict[j][0] + "\n");
			if ( availLanguageDict[j][1] == id) {	
				//title = 
				dump("found title for:" + id + " ==> " + availLanguageDict[j][0] + "\n");
				return availLanguageDict[j][0];
			}	

		}

		return '';
}

function GetLanguageVisibility(id) 
{
	
		dump("looking up visibility for:" + id + "\n");

		if (availLanguageDict) for (j = 0; j < availLanguageDict.length; j++) {

			//dump("ID:" + availLanguageDict[j][1] + " ==> " + availLanguageDict[j][2] + "\n");
			if ( availLanguageDict[j][1] == id) {	
				//title = 
				dump("found visibility for:" + id + " ==> " + availLanguageDict[j][2] + "\n");
				return availLanguageDict[j][2];
			}	

		}

		return false;
}


function AddRemoveLatin1(action) 
{
	
  try {
	arrayOfPrefs = languages_pref_string.split(', ');
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
				
				languages_pref_string = arrayOfPrefs.join(', ');
				return;
				
			}

		} //for
	
	if (action == 'add')	{

		arrayOfPrefs[arrayOfPrefs.length]= 'iso-8859-1';
		languages_pref_string = arrayOfPrefs.join(', ');

	}
	
}


function LoadActiveLanguages()
{
  var active_languages		   =  document.getElementById('active_languages'); 
  var active_languages_treeroot = document.getElementById('active_languages_root'); 
  var visible;

  
  try {
	arrayOfPrefs = languages_pref_string.split(', ');
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
					tit = ccm.GetLanguageTitle(NS_NewAtom(str));
				}

				catch (ex) {
			
					tit     = GetLanguageTitle(str);
					visible = GetLanguageVisibility(str);

					if (tit == '')	tit = str; 

					//if (title != '') tit = title;
					dump("failed to get title for:" + str + "\n" );

				}
			
			} //atom


			if (str) if (tit) if (visible) {
				dump("Adding Active Language: " + str + " ==> " + tit + "\n");

				// Create a treerow for the new Language
				var item = document.createElement('treeitem');
				var row  = document.createElement('treerow');
				var cell = document.createElement('treecell');
  
				// Copy over the attributes
				cell.setAttribute('value', tit);
				cell.setAttribute('id', str);
 
				// Add it to the active languages tree
				item.appendChild(row);
				row.appendChild(cell);
				active_languages_treeroot.appendChild(item);
				
				dump("*** Added Active Language: " + tit + "\n");

			} //if 

		} //for

}



function SelectAvailableLanguage()
{ 
  //Remove the selection in the active languages list
  var active_languages = document.getElementById('active_languages');
	
  if (active_languages.selectedCells.length > 0)
    active_languages.clearCellSelection();

  if (active_languages.selectedItems.length > 0)
    active_languages.clearItemSelection();

  enable_add_button();
} //SelectAvailableLanguage


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


function AddActiveLanguage(availSelected)

{
    dump("!!!!!! AddActiveLanguage receiving: " + availSelected);

}


function AddAvailableLanguage()
{

  var active_languages		   = window.opener.document.getElementById('active_languages'); 
  var active_languages_treeroot = window.opener.document.getElementById('active_languages_root'); 
  var available_languages	   = document.getElementById('available_languages');
  
  for (var nodeIndex=0; nodeIndex < available_languages.selectedItems.length; nodeIndex++) {
  
    var selItem =  available_languages.selectedItems[nodeIndex];
    var selRow  =  selItem.firstChild;
    var selCell =  selRow.firstChild;

	var Languagename		= selCell.getAttribute('value');
	var Languageid	    = selCell.getAttribute('id');
	var already_active	= false;	


	for (var item = active_languages_treeroot.firstChild; item != null; item = item.nextSibling) {

	    var row  =  item.firstChild;
	    var cell =  row.firstChild;
	    var active_Languageid = cell.getAttribute('id');

		if (active_Languageid == Languageid) {
			already_active = true;
			break;
		}
	}

	if (already_active == false) {

		// Create a treerow for the new Language
		var item = document.createElement('treeitem');
		var row  = document.createElement('treerow');
		var cell = document.createElement('treecell');
  
		// Copy over the attributes
		cell.setAttribute('value', Languagename);
		cell.setAttribute('id', Languageid);
 
		// Add it to the active languages tree
		item.appendChild(row);
		row.appendChild(cell);
		active_languages_treeroot.appendChild(item);

		// Select is only if the caller wants to.
		active_languages.selectItem(item);
		active_languages.ensureElementIsVisible(item);

	}//add new item

  } //loop selected languages

  available_languages.clearItemSelection();
  window.close();

} //AddAvailableLanguage


function AddAvailableLanguageNew()
{

  var available_languages	   = document.getElementById('available_languages'); 
  var availSelected            = new Array();
  
  for (var nodeIndex=0; nodeIndex < available_languages.selectedItems.length; nodeIndex++) {
  
    var selItem =  available_languages.selectedItems[nodeIndex];
    var selRow  =  selItem.firstChild;
    var selCell =  selRow.firstChild;

	var Languagename		= selCell.getAttribute('value');
	var Languageid	    = selCell.getAttribute('id');

	availSelected[nodeIndex]    = new Array(1);
	availSelected[nodeIndex][0]	= Languagename;	
	availSelected[nodeIndex][1]	= Languageid;

  } //loop selected languages

  available_languages.clearItemSelection();
  enable_save();
  dump("********** AddAvailableLanguage returning "+availSelected+"\n");
  window.returnValue = availSelected;
  dump (window.opener);
  window.close();
  return(availSelected);

} //AddAvailableLanguage


function RemoveActiveLanguage()
{
  
  var active_languages_treeroot = document.getElementById('active_languages_root'); 
  var tree = document.getElementById('active_languages');
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
    active_languages_treeroot.removeChild(selectedNode);

   } //while
  
  if (nextNode) {
    tree.selectItem(nextNode)
  } else {
    //tree.clearItemSelection();
  }
  
  enable_save();
} //RemoveActiveLanguage


function Save()
{

  // Iterate through the 'active languages  tree to collect the languages
  // that the user has chosen. 

  dump('Entering Save() function.\n');

  var active_languages		   = document.getElementById('active_languages'); 
  var active_languages_treeroot = document.getElementById('active_languages_root'); 
  
  var row           = null;
  var cell          = null;
  var languageid    = new String();
  var num_languages = 0;
  languages_pref_string = '';

  for (var item = active_languages_treeroot.firstChild; item != null; item = item.nextSibling) {

    row  =  item.firstChild;
    cell =  row.firstChild;
    languageid = cell.getAttribute('id');

	if (languageid.length > 1) {
	
        num_languages++;

		//separate >1 languages by commas
		if (num_languages > 1) {
			languages_pref_string = languages_pref_string + "," + " " + languageid;
		} else {
			languages_pref_string = languageid;
		}

	}
  }
	
	  try
	  {
		if (prefInt)
		{
			
			//AddRemoveLatin1('remove');
			prefInt.SetCharPref("intl.charset_menu.static", languages_pref_string);
			//prefInt.SetCharPref("browser.startup.homepage", languages_pref_string);
			//prefInt.CopyCharPref("intl.charset_menu.static", languages_pref_string);
 			//prefInt.SetCharPref("browser.startup.homepage", "www.betak.net");

			confirm_text = document.getElementById('confirm_text');
			dump('intl.charset_menu.static set to ' + languages_pref_string + '.\n');
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
  var tree = document.getElementById('active_languages'); 
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
  var tree = document.getElementById('active_languages');
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


function srGetAppLocale()
{
  var applicationLocale = null;

  if (!localeService) {
      try {
          localeService = Components.classes["component://netscape/intl/nslocaleservice"].getService();
      
          localeService = localeService.QueryInterface(Components.interfaces.nsILocaleService);
      } catch (ex) {
          dump("\n--** localeService failed: " + ex + "\n");
          return null;
      }
  }
  
  applicationLocale = localeService.GetApplicationLocale();
  if (!applicationLocale) {
    dump("\n--** localeService.GetApplicationLocale failed **--\n");
  }
  return applicationLocale;
}


function srGetStrBundleWithLocale(path, locale)
{
  var strBundle = null;

  if (!strBundleService) {
      try {
          strBundleService =
              Components.classes["component://netscape/intl/stringbundle"].getService(); 
          strBundleService = 
              strBundleService.QueryInterface(Components.interfaces.nsIStringBundleService);
      } catch (ex) {
          dump("\n--** strBundleService failed: " + ex + "\n");
          return null;
      }
  }

  strBundle = strBundleService.CreateBundle(path, locale); 
  if (!strBundle) {
        dump("\n--** strBundle createInstance failed **--\n");
  }
  return strBundle;
}

function srGetStrBundle(path)
{
  var appLocale = srGetAppLocale();
  return srGetStrBundleWithLocale(path, appLocale);
}


function localeSwitching(winType, baseDirectory, providerName)
{
  dump("\n ** Enter localeSwitching() ** \n");
  dump("\n ** winType=" +  winType + " ** \n");
  dump("\n ** baseDirectory=" +  baseDirectory + " ** \n");
  dump("\n ** providerName=" +  providerName + " ** \n");

  //
  var rdf;
  if(document.rdf) {
    rdf = document.rdf;
    dump("\n ** rdf = document.rdf ** \n");
  }
  else if(Components) {
    var isupports = Components.classes['component://netscape/rdf/rdf-service'].getService();
    rdf = isupports.QueryInterface(Components.interfaces.nsIRDFService);
    dump("\n ** rdf = Components... ** \n");
  }
  else {
    dump("can't find nuthin: no document.rdf, no Components. \n");
  }
  //

  var ds = rdf.GetDataSource("rdf:chrome");

  // For M4 builds, use this line instead.
  // var ds = rdf.GetDataSource("resource:/chrome/registry.rdf");
  var srcURL = "chrome://";
  srcURL += winType + "/locale/";
  dump("\n** srcURL=" + srcURL + " **\n");
  var sourceNode = rdf.GetResource(srcURL);
  var baseArc = rdf.GetResource("http://chrome.mozilla.org/rdf#base");
  var nameArc = rdf.GetResource("http://chrome.mozilla.org/rdf#name");
                      
  // Get the old targets
  var oldBaseTarget = ds.GetTarget(sourceNode, baseArc, true);
  dump("\n** oldBaseTarget=" + oldBaseTarget + "**\n");
  var oldNameTarget = ds.GetTarget(sourceNode, nameArc, true);
  dump("\n** oldNameTarget=" + oldNameTarget + "**\n");

  // Get the new targets 
  // file:/u/tao/gila/mozilla-org/html/projects/intl/chrome/
  // da-DK
  if (baseDirectory == "") {
        baseDirectory =  "resource:/chrome/";
  }

  var finalBase = baseDirectory;
  if (baseDirectory != "") {
        finalBase += winType + "/locale/" + providerName + "/";
  }
  dump("\n** finalBase=" + finalBase + "**\n");

  var newBaseTarget = rdf.GetLiteral(finalBase);
  var newNameTarget = rdf.GetLiteral(providerName);
  
  // Unassert the old relationships
  if (baseDirectory != "") {
        ds.Unassert(sourceNode, baseArc, oldBaseTarget);
  }

  ds.Unassert(sourceNode, nameArc, oldNameTarget);
  
  // Assert the new relationships (note that we want a reassert rather than
  // an unassert followed by an assert, once reassert is implemented)
  if (baseDirectory != "") {
        ds.Assert(sourceNode, baseArc, newBaseTarget, true);
  }
  ds.Assert(sourceNode, nameArc, newNameTarget, true);
  
  // Flush the modified data source to disk
  // (Note: crashes in M4 builds, so don't use Flush() until fix checked in)
  ds.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource).Flush();
 
  // Open up a new window to see your new chrome, since changes aren't yet dynamically
  // applied to the current window

  // BrowserOpenWindow('chrome://addressbook/content');
  dump("\n ** Leave localeSwitching() ** \n");
}


function localeTo(baseDirectory, localeName)
{
  dump("\n ** Enter localeTo() ** \n");

  localeSwitching("addressbook", baseDirectory, localeName); 
  localeSwitching("bookmarks", baseDirectory, localeName); 
  localeSwitching("directory", baseDirectory, localeName); 
  localeSwitching("editor", baseDirectory, localeName); 
  localeSwitching("global", baseDirectory, localeName); 
  localeSwitching("history", baseDirectory, localeName); 
  localeSwitching("messenger", baseDirectory, localeName); 
  localeSwitching("messengercompose", baseDirectory, localeName); 
  localeSwitching("navigator", baseDirectory, localeName); 
  localeSwitching("pref", baseDirectory, localeName); 
  localeSwitching("profile", baseDirectory, localeName); 
  localeSwitching("regviewer", baseDirectory, localeName); 
  localeSwitching("related", baseDirectory, localeName); 
  localeSwitching("sidebar", baseDirectory, localeName); 
  localeSwitching("wallet", baseDirectory, localeName); 
  localeSwitching("xpinstall", baseDirectory, localeName); 
  
  dump("\n ** Leave localeTo() ** \n");
}

