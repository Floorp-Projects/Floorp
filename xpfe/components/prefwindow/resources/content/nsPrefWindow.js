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
 
function initPanel ( aPrefTag )
  {
    if( hPrefWindow )
      hPrefWindow.onpageload( aPrefTag )
    else {
      dump("*** queueing up a panel...\n");
      queuedTag = aPrefTag;
    }
  } 
 
window.doneLoading = false; 
 
function nsPrefWindow( frame_id )
{
  if ( !frame_id )
    throw "Error: frame_id not supplied!";

  this.contentFrame   = frame_id
  this.wsm            = new nsWidgetStateManager( frame_id );
  this.wsm.attributes = ["pref", "preftype", "prefstring", "prefattribute"];
  this.pref           = null;
  
  this.cancelHandlers = [];
  this.okHandlers     = [];  
    
  // set up window
  this.onload();
}

nsPrefWindow.prototype =
  {
    onload:
      function ()
        {
          doSetOKCancel( this.onOK, this.onCancel );
          try 
            {
              this.pref = Components.classes["@mozilla.org/preferences;1"].getService();
              if( this.pref )
                {
                  this.pref = this.pref.QueryInterface( Components.interfaces.nsIPref );
                }
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
              this.closeBranches( window.arguments[1] );
          },
                  
      onOK:
        function ()
          {
            for( var i = 0; i < hPrefWindow.okHandlers.length; i++ )
              {
                hPrefWindow.okHandlers[i]();
              }
              
            var tag = document.getElementById( hPrefWindow.contentFrame ).getAttribute("tag");
            if( tag == "" )
              {
                tag = document.getElementById( hPrefWindow.contentFrame ).getAttribute("src");
              }
            hPrefWindow.wsm.savePageData( tag );
            hPrefWindow.savePrefs();
            close();
          },
        
      onCancel:
        function ()
          {
            for( var i = 0; i < hPrefWindow.cancelHandlers.length; i++ )
              {
                hPrefWindow.cancelHandlers[i]();
              }
            close();
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
                      break;
                    case "int":
                      return !aDefaultFlag ? pref.GetIntPref( aPrefString ) : pref.GetDefaultIntPref( aPrefString );
                      break;
                    case "color":
                    case "string":
                    default:
                    // BEN: please remove this hacky coe once you add a new
					// preftype for localizable pref.
                      if((aPrefString == "browser.startup.homepage") ||
                         (aPrefString == "wallet.Server") ||
                         (aPrefString == "intl.accept_languages") ||
                         (aPrefString == "intl.charset.default") ||
                         (aPrefString == "mailnews.start_page.url") ||
                         (aPrefString == "mailnews.view_default_charset") ||
                         (aPrefString == "mailnews.send_default_charset")) {
                            return  pref.getLocalizedUnicharPref( aPrefString );
                      } else {
                         return !aDefaultFlag ? pref.CopyUnicharPref( aPrefString ) : pref.CopyDefaultUnicharPref( aPrefString );
                      }
                      break;
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
                for( var elementID in pageData )
                  {
                    var element = this.wsm.contentArea.document.getElementById( elementID );
                    var itemObject = pageData[elementID];
                    if ( itemObject.pref == "true" )
                      {
                        var value = itemObject[itemObject.prefattribute];
                        switch( itemObject.preftype )
                          {
                            case "bool":
                              if( value == "true" && typeof(value) == "string" )
                                value = true;
                              else if( value == "false" && typeof(value) == "string" )
                                value = false;
                              break;
                            case "int":
                              value = parseInt(value);                              
                              break;
                            case "color":
                              if( toString(value) == "" )
                                {
                                  dump("*** ERROR CASE: illegal attempt to set an empty color pref. ignoring.\n");
                                  break;
                                }
                            case "string":
                            default:
                              if( typeof(value) != "string" )
                                {
                                  value = toString(value);
                                }
                              break;
                          }
                          
                        if( value != this.getPref( itemObject.preftype, itemObject.prefstring ) )
                          {
                            this.setPref( itemObject.preftype, itemObject.prefstring, value );
                          }
                      }
                  }
              }
              this.pref.SavePrefFile();
          },                        

      switchPage:
        function ()
          {
            var prefPanelTree = document.getElementById( "prefsTree" );
            var selectedItem = prefPanelTree.selectedItems[0];

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
            if( !this.wsm.dataManager.pageData[aPageTag] )
              {
                var prefElements = window.frames[this.contentFrame].document.getElementsByAttribute( "pref", "true" );
                this.wsm.dataManager.pageData[aPageTag] = [];
                for( var i = 0; i < prefElements.length; i++ )
                  {
                    var prefstring    = prefElements[i].getAttribute( "prefstring" );
                    var prefid        = prefElements[i].getAttribute( "id" );
                    var preftype      = prefElements[i].getAttribute( "preftype" );
                    var prefdefval    = prefElements[i].getAttribute( "prefdefval" );
                    var prefattribute = prefElements[i].getAttribute( "prefattribute" );

                    var prefvalue;
                    switch( preftype )
                      {
                        case "bool":
                          prefvalue = this.getPref( preftype, prefstring );
                          break;
                        case "int":
                          prefvalue = this.getPref( preftype, prefstring );
                          break;
                        case "string":
                        case "color":                          
                        default: 
                          prefvalue = this.getPref( preftype, prefstring );
                          break;
                      }
                      
                    if( prefvalue == "!/!ERROR_UNDEFINED_PREF!/!" )
                      {
                        prefvalue = prefdefval;
                      }
                    var root = this.wsm.dataManager.getItemData( aPageTag, prefid ); 
                    root[prefattribute] = prefvalue;              
                  }
              }      
            this.wsm.setPageData( aPageTag );  // do not set extra elements, accept hard coded defaults
            
            if( window.frames[ this.contentFrame ].Startup )
              {
                window.frames[ this.contentFrame ].Startup();
              }
          },

    closeBranches:
      function ( aComponentName )
        {
          var panelChildren = document.getElementById( "panelChildren" );
          var panelTree = document.getElementById( "prefsTree" );
          for( var i = 0; i < panelChildren.childNodes.length; i++ )
            {
              var currentItem = panelChildren.childNodes[i];
              if( currentItem.id != aComponentName && currentItem.id != "appearance" )
                currentItem.removeAttribute( "open" );
            }
          var openItem = document.getElementById( aComponentName );
          panelTree.selectItem( openItem );
        },

  };

