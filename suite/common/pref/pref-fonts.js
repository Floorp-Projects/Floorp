try
  {
    var enumerator = Components.classes["@mozilla.org/gfx/fontenumerator;1"].createInstance();
    if( enumerator )
      enumerator = enumerator.QueryInterface(Components.interfaces.nsIFontEnumerator);
    var fontCount = { value: 0 }
    fonts = enumerator.EnumerateAllFonts(fontCount);

    var pref = Components.classes["@mozilla.org/preferences;1"].getService( Components.interfaces.nsIPref );
  }
catch(e)
  {
  }

var fontTypes   = ["serif","sans-serif", /*"cursive", "fantasy",*/"monospace"];
var variableSize, fixedSize, languageList;
var languageData = [];
var currentLanguage;
var gPrefutilitiesBundle;

// manual data retrieval function for PrefWindow
function GetFields()
  {
    var dataObject = [];

    // store data for language independent widgets
    var lists = ["selectLangs", "defaultFont"];
    for( var i = 0; i < lists.length; i++ )
      {
        if( !dataObject.dataEls )
          dataObject.dataEls = [];
        dataObject.dataEls[ lists[i] ] = [];
        dataObject.dataEls[ lists[i] ].value = document.getElementById( lists[i] ).value;
      }

    dataObject.defaultFont = document.getElementById( "defaultFont" ).value;
    dataObject.fontDPI = document.getElementById( "browserScreenResolution" ).value;
    dataObject.useDocFonts = document.getElementById( "browserUseDocumentFonts" ).checked ? 0 : 1;

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

    var lists = ["selectLangs", "defaultFont"];
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

    var resolutionField = document.getElementById( "browserScreenResolution" );
    if( aDataObject.fontDPI != undefined )
      resolutionField.value = aDataObject.fontDPI;
    else
      {
        var prefvalue = parent.hPrefWindow.getPref( resolutionField.getAttribute("preftype"), resolutionField.getAttribute("prefstring") );
        if( prefvalue != "!/!ERROR_UNDEFINED_PREF!/!" )
          resolutionField.value = prefvalue;
      }
    var useDocFontsCheckbox = document.getElementById( "browserUseDocumentFonts" );
    if( aDataObject.useDocFonts != undefined )
      useDocFontsCheckbox.checked = aDataObject.useDocFonts ? false : true;
    else
      {
        var prefvalue = parent.hPrefWindow.getPref( useDocFontsCheckbox.getAttribute("preftype"), useDocFontsCheckbox.getAttribute("prefstring") );
        if( prefvalue != "!/!ERROR_UNDEFINED_PREF!/!" )
          useDocFontsCheckbox.checked = prefvalue ? false : true ;
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

    appendStrings:
      function ( aDataObject )
        {
          var popupNode = document.createElement( "menupopup" );
          faces = aDataObject.toString().split(",");
          faces.sort();
          for( var i = 0; i < faces.length; i++ )
            {
              if( faces[i] == "" )
                {
                  this.listElement.setAttribute( "value", faces[i] );
                  this.listElement.setAttribute( "label",
                                                gPrefutilitiesBundle.getString("nofontsforlang") );
                  this.listElement.setAttribute( "disabled", "true" );
                  gNoFontsForThisLang = true; // hack, hack hack!
                }
              else
                {
                  var itemNode = document.createElement( "menuitem" );
                  itemNode.setAttribute( "value", faces[i] );
                  itemNode.setAttribute( "label", faces[i] );
                  this.listElement.removeAttribute( "disabled" );
                  gNoFontsForThisLang = false; // hack, hack hack!
                  popupNode.appendChild( itemNode );
                }
            }
          this.listElement.appendChild( popupNode );
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
        var selectElement = new listElement( fontTypes[i] );
        selectElement.clearList();
        selectElement.appendStrings( enumerator.EnumerateFonts( languageList.value, fontTypes[i], fontCount ) );

        if( languageData[languageList.value] )
          {
            // data exists for this language, pre-select items based on this information
            var dataElements = selectElement.listElement.getElementsByAttribute( "value", languageData[languageList.value].types[fontTypes[i]] );
            var selectedItem = dataElements.length ? dataElements[0] : null;
            if (!gNoFontsForThisLang)
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
            try
              {
                var fontPrefString = "font.name." + fontTypes[i] + "." + languageList.value;
                var selectVal = parent.hPrefWindow.pref.CopyUnicharPref( fontPrefString );
                var dataEls = selectElement.listElement.getElementsByAttribute( "value", selectVal );
                selectedItem = dataEls.length ? dataEls[0] : null;
                if (selectedItem)
                  {
                    selectElement.listElement.value = selectVal;
                    selectElement.listElement.selectedItem = selectedItem;
                    var variableSizePref = "font.size.variable." + languageList.value;
                    var fixedSizePref = "font.size.fixed." + languageList.value;
                    var sizeVarVal = parent.hPrefWindow.pref.GetIntPref( variableSizePref );
                    var sizeFixedVal = parent.hPrefWindow.pref.GetIntPref( fixedSizePref );
                    variableSize.removeAttribute("disabled");
                    variableSize.value = sizeVarVal;
                    variableSize.selectedItem = variableSize.getElementsByAttribute( "value", sizeVarVal )[0];
                    fixedSize.removeAttribute("disabled");
                    fixedSize.value = sizeFixedVal;
                    fixedSize.selectedItem = fixedSize.getElementsByAttribute( "value", sizeFixedVal )[0];
                  }
                else
                  {
                    variableSize.setAttribute("disabled","true");
                    fixedSize.setAttribute("disabled","true");
                  }
              }
            catch(e)
              {
                // if we don't find an existing pref, it's no big deal.
              }
          }
      }
    currentLanguage = languageList.value;
  }

