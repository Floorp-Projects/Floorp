/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Pierre Chanial <chanial@noos.fr>.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

var BookmarksMenu = {
  _selection:null,
  _target:null,
  _orientation:null,
  

  //////////////////////////////////////////////////////////////////////////
  // Fill a context menu popup with menuitems appropriate for the current
  // selection.
  createContextMenu: function (aEvent)
  {
    var target = document.popupNode;
    target.focus() // buttons in the pt have -moz-user-focus: ignore -->
    this._selection   = this.getBTSelection(target);
    this._orientation = this.getBTOrientation(aEvent, target);
    this._target      = this.getBTTarget(target, this._orientation);
    BookmarksCommand.createContextMenu(aEvent, this._selection);
    this.onCommandUpdate();
    aEvent.target.addEventListener("mousemove", BookmarksMenuController.onMouseMove, false)
  },

  /////////////////////////////////////////////////////////////////////////
  // Clean up after closing the context menu popup
  destroyContextMenu: function (aEvent)
  {
    if (content) 
      content.focus()
    BookmarksMenuDNDObserver.onDragRemoveFeedBack(document.popupNode); // needed on cancel
    aEvent.target.removeEventListener("mousemove", BookmarksMenuController.onMouseMove, false)
  },

  /////////////////////////////////////////////////////////////////////////////
  // returns the formatted selection from aNode
  getBTSelection: function (aNode)
  {
    var item;
    switch (aNode.id) {
    case "bookmarks-ptf":
      item = "NC:PersonalToolbarFolder";
      break;
    case "BookmarksMenu":
      item = "NC:BookmarksRoot";
      break;
    default:
      item = aNode.id;
    }
    if (!this.isBTBookmark(item))
      return {length:0};
    var parent           = this.getBTContainer(aNode);
    var isExpanded       = aNode.hasAttribute("open") && aNode.open;
    var selection        = {};
    selection.item       = [RDF.GetResource(item)];
    selection.parent     = [RDF.GetResource(parent)];
    selection.isExpanded = [isExpanded];
    selection.length     = selection.item.length;
    BookmarksUtils.checkSelection(selection);
    return selection;
  },

  /////////////////////////////////////////////////////////////////////////
  // returns the insertion target from aNode
  getBTTarget: function (aNode, aOrientation)
  {
    var item, parent, index;
    switch (aNode.id) {
    case "bookmarks-ptf":
      parent = "NC:PersonalToolbarFolder";
      item = BookmarksToolbar.getLastVisibleBookmark();
      break;
    case "BookmarksMenu":
      parent = "NC:BookmarksRoot";
      break;
    case "bookmarks-button":
      parent = "NC:BookmarksRoot";
      break;
    case "bookmarks-chevron":
      parent = "NC:PersonalToolbarFolder";
      item = document.getElementById("bookmarks-ptf").lastChild;
      aOrientation == BookmarksUtils.DROP_AFTER;
      break;
    default:
      if (aOrientation == BookmarksUtils.DROP_ON)
        parent = aNode.id
      else {
        parent = this.getBTContainer(aNode);
        item = aNode;
      }
    }

    parent = RDF.GetResource(parent);
    if (aOrientation == BookmarksUtils.DROP_ON)
      return BookmarksUtils.getTargetFromFolder(parent);

    item = RDF.GetResource(item.id);
    RDFC.Init(BMDS, parent);
    index = RDFC.IndexOf(item);
    if (aOrientation == BookmarksUtils.DROP_AFTER)
      ++index;

    return { parent: parent, index: index };
  },

  /////////////////////////////////////////////////////////////////////////
  // returns the parent resource of a node in the personal toolbar.
  // this is determined by inspecting the source element and walking up the 
  // DOM tree to find the appropriate containing node.
  getBTContainer: function (aNode)
  {
    var parent;
    var item = aNode.id;
    if (!this.isBTBookmark(item))
      return "NC:BookmarksRoot"
    parent = aNode.parentNode.parentNode;
    parent = parent.id;
    switch (parent) {
    case "BookmarksMenu":
      return "NC:BookmarksRoot";
    case "PersonalToolbar":
    case "bookmarks-chevron":
      return "NC:PersonalToolbarFolder";
    case "bookmarks-button":
      return "NC:BookmarksRoot";
    default:
      return parent;
    }
  },

  ///////////////////////////////////////////////////////////////////////////
  // returns true if the node is a bookmark, a folder or a bookmark separator
  isBTBookmark: function (aURI)
  {
    if (!aURI)
      return false;
    var type = BookmarksUtils.resolveType(aURI);
    return (type == "BookmarkSeparator" ||
            type == "Bookmark"          ||
            type == "Folder"            ||
            type == "FolderGroup"       ||
            type == "PersonalToolbarFolder")
  },

  /////////////////////////////////////////////////////////////////////////
  // returns true if the node is a container. -->
  isBTContainer: function (aTarget)
  {
    return  aTarget.localName == "menu" || (aTarget.localName == "toolbarbutton" &&
           (aTarget.getAttribute("container") == "true" || aTarget.getAttribute("group") == "true"));
  },

  /////////////////////////////////////////////////////////////////////////
  // returns BookmarksUtils.DROP_BEFORE, DROP_ON or DROP_AFTER accordingly
  // to the event coordinates. Skin authors could break us, we'll cross that 
  // bridge when they turn us 90degrees.  -->
  getBTOrientation: function (aEvent, aTarget)
  {
    var target
    if (!aTarget)
      target = aEvent.target;
    else
      target = aTarget;
    if (target.localName == "menu"                 &&
        target.parentNode.localName != "menupopup")
      return BookmarksUtils.DROP_ON;
    if (target.id == "bookmarks-ptf" || 
        target.id == "bookmarks-chevron") {
      return BookmarksUtils.DROP_ON;
    }

    var overButtonBoxObject = target.boxObject.QueryInterface(Components.interfaces.nsIBoxObject);
    var overParentBoxObject = target.parentNode.boxObject.QueryInterface(Components.interfaces.nsIBoxObject);

    var size, border;
    var coordValue, clientCoordValue;
    switch (target.localName) {
      case "toolbarseparator":
      case "toolbarbutton":
        size = overButtonBoxObject.width;
        coordValue = overButtonBoxObject.x;
        clientCoordValue = aEvent.clientX;
        break;
      case "menuseparator": 
      case "menu":
      case "menuitem":
        size = overButtonBoxObject.height;
        coordValue = overButtonBoxObject.y-overParentBoxObject.y;
        clientCoordValue = aEvent.clientY;
        break;
      default: return BookmarksUtils.DROP_ON;
    }
    if (this.isBTContainer(target))
      if (target.localName == "toolbarbutton") {
        // the DROP_BEFORE area excludes the label
        var iconNode = document.getAnonymousElementByAttribute(target, "class", "toolbarbutton-icon");
        border = parseInt(document.defaultView.getComputedStyle(target,"").getPropertyValue("padding-left")) +
                 parseInt(document.defaultView.getComputedStyle(iconNode     ,"").getPropertyValue("width"));
        border = Math.min(size/5,Math.max(border,4));
      } else
        border = size/5;
    else
      border = size/2;
    
    // in the first region?
    if (clientCoordValue-coordValue < border)
      return BookmarksUtils.DROP_BEFORE;
    // in the last region?
    if (clientCoordValue-coordValue >= size-border)
      return BookmarksUtils.DROP_AFTER;
    // must be in the middle somewhere
    return BookmarksUtils.DROP_ON;
  },

  /////////////////////////////////////////////////////////////////////////
  // expand the folder targeted by the context menu.
  expandBTFolder: function ()
  {
    var target = document.popupNode.lastChild;
    if (document.popupNode.open)
      target.hidePopup();
    else
      target.showPopup(document.popupNode);
  },

  onCommandUpdate: function ()
  {
    var selection = this._selection;
    var target    = this._target;
    BookmarksController.onCommandUpdate(selection, target);
    if (document.popupNode.id == "bookmarks-ptf") {
      // disabling 'cut' and 'copy' on the empty area of the personal toolbar
      var commandNode = document.getElementById("cmd_bm_cut");
      commandNode.setAttribute("disabled", "true");
      commandNode = document.getElementById("cmd_bm_copy");
      commandNode.setAttribute("disabled", "true");
    }
  },

  loadBookmark: function (aTarget, aDS)
  {
    // Check for invalid bookmarks (most likely a static menu item like "Manage Bookmarks")
    if (!this.isBTBookmark(aTarget.id))
      return;
    var rSource   = RDF.GetResource(aTarget.id);
    var selection = BookmarksUtils.getSelectionFromResource(rSource);
    BookmarksCommand.openBookmark(selection, "current", aDS)
  }
}

var BookmarksMenuController = {

  supportsCommand: BookmarksController.supportsCommand,

  isCommandEnabled: function (aCommand)
  {
    // warning: this is not the function called in BookmarksController.onCommandUpdate
    var selection = BookmarksMenu._selection;
    var target    = BookmarksMenu._target;
    if (selection)
      return BookmarksController.isCommandEnabled(aCommand, selection, target);
    return false;
  },

  doCommand: function (aCommand)
  {
    BookmarksMenuDNDObserver.onDragRemoveFeedBack(document.popupNode);
    var selection = BookmarksMenu._selection;
    var target    = BookmarksMenu._target;
    switch (aCommand) {
    case "cmd_bm_expandfolder":
      BookmarksMenu.expandBTFolder();
      break;
    default:
      BookmarksController.doCommand(aCommand, selection, target);
    }
  },

  onMouseMove: function (aEvent)
  {
    var command = aEvent.target.getAttribute("command");
    var isDisabled = aEvent.target.getAttribute("disabled")
    if (isDisabled != "true" && (command == "cmd_bm_newfolder" || command == "cmd_bm_paste")) {
      BookmarksMenuDNDObserver.onDragSetFeedBack(document.popupNode, BookmarksMenu._orientation);
    } else {
      BookmarksMenuDNDObserver.onDragRemoveFeedBack(document.popupNode);
    }
  }
}

var BookmarksMenuDNDObserver = {

  ////////////////////
  // Public methods //
  ////////////////////

  onDragStart: function (aEvent, aXferData, aDragAction)
  {
    var target = aEvent.target;

    // Prevent dragging from an invalid region
    if (!this.canDrop(aEvent))
      return;

    // Prevent dragging out of menupopups on non Win32 platforms. 
    // a) on Mac drag from menus is generally regarded as being satanic
    // b) on Linux, this causes an X-server crash, (bug 151336)
    // c) on Windows, there is no hang or crash associated with this, so we'll leave 
    // the functionality there. 
    if (navigator.platform != "Win32" && target.localName != "toolbarbutton")
      return;

    // bail if dragging from the empty area of the bookmarks toolbar
    if (target.id == "bookmarks-ptf")
      return

    // a drag start is fired when leaving an open toolbarbutton(type=menu) 
    // (see bug 143031)
    if (this.isContainer(target) && 
        target.getAttribute("group") != "true") {
      if (this.isPlatformNotSupported) 
        return;
      if (!aEvent.shiftKey && !aEvent.altKey && !aEvent.ctrlKey)
        return;
      // menus open on mouse down
      target.firstChild.hidePopup();
    }
    var selection  = BookmarksMenu.getBTSelection(target);
    aXferData.data = BookmarksUtils.getXferDataFromSelection(selection);
  },

  onDragOver: function(aEvent, aFlavour, aDragSession) 
  {
    var orientation = BookmarksMenu.getBTOrientation(aEvent)
    if (aDragSession.canDrop)
      this.onDragSetFeedBack(aEvent.target, orientation);
    if (orientation != this.mCurrentDropPosition) {
      // emulating onDragExit and onDragEnter events since the drop region
      // has changed on the target.
      this.onDragExit(aEvent, aDragSession);
      this.onDragEnter(aEvent, aDragSession);
    }
    if (this.isPlatformNotSupported)
      return;
    if (this.isTimerSupported)
      return;
    this.onDragOverCheckTimers();
  },

  onDragEnter: function (aEvent, aDragSession)
  {
    var target = aEvent.target;
    var orientation = BookmarksMenu.getBTOrientation(aEvent);
    if (target.localName == "menupopup" || target.id == "bookmarks-ptf")
      target = target.parentNode;
    if (aDragSession.canDrop) {
      this.onDragSetFeedBack(target, orientation);
      this.onDragEnterSetTimer(target, aDragSession);
    }
    this.mCurrentDragOverTarget = target;
    this.mCurrentDropPosition   = orientation;
  },

  onDragExit: function (aEvent, aDragSession)
  {
    var target = aEvent.target;
    if (target.localName == "menupopup" || target.id == "bookmarks-ptf")
      target = target.parentNode;
    this.onDragRemoveFeedBack(target);
    this.onDragExitSetTimer(target, aDragSession);
    this.mCurrentDragOverTarget = null;
    this.mCurrentDropPosition = null;
  },

  onDrop: function (aEvent, aXferData, aDragSession)
  {
    var target = aEvent.target;
    this.onDragRemoveFeedBack(target);

    var selection = BookmarksUtils.getSelectionFromXferData(aDragSession);

    var orientation = BookmarksMenu.getBTOrientation(aEvent);
    var selTarget   = BookmarksMenu.getBTTarget(target, orientation);

    const kDSIID      = Components.interfaces.nsIDragService;
    const kCopyAction = kDSIID.DRAGDROP_ACTION_COPY + kDSIID.DRAGDROP_ACTION_LINK;

    // hide the 'open in tab' menuseparator because bookmarks
    // can be inserted after it if they are dropped after the last bookmark
    // a more comprehensive fix would be in the menupopup template builder
    var menuTarget = (target.localName == "toolbarbutton" ||
                      target.localName == "menu")         && 
                     orientation == BookmarksUtils.DROP_ON?
                     target.lastChild:target.parentNode;
    if (menuTarget.hasChildNodes() &&
        menuTarget.lastChild.id == "openintabs-menuitem") {
      menuTarget.removeChild(menuTarget.lastChild.previousSibling);
    }

    if (aDragSession.dragAction & kCopyAction)
      BookmarksUtils.insertSelection("drag", selection, selTarget);
    else
      BookmarksUtils.moveSelection("drag", selection, selTarget);

    var chevron = document.getElementById("bookmarks-chevron");
    if (chevron.getAttribute("open") == "true") {
      BookmarksToolbar.resizeFunc(null);
      BookmarksToolbar.updateOverflowMenu(document.getElementById("bookmarks-chevron-popup"));
    }

    // show again the menuseparator
    if (menuTarget.hasChildNodes() &&
        menuTarget.lastChild.id == "openintabs-menuitem") {
      var element = document.createElementNS(XUL_NS, "menuseparator");
      menuTarget.insertBefore(element, menuTarget.lastChild);
    }

  },

  canDrop: function (aEvent, aDragSession)
  {
    var target = aEvent.target;
    return BookmarksMenu.isBTBookmark(target.id)       && 
           target.id != "NC:SystemBookmarksStaticRoot" &&
           target.id.substring(0,5) != "find:"         ||
           target.id == "BookmarksMenu"                ||
           target.id == "bookmarks-button"             ||
           target.id == "bookmarks-chevron"            ||
           target.id == "bookmarks-ptf";
  },

  canHandleMultipleItems: true,

  getSupportedFlavours: function () 
  {
    var flavourSet = new FlavourSet();
    flavourSet.appendFlavour("moz/rdfitem");
    flavourSet.appendFlavour("text/x-moz-url");
    flavourSet.appendFlavour("application/x-moz-file", "nsIFile");
    flavourSet.appendFlavour("text/unicode");
    return flavourSet;
  }, 
  

  ////////////////////////////////////
  // Private methods and properties //
  ////////////////////////////////////

  springLoadedMenuDelay: 350, // milliseconds
  isPlatformNotSupported: navigator.platform.indexOf("Mac") != -1, // see bug 136524
  isTimerSupported: navigator.platform.indexOf("Win") == -1,

  mCurrentDragOverTarget: null,
  mCurrentDropPosition: null,
  loadTimer  : null,
  closeTimer : null,
  loadTarget : null,
  closeTarget: null,

  _observers : null,
  get mObservers ()
  {
    if (!this._observers) {
      this._observers = [
        document.getElementById("bookmarks-ptf"),
        document.getElementById("BookmarksMenu").parentNode,
        document.getElementById("bookmarks-chevron").parentNode,
        document.getElementById("PersonalToolbar")
      ]
    }
    return this._observers;
  },

  getObserverForNode: function (aNode)
  {
    if (!aNode)
      return null;
    var node = aNode;
    var observer;
    do {
      for (var i=0; i < this.mObservers.length; i++) {
        observer = this.mObservers[i];
        if (observer == node)
          return observer;
      }
      node = node.parentNode;
    } while (node != document)
    return null;
  },

  onDragCloseMenu: function (aNode)
  {
    var children = aNode.childNodes;
    for (var i = 0; i < children.length; i++) {
      if (this.isContainer(children[i]) && 
          children[i].getAttribute("open") == "true") {
        this.onDragCloseMenu(children[i].lastChild);
        if (children[i] != this.mCurrentDragOverTarget || this.mCurrentDropPosition != BookmarksUtils.DROP_ON)
          children[i].lastChild.hidePopup();
      }
    } 
  },

  onDragCloseTarget: function ()
  {
    var currentObserver = this.getObserverForNode(this.mCurrentDragOverTarget);
    // close all the menus not hovered by the mouse
    for (var i=0; i < this.mObservers.length; i++) {
      if (currentObserver != this.mObservers[i])
        this.onDragCloseMenu(this.mObservers[i]);
      else
        this.onDragCloseMenu(this.mCurrentDragOverTarget.parentNode);
    }
  },

  onDragLoadTarget: function (aTarget) 
  {
    if (!this.mCurrentDragOverTarget)
      return;
    // Load the current menu
    if (this.mCurrentDropPosition == BookmarksUtils.DROP_ON && 
        this.isContainer(aTarget)             && 
        aTarget.getAttribute("group") != "true")
      aTarget.lastChild.showPopup(aTarget);
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
      var This = this;
      this.loadTimer=setTimeout(function () {This.onDragLoadTarget(targetToBeLoaded)}, This.springLoadedMenuDelay);
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
    var This = this;
    if (this.isTimerSupported) {
      clearTimeout(this.closeTimer)
      this.closeTimer=setTimeout(function () {This.onDragCloseTarget()}, This.springLoadedMenuDelay);
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
        setTimeout(function () { if (This.mCurrentDragOverTarget) {This.onDragRemoveFeedBack(This.mCurrentDragOverTarget); This.mCurrentDragOverTarget=null} This.loadTimer=null; This.onDragCloseTarget() }, 0);
    }
  },

  onDragSetFeedBack: function (aTarget, aOrientation)
  {
   switch (aTarget.localName) {
      case "toolbarseparator":
      case "toolbarbutton":
        switch (aOrientation) {
          case BookmarksUtils.DROP_BEFORE: 
            aTarget.setAttribute("dragover-left", "true");
            break;
          case BookmarksUtils.DROP_AFTER:
            aTarget.setAttribute("dragover-right", "true");
            break;
          case BookmarksUtils.DROP_ON:
            aTarget.setAttribute("dragover-top"   , "true");
            aTarget.setAttribute("dragover-bottom", "true");
            aTarget.setAttribute("dragover-left"  , "true");
            aTarget.setAttribute("dragover-right" , "true");
            break;
        }
        break;
      case "menuseparator": 
      case "menu":
      case "menuitem":
        switch (aOrientation) {
          case BookmarksUtils.DROP_BEFORE: 
            aTarget.setAttribute("dragover-top", "true");
            break;
          case BookmarksUtils.DROP_AFTER:
            aTarget.setAttribute("dragover-bottom", "true");
            break;
          case BookmarksUtils.DROP_ON:
            break;
        }
        break;
      case "toolbar": 
        var newTarget = BookmarksToolbar.getLastVisibleBookmark();
        if (newTarget)
          newTarget.setAttribute("dragover-right", "true");
        break;
      case "hbox":
      case "menupopup": break; 
     default: dump("No feedback for: "+aTarget.localName+"\n");
    }
  },

  onDragRemoveFeedBack: function (aTarget)
  { 
    var newTarget;
    var bt;
    if (aTarget.id == "PersonalToolbar" || aTarget.id == "bookmarks-ptf") { 
      newTarget = BookmarksToolbar.getLastVisibleBookmark();
      if (newTarget)
        newTarget.removeAttribute("dragover-right");
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
    return aTarget.localName == "menu"          || 
           aTarget.localName == "toolbarbutton" &&
           aTarget.getAttribute("type") == "menu";
  }
}

var BookmarksToolbar = 
{
  ////////////////////////////////////////////////
  // loads a bookmark with the mouse middle button
  loadBookmarkMiddleClick: function (aEvent, aDS)
  {
    if (aEvent.button != 1)
      return;
    // unlike for command events, we have to close the menus manually
    BookmarksMenuDNDObserver.mCurrentDragOverTarget = null;
    BookmarksMenuDNDObserver.onDragCloseTarget();
    BookmarksUtils.loadBookmarkBrowser(aEvent, aEvent.target, aDS);
  },

  /////////////////////////////////////////////////////////////////////////////
  // returns the node of the last visible bookmark on the toolbar -->
  getLastVisibleBookmark: function ()
  {
    var buttons = document.getElementById("bookmarks-ptf");
    var button = buttons.firstChild;
    if (!button)
      return null; // empty bookmarks toolbar
    do {
      if (button.collapsed)
        return button.previousSibling;
      button = button.nextSibling;
    } while (button)
    return buttons.lastChild;
  },

  updateOverflowMenu: function (aMenuPopup)
  {
    var hbox = document.getElementById("bookmarks-ptf");
    for (var i = 0; i < hbox.childNodes.length; i++) {
      var button = hbox.childNodes[i];
      var menu = aMenuPopup.childNodes[i];
      if (menu.hidden == button.collapsed)
        menu.hidden = !menu.hidden;
    }
  },

  resizeFunc: function(event) 
  { 
    if (!event) // timer callback case
      BookmarksToolbarRDFObserver._overflowTimerInEffect = false;
    else if (event.target != document)
      return; // only interested in chrome resizes

    var buttons = document.getElementById("bookmarks-ptf");
    if (!buttons)
      return;

    var chevron = document.getElementById("bookmarks-chevron");
    if (!buttons.firstChild) {
      // No bookmarks means no chevron
      chevron.collapsed = true;
      return;
    }

    chevron.collapsed = false;
    var chevronWidth = chevron.boxObject.width;
    chevron.collapsed = true;

    var remainingWidth = buttons.boxObject.width;
    var overflowed = false;

    for (var i=0; i<buttons.childNodes.length; i++) {
      var button = buttons.childNodes[i];
      button.collapsed = overflowed;
      
      if (i == buttons.childNodes.length - 1)
        chevronWidth = 0;
      remainingWidth -= button.boxObject.width;
      if (remainingWidth < chevronWidth) {
        overflowed = true;
        // This button doesn't fit. Show it in the menu. Hide it in the toolbar.
        if (!button.collapsed)
          button.collapsed = true;
        if (chevron.collapsed) {
          chevron.collapsed = false;
        }
      }
    }
  },

  // Fill in tooltips for personal toolbar
  fillInBTTooltip: function (tipElement)
  {

    var title = tipElement.label;
    var url = tipElement.statusText;

    if (!title && !url) {
      // bail out early if there is nothing to show
      return false;
    }

    var tooltipTitle = document.getElementById("btTitleText");
    var tooltipUrl = document.getElementById("btUrlText"); 
    if (title && title != url) {
      tooltipTitle.removeAttribute("hidden");
      tooltipTitle.setAttribute("value", title);
    } else  {
      tooltipTitle.setAttribute("hidden", "true");
    }
    if (url) {
      tooltipUrl.removeAttribute("hidden");
      tooltipUrl.setAttribute("value", url);
    } else {
      tooltipUrl.setAttribute("hidden", "true");
    }
    return true; // show tooltip
  }
}

// Implement nsIRDFObserver so we can update our overflow state when items get
// added/removed from the toolbar
var BookmarksToolbarRDFObserver =
{
  onAssert: function (aDataSource, aSource, aProperty, aTarget)
  {
    this.setOverflowTimeout(aSource, aProperty);
  },
  onUnassert: function (aDataSource, aSource, aProperty, aTarget)
  {
    this.setOverflowTimeout(aSource, aProperty);
  },
  onChange: function (aDataSource, aSource, aProperty, aOldTarget, aNewTarget) {},
  onMove: function (aDataSource, aOldSource, aNewSource, aProperty, aTarget) {},
  onBeginUpdateBatch: function (aDataSource) {},
  onEndUpdateBatch:   function (aDataSource) {
    if (this._overflowTimerInEffect)
      return;
    this._overflowTimerInEffect = true;
    setTimeout(BookmarksToolbar.resizeFunc, 0, null);
  },

  _overflowTimerInEffect: false,
  setOverflowTimeout: function (aSource, aProperty)
  {
    if (this._overflowTimerInEffect)
      return;
    if (aSource.Value != "NC:PersonalToolbarFolder" || aProperty.Value == NC_NS+"LastModifiedDate")
      return;
    this._overflowTimerInEffect = true;
    setTimeout(BookmarksToolbar.resizeFunc, 0, null);
  }
}
