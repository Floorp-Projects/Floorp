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
 *   Alec Flett <alecf@netscape.com>
 */


/** class WizardStateManager ( string frame_id, object pageMap )
 *  - purpose: object for managing data in iframe multi-panel dialogs.
 *  - in: string frame id/name set of iframe, pageMap showing navigation of wizard
 *  - out: nothing (object)
 **/
function WidgetStateManager( frame_id )
{
  // data members
  /**
   *  hash table for data values:
   *    page1  =>  id1  =>  value1
   *    page1  =>  id2  =>  value2
   *    page2  =>  id1  =>  value1
   *    page2  =>  id2  =>  value2
   **/
  this.PageData         = [];
  this.content_frame    = window.frames[frame_id];

  // member functions
  this.SavePageData     = WSM_SavePageData;
  this.SetPageData      = WSM_SetPageData;
  this.GetTagFromURL    = WSM_GetTagFromURL;
  this.GetURLFromTag    = WSM_GetURLFromTag;
  this.toString         = WSM_toString;
  this.ConvertToArray   = WSM_ConvertToArray;
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
function WSM_SavePageData( currentPageTag )
{
  var doc = this.content_frame.document;
  if( this.content_frame.GetFields ) {
    // data user-provided from GetFields function
    var data = this.content_frame.GetFields();
  } 
  else {
    // autosave data from form fields in the document
    var fields = doc.getElementsByTagName("FORM")[0].elements;   
    var data = [];
    for( var i = 0; i < fields.length; i++ ) 
    { 
      data[i] = []; 
      var formElement = fields[i];
      data[i][0] = formElement.id;
      if( formElement.nodeName.toLowerCase() == "select" ) { // select & combo
        // data[i][0] = formElement.getAttribute("id");
        // here's a bug. this gives errors, even though this style of thing works
        // in 4.x
        // data[i][1] = formElement.options[formElement.options.selectedIndex].value;
        // dump("*** CURRSELECT:: VALUE: " + data[i][1] + "\n");
      }
      else if( formElement.nodeName.toLowerCase() == "textarea" ) { // textarea
        data[i][0] = currTextArea.getAttribute("id");
        data[i][1] = currTextArea.value;
      }
      else if( formElement.type == "checkbox" || formElement.type == "radio" ) {
        // form element is a checkbox or a radio group item
        data[i][1] =  formElement.checked;
      }  
      else if( formElement.type == "text" &&
               formElement.getAttribute( "datatype" ) == "nsIFileSpec" ) {
        if( formElement.value ) {
          var progId = "component://netscape/filespec";
          var filespec = Components.classes[progId].createInstance( Components.interfaces.nsIFileSpec );
          filespec.nativePath = formElement.value;
          data[i][1] = filespec;
        } else
          data[i][1] = null;
      }
      else {
        // generic form element: text, password field, buttons, textareas, etc
        data[i][1] = formElement.value;
      }
    }
    // trees are not supported here as they are too complex and customisable 
    // to provide default handling for.
  }
  this.PageData[currentPageTag] = []; 
  if( data.length )  {
    for( var i = 0; i < data.length; i++ ) {
      this.PageData[currentPageTag][data[i][0]] = data[i][1];
    }
  } 
  else
    this.PageData[currentPageTag]["nodata"] = true; // no fields or functions
}

/** void SetPageData() ;
 *  - purpose: populates the loaded page with appropriate data from the data 
 *  -          table.
 *  -          for full reference, please refer to user manual at:
 *  -            http://www.mozilla.org/xpapps/ui/wizards.html
 *  - in:  nothing.
 *  - out: nothing.
 **/
function WSM_SetPageData( currentPageTag )
{
	var doc = this.content_frame.document;
  if ( this.PageData[currentPageTag] != undefined ) {
    if ( !this.PageData[currentPageTag]["nodata"] ) {
    	for( var i in this.PageData[currentPageTag] ) {
        var value = this.PageData[currentPageTag][i];
        if( this.content_frame.SetFields ) {
          // user-provided SetFields function
          this.content_frame.SetFields( i, this.PageData[currentPageTag][i] );
        }
        else {
          // automated data provision
          // plundering code liberally from AccountManager.js
          var formElement = doc.getElementById( i );
          if( formElement && formElement.nodeName.toLowerCase() == "input" ) {
            if( formElement.type == "checkbox" || formElement.type == "radio" ) {
              // formElement is something interesting like a checkbox or radio
              if( value == undefined )
                formElement.checked = formElement.defaultChecked;
              else {
                if( formElement.getAttribute( "reversed" ) )
                  formElement.checked = !value;
                else
                  formElement.checked = value;
              }
            }
            else if( formElement.type == "text" &&
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
            else {
              // some other type of form element
              if( value == undefined )
                formElement.value = formElement.defaultValue;
              else
                formElement.value = value;
            }
          } 
          else if( formElement && formElement.nodeName.toLowerCase() == "select" ) {
            // select the option element that has the value that matches data[i][1]
            /* commenting out while select widgets appear to be broken
            if( formElement.hasChildNodes() ) {
              for( var j = 0; j < formElement.childNodes.length; j++ ) {
                var currNode = formElement.childNodes[i];
                if( currNode.nodeName.toLowerCase() == "option" ) {
                  if( currNode.getAttribute("value") == value )
                    currNode.selected = true;
                }
              }
            }
           */
          }
          else if( formElement && formElement.nodeName.toLowerCase() == "textarea" ) {
            formElement.value = value;
          }
        }
      }
    }
  }
}   

/** string GetTagFromURL( string tag, string prefix, string postfix ) ;
 *  - purpose: fetches a tag from a URL
 *  - in:   string url representing the complete location of the page.
 *  - out:  string tag representing the specific page to be loaded
 **/               
function WSM_GetTagFromURL( url, prefix, suffix )
{
  return url.substring( prefix.length, url.lastIndexOf(suffix) );
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
    for( var k in this.PageData[i] ) {
      string += "WSM.PageData[" + i + "][" + k + "] : " + this.PageData[i][k] + ";\n";
    }
  }
  return string;
}

function WSM_ConvertToArray( nodeList )
{
  var retArray = [];
  for( var i = 0; i < nodeList.length; i++ )
  {
    retArray[i] = nodeList[i];
  }
  return retArray;
}