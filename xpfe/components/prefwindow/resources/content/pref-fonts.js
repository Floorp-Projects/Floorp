/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Gervase Markham <gerv@gerv.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

try
  {
    var fontList = Components.classes["@mozilla.org/gfx/fontlist;1"].createInstance();
    if (fontList)
      fontList = fontList.QueryInterface(Components.interfaces.nsIFontList);

    var pref = Components.classes["@mozilla.org/preferences;1"].getService( Components.interfaces.nsIPref );
  }
catch(e)
  {
    dump("failed to get font list or pref object: "+e+" in pref-fonts.js\n");
  }

var fontTypes   = ["serif", "sans-serif", "cursive", "fantasy", "monospace"];
var variableSize, fixedSize, languageList;
var languageData = [];
var currentLanguage;
var gPrefutilitiesBundle;

// manual data retrieval function for PrefWindow
function GetFields()
  {
    var dataObject = parent.hPrefWindow.wsm.dataManager.pageData["chrome://communicator/content/pref/pref-fonts.xul"];

    // store data for language independent widgets
    var lists = ["selectLangs", "proportionalFont"];
    for( var i = 0; i < lists.length; i++ )
      {
        if( !dataObject.dataEls )
          dataObject.dataEls = [];
        dataObject.dataEls[ lists[i] ] = [];
        dataObject.dataEls[ lists[i] ].value = document.getElementById( lists[i] ).value;
      }

   dataObject.defaultFont = document.getElementById( "proportionalFont" ).value;
   dataObject.fontDPI = document.getElementById( "screenResolution" ).value;
   dataObject.useDocFonts = document.getElementById( "browserUseDocumentFonts" ).checked ? 1 : 0;

    // save current state for language dependent fields and store
    saveState();
    dataObject.languageData = languageData;

    return dataObject;
  }

// manual data setting function for PrefWindow
function SetFields( aDataObject )
  {
    languageData = aDataObject.languageData ? aDataObject.languageData : languageData ;
    currentLanguage = aDataObject.currentLanguage ? aDataObject.currentLanguage : null ;

    var lists = ["selectLangs", "proportionalFont"];
    var prefvalue;
    
    for( var i = 0; i < lists.length; i++ )
      {
        var element = document.getElementById( lists[i] );
        if( aDataObject.dataEls )
          {
            element.value = aDataObject.dataEls[ lists[i] ].value;
            element.selectedItem = element.getElementsByAttribute( "value", aDataObject.dataEls[ lists[i] ].value )[0];
          }
        else
          {
            var prefstring = element.getAttribute( "prefstring" );
            var preftype = element.getAttribute( "preftype" );
            if( prefstring && preftype )
              {
                var prefvalue = parent.hPrefWindow.getPref( preftype, prefstring );
                element.value = prefvalue;
                element.selectedItem = element.getElementsByAttribute( "value", prefvalue )[0];
              }
          }
      }

    var screenResolution = document.getElementById( "screenResolution" );
    var resolution;
    
    if( aDataObject.fontDPI )
      {
        resolution = aDataObject.fontDPI;
      }
    else
      {
        prefvalue = parent.hPrefWindow.getPref( "int", "browser.display.screen_resolution" );
        if( prefvalue != "!/!ERROR_UNDEFINED_PREF!/!" )
            resolution = prefvalue;
        else
            resolution = 96; // If it all goes horribly wrong, fall back on 96.
      }
    
    setResolution( resolution );
    
    if ( parent.hPrefWindow.getPrefIsLocked( "browser.display.screen_resolution" ) ) {
        screenResolution.disabled = true;
    }

    var useDocFontsCheckbox = document.getElementById( "browserUseDocumentFonts" );
    if( aDataObject.useDocFonts != undefined )
      useDocFontsCheckbox.checked = aDataObject.useDocFonts ? true : false;
    else
      {
        prefvalue = parent.hPrefWindow.getPref( "int", "browser.display.use_document_fonts" );
        if( prefvalue != "!/!ERROR_UNDEFINED_PREF!/!" )
          useDocFontsCheckbox.checked = prefvalue ? true : false ;
      }
    if ( parent.hPrefWindow.getPrefIsLocked( "browser.display.use_document_fonts" ) ) {
        useDocFontsCheckbox.disabled = true;
    }
  }

function Startup()
  {
    variableSize = document.getElementById( "sizeVar" );
    fixedSize    = document.getElementById( "sizeMono" );
    languageList = document.getElementById( "selectLangs" );

    gPrefutilitiesBundle = document.getElementById("bundle_prefutilities");

    // register our ok callback function
    parent.hPrefWindow.registerOKCallbackFunc( saveFontPrefs );

    // eventually we should detect the default language and select it by default
    selectLanguage();
    
    // Allow user to ask the OS for a DPI if we are under X
    if (navigator.appVersion.indexOf("X11") != -1)
      {
         document.getElementById( "systemResolution" ).removeAttribute( "hidden" ); 
      }
    
    // Set up the labels for the standard issue resolutions
    var resolution;
    resolution = document.getElementById( "screenResolution" );
    var dpi = resolution.getAttribute( "dpi" );
    resolution = document.getElementById( "otherResolution" );
    resolution.setAttribute( "value", "72" );
    resolution.setAttribute( "label", dpi.replace(/\$val/, "72" ) );
    resolution = document.getElementById( "defaultResolution" );
    resolution.setAttribute( "value", "96" );
    resolution.setAttribute( "label", dpi.replace(/\$val/, "96" ) );

    // This prefstring is a contrived pref whose sole purpose is to lock some
    // elements in this panel.  The value of the pref is not used and does not matter.
    if ( parent.hPrefWindow.getPrefIsLocked( "browser.display.languageList" ) ) {
      disableAllFontElements();
    }
  }

function listElement( aListID )
  {
    this.listElement = document.getElementById( aListID );
  }

listElement.prototype =
  {
    clearList:
      function ()
        {
          // remove the menupopup node child of the menulist.
          this.listElement.removeChild( this.listElement.firstChild );
        },

    appendString:
      function ( aString )
        {
          var menuItemNode = document.createElement( "menuitem" );
          if( menuItemNode )
            {
              menuItemNode.setAttribute( "label", aString );
              this.listElement.firstChild.appendChild( menuItemNode );
            }
        },

    appendFontNames: 
      function ( aDataObject ) 
        { 
          var popupNode = document.createElement( "menupopup" ); 
          var strDefaultFontFace = "";
          var fontName;
          while (aDataObject.hasMoreElements()) {
            fontName = aDataObject.getNext();
            fontName = fontName.QueryInterface(Components.interfaces.nsISupportsWString);
            var fontNameStr = fontName.toString();
            if (strDefaultFontFace == "")
              strDefaultFontFace = fontNameStr;
            var itemNode = document.createElement( "menuitem" );
            itemNode.setAttribute( "value", fontNameStr );
            itemNode.setAttribute( "label", fontNameStr );
            popupNode.appendChild( itemNode );
          }
          if (strDefaultFontFace != "") {
            this.listElement.removeAttribute( "disabled" );
          } else {
            this.listElement.setAttribute( "value", strDefaultFontFace );
            this.listElement.setAttribute( "label",
                                    gPrefutilitiesBundle.getString("nofontsforlang") );
            this.listElement.setAttribute( "disabled", "true" );
          }
          this.listElement.appendChild( popupNode ); 
          return strDefaultFontFace;
        } 
  };

function saveFontPrefs()
  {
    // if saveState function is available, assume can call it.
    // why is this extra qualification required?!!!!
    if( top.hPrefWindow.wsm.contentArea.saveState )
      {
        saveState();
        parent.hPrefWindow.wsm.dataManager.pageData["chrome://communicator/content/pref/pref-fonts.xul"] = GetFields();
      }

    // saving font prefs
    var dataObject = parent.hPrefWindow.wsm.dataManager.pageData["chrome://communicator/content/pref/pref-fonts.xul"];
    var pref = parent.hPrefWindow.pref;
    for( var language in dataObject.languageData )
      {
        for( var type in dataObject.languageData[language].types )
          {
            var fontPrefString = "font.name." + type + "." + language;
            var currValue = "";
            try
              {
                currValue = pref.CopyUnicharPref( fontPrefString );
              }
            catch(e)
              {
              }
            if( currValue != dataObject.languageData[language].types[type] )
              pref.SetUnicharPref( fontPrefString, dataObject.languageData[language].types[type] );
          }
        var variableSizePref = "font.size.variable." + language;
        var fixedSizePref = "font.size.fixed." + language;
        var currVariableSize = 12, currFixedSize = 12;
        try
          {
            currVariableSize = pref.GetIntPref( variableSizePref );
            currFixedSize = pref.GetIntPref( fixedSizePref );
          }
        catch(e)
          {
          }
        if( currVariableSize != dataObject.languageData[language].variableSize )
          pref.SetIntPref( variableSizePref, dataObject.languageData[language].variableSize );
        if( currFixedSize != dataObject.languageData[language].fixedSize )
          pref.SetIntPref( fixedSizePref, dataObject.languageData[language].fixedSize );
      }

    // font scaling
    var fontDPI       = parseInt( dataObject.fontDPI );
    var documentFonts = dataObject.useDocFonts;
    var defaultFont   = dataObject.defaultFont;

    try
      {
        var currDPI = pref.GetIntPref( "browser.display.screen_resolution" );
        var currFonts = pref.GetIntPref( "browser.display.use_document_fonts" );
        var currDefault = pref.CopyUnicharPref( "font.default" );
      }
    catch(e)
      {
      }
    if( currDPI != fontDPI )
      pref.SetIntPref( "browser.display.screen_resolution", fontDPI );
    if( currFonts != documentFonts )
      pref.SetIntPref( "browser.display.use_document_fonts", documentFonts );
    if( currDefault != defaultFont )
      {
        pref.SetUnicharPref( "font.default", defaultFont );
      }
  }

function saveState()
  {
    for( var i = 0; i < fontTypes.length; i++ )
      {
        // preliminary initialisation
        if( currentLanguage && !languageData[currentLanguage] )
          languageData[currentLanguage] = [];
        if( currentLanguage && !languageData[currentLanguage].types )
          languageData[currentLanguage].types = [];
        // save data for the previous language
        if( currentLanguage && languageData[currentLanguage] &&
            languageData[currentLanguage].types )
          languageData[currentLanguage].types[fontTypes[i]] = document.getElementById( fontTypes[i] ).value;
      }

    if( currentLanguage && languageData[currentLanguage] &&
        languageData[currentLanguage].types )
      {
        languageData[currentLanguage].variableSize = parseInt( variableSize.value );
        languageData[currentLanguage].fixedSize = parseInt( fixedSize.value );
      }
  }

function selectLanguage()
  {
    // save current state
    saveState();

    if( !currentLanguage )
      currentLanguage = languageList.value;

    for( var i = 0; i < fontTypes.length; i++ )
      {
        // build and populate the font list for the newly chosen font type
        var fontEnumerator = fontList.availableFonts(languageList.value, fontTypes[i]);
        var selectElement = new listElement( fontTypes[i] );
        selectElement.clearList();
        var strDefaultFontFace = selectElement.appendFontNames(fontEnumerator);
        //the first font face name returned by the enumerator is our last resort
        var defaultListSelection = selectElement.listElement.getElementsByAttribute( "value", strDefaultFontFace)[0];

        //fall-back initialization values (first font face list entry)
        var defaultListSelection = strDefaultFontFace ? selectElement.listElement.getElementsByAttribute( "value", strDefaultFontFace)[0] : null;

        if( languageData[languageList.value] )
          {
            // data exists for this language, pre-select items based on this information
            var dataElements = selectElement.listElement.getElementsByAttribute( "value", languageData[languageList.value].types[fontTypes[i]] );
            var selectedItem = dataElements.length ? dataElements[0] : defaultListSelection;

            if (strDefaultFontFace)
              {
                selectElement.listElement.selectedItem = selectedItem;
                variableSize.removeAttribute("disabled");
                fixedSize.removeAttribute("disabled");
                variableSize.selectedItem = variableSize.getElementsByAttribute( "value", languageData[languageList.value].variableSize )[0];
                fixedSize.selectedItem = fixedSize.getElementsByAttribute( "value", languageData[languageList.value].fixedSize )[0];
              }
            else
              {
                variableSize.setAttribute("disabled","true");
                fixedSize.setAttribute("disabled","true");
              }
          }
        else
          {

            if (strDefaultFontFace) {
                //initialze pref panel only if font faces are available for this language family

                var selectVal;
                var selectedItem;
        
                try {
                    var fontPrefString = "font.name." + fontTypes[i] + "." + languageList.value;
                    selectVal   = parent.hPrefWindow.pref.CopyUnicharPref( fontPrefString );
                    var dataEls = selectElement.listElement.getElementsByAttribute( "value", selectVal );
                    selectedItem = dataEls.length ? dataEls[0] : defaultListSelection;
                }

                catch(e) {
                    //always initialize: fall-back to default values
                    selectVal       = strDefaultFontFace;
                    selectedItem    = defaultListSelection;
                }

                selectElement.listElement.value = selectVal;
                selectElement.listElement.selectedItem = selectedItem;

                variableSize.removeAttribute("disabled");
                fixedSize.removeAttribute("disabled");


                try {
                    var variableSizePref = "font.size.variable." + languageList.value;
                    var fixedSizePref = "font.size.fixed." + languageList.value;

                    var sizeVarVal = parent.hPrefWindow.pref.GetIntPref( variableSizePref );
                    var sizeFixedVal = parent.hPrefWindow.pref.GetIntPref( fixedSizePref );

                    variableSize.value = sizeVarVal;
                    variableSize.selectedItem = variableSize.getElementsByAttribute( "value", sizeVarVal )[0];

                    fixedSize.value = sizeFixedVal;
                    fixedSize.selectedItem = fixedSize.getElementsByAttribute( "value", sizeFixedVal )[0];
                }

                catch(e) {
                    //font size lists can simply deafult to the first entry
                }

            }
            else
            {
                //disable otherwise                
                variableSize.setAttribute("disabled","true");
                fixedSize.setAttribute("disabled","true");
            }
          }
      }
    currentLanguage = languageList.value;
  }

function changeScreenResolution()
  {
    var screenResolution = document.getElementById("screenResolution");
    var userResolution = document.getElementById("userResolution");
  
    if (screenResolution.value == "other")
      {
        // If the user selects "Other..." we bring up the calibrate screen dialog
        var rv = { newdpi : 0 };
        calscreen = window.openDialog("chrome://communicator/content/pref/pref-calibrate-screen.xul", 
                                      "_blank", 
                                      "modal,chrome,centerscreen,resizable=no,titlebar",
                                      rv);
        if (rv.newdpi != -1) 
          {
            // They have entered values, and we have a DPI value back
            var dpi = screenResolution.getAttribute( "dpi" );
            setResolution ( rv.newdpi );
          }
        else
          {
            // They've cancelled. We can't leave "Other..." selected, so...
            var defaultResolution = document.getElementById("defaultResolution");
            screenResolution.selectedItem = defaultResolution;
            userResolution.setAttribute("hidden", "true");
          }
      }
    else if (!(screenResolution.value == userResolution.value))
      {
        // User has selected one of the hard-coded resolutions
        userResolution.setAttribute("hidden", "true");
      }
  }

function setResolution( resolution )
  {
    // Given a number, if it's equal to a hard-coded resolution we use that,
    // otherwise we set the userResolution field.
    var screenResolution = document.getElementById( "screenResolution" );
    var userResolution = document.getElementById( "userResolution" );

    var item = screenResolution.getElementsByAttribute( "value", resolution )[0];
    if (item)
      {
        // If it's one of the hard-coded values, we'll select it directly 
        screenResolution.selectedItem = item;
        userResolution.setAttribute( "hidden", "true" );
      }   
    else
      {
        // Otherwise we need to set up the userResolution field
        var dpi = screenResolution.getAttribute( "dpi" );
        userResolution.setAttribute( "value", resolution );
        userResolution.setAttribute( "label", dpi.replace(/\$val/, resolution) );
        userResolution.removeAttribute( "hidden" );
        screenResolution.selectedItem = userResolution;   
      }
  }
  
// "Calibrate screen" dialog code

function Init()
  {
      sizeToContent();
      doSetOKCancel(onOK, onCancel);
      document.getElementById("horizSize").focus();
  }
  
function onOK()
  {
    // Get value from the dialog to work out dpi
    var horizSize = parseFloat(document.getElementById("horizSize").value);
    var units = document.getElementById("units").value;
  
    if (!horizSize || horizSize < 0)
      {
        // We can't calculate anything without a proper value
        window.arguments[0].newdpi = -1;
        return true;
      }
      
    // Convert centimetres to inches.
    // The magic number is allowed because it's a fundamental constant :-)
    if (units === "centimetres")
      {
        horizSize /= 2.54;
      }
  
    // These shouldn't change, but you can't be too careful.
    var horizBarLengthPx = document.getElementById("horizRuler").boxObject.width;
  
    var horizDPI = parseInt(horizBarLengthPx) / horizSize;
  
    // Average the two <shrug>.
    window.arguments[0].newdpi = Math.round(horizDPI);
  
    return true;
  }

function onCancel()
  {
      // We return zero to show that no value has been given.
      window.arguments[0].newdpi = -1;
      return true;
  }

// disable font items, but not the browserUseDocumentFonts checkbox nor the resolution
// menulist
function disableAllFontElements()
  {
      var doc_ids = [ "selectLangs", "proportionalFont",
                      "sizeVar", "serif", "sans-serif",
                      "cursive", "fantasy", "monospace",
                      "sizeMono" ];
      for (i=0; i<doc_ids.length; i++) {
          element = document.getElementById( doc_ids[i] );
          element.disabled = true;
      }
  }

