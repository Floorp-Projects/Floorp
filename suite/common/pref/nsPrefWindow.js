/* -*- Mode: Java; tab-width: 4; c-basic-offset: 4; -*-
 * 
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben "Count XULula" Goodger <ben@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const _DEBUG = false; 
 
/** PrefWindow IV
 *  =============
 *  This is a general page switcher and pref loader.
 *  =>> CHANGES MUST BE REVIEWED BY ben@netscape.com!! <<=
 **/ 

var hPrefWindow = null;
var queuedTag; 

function initPanel ( aPrefTag )
  {
    if( hPrefWindow )
      hPrefWindow.onpageload( aPrefTag )
    else
      queuedTag = aPrefTag;
  } 

function onLoad()
{
  hPrefWindow = new nsPrefWindow('panelFrame');

  if (!hPrefWindow)
    throw "failed to create prefwindow";
  else
    hPrefWindow.init();
}

function nsPrefWindow( frame_id )
{
  if ( !frame_id )
    throw "Error: frame_id not supplied!";

  this.contentFrame   = frame_id
  this.wsm            = new nsWidgetStateManager( frame_id );
  this.wsm.attributes = ["preftype", "prefstring", "prefattribute", "disabled"];
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
              this.pref = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefService).getBranch(null);
              this.chromeRegistry = Components.classes["@mozilla.org/chrome/chrome-registry;1"].getService(Components.interfaces.nsIChromeRegistrySea);
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
                this.onpageload( window.queuedTag );
  
            if( window.arguments[1] )
              this.openBranch( window.arguments[1], window.arguments[2] );
          },
                  
      onAccept:
        function ()
          {
            var tag = document.getElementById( hPrefWindow.contentFrame ).getAttribute("tag");
            if( tag == "" )
                tag = document.getElementById( hPrefWindow.contentFrame ).getAttribute("src");
            hPrefWindow.wsm.savePageData( tag );
            for( var i = 0; i < hPrefWindow.okHandlers.length; i++ )
              try {
                hPrefWindow.okHandlers[i]();
              } catch (e) {
                dump("some silly ok handler /*"+hPrefWindow.okHandlers[i]+"*/ failed: "+ e);
              }
            hPrefWindow.savePrefs();

            return true;
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

            return true;
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
            return this.pref.prefIsLocked(aPrefString);
          },
      getPref:
        function ( aPrefType, aPrefString )
          {
            try
              {
                switch ( aPrefType )
                  {
                    case "bool":
                      return this.pref.getBoolPref( aPrefString );
                    case "int":
                      return this.pref.getIntPref( aPrefString );
                    case "localizedstring":
                      return this.pref.getComplexValue( aPrefString, Components.interfaces.nsIPrefLocalizedString ).data;
                    case "color":
                    case "string":
                    default:
                       return this.pref.getComplexValue( aPrefString, Components.interfaces.nsISupportsString ).data;
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
                      this.pref.setBoolPref( aPrefString, aValue );
                      break;
                    case "int":
                      this.pref.setIntPref( aPrefString, aValue );
                      break;
                    case "color":
                    case "string":
                    case "localizedstring":
                    default:
                      var supportsString = Components.classes["@mozilla.org/supports-string;1"].createInstance(Components.interfaces.nsISupportsString);
                      supportsString.data = aValue;
                      this.pref.setComplexValue( aPrefString, Components.interfaces.nsISupportsString, supportsString );
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
                          else if (elt == "colorpicker")
                            prefattribute = "color";
                          else if (elt == "button")
                            prefattribute = "disabled";
                        }
                        
                        var value = itemObject[prefattribute];
                        var preftype = itemObject.preftype;
                        if (!preftype) {
                          if (elt == "textbox" || elt == "colorpicker")
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
                                  value = toString(value);
                              break;
                          }

                        // the pref is not saved, if the pref value is not
                        // changed or the pref is locked.
                        if( !this.getPrefIsLocked(itemObject.prefstring) &&
                           (value != this.getPref( preftype, itemObject.prefstring)))
                            this.setPref( preftype, itemObject.prefstring, value );
                      }
                  }
              }
              }
              try 
                {
                  Components.classes["@mozilla.org/preferences-service;1"]
                            .getService(Components.interfaces.nsIPrefService)
                            .savePrefFile(null);
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
                oldURL = document.getElementById( this.contentFrame ).getAttribute("src");
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
                      if (elt == "textbox" || elt == "colorpicker")
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
                      else if (elt == "colorpicker")
                        prefattribute = "color";
                      else if (elt == "button")
                        prefattribute = "disabled";
                    }
                    var prefvalue = this.getPref( preftype, prefstring );
                    if( prefvalue == "!/!ERROR_UNDEFINED_PREF!/!" )
                        prefvalue = prefdefval;
                    var root = this.wsm.dataManager.getItemData( aPageTag, prefid ); 
                    root[prefattribute] = prefvalue;              
                    var isPrefLocked = this.getPrefIsLocked(prefstring);
                    if (isPrefLocked)
                      root.disabled = "true";
                    root.localname = prefElements[i].localName;
                  }
              }      
            this.wsm.setPageData( aPageTag );  // do not set extra elements, accept hard coded defaults
            
            if( 'Startup' in window.frames[ this.contentFrame ])
                window.frames[ this.contentFrame ].Startup();

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
          panelTree.view.selection.select( index );
        }

  };
