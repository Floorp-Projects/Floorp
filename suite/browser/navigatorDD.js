/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *  - Kevin Puetz (puetzk@iastate.edu)
 *  - Ben Goodger <ben@netscape.com> 
 *  - Blake Ross <blaker@netscape.com>
 *  - Pierre Chanial <pierrechanial@netscape.net>
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
      return this.rdf.GetResource(aString, true);
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
    },
  _rdf: null,
  get rdf() {
    if (!this._rdf) {
      this._rdf = Components.classes["@mozilla.org/rdf/rdf-service;1"]
                            .getService(Components.interfaces.nsIRDFService);
    }
    return this._rdf;
  }
}

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
}

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
      setTimeout(openHomeDialog, 0, url);
    },

  onDragOver: function (aEvent, aFlavour, aDragSession)
    {
      var statusTextFld = document.getElementById("statusbar-display");
      statusTextFld.label = gNavigatorBundle.getString("droponhomebutton");
      aDragSession.dragAction = Components.interfaces.nsIDragService.DRAGDROP_ACTION_LINK;
    },

  onDragExit: function (aEvent, aDragSession)
    {
      var statusTextFld = document.getElementById("statusbar-display");
      statusTextFld.label = "";
    },

  getSupportedFlavours: function ()
    {
      var flavourSet = new FlavourSet();
      flavourSet.appendFlavour("application/x-moz-file", "nsIFile");
      flavourSet.appendFlavour("text/x-moz-url");
      flavourSet.appendFlavour("text/unicode");
      return flavourSet;
    }
}

function openHomeDialog(aURL)
{
  var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(Components.interfaces.nsIPromptService);
  var promptTitle = gNavigatorBundle.getString("droponhometitle");
  var promptMsg   = gNavigatorBundle.getString("droponhomemsg");
  var okButton    = gNavigatorBundle.getString("droponhomeokbutton");
  var pressedVal = promptService.confirmEx(window, promptTitle, promptMsg,
                          (promptService.BUTTON_TITLE_IS_STRING * promptService.BUTTON_POS_0) +
                          (promptService.BUTTON_TITLE_CANCEL * promptService.BUTTON_POS_1),
                          okButton, null, null, null, {value:0});

  if (pressedVal == 0) {
    nsPreferences.setUnicharPref("browser.startup.homepage", aURL);
    setTooltipText("home-button", aURL);
  }
}

var goButtonObserver = {
  onDragOver: function(aEvent, aFlavour, aDragSession)
    {
      aEvent.target.setAttribute("dragover", "true");
      return true;
    },
  onDragExit: function (aEvent, aDragSession)
    {
      aEvent.target.removeAttribute("dragover");
    },
  onDrop: function (aEvent, aXferData, aDragSession)
    {
      var xferData = aXferData.data.split("\n");
      var uri = xferData[0] ? xferData[0] : xferData[1];
      if (uri)
        loadURI(uri);
    },
  getSupportedFlavours: function ()
    {
      var flavourSet = new FlavourSet();
      flavourSet.appendFlavour("application/x-moz-file", "nsIFile");
      flavourSet.appendFlavour("text/x-moz-url");
      flavourSet.appendFlavour("text/unicode");
      return flavourSet;
    }
}

var searchButtonObserver = {
  onDragOver: function(aEvent, aFlavour, aDragSession)
    {
      aEvent.target.setAttribute("dragover", "true");
      return true;
    },
  onDragExit: function (aEvent, aDragSession)
    {
      aEvent.target.removeAttribute("dragover");
    },
  onDrop: function (aEvent, aXferData, aDragSession)
    {
      var xferData = aXferData.data.split("\n");
      var uri = xferData[1] ? xferData[1] : xferData[0];
      if (uri)
        OpenSearch('internet',false, uri);
    },
  getSupportedFlavours: function ()
    {
      var flavourSet = new FlavourSet();
      flavourSet.appendFlavour("application/x-moz-file", "nsIFile");
      flavourSet.appendFlavour("text/x-moz-url");
      flavourSet.appendFlavour("text/unicode");
      return flavourSet;
    }
}

var personalToolbarDNDObserver = {

  ////////////////////
  // Public methods //
  ////////////////////

  onDragStart: function (aEvent, aXferData, aDragAction)
    {

      // Prevent dragging from an invalid region
      if (!this.canDrop(aEvent))
        return;

      // Prevent dragging out of menupopups on non Win32 platforms. 
      // a) on Mac drag from menus is generally regarded as being satanic
      // b) on Linux, this causes an X-server crash, see bug 79003, 96504 and 139471
      // c) on Windows, there is no hang or crash associated with this, so we'll leave 
      // the functionality there. 
      if (navigator.platform != "Win32" && aEvent.target.localName != "toolbarbutton")
        return;

      // prevent dragging folders in the personal toolbar and menus.
      // PCH: under linux, since containers open on mouse down, we hit bug 96504. 
      // In addition, a drag start is fired when leaving an open toolbarbutton(type=menu) 
      // menupopup (see bug 143031)
      if (this.isContainer(aEvent.target) && aEvent.target.getAttribute("group") != "true") {
        if (this.isPlatformNotSupported) 
          return;
        if (!aEvent.shiftKey && !aEvent.altKey)
          return;
        // menus open on mouse down
        aEvent.target.firstChild.hidePopup();
      }

      // Prevent dragging non-bookmark menuitem or menus
      var uri = aEvent.target.id;
      if (!this.isBookmark(uri))
        return;

      //PCH: cleanup needed here, url is already calculated in isBookmark()
      var db = document.getElementById("innermostBox").database;
      var url = RDFUtils.getTarget(db, uri, NC_RDF("URL"));
      if (url)
        url = url.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
      else
        url = "";
      var name = RDFUtils.getTarget(db, uri, NC_RDF("Name"));
      if (name)
        name = name.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
      else
        name = "";
      var urlString = url + "\n" + name;
      var htmlString = "<A HREF='" + url + "'>" + name + "</A>";
      aXferData.data = new TransferData();
      aXferData.data.addDataForFlavour("moz/rdfitem", uri);
      aXferData.data.addDataForFlavour("text/x-moz-url", urlString);
      aXferData.data.addDataForFlavour("text/html", htmlString);
      aXferData.data.addDataForFlavour("text/unicode", url);

      return;
    },

  onDragOver: function(aEvent, aFlavour, aDragSession) 
  {
    if (aDragSession.canDrop)
      this.onDragSetFeedBack(aEvent);
    if (this.isPlatformNotSupported)
      return;
    if (this.isTimerSupported)
      return;
    this.onDragOverCheckTimers();
    return;
  },

  onDragEnter: function (aEvent, aDragSession)
  {
    var target = aEvent.target;
    if (target.localName == "menupopup" || target.localName == "hbox")
      target = target.parentNode;
    if (aDragSession.canDrop) {
      this.onDragSetFeedBack(aEvent);
      this.onDragEnterSetTimer(target, aDragSession);
    }
    this.mCurrentDragOverTarget = target;
    return;
  },

  onDragExit: function (aEvent, aDragSession)
  {
    var target = aEvent.target;
    if (target.localName == "menupopup" || target.localName == "hbox")
      target = target.parentNode;
    this.onDragRemoveFeedBack(target);
    this.onDragExitSetTimer(target, aDragSession);
    this.mCurrentDragOverTarget = null;
    return;
  },

  onDrop: function (aEvent, aXferData, aDragSession)
  {

    this.onDragRemoveFeedBack(aEvent.target);
    var dropPosition = this.determineDropPosition(aEvent);

    // PCH: BAD!!! We should use the flavor
    // this code should be merged with the one in bookmarks.xml
    var xferData = aXferData.data.split("\n");
    if (xferData[0] == "")
      return;
    var elementRes = RDFUtils.getResource(xferData[0]);

    var childDB = document.getElementById("innermostBox").database;
    var rdfContainer = Components.classes["@mozilla.org/rdf/container;1"].createInstance(Components.interfaces.nsIRDFContainer);

    // if dragged url is already bookmarked, remove it from current location first
    var parentContainer = this.findParentContainer(aDragSession.sourceNode);
    if (parentContainer) {
      rdfContainer.Init(childDB, parentContainer);
      rdfContainer.RemoveElement(elementRes, false);
    }

    // determine charset of link
    var linkCharset = aDragSession.sourceDocument ? aDragSession.sourceDocument.characterSet : null;
    // determine title of link
    var linkTitle;
    // look it up in bookmarks
    var bookmarksDS = RDFUtils.rdf.GetDataSource("rdf:bookmarks");
    var nameRes = RDFUtils.getResource(NC_RDF("Name"));
    var nameFromBookmarks = bookmarksDS.GetTarget(elementRes, nameRes, true);
    if (nameFromBookmarks)
      nameFromBookmarks = nameFromBookmarks.QueryInterface(Components.interfaces.nsIRDFLiteral);
    if (nameFromBookmarks)
      linkTitle = nameFromBookmarks.Value;
    else if (xferData.length >= 2)
      linkTitle = xferData[1]
    else {
      // look up this URL's title in global history
      var historyDS = RDFUtils.rdf.GetDataSource("rdf:history");
      var titlePropRes = RDFUtils.getResource(NC_RDF("Name"));
      var titleFromHistory = historyDS.GetTarget(elementRes, titlePropRes, true);
      if (titleFromHistory)
        titleFromHistory = titleFromHistory.QueryInterface(Components.interfaces.nsIRDFLiteral);
      if (titleFromHistory)
        linkTitle = titleFromHistory.Value;
    }

    var dropIndex;
    if (aEvent.target.id == "bookmarks-button") 
      // dropPosition is always DROP_ON
      parentContainer = RDFUtils.getResource("NC:BookmarksRoot");
    else if (aEvent.target.id == "innermostBox") 
      parentContainer = RDFUtils.getResource("NC:PersonalToolbarFolder");
    else if (dropPosition == this.DROP_ON) 
      parentContainer = RDFUtils.getResource(aEvent.target.id);
    else {
      parentContainer = this.findParentContainer(aEvent.target);
      rdfContainer.Init(childDB, parentContainer);
      var dropElementRes = RDFUtils.getResource(aEvent.target.id);
      dropIndex = rdfContainer.IndexOf(dropElementRes);
    }
    switch (dropPosition) {
      case this.DROP_BEFORE:
        if (dropIndex<1) dropIndex = 1;
        break;
      case this.DROP_ON:
        dropIndex = -1;
        break;
      case this.DROP_AFTER:
        if (dropIndex <= rdfContainer.GetCount()) ++dropIndex;         
        if (dropIndex<1) dropIndex = -1;
        break;
    }

    this.insertBookmarkAt(xferData[0], linkTitle, linkCharset, parentContainer, dropIndex);       
    return;
  },

  canDrop: function (aEvent, aDragSession)
  {
    var target = aEvent.target;
    return target.id && target.localName != "menupopup" && target.localName != "toolbar" &&
           target.localName != "menuseparator" && target.localName != "toolbarseparator" &
           target.id != "NC:SystemBookmarksStaticRoot" &&
           target.id.substring(0,5) != "find:";
  },

  getSupportedFlavours: function () 
  {
    var flavourSet = new FlavourSet();
    flavourSet.appendFlavour("moz/rdfitem");
    flavourSet.appendFlavour("text/unicode");
    flavourSet.appendFlavour("application/x-moz-file", "nsIFile");
    flavourSet.appendFlavour("text/x-moz-url");
    return flavourSet;
  }, 
  

  ////////////////////////////////////
  // Private methods and properties //
  ////////////////////////////////////

  DROP_BEFORE:-1,
  DROP_ON    : 0,
  DROP_AFTER : 1,
  springLoadedMenuDelay: 350, // milliseconds
  isPlatformNotSupported: navigator.platform.indexOf("Linux") != -1,
  isTimerSupported: navigator.platform.indexOf("Win") > -1 || navigator.platform.indexOf("Mac") > -1,

  mCurrentDragOverTarget: null,
  loadTimer  : null,
  closeTimer : null,
  loadTarget : null,
  closeTarget: null,


  onDragCloseMenu: function (aNode)
  {
    var children = aNode.childNodes;
    for (var i = 0; i < children.length; i++) {
      if (children[i].id == "innermostBox") {
        this.onDragCloseMenu(children[i]);
      }
      else if (this.isContainer(children[i]) && children[i].getAttribute("open") == "true") {
        this.onDragCloseMenu(children[i].firstChild);
        if (children[i] != this.mCurrentDragOverTarget)
          children[i].firstChild.hidePopup();
      }
    } 
  },

  onDragCloseTarget: function ()
  {
    // if the mouse is not over a menu, then close everything.
    if (!this.mCurrentDragOverTarget) {
      this.onDragCloseMenu(document.getElementById("PersonalToolbar"));
      return
    }
    // The bookmark button is not a sibling of the folders in the PT
    if (this.mCurrentDragOverTarget.parentNode.id == "innermostBox")
      this.onDragCloseMenu(document.getElementById("PersonalToolbar"));
    else
      this.onDragCloseMenu(this.mCurrentDragOverTarget.parentNode);
  },

  onDragLoadTarget: function (aTarget) 
  {
    if (!this.mCurrentDragOverTarget)
      return;
    // Load the current menu
    if (this.isContainer(aTarget) && aTarget.getAttribute("group") != "true")
      aTarget.firstChild.showPopup(aTarget, -1, -1, "menupopup");
  },

  onDragOverCheckTimers: function ()
  {
    var now = new Date().getTime();
    if (this.closeTimer && now-this.springLoadedMenuDelay>this.closeTimer) {
      this.onDragCloseTarget();
      this.closeTimer = null;
    }
    if (this.loadTimer && (now-this.springLoadedMenuDelay>this.loadTimer)) {
      this.onDragLoadTarget(this.loadTarget);
      this.loadTimer = null;
    }
  },

  onDragEnterSetTimer: function (aTarget, aDragSession)
  {
    if (this.isPlatformNotSupported)
      return;
    if (this.isTimerSupported) {
      var targetToBeLoaded = aTarget;
      clearTimeout(this.loadTimer);
      if (aTarget == aDragSession.sourceNode)
        return;
      //XXX Hack: see bug 139645
      var thisHack = this;
      this.loadTimer=setTimeout(function () {thisHack.onDragLoadTarget(targetToBeLoaded)}, this.springLoadedMenuDelay);
    } else {
      var now = new Date().getTime();
      this.loadTimer  = now;
      this.loadTarget = aTarget;
    }
  },

  onDragExitSetTimer: function (aTarget, aDragSession)
  {
    if (this.isPlatformNotSupported)
      return;
    var thisHack = this;
    if (this.isTimerSupported) {
      clearTimeout(this.closeTimer)
      this.closeTimer=setTimeout(function () {thisHack.onDragCloseTarget()}, this.springLoadedMenuDelay);
    } else {
      var now = new Date().getTime();
      this.closeTimer  = now;
      this.closeTarget = aTarget;
      this.loadTimer = null;

      // If user isn't rearranging within the menu, close it
      // To do so, we exploit a Mac bug: timeout set during
      // drag and drop on Windows and Mac are fired only after that the drop is released.
      // timeouts will pile up, we may have a better approach but for the moment, this one
      // correctly close the menus after a drop/cancel outside the personal toolbar.
      // The if statement in the function has been introduced to deal with rare but reproducible
      // missing Exit events.
      if (aDragSession.sourceNode.localName != "menuitem" && aDragSession.sourceNode.localName != "menu")
        setTimeout(function () { if (thisHack.mCurrentDragOverTarget) {thisHack.onDragRemoveFeedBack(thisHack.mCurrentDragOverTarget); thisHack.mCurrentDragOverTarget=null} thisHack.loadTimer=null; thisHack.onDragCloseTarget() }, 0);
    }
  },

  onDragSetFeedBack: function (aEvent)
  {
    var target = aEvent.target
    var dropPosition = this.determineDropPosition(aEvent)
    switch (target.localName) {
      case "toolbarseparator":
      case "toolbarbutton":
        if (this.isContainer(target)) {
          target.setAttribute("dragover-left"  , "true");
          target.setAttribute("dragover-right" , "true");
          target.setAttribute("dragover-top"   , "true");
          target.setAttribute("dragover-bottom", "true");
        }
        else {
          switch (dropPosition) {
            case this.DROP_BEFORE: 
              target.removeAttribute("dragover-right");
              target.setAttribute("dragover-left", "true");
              break;
            case this.DROP_AFTER:
              target.removeAttribute("dragover-left");
              target.setAttribute("dragover-right", "true");
              break;
          }
        }
        break;
      case "menuseparator": 
      case "menu":
      case "menuitem":
        switch (dropPosition) {
          case this.DROP_BEFORE: 
            target.removeAttribute("dragover-bottom");
            target.setAttribute("dragover-top", "true");
            break;
          case this.DROP_AFTER:
            target.removeAttribute("dragover-top");
            target.setAttribute("dragover-bottom", "true");
            break;
        }
        break;
      case "hbox"     : 
        target.lastChild.setAttribute("dragover-right", "true");
        break;
      case "menupopup": 
      case "toolbar"  : break;
      default: dump("No feedback for: "+target.localName+"\n");
    }
  },

  onDragRemoveFeedBack: function (aTarget)
  {
    if (aTarget.localName == "hbox") {
      aTarget.lastChild.removeAttribute("dragover-right");
    } else {
      aTarget.removeAttribute("dragover-left");
      aTarget.removeAttribute("dragover-right");
      aTarget.removeAttribute("dragover-top");
      aTarget.removeAttribute("dragover-bottom");
    }
  },

  onDropSetFeedBack: function (aTarget)
  {
    //XXX Not yet...
  },

  isContainer: function (aTarget)
  {
    return (aTarget.localName == "menu" || aTarget.localName == "toolbarbutton") &&
           (aTarget.getAttribute("container") == "true" || aTarget.getAttribute("group") == "true");
  },


  ///////////////////////////////////////////////////////
  // Methods that need to be moved into BookmarksUtils //
  ///////////////////////////////////////////////////////

  //XXXPCH this function returns wrong vertical positions
  //XXX skin authors could break us, we'll cross that bridge when they turn us 90degrees
  determineDropPosition: function (aEvent)
  {
    var overButtonBoxObject = aEvent.target.boxObject.QueryInterface(Components.interfaces.nsIBoxObject);
    // most things only drop on the left or right
    var regionCount = 2;

    // you can drop ONTO containers, so there is a "middle" region
    if (this.isContainer(aEvent.target))
      return this.DROP_ON;
      
    var measure;
    var coordValue;
    var clientCoordValue;
    if (aEvent.target.localName == "menuitem") {
      measure = overButtonBoxObject.height/regionCount;
      coordValue = overButtonBoxObject.y;
      clientCoordValue = aEvent.clientY;
    }
    else if (aEvent.target.localName == "toolbarbutton") {
      measure = overButtonBoxObject.width/regionCount;
      coordValue = overButtonBoxObject.x;
      clientCoordValue = aEvent.clientX;
    }
    else
      return this.DROP_ON;
    
    // in the first region?
    if (clientCoordValue < (coordValue + measure))
      return this.DROP_BEFORE;
    // in the last region?
    if (clientCoordValue >= (coordValue + (regionCount - 1)*measure))
      return this.DROP_AFTER;
    // must be in the middle somewhere
    return this.DROP_ON;
  },

  isBookmark: function (aURI)
  {
    if (!aURI)
      return false;
    var db = document.getElementById("innermostBox").database;
    var typeValue = RDFUtils.getTarget(db, aURI, _RDF("type"));
    typeValue = RDFUtils.getValueFromResource(typeValue);
    return (typeValue == NC_RDF("BookmarkSeparator") ||
            typeValue == NC_RDF("Bookmark") ||
            typeValue == NC_RDF("Folder"))
  },

  // returns the parent resource of the dragged element. This is determined
  // by inspecting the source element of the drag and walking up the DOM tree
  // to find the appropriate containing node.
  findParentContainer: function (aElement)
  {
    if (!aElement) return null;
    switch (aElement.localName) {
      case "toolbarbutton":
        var box = aElement.parentNode;
        return RDFUtils.getResource(box.getAttribute("ref"));
      case "menu":
      case "menuitem":
        var parentNode = aElement.parentNode.parentNode;
     
        if (parentNode.getAttribute("type") != NC_RDF("Folder") &&
            parentNode.getAttributeNS("http://www.w3.org/1999/02/22-rdf-syntax-ns#", "type") != "http://home.netscape.com/NC-rdf#Folder")
          return RDFUtils.getResource("NC:BookmarksRoot");
        return RDFUtils.getResource(parentNode.id);
      case "treecell":
        var treeitem = aElement.parentNode.parentNode.parentNode.parentNode;
        var res = treeitem.getAttribute("ref");
        if (!res)
          res = treeitem.id;            
        return RDFUtils.getResource(res);
    }
    return null;
  },

  insertBookmarkAt: function (aURL, aTitle, aCharset, aFolderRes, aIndex)
  {
    const kBMSContractID = "@mozilla.org/browser/bookmarks-service;1";
    const kBMSIID = Components.interfaces.nsIBookmarksService;
    const kBMS = Components.classes[kBMSContractID].getService(kBMSIID);
    kBMS.createBookmarkWithDetails(aTitle, aURL, aCharset, aFolderRes, aIndex);
  }

}
