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

function _RDF(aType) 
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
      return aResource ? aResource.Value : null;
    }
};

function isBookmark(aURI)
  {
    var db = document.getElementById("innermostBox").database;
    var typeValue = RDFUtils.getTarget(db, aURI, _RDF("type"));
    typeValue = RDFUtils.getValueFromResource(typeValue);
    return (typeValue == NC_RDF("BookmarkSeparator") ||
            typeValue == NC_RDF("Bookmark") ||
            typeValue == NC_RDF("Folder")) 
  }

var personalToolbarObserver = {
  onDragStart: function (aEvent, aXferData, aDragAction)
    {
      var personalToolbar = document.getElementById("PersonalToolbar");
      if (aEvent.target == personalToolbar) return;
        
      var db = document.getElementById("innermostBox").database;
      var uri = aEvent.target.id;

      if (!isBookmark(uri)) return;
      var url = RDFUtils.getTarget(db, uri, NC_RDF("URL"));
      var name = RDFUtils.getTarget(db, uri, NC_RDF("Name"));

      var urlString = url + "\n" + name;
      var htmlString = "<A HREF='" + uri + "'>" + name + "</A>";

      aXferData.data = new TransferData();
      aXferData.data.addDataForFlavour("moz/rdfitem", uri);
      aXferData.data.addDataForFlavour("text/x-moz-url", urlString);
      aXferData.data.addDataForFlavour("text/html", htmlString);
      aXferData.data.addDataForFlavour("text/unicode", url);
    },
  
  onDrop: function (aEvent, aXferData, aDragSession) 
    {
      var dropElement = aEvent.target.id;
      var elementRes = RDFUtils.getResource(aXferData.data);
      var dropElementRes = RDFUtils.getResource(dropElement);
      var personalToolbarRes = RDFUtils.getResource("NC:PersonalToolbarFolder");
      
      var childDB = document.getElementById("innermostBox").database;
      const kCtrContractID = "@mozilla.org/rdf/container;1";
      const kCtrIID = Components.interfaces.nsIRDFContainer;
      var rdfContainer = Components.classes[kCtrContractID].getService(kCtrIID);
      
      var parentContainer = this.findParentContainer(aDragSession.sourceElement);
      if (parentContainer) 
        { 
          rdfContainer.Init(childDB, parentContainer);
          rdfContainer.RemoveElement(elementRes, true);
        }
      else
        {
          // look up this URL's title in global history
          var potentialTitle = null;
          var historyDS = gRDFService.GetDataSource("rdf:history");
          var historyTitleProperty = RDFUtils.getResource(NC_RDF("Name"));
          var titleFromHistory = historyDS.GetTarget(elementRes, historyTitleProperty, true);
          if (titleFromHistory)
            titleFromHistory = titleFromHistory.QueryInterface(Components.interfaces.nsIRDFLiteral);
          if (titleFromHistory)
            potentialTitle = titleFromHistory.Value;
          linkTitle = potentialTitle ? potentialTitle : element;
          childDB.Assert(elementRes, historyTitleProperty, 
                         gRDFService.GetLiteral(linkTitle), true);
        }
      rdfContainer.Init (childDB, personalToolbarRes);
      var dropIndex = rdfContainer.IndexOf(dropElementRes);
      // determine the drop position
      var dropPosition = this.determineDropPosition(aEvent);
      switch (dropPosition) {
      case -1:
        rdfContainer.Init(childDB, personalToolbarRes);
        dump("*** elt= " + elementRes.Value + "\n");
        rdfContainer.InsertElementAt(elementRes, dropIndex, true);
        break;
      case 0:
        // do something here to drop into subfolders
        rdfContainer.Init(childDB, dropElementRes);
        rdfContainer.AppendElement(elementRes);
        break;
      case 1:
      default:
        rdfContainer.Init (childDB, personalToolbarRes);
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
      var flavourSet = new FlavourSet();
      flavourSet.appendFlavour("moz/rdfitem");
      // application/x-moz-file
      flavourSet.appendFlavour("text/unicode");
      return flavourSet;
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
    },

  // returns the parent resource of the dragged element. This is determined
  // by inspecting the source element of the drag and walking up the DOM tree
  // to find the appropriate containing node.    
  findParentContainer: function (aElement) 
    {
      switch (aElement.localName) 
        {
          case "button":
          case "menubutton":
            var box = aElement.parentNode;
            return RDFUtils.getResource(box.getAttribute("ref"));
          case "menu":
          case "menuitem":
            var menu = aElement.parentNode.parentNode;
            return RDFUtils.getResource(menu.id);
          case "treecell":
            var treeitem = aElement.parentNode.parentNode.parentNode.parentNode;
            return RDFUtils.getResource(treeitem.id);
        }
      return null;
    }
}; 

var proxyIconDNDObserver = {
  onDragStart: function (aEvent, aXferData, aDragAction)
    {
      var urlBar = document.getElementById("urlbar");
      
      // XXX - do we want to allow the user to set a blank page to their homepage?
      //       if so then we want to modify this a little to set about:blank as 
      //       the homepage in the event of an empty urlbar. 
      if (!urlBar.value) return;

      var urlString = urlBar.value + "\n" + window._content.document.title;
      var htmlString = "<a href=\"" + urlBar.value + "\">" + urlBar.value + "</a>";

      aXferData.data = new TransferData();
      aXferData.data.addDataForFlavour("text/x-moz-url", urlString);
      aXferData.data.addDataForFlavour("text/unicode", urlBar.value);
      aXferData.data.addDataForFlavour("text/html", htmlString);
    }
};

var homeButtonObserver = {
  onDragStart: function (aEvent, aXferData, aDragAction)
    {
      var homepage = nsPreferences.getLocalizedUnicharPref("browser.startup.homepage", "about:blank");

      if (homepage)
        {
          // XXX find a readable title string for homepage, perhaps do a history lookup.
          var htmlString = "<a href=\"" + homepage + "\">" + homepage + "</a>";
          aXferData.data = new TransferData();
          aXferData.data.addDataForFlavour("text/x-moz-url", homepage + "\n" + homepage);
          aXferData.data.addDataForFlavour("text/html", htmlString);
          aXferData.data.addDataForFlavour("text/unicode", homepage);
        }
    },
  
  onDrop: function (aEvent, aXferData, aDragSession)
    {
      var url = retrieveURLFromData(aXferData.data, aXferData.flavour.contentType);
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
      statusTextFld.setAttribute("value", bundle.GetStringFromName("droponhomebutton"));
      aDragSession.dragAction = Components.interfaces.nsIDragService.DRAGDROP_ACTION_LINK;
    },
    
  onDragExit: function (aEvent, aDragSession)
    {
      statusTextFld.setAttribute("value", "");
    },
        
  getSupportedFlavours: function ()
    {
      var flavourSet = new FlavourSet();
      flavourSet.appendFlavour("application/x-moz-file", "nsIFile");
      flavourSet.appendFlavour("text/x-moz-url");
      flavourSet.appendFlavour("text/unicode");
      return flavourSet;
    }
};
