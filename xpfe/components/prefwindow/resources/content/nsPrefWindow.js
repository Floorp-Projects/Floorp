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
 *   Ben "Count XULula" Goodger <ben@netscape.com>
 */

const _DEBUG = false; 
 
/** PrefWindow IV
 *  =============
 *  This is a general page switcher and pref loader.
 *  =>> CHANGES MUST BE REVIEWED BY ben@netscape.com!! <<=
 **/ 

var queuedTag; 
function initPanel ( aPrefTag )
  {
    if( hPrefWindow )
      hPrefWindow.onpageload( aPrefTag )
    else
      queuedTag = aPrefTag;
  } 
 
window.doneLoading = false; 
 
function nsPrefWindow( frame_id )
{
  if ( !frame_id )
    throw "Error: frame_id not supplied!";

  this.contentFrame   = frame_id
  this.wsm            = new nsWidgetStateManager( frame_id );
  this.wsm.attributes = ["preftype", "prefstring", "prefattribute", "prefinverse", "disabled"];
  this.pref           = null;
  this.chromeRegistry = null;
  this.observerService= null;
  
  this.cancelHandlers = [];
  this.okHandlers     = [];  

  // if there is a system pref switch
  this.pagePrefChanged = false;
  // the set of pages, which are updated after a system pref switch
  this.pagePrefUpdated = [];

  // set up window
  this.onload();
}

nsPrefWindow.prototype =
  {
    onload:
      function ()
        {
          try 
            {
              this.pref = Components.classes["@mozilla.org/preferences;1"].getService(Components.interfaces.nsIPref);
              this.chromeRegistry = Components.classes["@mozilla.org/chrome/chrome-registry;1"].getService(Components.interfaces.nsIXULChromeRegistry);
              this.observerService = Components.classes["@mozilla.org/observer-service;1"].getService(Components.interfaces.nsIObserverService);
            }
          catch(e) 
            {
              dump("*** Failed to create prefs object\n");
              return;
            }
        },

      init: 
        function ()
          {        
            if( window.queuedTag )
              {
                this.onpageload( window.queuedTag );
              }
  
            if( window.arguments[1] )
              this.openBranch( window.arguments[1], window.arguments[2] );
          },
                  
      onOK:
        function ()
          {
            var tag = document.getElementById( hPrefWindow.contentFrame ).getAttribute("tag");
            if( tag == "" )
              {
                tag = document.getElementById( hPrefWindow.contentFrame ).getAttribute("src");
              }
            hPrefWindow.wsm.savePageData( tag );
            for( var i = 0; i < hPrefWindow.okHandlers.length; i++ )
              try {
                hPrefWindow.okHandlers[i]();
              } catch (e) {
                dump("some silly ok handler /*"+hPrefWindow.okHandlers[i]+"*/ failed: "+ e);
              }
            hPrefWindow.savePrefs();
          },
        
      onCancel:
        function ()
          {
            for( var i = 0; i < hPrefWindow.cancelHandlers.length; i++ )
              try {
                hPrefWindow.cancelHandlers[i]();
              } catch (e) {
                dump("some silly cancel handler /*"+hPrefWindow.cancelHandlers[i]+"*/ failed: "+ e);
              }
          },

      registerOKCallbackFunc:
        function ( aFunctionReference )
          { 
            this.okHandlers[this.okHandlers.length] = aFunctionReference;
          },

      registerCancelCallbackFunc:
        function ( aFunctionReference )
          {
            this.cancelHandlers[this.cancelHandlers.length] = aFunctionReference;
          },
      getPrefIsLocked:
        function ( aPrefString )
          {
            return hPrefWindow.pref.PrefIsLocked(aPrefString);
          },
      getPref:
        function ( aPrefType, aPrefString, aDefaultFlag )
          {
            var pref = hPrefWindow.pref;
            try
              {
                switch ( aPrefType )
                  {
                    case "bool":
                      return !aDefaultFlag ? pref.GetBoolPref( aPrefString ) : pref.GetDefaultBoolPref( aPrefString );
                    case "int":
                      return !aDefaultFlag ? pref.GetIntPref( aPrefString ) : pref.GetDefaultIntPref( aPrefString );
                    case "localizedstring":
                      return pref.getLocalizedUnicharPref( aPrefString );
                    case "color":
                    case "string":
                    default:
                         return !aDefaultFlag ? pref.CopyUnicharPref( aPrefString ) : pref.CopyDefaultUnicharPref( aPrefString );
                  }
              }
            catch (e)
              {
                if( _DEBUG ) 
                  {
                    dump("*** no default pref for " + aPrefType + " pref: " + aPrefString + "\n");
                    dump(e + "\n");
                  }
              }
            return "!/!ERROR_UNDEFINED_PREF!/!";
          }    ,

      setPref:
        function ( aPrefType, aPrefString, aValue )
          {
            try
              {
                switch ( aPrefType )
                  {
                    case "bool":
                      hPrefWindow.pref.SetBoolPref( aPrefString, aValue );
                      break;
                    case "int":
                      hPrefWindow.pref.SetIntPref( aPrefString, aValue );
                      break;
                    case "color":
                    case "string":
                    case "localizedstring":
                    default:
                      hPrefWindow.pref.SetUnicharPref( aPrefString, aValue );
                      break;
                  }
              }
            catch (e)
              {
                dump(e + "\n");
              }
          },
          
      savePrefs:
        function ()
          {
            for( var pageTag in this.wsm.dataManager.pageData )
              {
                var pageData = this.wsm.dataManager.getPageData( pageTag );
                if ("initialized" in pageData && pageData.initialized)
                  {
                for( var elementID in pageData )
                  {
                    if (elementID == "initialized") continue;
                    var itemObject = pageData[elementID];
                    if (typeof(itemObject) != "object") break;
                    if ( "prefstring" in itemObject && itemObject.prefstring )
                      {
                        var elt = itemObject.localname;
                        var prefattribute = itemObject.prefattribute;
                        if (!prefattribute) {
                          if (elt == "radiogroup" || elt == "textbox" || elt == "menulist")
                            prefattribute = "value";
                          else if (elt == "checkbox" || elt == "listitem")
                            prefattribute = "checked";
                          else if (elt == "button")
                            prefattribute = "disabled";
                        }
                        
                        var value = itemObject[prefattribute];
                        var preftype = itemObject.preftype;
                        if (!preftype) {
                          if (elt == "textbox")
                            preftype = "string";
                          else if (elt == "checkbox" || elt == "listitem" || elt == "button")
                            preftype = "bool";
                          else if (elt == "radiogroup" || elt == "menulist")
                            preftype = "int";
                        }
                        switch( preftype )
                          {
                            case "bool":
                              if( value == "true" && typeof(value) == "string" )
                                value = true;
                              else if( value == "false" && typeof(value) == "string" )
                                value = false;
                              if (itemObject.prefinverse == "true")
                                value = !value;
                              break;
                            case "int":
                              value = parseInt(value, 10);                              
                              break;
                            case "color":
                              if( toString(value) == "" )
                                {
                                  dump("*** ERROR CASE: illegal attempt to set an empty color pref. ignoring.\n");
                                  break;
                                }
                            case "string":
                            case "localizedstring":
                            default:
                              if( typeof(value) != "string" )
                                {
                                  value = toString(value);
                                }
                              break;
                          }

                        // the pref is not saved, if the pref value is not
                        // changed or the pref is locked.
                        if( !this.getPrefIsLocked(itemObject.prefstring) &&
                           (value != this.getPref( preftype, itemObject.prefstring)))
                          {
                            this.setPref( preftype, itemObject.prefstring, value );
                          }
                      }
                  }
              }
              }
              try 
                {
                  this.pref.savePrefFile(null);
                }
              catch (e)
                {
                  try
                    {
                      var prefUtilBundle = document.getElementById("bundle_prefutilities");
                      var alertText = prefUtilBundle.getString("prefSaveFailedAlert");
                      var titleText = prefUtilBundle.getString("prefSaveFailedTitle");
                      var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                                                    .getService(Components.interfaces.nsIPromptService);
                      promptService.alert(window, titleText, alertText);
                    }
                  catch (e)
                    {
                      dump(e + "\n");
                    }
                }
          },                        

      switchPage:
        function ()
          {
            var prefPanelTree = document.getElementById( "prefsTree" );
            var selectedItem = prefPanelTree.contentView.getItemAtIndex(prefPanelTree.currentIndex);

            var oldURL = document.getElementById( this.contentFrame ).getAttribute("tag");
            if( !oldURL )
              {
                oldURL = document.getElementById( this.contentFrame ).getAttribute("src");
              }
            this.wsm.savePageData( oldURL );      // save data from the current page. 
            var newURL = selectedItem.firstChild.firstChild.getAttribute("url");
            var newTag = selectedItem.firstChild.firstChild.getAttribute("tag");
            if( newURL != oldURL )
              {
                document.getElementById( this.contentFrame ).setAttribute( "src", newURL );
                if( !newTag )
                  document.getElementById( this.contentFrame ).removeAttribute( "tag" );
                else
                  document.getElementById( this.contentFrame ).setAttribute( "tag", newTag );
              }
          },
              
      onpageload: 
        function ( aPageTag )
          {
            var header = document.getElementById("header");
            header.setAttribute("title",
                                window.frames[this.contentFrame].document.documentElement.getAttribute("headertitle"));

            // update widgets states when it is first loaded, or there are
            // system pref switch. (i.e., to refect the changed lock status).
            if(!(aPageTag in this.wsm.dataManager.pageData) ||
               (this.pagePrefChanged && (!(aPageTag in this.pagePrefUpdated))))
              {
                var prefElements = window.frames[this.contentFrame].document.getElementsByAttribute( "prefstring", "*" );
				if (this.pagePrefChanged)
         			this.pagePrefUpdated[aPageTag] = [];
                this.wsm.dataManager.pageData[aPageTag] = [];
                for( var i = 0; i < prefElements.length; i++ )
                  {
                    var prefstring    = prefElements[i].getAttribute( "prefstring" );
                    var prefid        = prefElements[i].getAttribute( "id" );
                    var preftype      = prefElements[i].getAttribute( "preftype" );
                    var elt = prefElements[i].localName;
                    if (!preftype) {
                      if (elt == "textbox")
                        preftype = "string";
                      else if (elt == "checkbox" || elt == "listitem" || elt == "button")
                        preftype = "bool";
                      else if (elt == "radiogroup" || elt == "menulist")
                        preftype = "int";
                    }
                    var prefdefval    = prefElements[i].getAttribute( "prefdefval" );
                    var prefattribute = prefElements[i].getAttribute( "prefattribute" );
                    if (!prefattribute) {
                      if (elt == "radiogroup" || elt == "textbox" || elt == "menulist")
                        prefattribute = "value";
                      else if (elt == "checkbox" || elt == "listitem")
                        prefattribute = "checked";
                      else if (elt == "button")
                        prefattribute = "disabled";
                    }
                    var prefvalue = this.getPref( preftype, prefstring );
                    if( prefvalue == "!/!ERROR_UNDEFINED_PREF!/!" )
                      {
                        prefvalue = prefdefval;
                      }
                    var root = this.wsm.dataManager.getItemData( aPageTag, prefid ); 
                    root[prefattribute] = prefvalue;              
                    var isPrefLocked = this.getPrefIsLocked(prefstring);
                    if (isPrefLocked)
                      root.disabled = "true";
                    root.localname = prefElements[i].localName;
                    if (preftype == "bool") {
                      root.prefinverse = prefElements[i].getAttribute("prefinverse");
                      if (root.prefinverse == "true")
                        root[prefattribute] = !prefvalue;
                    }
                  }
              }      
            this.wsm.setPageData( aPageTag );  // do not set extra elements, accept hard coded defaults
            
            if( 'Startup' in window.frames[ this.contentFrame ])
              {
                window.frames[ this.contentFrame ].Startup();
              }
            this.wsm.dataManager.pageData[aPageTag].initialized=true;
          },

    openBranch:
      function ( aComponentName, aSelectItem )
        {
          var panelTree = document.getElementById( "prefsTree" );
          var selectItem = document.getElementById( aSelectItem );
          var selectItemroot = document.getElementById( aComponentName );
          var parentIndex = panelTree.contentView.getIndexOfItem( selectItemroot );
          if (parentIndex != -1 && !panelTree.view.isContainerOpen(parentIndex))
             panelTree.view.toggleOpenState(parentIndex);
          var index = panelTree.view.getIndexOfItem( selectItem );
          if (index == -1)
            return;
          panelTree.treeBoxObject.selection.select( index );
        }

  };

