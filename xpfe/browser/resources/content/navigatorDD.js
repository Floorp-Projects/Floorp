/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *  - Kevin Puetz (puetzk@iastate.edu)
 *  - Ben Goodger <ben@netscape.com>
 */

////////////////////////////////////////////////////////////////////////////
// XXX - WARNING - DRAG AND DROP API CHANGE ALERT - XXX
// This file has been extensively modified in a checkin planned for Mozilla
// 0.8, and the API has been modified. DO NOT MODIFY THIS FILE without 
// approval from ben@netscape.com, otherwise your changes will be lost. 

var gRDFService = nsJSComponentManager.getService("@mozilla.org/rdf/rdf-service;1",
                                                  "nsIRDFService"); 

function RDF(aType) 
  {
    return "http://www.w3.org/1999/02/22-rdf-syntax-ns#" + aType;
  }
function NC_RDF(aType)
  {
    return "http://home.netscape.com/NC-rdf#" + aType;  
  }

var RDFUtils = {
  getResource: function(aString)
    {
      return gRDFService.GetResource(aString, true);
    },
  
  getTarget: function(aDS, aSourceID, aPropertyID)
    {
      var source = this.getResource(aSourceID);
      var property = this.getResource(aPropertyID);
      return aDS.GetTarget(source, property, true);
    },
  
  getValueFromResource: function(aResource)
    {
      aResource = aResource.QueryInterface(Components.interfaces.nsIRDFResource);
      return aResource ? target.Value : null;
    }
};

function isBookmark(aURI)
  {
    var rv = true;
    var typeValue = RDFUtils.getTarget(childWithDatabase.database, uri, RDF("type"));
    typeValue = RDFUtils.getValueFromResource(typeValue);
    if (typeValue != NC_RDF("BookmarkSeparator") && 
        typeValue != NC_RDF("Bookmark") &&
        typeValue != NC_RDF("Folder")) 
      rv = false;
    return rv;
  }

function isPToolbarDNDEnabled()
  {
    var prefs = nsJSComponentManager.getService("@mozilla.org/preferences;1",
                                                "nsIPref");
    var dragAndDropEnabled = false;                                                  
    try {
      dragAndDropEnabled = prefs.GetBoolPref("browser.enable.tb_dnd");
    }
    catch(e) {
    }
    
    return dragAndDropEnabled;
  }
  
var personalToolbarObserver = {
  onDragStart: function (aEvent)
    {
      // temporary
      if (!isPToolbarDNDEnabled())
        return false;
        
      var personalToolbar = document.getElementById("PersonalToolbar");
      if (aEvent.target == personalToolbar)
        return null;
        
      var childWithDatabase = document.getElementById("innermostBox");
      var uri = aEvent.target.id;
      //if (!isBookmark(uri)) 
      //  return;

      var title = aEvent.target.value;
      var htmlString = "<A HREF='" + uri + "'>" + title + "</A>";

      var flavourList = { };
      flavourList["moz/toolbaritem"] = { width: 2, data: uri };
      flavourList["text/x-moz-url"] = { width: 2, data: uri + "\n" + "[ TEMP TITLE ]" };
      flavourList["text/html"] = { width: 2, data: htmlString };
      flavourList["text/unicode"] = { width: 2, data: uri };
      return flavourList;
    },
  
  onDrop: function (aEvent, aData, aDragSession) 
    {
      // temporary
      if (!isPToolbarDNDEnabled())
        return false;
        
      var element = aData.data.data;
      var dropElement = aEvent.target.id;
      var elementRes = RDFUtils.getResource(element);
      var dropElementRes = RDFUtils.getResource(dropElement);
      var personalToolbarRes = RDFUtils.getResource("NC:PersonalToolbarFolder");
      
      var childDB = document.getElementById("innermostBox").database;
      var rdfContainer = nsJSComponentManager.createInstance("@mozilla.org/rdf/container;1",
                                                             "nsIRDFContainer");
      rdfContainer.Init(childDB, personalToolbarRes);
      
      var elementIsOnToolbar = rdfContainer.IndexOf(elementRes);
      if (elementIsOnToolbar > 0) 
        rdfContainer.RemoveElement(elementRes, true);
      else if (dropIndex == -1)
        {
          // look up this URL's title in global history
          var potentialTitle = null;
          var historyDS = gRDFService.GetDataSource("rdf:history");
          var historyEntry = gRDFService.GetResource(element);
          var historyTitleProperty = gRDFService.GetResource(NC_RDF("Name"));
          var titleFromHistory = historyDS.GetTarget(historyEntry, historyTitleProperty, true);
          if (titleFromHistory)
            titleFromHistory = titleFromHistory.QueryInterface(Components.interfaces.nsIRDFLiteral);
          if (titleFromHistory)
            potentialTitle = titleFromHistory.Value;
          linkTitle = potentialTitle ? potentialTitle : element;
          childDB.Assert(gRDFService.GetResource(element, true), 
                         gRDFService.GetResource(NC_RDF("Name"), true),
                         gRDFService.GetLiteral(linkTitle),
                         true);
        }
      var dropIndex = rdfContainer.IndexOf(dropElementRes);
      // determine the drop position
      var dropPosition = this.determineDropPosition(aEvent);
      switch (dropPosition) {
      case -1:
        rdfContainer.InsertElementAt(elementRes, dropIndex, true);
        break;
      case 0:
        // do something here to drop into subfolders
        var childContainer = nsJSComponentManager.createInstance("@mozilla.org/rdf/container;1",
                                                                 "nsIRDFContainer");
        childContainer.Init(childDB, dropElementRes);
        childContainer.AppendElement(elementRes);
        break;
      case 1:
      default:
        rdfContainer.InsertElementAt(elementRes, dropIndex+1, true);
        break;
      }
      return true;
    },
  
  mCurrentDragOverButton: null,
  mCurrentDragPosition: null,
  
  onDragExit: function (aEvent, aDragSession)
    {
      if (this.mCurrentDragOverButton)
        {
          this.mCurrentDragOverButton.removeAttribute("dragover-left");
          this.mCurrentDragOverButton.removeAttribute("dragover-right");
          this.mCurrentDragOverButton.removeAttribute("dragover-top");
          this.mCurrentDragOverButton.removeAttribute("open");
        }
    },
  
  onDragOver: function (aEvent, aFlavour, aDragSession)
    {
      // temporary
      if (!isPToolbarDNDEnabled())
        return false;

      var dropPosition = this.determineDropPosition(aEvent);
      
      if (this.mCurrentDragOverButton != aEvent.target ||
          (this.mCurrentDragOverButton == aEvent.target && 
           this.mCurrentDragPosition != dropPosition)) 
        {
          if (this.mCurrentDragOverButton)
            {
              this.mCurrentDragOverButton.removeAttribute("dragover-left");
              this.mCurrentDragOverButton.removeAttribute("dragover-right");
              this.mCurrentDragOverButton.removeAttribute("dragover-top");
              this.mCurrentDragOverButton.removeAttribute("open");
            }
          this.mCurrentDragOverButton = aEvent.target;
          this.mCurrentDragPosition = dropPosition;
        }
      
      switch (dropPosition) 
        {
          case -1: 
            aEvent.target.setAttribute("dragover-left", "true");
            break;
          case 1:
            aEvent.target.setAttribute("dragover-right", "true");
            break;
          case 0:
          default:
            if (aEvent.target.getAttribute("container") == "true") {
              aEvent.target.setAttribute("dragover-top", "true");
              //cant open a menu during a drag! suck!
              //aEvent.target.setAttribute("open", "true");
            }
            break;
        }
        
       return true;
    },

  getSupportedFlavours: function ()
    {
      // temporary
      if (!isPToolbarDNDEnabled())
        return false;
        
      var flavourList = { };
      flavourList["moz/toolbaritem"] = { width: 2, iid: "nsISupportsWString" };
      flavourList["text/unicode"] = { width: 2, iid: "nsISupportsWString" };
      return flavourList;
    },

  determineDropPosition: function (aEvent)
    {
      var overButton = aEvent.target;
      var overButtonBoxObject = overButton.boxObject.QueryInterface(Components.interfaces.nsIBoxObject);

      if (aEvent.clientX < (overButtonBoxObject.x + overButtonBoxObject.width/3)) 
        {
          if (aEvent.clientY > overButtonBoxObject.y && 
              aEvent.clientY < overButtonBoxObject.y + overButtonBoxObject.height)
            return -1;
        }
      else if (aEvent.clientX > (overButtonBoxObject.x + 2*(overButtonBoxObject.width/3))) 
        {
          if (aEvent.clientY > overButtonBoxObject.y && 
              aEvent.clientY < overButtonBoxObject.y + overButtonBoxObject.height) 
            return 1;
        }
      else if (aEvent.clientX > (overButtonBoxObject.x + overButtonBoxObject.width/3) &&
               aEvent.clientX < ((overButtonBoxObject.x + 2*(overButtonBoxObject.width/3)))) 
        {
          if (aEvent.clientY > overButtonBoxObject.y && 
              aEvent.clientY < overButtonBoxObject.y + overButtonBoxObject.height) 
            return 0;
        }
        
      return 0;
    }
}; 

//
// DragProxyIcon
//
// Called when the user is starting a drag from the proxy icon next to the URL bar. Basically
// just gets the url from the url bar and places the data (as plain text) in the drag service.
//
// This is by no means the final implementation, just another example of what you can do with
// JS. Much still left to do here.
// 

var proxyIconDNDObserver = {
  onDragStart: function ()
    {
      var urlBar = document.getElementById("urlbar");
      var flavourList = { };
      flavourList["text/unicode"] = { width: 2, data: urlBar.value };
      flavourList["text/x-moz-url"] = { width: 2, data: urlBar.value + "\n" + window._content.document.title };
      var htmlString = "<a href=\"" + urlBar.value + "\">" + urlBar.value + "</a>";
      flavourList["text/html"] = { width: 2, data: htmlString };
      return flavourList;
    }
};

var homeButtonObserver = {
  onDrop: function (aEvent, aData, aDragSession)
    {
      var data = aData.length ? aData[0] : aData;
      var url = retrieveURLFromData(data);
      var commonDialogService = nsJSComponentManager.getService("@mozilla.org/appshell/commonDialogs;1",
                                "nsICommonDialogs");
      var pressedVal = { };                            
      var promptTitle = gNavigatorBundle.getString("droponhometitle");
      var promptMsg   = gNavigatorBundle.getString("droponhomemsg");
      var okButton    = gNavigatorBundle.getString("droponhomeokbutton");
      var iconURL     = "chrome://navigator/skin/home.gif"; // evil evil common dialog code! evil! 

      commonDialogService.UniversalDialog(window, null, promptTitle, promptMsg, null, 
                                          okButton, null, null, null, null, null, { }, { },
                                          iconURL, { }, 2, 0, null, pressedVal);

      if (pressedVal.value == 0) {
        nsPreferences.setUnicharPref("browser.startup.homepage", url);
        setTooltipText("home-button", url);
      }
    },
    
  onDragOver: function (aEvent, aFlavour, aDragSession)
    {
      var statusTextFld = document.getElementById("statusbar-display");
      statusTextFld.setAttribute("value", gNavigatorBundle.getString("droponhomebutton"));
    },
    
  onDragExit: function (aEvent, aDragSession)
    {
      statusTextFld.setAttribute("value", "");
    },
        
  onDragStart: function ()
    {
      var homepage = nsPreferences.getLocalizedUnicharPref("browser.startup.homepage", "about:blank");
      var flavourList = { };
      var htmlString = "<a href=\"" + homepage + "\">" + homepage + "</a>";
      flavourList["text/x-moz-url"] = { width: 2, data: homepage };
      flavourList["text/html"] = { width: 2, data: htmlString };
      flavourList["text/unicode"] = { width: 2, data: homepage };
      return flavourList;
    },
  
  getSupportedFlavours: function ()
    {
      var flavourList = { };
      //flavourList["moz/toolbaritem"] = { width: 2 };
      flavourList["text/unicode"] = { width: 2, iid: "nsISupportsWString" };
      flavourList["application/x-moz-file"] = { width: 2, iid: "nsIFile" };
      return flavourList;
    }
};
