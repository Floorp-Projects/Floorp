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
 *   Ben "Count XULula" Goodger <rgoodger@ihug.co.nz>
 *   Alec Flett <alecf@netscape.com>
 */


/** class WizardStateManager ( string frame_id, object pageMap )
 *  - purpose: object for managing data in iframe multi-panel dialogs.
 *  - in: string frame id/name set of iframe, pageMap showing navigation of wizard
 *  - out: nothing (object)
 **/
function WidgetStateManager( frame_id, panelPrefix, panelSuffix )
{
  // data members
  /**
   *  hash table for data values:
   *    page1  =>  id1  =>  value1
   *    page1  =>  id2  =>  value2
   *    page2  =>  id1  =>  value1
   *    page2  =>  id2  =>  value2
   **/
  this.PageData          = [];
  this.content_frame     = window.frames[frame_id];

  this.panelPrefix       = ( panelPrefix ) ? panelPrefix : null;
  this.panelSuffix       = ( panelSuffix ) ? panelSuffix : null;
  
  // member functions
  this.SavePageData      = WSM_SavePageData;
  this.SetPageData       = WSM_SetPageData;
  this.PageIsValid       = WSM_PageIsValid;
  this.GetTagFromURL     = WSM_GetTagFromURL;
  this.GetURLFromTag     = WSM_GetURLFromTag;
  this.toString          = WSM_toString;
  this.AddAttributes     = WSM_AddAttributes;
  this.ElementIsIgnored  = WSM_ElementIsIgnored;
  this.HasValidElements  = WSM_HasValidElements;
  this.LookupElement     = WSM_LookupElement;
  this.GetDataForTag     = WSM_GetDataForTag;
  this.SetDataForTag     = WSM_SetDataForTag;
}

/** void SavePageData() ;
 *  - purpose: retrieves form/widget data from a generic page stored in a frame.
 *  -          useful for retrieving and persisting wizard/prefs page data that
 *  -          has not been added to permanent storage. 
 *  -          for full reference, please refer to user manual at:
 *  -            http://www.mozilla.org/xpapps/ui/wizards.html
 *  - in:  nothing
 *  - out: nothing
 **/               
function WSM_SavePageData( currentPageTag, optAttributes, exclElements, inclElements )
{
  // 11/26/99: changing this to support the saving of an optional number of extra
  // attributes other than the default value. these attributes are specified as 
  // strings in an array passed in as the second parameter. these values are stored
  // in the table associated with the element:
  // 
  // this.wsm.PageData[pageTag][element][id]    - id of element       (default)
  // this.wsm.PageData[pageTag][element][value] - value of element    (default)
  // this.wsm.PageData[pageTag][element][foo]   - optional attribute  (default)
  // 11/27/99: changing this to support the exclusion of specified elements.
  // typically this is includes fieldsets, legends and labels. These are default
  // and do not need to be passed. 
  
  if( !currentPageTag )
    currentPageTag = this.GetTagFromURL( this.content_frame.location.href, this.panelPrefix, this.panelSuffix, true );
  
  var doc = this.content_frame.document;
  var thisTagData = this.GetDataForTag(currentPageTag);

  if( this.content_frame.GetFields ) {
    // user GetFields function
    this.SetDataForTag(currentPageTag, this.content_frame.GetFields());
    var string = "";
    for( var i in thisTagData )
    {
      string += "element: " + i + "\n";
    }
  }
  else if (doc && doc.controls) {
    var fields = doc.controls;
    var data = [];
    for( i = 0; i < fields.length; i++ ) 
    { 
      data[i] = []; 
      var formElement = fields[i];
      var elementEntry = thisTagData[formElement.id] = [];

      // check to see if element is excluded
      if( !this.ElementIsIgnored( formElement, exclElements ) )
        elementEntry.excluded = false;
      else 
        elementEntry.excluded = true;

      if( formElement.localName.toLowerCase() == "select" ) { // select & combo
        /* commenting out until select fields work properly, or someone tells me how
           to do this (also, is it better to store .value, or .text?):*/
          if( formElement.getAttribute("multiple") ) {
            // multiple selections
            for( var j = 0, idx = 0; j < formElement.options.length; j++ )
            {
              if( formElement.options[j].selected ) {
                elementEntry.value[idx] = formElement.options[j].value;
                idx++;
              }
            }
          }
          else {
              // single selections
              if (formElement.options[formElement.selectedIndex]) {
                  var value = formElement.options[formElement.selectedIndex].value;
                  dump("*** VALUE=" + value + "\n");
                  formElement.arbitraryvalue = value;
                  this.AddAttributes( formElement, elementEntry, "arbitraryvalue", optAttributes );
                  this.AddAttributes( formElement, elementEntry, "value", optAttributes);
              }
          }
      }
      else if( formElement.getAttribute("type") &&
               ( formElement.type.toLowerCase() == "checkbox" ||
                 formElement.type.toLowerCase() == "radio" ) ) {
        // XXX 11/04/99
        this.AddAttributes( formElement, elementEntry, "checked", optAttributes );
      }
      else if( formElement.type == "text" &&
               formElement.getAttribute( "datatype" ) == "nsIFileSpec" &&
               formElement.value ) {
        try {
          var filespec = Components.classes["@mozilla.org/filespec;1"].createInstance();
          filespec = filespec.QueryInterface( Components.interfaces.nsIFileSpec );
        }
        catch(e) {
          dump("*** Failed to create filespec object\n");
        }
        filespec.nativePath = formElement.value;
        this.AddAttributes( formElement, elementEntry, "filespec", optAttributes )
      }
      else
        this.AddAttributes( formElement, elementEntry, "value", optAttributes );  // generic

      elementEntry.id       = formElement.id;
      elementEntry.localName = formElement.localName;
      // save the type attribute on the element if one is present
      elementEntry.elType   = ( formElement.type ) ? formElement.type : null;
    }
  }
  if( !this.HasValidElements( thisTagData ) )
    thisTagData.noData = true; // page has no (valid) elements
}

/** void SetPageData() ;
 *  - purpose: populates the loaded page with appropriate data from the data 
 *  -          table.
 *  -          for full reference, please refer to user manual at:
 *  -            http://www.mozilla.org/xpapps/ui/wizards.html
 *  - in:  nothing.
 *  - out: nothing.
 **/
function WSM_SetPageData( currentPageTag, hasExtraAttributes )
{
  if( !currentPageTag )
    currentPageTag = this.GetTagFromURL( this.content_frame.location.href, this.panelPrefix, this.panelSuffix, true );
  
	var doc = this.content_frame.document;
  var thisTagData = this.GetDataForTag(currentPageTag);
  if ( thisTagData && !thisTagData.nodata) {
  	for( var i in thisTagData ) {
      if( thisTagData[i].excluded || !i )
        continue;     // element is excluded, goto next
     
      var id    = thisTagData[i].id;
      var value = thisTagData[i].value;

      dump("*** id & value: " + id + " : " + value + "\n");
      
      if( this.content_frame.SetFields && !hasExtraAttributes )
        this.content_frame.SetFields( id, value );  // user provided setfields
      else if( this.content_frame.SetFields && hasExtraAttributes )
        this.content_frame.SetFields( id, value, thisTagData[i]); // SetFields + attrs
      else {                              // automated data provision
        var formElement = doc.getElementById( i );
        
        if( formElement && hasExtraAttributes ) {        // if extra attributes are set, set them
          for( var attName in thisTagData[i] ) 
          {
            // for each attribute set for this element
            if( attName == "value" || attName == "id" )
              continue;                   // don't set value/id (value = default, id = dangerous)
            var attValue  = thisTagData[i][attName];
            formElement.setAttribute( attName, attValue );
          }
        }
        
        // default "value" attributes        
        if( formElement && formElement.localName.toLowerCase() == "input" ) {
          if( formElement.type.toLowerCase() == "checkbox" ||
              formElement.type.toLowerCase() == "radio" ) {
            if( value == undefined )
              formElement.checked = formElement.defaultChecked;
            else 
              formElement.checked = value;
/*            oops.. appears we've reimplemented 'reversed'. this will be why its not working for alecf. 
              if( formElement.getAttribute( "reversed" ) )
                formElement.checked = !value;
              else
                formElement.checked = value;
                */
          }
          else if( formElement.type.toLowerCase() == "text" &&
               formElement.getAttribute( "datatype" ) == "nsIFileSpec" ) {
            // formElement has something to do with fileSpec. looked important
            if( value ) {
              var filespec = value.QueryInterface( Components.interfaces.nsIFileSpec );
              try {
                formElement.value = filespec.nativePath;
              } 
              catch( ex ) {
                dump("Still need to fix uninitialized filespec problem!\n");
              }
            } 
            else
              formElement.value = formElement.defaultValue;
          }
          else {                          // some other type of form element
            if( value == undefined )
              formElement.value = formElement.defaultValue;
            else
              formElement.value = value;
          }
        } 
        else if( formElement && formElement.localName.toLowerCase() == "select" ) {
          /* commenting this out until select widgets work properly */
            if( formElement.getAttribute("multiple") &&
                typeof(value) == "object" ) {
              // multiple selected items
              for( var j = 0; j < value.length; j++ )
              {
                for ( var k = 0; k < formElement.options.length; k++ )
                {
                  if( formElement.options[k].value == value[j] )
                    formElement.options[k].selected = true;
                }
              }
            }
            else {
              // single selected item
              for ( k = 0; k < formElement.options.length; k++ )
              {
                dump("*** value=" + value + "; options[k].value=" + formElement.options[k].value + "\n");
                if( formElement.options[k].value == value )
                  formElement.options[k].selected = true;
              }
            }            
        }
        else if( formElement && formElement.localName.toLowerCase() == "textarea" )
          formElement.value = value;
      }
    }
  }
  // initialize the pane
  if (this.content_frame.onInit) {
    dump("Calling custom onInit()\n");
    this.content_frame.onInit();
  }
    
}   

/** boolean PageIsValid()
 * - purpose: returns whether the given page is in a valid state
 * - in:
 * - out: 
 */
function WSM_PageIsValid()
{
  if( this.content_frame.validate )
    return this.content_frame.validate();

  // page is valid by default
  return true;
}


/** string GetTagFromURL( string tag, string prefix, string postfix ) ;
 *  - purpose: fetches a tag from a URL
 *  - in:   string url representing the complete location of the page.
 *  - out:  string tag representing the specific page to be loaded
 **/               
function WSM_GetTagFromURL( url, prefix, suffix, mode )
{
  // NOTE TO SELF: this is an accident WAITING to happen
  if (!prefix) return undefined;
  if( mode )
    return url.substring( prefix.length, url.lastIndexOf(suffix) );
  else
    return url.substring( url.lastIndexOf(prefix) + 1, url.lastIndexOf(suffix) );
}

/** string GetUrlFromTag( string tag, string prefix, string postfix ) ;
 *  - purpose: creates a complete URL based on a tag.
 *  - in:  string tag representing the specific page to be loaded
 *  - out: string url representing the complete location of the page.
 **/               
function WSM_GetURLFromTag( tag, prefix, postfix ) 
{
  return prefix + tag + postfix;
}

/** string toString() ;
 *  - purpose: returns a string representation of the object
 *  - in: nothing;
 *  - out: nothing;
 **/
function WSM_toString()
{
  var string = "";
  for( var i in this.PageData ) {
    for( var j in this.PageData[i] ) {
      for( var k in this.PageData[i][j] ) {
        string += "WSM.PageData[" + i + "][" + j + "][" + k + "] : " + this.PageData[i][j][k] + ";\n";
      }
    }
  }
  return string;
}

/** void AddAttributes( DOMElement formElement, AssocArray elementEntry, 
                        String valueAttribute, StringArray optAttributes ) ;
 *  - purpose: adds name/value entries to associative array.
 *  - in:       formElement - element to add attributes from
 *  -           elementEntry - the associative array to add data to
 *  -           valueAttribute - string representing which attribute represents 
 *  -               value (e.g. "value", "checked")
 *  -           optAttributes - array of extra attributes to store. 
 *  - out: nothing;
 **/
function WSM_AddAttributes( formElement, elementEntry, valueAttribute, optAttributes )
{
  // 07/01/00 adding in a reversed thing here for alecf's ultra picky mail prefs :P:P
  if( formElement.getAttribute("reversed") )
    elementEntry.value = !formElement[valueAttribute]; // get the value (e.g. "checked")
  else
    elementEntry.value = formElement[valueAttribute]; // get the value (e.g. "checked")
  
  if( optAttributes ) {   // if we've got optional attributes, add em
    for(var k = 0; k < optAttributes.length; k++ ) 
    {
      attValue = formElement.getAttribute( optAttributes[k] );
      if( attValue )
        elementEntry[optAttributes[k]] = attValue;
    }
  }
}

/** string ElementIsIgnored( DOMElement element, StringArray exclElements ) ;
 *  - purpose: check to see if the current element is one of the ignored elements
 *  - in:       element - element to check, 
 *  -           exclElements - array of string ignored attribute localNames;
 *  - out:      boolean if element is ignored (true) or not (false);
 **/
function WSM_ElementIsIgnored( element, exclElements )
{
  if (!exclElements) return false;
  for( var i = 0; i < exclElements.length; i++ )
  {
    if( element.localName.toLowerCase() == exclElements[i] )
      return true;
  }
  return false;
}

/** string HasValidElements( AssocArray dataStore ) ;
 *  - purpose:  checks to see if there are any elements on this page that are 
 *  -           valid.
 *  - in:       associative array representing the page;
 *  - out:      boolean whether or not valid elements are present (true) or not (false);
 **/
function WSM_HasValidElements( dataStore )
{
  for( var i in dataStore ) 
  {
    if( !dataStore[i].excluded )
      return true;
  }
  return false;
}


function WSM_LookupElement( element, lookby, property )
{
  for(var i in this.PageData )
  {
    for( var j in this.PageData[i] )
    {
      if(!lookby) 
        lookby = "id";    // search by id by default
      if( j[lookby] == element && !property )
        return j;
      else if( j[lookby] == element ) {
        var container = [];
        for( var k = 0; k < property.length; k++ )
        {
          container[k] = this.PageData[i][k][property[k]];
        }
        return container; // only support one single match per hash just now
      }
    }
  }
  return undefined;
}

/**
 * array GetDataForTag(string tag)
 * - purpose: to get the array associated with this tag
 *            creates the array if one doesn't already exist
 * - in:      tag
 * - out:     the associative array for this page
 */
function WSM_GetDataForTag(tag) {
  if (!this.PageData[tag])
    this.PageData[tag] = [];
  return this.PageData[tag];
}

function WSM_SetDataForTag(tag, data) {
  this.PageData[tag] = data;
}

/* it will be dark soon */
