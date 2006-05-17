var prefInt;  
var regionsBundle;
var languagesBundle;
var acceptedBundle;
var activeLanguageList;
var availLanguageList;
var availLanguageDict;
var languages_pref_string;


function GetBundles()
{
  if (!regionsBundle)    regionsBundle   = srGetStrBundle("chrome://global/locale/regionNames.properties"); 
  if (!languagesBundle)  languagesBundle = srGetStrBundle("chrome://global/locale/languageNames.properties"); 
  if (!acceptedBundle)   acceptedBundle  = srGetStrBundle("resource:/res/language.properties"); 

}


function InitAvailable()
{

  dump("********** InitAvailable()\n");    

	try
	{
    if (!prefInt) {
		  
      languages_pref_string   = new String();
      prefInt = Components.classes["component://netscape/preferences"];

		  if (prefInt) {
			  prefInt = prefInt.getService();
			  prefInt = prefInt.QueryInterface(Components.interfaces.nsIPref);
  		  languages_pref_string = prefInt.CopyCharPref("intl.accept_languages");
			  dump("*** Language PrefString: " + languages_pref_string + "\n");

		  }
    }
  }
  
	catch(ex)
	{
		dump("failed to get prefs or Language mgr. services!\n");
		prefInt = null;
	}

  GetBundles()
	ReadAvailableLanguages();
	LoadAvailableLanguages();
}


function InitActive2()
{

	try
	{
    if (!prefInt) {
		  prefInt = Components.classes["component://netscape/preferences"];
    }

		if (prefInt) {

      languages_pref_string   = new String();
			prefInt = prefInt.getService();
			prefInt = prefInt.QueryInterface(Components.interfaces.nsIPref);
	    languages_pref_string = prefInt.CopyCharPref("intl.accept_languages");
			dump("*** Language PrefString: " + languages_pref_string + "\n");

		}
	}
  
	catch(ex)
	{
		dump("failed to get prefs or charset mgr. services!\n");
		prefInt = null;
	}

  GetBundles()
	ReadAvailableLanguages();
	LoadActiveLanguages2();

}


function InitActive()
{

	try
	{
    if (!prefInt) {
		  prefInt = Components.classes["component://netscape/preferences"];
    }

		if (prefInt) {

      languages_pref_string   = new String();
			prefInt = prefInt.getService();
			prefInt = prefInt.QueryInterface(Components.interfaces.nsIPref);
	    languages_pref_string = prefInt.CopyCharPref("intl.accept_languages");
			dump("*** Language PrefString: " + languages_pref_string + "\n");

		}
	}
  
	catch(ex)
	{
		dump("failed to get prefs or charset mgr. services!\n");
		prefInt = null;
	}

  GetBundles()
	ReadAvailableLanguages();
	LoadActiveLanguages();

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

    availLanguageDict		= new Array();
    var str = new String();
    var i =0;

    dump('ReadAvailableLanguages()\n');
    acceptedBundleEnum = acceptedBundle.getSimpleEnumeration(); 


    while (acceptedBundleEnum.HasMoreElements()) { 			

       //progress through the bundle 
       curItem = acceptedBundleEnum.GetNext(); 

       //"unpack" the item, nsIPropertyElement is now partially scriptable 
       curItem = curItem.QueryInterface(Components.interfaces.nsIPropertyElement); 

       //dump string name (key) 
       var stringName = curItem.getKey(); 
       stringNameProperty = stringName.split('.');

       if (stringNameProperty[1] == 'accept') {

          //dump the UI string (value) 
           var visible   = curItem.getValue(); 

          if (visible == 'true') {

             str = stringNameProperty[0];
             stringLangRegion = stringNameProperty[0].split('-');
                     
             if (stringLangRegion[0]) {
               var tit = languagesBundle.GetStringFromName(stringLangRegion[0]);
          
               if (stringLangRegion[1]) {
                 
                 try {   
                  var tit = tit + "/" + regionsBundle.GetStringFromName(stringLangRegion[1]);
                 }
               
                 catch (ex) {
                    dump("No region string for:" + stringLangRegion[1] + "\n");
                 }
               } //if region
             } //if language
				    
  		       if (str) if (tit) {
                
                dump("Loading available language:" + str + " = " + tit + ", " + i + "\n"); 
				        availLanguageDict[i] = new Array(2);
				        availLanguageDict[i][0]	= tit;	
				        availLanguageDict[i][1]	= str;
				        availLanguageDict[i][2]	= true;
                i++;

				        if (tit) {}
				        else dump('Not label for :' + str + ', ' + tit+'\n');
			        
             } // if str&tit
            } //if visible
			    } //if accepted
		    } //while

		availLanguageDict.sort();
}


function LoadAvailableLanguages()
{
  availLanguageList	              	= new Array();
  var available_languages			      = document.getElementById('available_languages'); 
  var available_languages_treeroot	= document.getElementById('available_languages_root');

  dump("Loading available languages!\n");
  dump("Dict length: " + availLanguageDict.length + "\n");
  
		if (availLanguageDict) for (i = 0; i < availLanguageDict.length; i++) {

		 	try {  //let's beef up our error handling for languages without label / title

					dump("*** Creating new row\n");

					// Create a treerow for the new Language
					var item = document.createElement('treeitem');
					var row  = document.createElement('treerow');
					var cell = document.createElement('treecell');
  
					tit = availLanguageDict[i][0];	
					str = availLanguageDict[i][1];	

					dump("*** Tit: " + tit + "\n");
					dump("*** Str: " + str + "\n");


					// Copy over the attributes
					cell.setAttribute('value', tit);
					cell.setAttribute('id', str);
 
					// Add it to the active languages tree
					dump("*** Append row \n");
					item.appendChild(row);
					dump("*** Append cell \n");
					row.appendChild(cell);
	
  				dump("*** Append item \n");
					available_languages_treeroot.appendChild(item);

					dump("*** Tit: " + tit + "\n");
					dump("*** Str: " + str + "\n");

					// Select first item
					if (i == 0) {
					}

					dump("*** Added Available Language: " + tit + "\n");

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
  activeLanguageList = new Array();
  var active_languages =  document.getElementById('active_languages'); 
  var active_languages_treeroot = document.getElementById('active_languages_root'); 
  var visible;

  dump("Loading: " + languages_pref_string + "!\n");
  
  try {
	  arrayOfPrefs = languages_pref_string.split(', ');
  } 
  
  catch (ex) {
	  dump("failed to split the preference string!\n");
  }
	
		if (arrayOfPrefs) for (i = 0; i < arrayOfPrefs.length; i++) {

			str = arrayOfPrefs[i];
      tit = GetLanguageTitle(str);
      
			if (str) if (tit) {
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


function LoadActiveLanguages2()
{
  
  try {
    activeLanguageList = new Array();
    var active_languages =  document.getElementById('available_languages'); 
    var active_languages_treeroot = document.getElementById('available_languages_root'); 
    var visible;
  }

  catch (ex) {
    dump("Failed to initialize variables\n");
  }

  dump("Loading: " + languages_pref_string + "!\n");
  
  try {
	  arrayOfPrefs = languages_pref_string.split(', ');
  } 
  
  catch (ex) {
	  dump("failed to split the preference string!\n");
  }
	
		if (arrayOfPrefs) for (i = 0; i < arrayOfPrefs.length; i++) {

			str = arrayOfPrefs[i];
      tit = GetLanguageTitle(str);
      
			if (str) if (tit) {
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
