//GLOBALS

//pref service and pref string
var prefInt = parent.hPrefWindow.pref;  

//locale bundles
var regionsBundle;
var languagesBundle;
var acceptedBundle;

//dictionary of all supported locales
var availLanguageDict;

//XUL tree handles
var available_languages;
var available_languages_treeroot;

//XUL tree handles
var active_languages;
var active_languages_treeroot;

//XUL window pref window interface object
var pref_string = new String();

function GetBundles()
{
  if (!regionsBundle)    regionsBundle   = srGetStrBundle("chrome://global/locale/regionNames.properties"); 
  if (!languagesBundle)  languagesBundle = srGetStrBundle("chrome://global/locale/languageNames.properties"); 
  if (!acceptedBundle)   acceptedBundle  = srGetStrBundle("resource:/res/language.properties"); 
}


function Init()
{
  doSetOKCancel(AddAvailableLanguage);
  
  dump("********** Init()\n");    

  try {
    GetBundles()
	  ReadAvailableLanguages();
  }

  catch(ex) {
    dump("*** Couldn't get string bundles\n");
  }

  if (!window.arguments) {
    
    //no caller arguments - load base window       
    
    try {
      active_languages              = document.getElementById('active_languages'); 
      active_languages_treeroot     = document.getElementById('active_languages_root'); 
      prefwindow_proxy_object       = document.getElementById('intlAcceptLanguages');
    } //try

    catch(ex) {
      dump("*** Couldn't get XUL element handles\n");
    } //catch

    try {
      parent.initPanel('chrome://communicator/content/pref/pref-languages.xul');
    }

    catch(ex) {

      dump("*** Couldn't initialize page switcher and pref loader.\n");
      //pref service backup

    } //catch

    try {
        pref_string = parent.hPrefWindow.getPref( "string", "intl.accept_languages");
        //fall-back (hard-coded for Beta2, need to move to XUL/DTD)
        if (!pref_string) pref_string = "en";
    } //try

    catch(ex) {
      dump("*** Couldn't read pref string\n");
    } //catch

    try {
      dump("*** Language PrefString: " + pref_string + "\n");
    } //try

    catch(ex) {
      dump("*** Pref object doesn't exist\n");
    } //catch

    LoadActiveLanguages();

  } else {

    //load available languages popup
    try {

      //add language popup
      available_languages 			    = document.getElementById('available_languages'); 
      available_languages_treeroot  = document.getElementById('available_languages_root');
      active_languages		          = window.opener.document.getElementById('active_languages'); 
      active_languages_treeroot     = window.opener.document.getElementById('active_languages_root'); 
      pref_string                   = window.opener.document.getElementById('intlAcceptLanguages').value;

    } //try

    catch(ex) {
      dump("*** Couldn't get XUL element handles\n");
    } //catch

    try {
      dump("*** AddAvailable Language PrefString: " + pref_string + "\n");
    } //try

    catch(ex) {
      dump("*** Pref object doesn't exist\n");
    } //catch


	  LoadAvailableLanguages();

  }
}


function AddLanguage() 
{
    dump("********** AddLanguage()\n");    
    //cludge: make pref string available from the popup
    document.getElementById('intlAcceptLanguages').value = pref_string;
    window.openDialog("chrome://communicator/content/pref/pref-languages-add.xul","","modal=yes,chrome,resizable=no", "addlangwindow");
    UpdateSavePrefString();

}


function ReadAvailableLanguages()
{

    availLanguageDict		= new Array();
    var visible = new String();
    var str = new String();
    var i =0;

    acceptedBundleEnum = acceptedBundle.getSimpleEnumeration(); 

    while (acceptedBundleEnum.hasMoreElements()) { 			

       //progress through the bundle 
       curItem = acceptedBundleEnum.getNext(); 

       //"unpack" the item, nsIPropertyElement is now partially scriptable 
       curItem = curItem.QueryInterface(Components.interfaces.nsIPropertyElement); 

       //dump string name (key) 
       var stringName = curItem.getKey(); 
       stringNameProperty = stringName.split('.');

       if (stringNameProperty[1] == 'accept') {

          //dump the UI string (value) 
           visible   = curItem.getValue(); 

          //if (visible == 'true') {

             str = stringNameProperty[0];
             stringLangRegion = stringNameProperty[0].split('-');
                     
             if (stringLangRegion[0]) {

                 try {   
                    var tit = languagesBundle.GetStringFromName(stringLangRegion[0]);
                 }
          
                 catch (ex) {
                    dump("No language string for:" + stringLangRegion[1] + "\n");
                 }


                 if (stringLangRegion[1]) {
                 
                   try {   
                    var tit = tit + "/" + regionsBundle.GetStringFromName(stringLangRegion[1]);
                   }
               
                   catch (ex) {
                      dump("No region string for:" + stringLangRegion[1] + "\n");
                   }

                 } //if region

                 tit = tit + "  [" + str + "]";

             } //if language
				    
  		       if (str) if (tit) {
                
				        availLanguageDict[i] = new Array(3);
				        availLanguageDict[i][0]	= tit;	
				        availLanguageDict[i][1]	= str;
				        availLanguageDict[i][2]	= visible;
                i++;

				        if (tit) {}
				        else dump('Not label for :' + str + ', ' + tit+'\n');
			        
             } // if str&tit
            //} //if visible
			    } //if accepted
		    } //while

		availLanguageDict.sort();
}


function LoadAvailableLanguages()
{

  dump("Loading available languages!\n");
  
		if (availLanguageDict) for (i = 0; i < availLanguageDict.length; i++) {

  		if (availLanguageDict[i][2] == 'true') { 	

        AddTreeItem(document, available_languages_treeroot, availLanguageDict[i][1], availLanguageDict[i][0]);
   
      } //if

		} //for
}


function LoadActiveLanguages()
{

  dump("LoadActiveLanguages, PrefString: " + pref_string + "\n");

  try {
	  arrayOfPrefs = pref_string.split(', ');
  } 
  
  catch (ex) {
	  dump("failed to split the preference string!\n");
  }
	
		if (arrayOfPrefs) for (i = 0; i < arrayOfPrefs.length; i++) {

			str = arrayOfPrefs[i];
      tit = GetLanguageTitle(str);
      
			if (str) {

        if (!tit) 
           tit = '[' + str + ']';
        AddTreeItem(document, active_languages_treeroot, str, tit);

			} //if 
		} //for
}


function LangAlreadyActive(langId)
{

  dump("*** LangAlreadyActive, PrefString: " + pref_string + "\n");

  var found = false;
  try {
	  arrayOfPrefs = pref_string.split(', ');

		if (arrayOfPrefs) for (i = 0; i < arrayOfPrefs.length; i++) {
      if (arrayOfPrefs[i] == langId) {
         found = true;
         break;
      }
    }

    return found;
  }

  catch(ex){
     return false;
  }
}


function AddAvailableLanguage()
{
  var Languagename = new String();
	var Languageid = new String();


  //selected languages
  for (var nodeIndex=0; nodeIndex < available_languages.selectedItems.length; nodeIndex++) {
  
    var selItem =  available_languages.selectedItems[nodeIndex];
    var selRow  =  selItem.firstChild;
    var selCell =  selRow.firstChild;

	  Languagename		= selCell.getAttribute('value');
	  Languageid	    = selCell.getAttribute('id');
	  already_active	= false;	
    
    if (!LangAlreadyActive(Languageid)) {

      AddTreeItem(window.opener.document, active_languages_treeroot, Languageid, Languagename);

	  }//if

  } //loop selected languages


  //user-defined languages
  var otherField = document.getElementById( "languages.other" );
  
  if (otherField.value) {

    dump("Other field: " + otherField.value + "\n");
	  
    Languageid	    = otherField.value;
    Languageid      = Languageid.toLowerCase();
	  Languagename		= GetLanguageTitle(Languageid);
    if (!Languagename)
       Languagename = '[' + Languageid + ']';
	  already_active	= false;	
  
    if (!LangAlreadyActive(Languageid)) {

      AddTreeItem(window.opener.document, active_languages_treeroot, Languageid, Languagename);

	  }//if
  }

  available_languages.clearItemSelection();
  window.close();

} //AddAvailableLanguage


function RemoveActiveLanguage()
{

  var nextNode = null;
  var numSelected = active_languages.selectedItems.length;
  var deleted_all = false;

  while (active_languages.selectedItems.length > 0) {
    
	var selectedNode = active_languages.selectedItems[0];
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
    active_languages.selectItem(nextNode)
  } else {
    //active_languages.clearItemSelection();
  }

  UpdateSavePrefString();

} //RemoveActiveLanguage


function GetLanguageTitle(id) 
{
	
		if (availLanguageDict) for (j = 0; j < availLanguageDict.length; j++) {

			if ( availLanguageDict[j][1] == id) {	
				//title = 
				dump("found title for:" + id + " ==> " + availLanguageDict[j][0] + "\n");
				return availLanguageDict[j][0];
			}	
		}
		return '';
}


function AddTreeItem(doc, treeRoot, langID, langTitle)
{
	try {  //let's beef up our error handling for languages without label / title

			// Create a treerow for the new Language
			var item = doc.createElement('treeitem');
			var row  = doc.createElement('treerow');
			var cell = doc.createElement('treecell');

			// Copy over the attributes
			cell.setAttribute('value', langTitle);
			cell.setAttribute('id', langID);

			// Add it to the active languages tree
			item.appendChild(row);
			row.appendChild(cell);

			treeRoot.appendChild(item);
			dump("*** Added tree item: " + langTitle + "\n");

	} //try

	catch (ex) {
		dump("*** Failed to add item: " + langTitle + "\n");
	} //catch 

}


function UpdateSavePrefString()
{
  var num_languages = 0;
  pref_string = null;

  dump("*** UpdateSavePrefString()\n");

  for (var item = active_languages_treeroot.firstChild; item != null; item = item.nextSibling) {

    row  =  item.firstChild;
    cell =  row.firstChild;
    languageid = cell.getAttribute('id');

	  if (languageid.length > 1) {
	  
          num_languages++;

		  //separate >1 languages by commas
		  if (num_languages > 1) {
			  pref_string = pref_string + "," + " " + languageid;
		  } else {
			  pref_string = languageid;
		  } //if
	  } //if
  }//for

  dump("*** Pref string set to: " + pref_string + "\n");
  parent.hPrefWindow.setPref( "string", "intl.accept_languages", pref_string );
  //Save();

}

function Save()
{

  dump('*** Save()\n');

  try
	{
    var prefInt = null;

    if (!prefInt) {
		
        prefInt = Components.classes["@mozilla.org/preferences;1"];

		    if (prefInt) {
			    prefInt = prefInt.getService();
			    prefInt = prefInt.QueryInterface(Components.interfaces.nsIPref);
		    }
    }

		if (prefInt)
		{
			prefInt.SetCharPref("intl.accept_languages", pref_string);
			dump('*** saved pref: ' + pref_string + '.\n');
		}
	}

	catch(ex)
	{
		dump("*** Couldn't save!\n");
	}

} //Save


function MoveUp() {

  if (active_languages.selectedItems.length == 1) {
    var selected = active_languages.selectedItems[0];
    var before = selected.previousSibling
    if (before) {
      before.parentNode.insertBefore(selected, before);
      active_languages.selectItem(selected);
      active_languages.ensureElementIsVisible(selected);
    }
  }

  UpdateSavePrefString();

} //MoveUp

  
function MoveDown() {

  if (active_languages.selectedItems.length == 1) {
    var selected = active_languages.selectedItems[0];
    if (selected.nextSibling) {
      if (selected.nextSibling.nextSibling) {
        selected.parentNode.insertBefore(selected, selected.nextSibling.nextSibling);
      }
      else {
        selected.parentNode.appendChild(selected);
      }
      active_languages.selectItem(selected);
    }
  }

  UpdateSavePrefString();

} //MoveDown
