/* -*- Mode: Java; tab-width: 4; c-basic-offset: 4; -*-
 * 
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributor(s):
 *   Ben "Count XULula" Goodger <rgoodger@ihug.co.nz>
 *   Syd Logan <syd@netscape.com>
 */

window.doneLoading = false; 
 
/** class PrefWindow ( string frame_id );
 *  - purpose: object representing the state of the prefs window
 *  - in:      string representing the panel content frame
 *  - out:     nothing;
 **/
function PrefWindow( frame_id )
{
  if ( !frame_id )
    throw "Error: frame_id not supplied!";

  // data members
  this.contentFrame   = frame_id
  // chrome folder structure
  this.folder         = new ChromeFolder( "chrome://pref/" );
  // instantiate the WidgetStateManager
  this.wsm            = new WidgetStateManager( frame_id, this.folder.content, ".xul" );
  this.pref           = null;
    
  // member functions
  this.onload         = PREF_onload;
  this.onok = PREF_onok;
  this.AddOnOK = PREF_AddOnOK;
  this.AddOnCancel = PREF_AddOnCancel;
  this.onokUser           = new Array;
  this.okCount = 0;
  this.oncancel = PREF_oncancel;
  this.oncancelUser       = new Array;
  this.cancelCount = 0;
  this.SwitchPage     = PREF_SwitchPage;
  this.onpageload     = PREF_onpageload;
  this.DoSavePrefs    = PREF_DoSavePrefs;
  this.ParsePref      = PREF_ParsePref;
  
  // set up window
  this.onload();
}

/** void onload();
 *  - purpose: startup routine for prefs dialog.
 *  - in:      nothing;
 *  - out:     nothing;
 **/
function PREF_onload()
{
  // set ok/cancel handlers
  doSetOKCancel( this.onok, null );
  // create pref object
  try {
    this.pref = Components.classes["@mozilla.org/preferences;1"].getService();
    if(this.pref)
      this.pref = this.pref.QueryInterface( Components.interfaces.nsIPref );
  }
  catch(e) {
    dump("*** Failed to create prefs object\n");
    return;
  }
  // this.wsm.PageData.splice(0);    // brutally empty the PageData Array
  if( window.queuedTag )
    this.onpageload( window.queuedTag );

  // enable image blocker if "imageblocker.enabled" pref is true
  try {
    if (this.pref.GetBoolPref("imageblocker.enabled")) {
      element = document.getElementById("cookiesCell");
      valueWithImages = element.getAttribute("valueWithImages");
      element.setAttribute("value",valueWithImages);
    }
  } catch(e) {
    dump("imageblocker.enabled pref is missing from all.js");
  }

}

function PREF_onok()
{
	for ( var i = 0; i < window.handle.okCount; i++ )
		window.handle.onokUser[i]();
	PREF_onokprivate();
}

function PREF_oncancel()
{
	for ( var i = 0; i < window.handle.cancelCount; i++ )
		window.handle.oncancelUser[i]();
	PREF_oncancelprivate();
}

/** void onok();
 *  - purpose: save all pref panel data and quit pref dialog
 *  - in:      nothing;
 *  - out:     nothing;
 **/
function PREF_onokprivate()
{
  var url = document.getElementById( window.handle.contentFrame ).getAttribute("src");
  var tag = window.handle.wsm.GetTagFromURL( url, window.handle.folder.content, ".xul" );
  var extras  = ["pref", "preftype", "prefstring", "prefindex"];
  window.handle.wsm.SavePageData( tag, extras, false, false );      // save data from the current page. 
  window.handle.DoSavePrefs();
  window.opener.prefWindow = 0;
  window.close();
}

function PREF_oncancelprivate()
{
  window.opener.prefWindow = 0;
  window.close();
}

function PREF_AddOnOK( func )
{
  this.onokUser[this.okCount] = func;
  this.okCount += 1;
}

function PREF_AddOnCancel( func )
{
  this.oncancelUser[this.cancelCount] = func;
  this.cancelCount += 1;
}

function PREF_DoSavePrefs()
{
  for( var i in window.handle.wsm.PageData ) {
    for ( var k in window.handle.wsm.PageData[i] ) {
      window.handle.ParsePref( k, window.handle.wsm.PageData[i][k] );
    }
  }
  // actually save the prefs file. 
  this.pref.SavePrefFile();
}

function PREF_ParsePref( elementID, elementObject )
{
  var whp = window.handle.pref; // shortcut
  
  // since we no longer use the convoluted id strings, we now use the id passed
  // in as an argument to this function to grab the appropriate element in the
  if( elementObject.pref ) {
    switch( elementObject.preftype ) 
    {
      case "bool":
        if( elementObject.prefindex === "true" || elementObject.prefindex === "false" ) {
          // bool prefs with a prefindex, e.g. boolean radios. the value we set
          // is actually the prefindex, not the checked value. 
          if( elementObject.value == true ) {
            if( typeof( elementObject.prefindex ) == "string" ) {
              if( elementObject.prefindex == "true" ) 
                elementObject.prefindex = true;
              else
                elementObject.prefindex = false;
            }
            // only set a pref if it is different to the current value. 
            try { 
              var oldPref = whp.GetBoolPref( elementObject.prefstring )
            } 
            catch(e) {
              var oldPref = false;
            }
            if( oldPref != elementObject.prefindex ) {
              dump("** set bool pref " + elementObject.prefstring + " to " + elementObject.prefindex + "\n");
              whp.SetBoolPref( elementObject.prefstring, elementObject.prefindex );  // bool pref
            }
            break;
          }
        }
        try { 
          var oldPref = whp.GetBoolPref( elementObject.prefstring )
        } 
        catch(e) {
          var oldPref = false;
        }
        if( oldPref != elementObject.prefindex ) {
          dump("*** set bool pref " + elementObject.prefstring + " to " + elementObject.prefindex + "\n");
          whp.SetBoolPref( elementObject.prefstring, elementObject.value );  // bool pref
        }
        break;

      case "int":
        dump("*** elementObject.elType = " + elementObject.elType + "\n");

        if( elementObject.elType.toLowerCase() == "radio" && !elementObject.value )
          return false; // ignore, go to next
        if( !elementObject.prefindex ) {
          // element object does not have a prefindex (e.g. select widgets)
          var prefvalue = elementObject.value;
        }
        else {
          // element object has prefindex attribute (e.g. radio buttons)
          var prefvalue = elementObject.prefindex;
        }
        try {
          var oldPref = whp.GetIntPref( elementObject.prefstring );
        }
        catch(e) {
          var oldPref = 0;  // no default pref found, default int prefs to zero
        }
        dump("*** going to set int pref: " + elementObject.prefstring + " = " + prefvalue + "\n");
        if( oldPref != prefvalue ) 
          whp.SetIntPref( elementObject.prefstring, prefvalue );  // integer pref
        break;

      case "string":
      case "color":
        dump("*** " + elementObject.prefstring + "\n");
        try {
          var charPref = whp.CopyUnicharPref( elementObject.prefstring );
        }
        catch(e) {
          var charPref = "";
        }
        // we cannot set a color pref to "" or else all hell will break loose.
        // we must REFUSE to set this pref if there is an empty string.
        if( elementObject.preftype == "color" && charPref == "" ) {
          dump("*** empty string color pref was going to be set, but we're avoiding this just in the nick of time...\n");
          break;
        }
        if( elementObject.elType.toLowerCase() == "radio") {
            if  (!elementObject.value) {
                return false; // ignore, go to next
            }
            else {
                var prefvalue = elementObject.prefindex; // element object has prefindex attribute (e.g. radio buttons)
            }
        }
        else {
          var prefvalue = elementObject.value; // element object does not have a prefindex (e.g. select widgets)
        }
        
        if( charPref != prefvalue )  { // do we care about whitespace? 
          try {
          whp.SetUnicharPref( elementObject.prefstring, prefvalue );  // string pref
          }
          catch (ex) {
            dump("*** failure when calling SetUnicharPref: " + ex + "\n");
          }
        }
        break;
      
      default:
        // TODO: insert implementation for other pref types;
        break;
    }
  }
}

/** void SwitchPage ( DOMElement node );
 *  - purpose: switches the page in the content frame to that specified in the
 *  -          tag attribute of the selected treecell
 *  - in:      DOMElement representing the clicked node in the panel tree
 *  - out:     nothing;
 **/
function PREF_SwitchPage()
{
  var prefPanelTree = document.getElementById( "prefsTree" );
  var selectedItem = prefPanelTree.selectedItems[0];

  var url = document.getElementById( this.contentFrame ).getAttribute("src");
  var oldTag = this.wsm.GetTagFromURL( url, "/", ".xul", false );
  // extra attributes to be saved
  var extras  = ["pref", "preftype", "prefstring", "prefindex"];
  dump("*** going to save page data for tag: " + oldTag + "\n");
  this.wsm.SavePageData( oldTag, extras, false, false );      // save data from the current page. 

  var newURL = selectedItem.firstChild.firstChild.getAttribute("url");
  dump("*** newURL: " + newURL + "\n");
  var newTag = this.wsm.GetTagFromURL( newURL, "/", ".xul", false );
  // if the page clicked is the same one, don't bother reloading it.
  if( newTag != oldTag )
    document.getElementById( this.contentFrame ).setAttribute( "src", newURL );
  else 
    return;
}

/** void onpageload ( string tag );
 *  - purpose: sets page data on the page which has just loaded
 *  - in:      string tag representing filename minus extension
 *  - out:     nothing;
 **/
function PREF_onpageload( tag )
{
  dump("*** PREF_onpageload('" + tag + "');\n");

  // remove wallet functions (unless overruled by the "wallet.enabled" pref)
  try {
    if (!this.pref.GetBoolPref("wallet.enabled")) {
      var element;
      if (tag == "pref-wallet") {
        element = window.frames[this.contentFrame].document.getElementById("walletArea");
        element.setAttribute("style","display: none;" );
      }
    }
  } catch(e) {
    dump("wallet.enabled pref is missing from all.js");
  }

  // enable image blocker if "imageblocker.enabled" pref is true
  try {
    if (this.pref.GetBoolPref("imageblocker.enabled")) {
      var element;
      if (tag == "pref-cookies") {
        element = window.frames[this.contentFrame].document.getElementById("imagesArea");
        element.setAttribute("style","display: inline;" );
        element = window.frames[this.contentFrame].document.getElementById("cookieHeader");
        titleWithImages = element.getAttribute("titleWithImages");
        element.setAttribute("title",titleWithImages);
        descriptionWithImages = element.getAttribute("descriptionWithImages");
        element.setAttribute
          ("description",descriptionWithImages );
        element = window.frames[this.contentFrame].document.getElementById("cookieWindow");
        titleWithImages = element.getAttribute("titleWithImages");
        element.setAttribute("title",titleWithImages);
      }
    }
  } catch(e) {
    dump("imageblocker.enabled pref is missing from all.js");
  }

  if( !this.wsm.PageData[tag] ) {
    // there is no entry in the data hash for this page, so we need to initialise
    // the form values from existing prefs. The best way to do this is to pack
    // the PageData table for this element with values from prefs, rather than
    // setting attributes here because we can then leverage the setting ability
    // of wsm.SetPageData
    
    // step one: get all the elements with pref ids. (this may change to get
    // elements with some other pref attributes - likely)
    // yes I know this is an evil function to use, sue me. 
    prefElements = window.frames[this.contentFrame].document.getElementsByAttribute( "pref", "true" );

    this.wsm.PageData[tag] = [];   
    for( var i = 0; i < prefElements.length; i++ )
    {
      var prefstring  = prefElements[i].getAttribute("prefstring");
      var prefid      = prefElements[i].getAttribute("id");
      var preftype    = prefElements[i].getAttribute("preftype");
      var prefdefval  = prefElements[i].getAttribute("prefdefval");
      switch( preftype ) {
        case "bool":
          try {
            var prefvalue = this.pref.GetBoolPref( prefstring );
            dump("*** user prefs\n");
          }
          catch(e) {
            // radio elements need to have at least one element be set.
            dump("*** failed in default prefs\n")
            if( prefElements[i].nodeName.toLowerCase() == "radio" )
              dump("*** NO DEFAULT PREF for " + prefstring + ", and one IS needed for radio items.\n");
            continue;
          }
          break;
        case "int":
          try {
            var prefvalue = this.pref.GetIntPref( prefstring );
          }
          catch(e) {
            // radio elements need at least one value to be set.
            if( prefElements[i].nodeName.toLowerCase() == "radio" )
              dump("*** NO DEFAULT PREF for " + prefstring + ", and one IS needed for radio items.\n");
            continue;
          }
          break;
        case "string":
        case "color":
          try {
            var prefvalue = this.pref.CopyUnicharPref( prefstring );
          } 
          catch(e) {
            continue;
          }
          break;
        default:
          // TODO: define cases for other pref types if required
          break;
      }
      this.wsm.PageData[tag][prefid]  = [];   
      var root                        = this.wsm.PageData[tag][prefid];
      root["id"]                      = prefid;
      if( prefElements[i].type.toLowerCase() == "radio") {
        // radio elements are a special case - we have a value in the prefs for
        // only one of however many there are, and in thsi case "value" represents
        // the index of the one that we want to check, not an actual settable value.
        // here is where the "prefindex" attribute on such elements comes into
        // play. We compare this with the value read from nsIPref, and if it matches,
        // that is the radioelement that needs to be checked.
        var prefindex = prefElements[i].getAttribute("prefindex");
        if( prefElements[i].getAttribute("preftype").toLowerCase() == "bool" )
          prefvalue = prefvalue.toString(); // need to explicitly type convert on bool prefs.
        
        dump("*** prefindex = " + prefindex + "; prefvalue = " + prefvalue + "\n");
        if( prefindex == prefvalue ) 
          prefvalue = true;     // match! this element is checked!
        else 
          prefvalue = false;    // no match. This element.checked = false;
        dump("*** prefvalue = " + prefvalue + "\n");
      }
      root["value"]     = prefvalue;
    }
  } 
  this.wsm.SetPageData( tag, false );  // do not set extra elements, accept hard coded defaults

  // run startup function on panel if one is available.
  if( window.frames[ this.contentFrame ].Startup )
    window.frames[ this.contentFrame ].Startup();
}

/** class ChromeFolder ( string root );
 *  - purpose: a general object to provide easy access to chrome folders 
 *  - in:      string representing root, e.g. "chrome://bookmarks/"
 *  - out:     nothing;
 **/
function ChromeFolder( root )
{
  if( root.charAt(root.length-1) != "/" )
    root += "/";
  this.content  = root + "content/";
  this.skin     = root + "skin/";
  this.locale   = root + "locale/";
}
