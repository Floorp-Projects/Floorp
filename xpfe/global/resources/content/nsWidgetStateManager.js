/* -*- Mode: Java; tab-width: 2; c-basic-offset: 2; -*-
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

/** Presenting widgetStateManager the Third.
 *   a production of ye olde bard Ben Goodger and his Merry XUL Widget Crewe.
 *  =>> MODIFICATIONS MUST BE REVIEWED BY ben@netscape.com!!! <<=
 **/
 
function nsWidgetStateManager ( aFrameID )
  {
   
    this.dataManager = 
      {
        /** Persisted Data Hash Table
         *  Page_ID -> Element_ID -> Property -> Value
         **/
        pageData: [],

        setPageData: 
          function ( aPageTag, aDataObject )
            {
              this.pageData[aPageTag] = aDataObject;
            },

        getPageData:
          function ( aPageTag )
            {
              if( !this.pageData[aPageTag] )
                this.pageData[aPageTag] = [];
              return this.pageData[aPageTag];
            },
        
        setItemData:
          function ( aPageTag, aItemID, aDataObject )
            {
              if( !this.pageData[aPageTag] )
                this.pageData[aPageTag] = [];
              this.pageData[aPageTag][aItemID] = aDataObject;
            },
            
        getItemData:  
          function ( aPageTag, aItemID )
            {
              if( !this.pageData[aPageTag][aItemID] )
                this.pageData[aPageTag][aItemID] = [];
              return this.pageData[aPageTag][aItemID];
            },
      }
  
    this.contentID    = aFrameID;
    
    wsm               = this;

    /** Element Handlers
     *  Provides default get and set handler functions for supported
     *  widgets. Clients can override or add new widgets.
     **/
    this.handlers     = 
      {
        menulist: 
          {  get: wsm.get_Menulist,    set: wsm.set_Menulist      },
        radiogroup:
          {  get: wsm.get_Radiogroup,  set: wsm.set_Radiogroup    },
        checkbox:
          {  get: wsm.get_Checkbox,    set: wsm.set_Checkbox      },
        textfield:
          {  get: wsm.get_Textfield,   set: wsm.set_Textfield     },
        default_handler:
          {  get: wsm.get_Default,     set: wsm.set_Default       },
      }
   
    // extra attributes to scan and save.
    this.attributes   = [];
  } 
  
nsWidgetStateManager.prototype = 
  {
    get contentArea()
      { 
        return window.frames[ this.contentID ]; 
      },

    savePageData: 
      function ( aPageTag )
        {
          if( this.contentArea.GetFields )
            {
              // save page data based on user supplied function in content area
              var dataObject = this.contentArea.GetFields();
              this.dataManager.setPageData( aPageTag, dataObject );
            }

            // Automatic element retrieval. This is done in two ways. 
            // 1) if an element id array is present in the document, this is
            //    used to build a list of elements to persist. <-- performant
            // 2) otherwise, all elements with 'wsm_persist' set to true
            //    are persisted <-- non-performant.
           if( this.contentArea._elementIDs )
              {
                var elements = [];
                for( var i = 0; i < this.contentArea._elementIDs.length; i++ )
                  {
                    var elt = this.contentArea.document.getElementById( this.contentArea._elementIDs[i] );
                    if (elt) {
                      elements[elements.length] = elt;
                    } else {
                      // see bug #40329. People forget this too often, and it breaks Prefs
                      dump("*** FIX ME: '_elementIDs' in '" + this.contentArea.location.href.split('/').pop() +
                           "' contains a reference to a non-existent element ID '" + 
                           this.contentArea._elementIDs[i] + "'.\n");
                    }
                  }
              }
            else 
              {
                var elements = this.contentArea.document.getElementsByAttribute( "wsm_persist", "true" );
              }
            for( var i = 0; i < elements.length; i++ )
              {
                var elementID   = elements[i].id;
                var elementType = elements[i].localName;
                if (!this.dataManager.pageData[aPageTag])
                    this.dataManager.pageData[aPageTag] = [];
                this.dataManager.pageData[aPageTag][elementID] = [];
                // persist element Type
                this.dataManager.pageData[aPageTag][elementID].localName = elementType;
                // persist attributes
                var get_Func = this.handlers[elementType] != undefined ? 
                                this.handlers[elementType].get : 
                                this.handlers.default_handler.get;
                this.dataManager.setItemData( aPageTag, elementID, get_Func( elementID ) );
              }
        },

    setPageData:
      function ( aPageTag )
        {
          var pageData = this.dataManager.getPageData( aPageTag );
          if( this.contentArea.SetFields )
            {
              this.contentArea.SetFields( pageData );
              return;
            }
          
          for( var elementID in pageData )
            {
              var element = this.contentArea.document.getElementById( elementID );
              if( element ) 
                {
                  var elementType = element.localName;
                  var set_Func = this.handlers[elementType] != undefined ?
                                  this.handlers[elementType].set :
                                  this.handlers.default_handler.set;
                  set_Func( elementID, pageData[elementID] );
                }
            }
        },
  
  
    /** Widget Get/Set Function Implementations
     *  These can be overridden by the client.
     **/
    generic_Set:
      function ( aElement, aDataObject )
        {
          if( aElement )
            {
              for( var property in aDataObject )
                {
                  aElement.setAttribute( property, aDataObject[property] );
                }
            }
        },
        
    generic_Get:
      function ( aElement )
        {
          if( aElement )
            {
              var dataObject = [];
              var wsmAttributes = aElement.getAttribute( "wsm_attributes" );
              var attributes = wsm.attributes;              // make a copy
              if( wsmAttributes != "" )
                {
                  attributes.push( wsmAttributes.split(" ") );  // modify the copy
                }
              for( var i = 0; i < attributes.length; i++ )
                {
                  dataObject[attributes[i]] = aElement.getAttribute( attributes[i] );
                }
              return dataObject;
            }        
            return null;
        },
  
    // <menulist>
    set_Menulist:
      function ( aElementID, aDataObject )
        {
          var element = wsm.contentArea.document.getElementById( aElementID );
          // set all generic properties
          wsm.generic_Set( element, aDataObject );
          // set menulist specific properties
          if( aDataObject.data != undefined )
            { 
              var matchElement = element.getElementsByAttribute( "data", aDataObject.data );
              if (matchElement.length && matchElement[0])
                element.selectedItem = matchElement[0];
            }
        },
        
    get_Menulist:
      function ( aElementID )
        {
          var element     = wsm.contentArea.document.getElementById( aElementID );
          // retrieve all generic attributes
          var dataObject  = wsm.generic_Get( element );
          // retrieve all menulist specific attributes
          if( dataObject )
            {
              dataObject.data = element.getAttribute( "data" );
              return dataObject;
            }
          return null;
        },

    // <radiogroup>        
    set_Radiogroup:
      function ( aElementID, aDataObject )
        {
          
          var element = wsm.contentArea.document.getElementById( aElementID );
          wsm.generic_Set( element, aDataObject );
          if( aDataObject.data != undefined )
            { 
              element.selectedItem = element.getElementsByAttribute( "data", aDataObject.data )[0];
            }
        },
    
    get_Radiogroup:
      function ( aElementID )
        {
          var element = wsm.contentArea.document.getElementById( aElementID );
          var dataObject = wsm.generic_Get( element );
          if( dataObject )
            {
              dataObject.data = element.getAttribute( "data" );
              return dataObject;
            }
          return null;
        },
        
    // <textfield>
    set_Textfield:
      function ( aElementID, aDataObject )
        {
          var element = wsm.contentArea.document.getElementById( aElementID );
          wsm.generic_Set( element, aDataObject );
        },
     
    get_Textfield:
      function ( aElementID )
        {
          var element = wsm.contentArea.document.getElementById( aElementID );
          var dataObject = wsm.generic_Get( element );
          if( dataObject )
            {
              dataObject.value = wsm.contentArea.document.getElementById( aElementID ).value;
              //dataObject.data = wsm.contentArea.document.getElementById( aElementID ).data;
              return dataObject;
            }
          return null;
        },
     
    // <checkbox>
    set_Checkbox:
      function ( aElementID, aDataObject )
        {
          var element = wsm.contentArea.document.getElementById( aElementID );
          wsm.generic_Set( element, aDataObject );
        },
    
    get_Checkbox:
      function ( aElementID )
        {
          var element = wsm.contentArea.document.getElementById( aElementID );
          var dataObject = wsm.generic_Get( element );
          if( dataObject )
            { 
              dataObject.checked = wsm.contentArea.document.getElementById( aElementID ).checked;
              return dataObject;
            }
          return null;
        },

    // <default>
    set_Default:
      function ( aElementID, aDataObject )
        {
          var element = wsm.contentArea.document.getElementById( aElementID );
          wsm.generic_Set( element, aDataObject );
        },
    
    get_Default:
      function ( aElementID )
        {
          var element = wsm.contentArea.document.getElementById( aElementID );
          var dataObject = wsm.generic_Get( element );
          return dataObject ? dataObject : null;
        },
  }

  
/* it will be dark soon */
/* MANOS MADE ME PERMANENT! */
/* there is no way out of here */


