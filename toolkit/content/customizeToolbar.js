/* 
  The contents of this file are subject to the Netscape Public
  License Version 1.1 (the "License"); you may not use this file
  except in compliance with the License. You may obtain a copy of
  the License at http://www.mozilla.org/NPL/
  
  Software distributed under the License is distributed on an "AS
  IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
  implied. See the License for the specific language governing
  rights and limitations under the License.
  
  The Original Code is Mozilla Communicator client code, released
  March 31, 1998.
  
  The Initial Developer of the Original Code is David Hyatt. 
  Portions created by David Hyatt are
  Copyright (C) 2002 David Hyatt. All
  Rights Reserved.
  
  Contributor(s): 
    David Hyatt (hyatt@apple.com)
    Blake Ross (blaker@netscape.com)
    Joe Hewitt (hewitt@netscape.com)
*/

const kRowMax = 4;

var gToolboxDocument = null;
var gToolbox = null;
var gCurrentDragOverItem = null;
var gToolboxChanged = false;
var gPreviousSets = {};
var gPreviousMode = null;
var gPreviousIconSize = null;

function onLoad()
{
  gToolboxDocument = window.opener ? window.opener.document : window.parent.document;
  gToolbox = gToolboxDocument.getElementsByTagName("toolbox")[0];
  
  gToolbox.addEventListener("draggesture", onToolbarDragGesture, false);
  gToolbox.addEventListener("dragover", onToolbarDragOver, false);
  gToolbox.addEventListener("dragexit", onToolbarDragExit, false);
  gToolbox.addEventListener("dragdrop", onToolbarDragDrop, false);

  var mode = gToolbox.getAttribute("mode");
  document.getElementById("modelist").value = mode;

  var iconSize = gToolbox.getAttribute("iconsize");
  document.getElementById("smallicons").checked = iconSize == "small";

  // The dialog version has the option to position/size itself.
  if (window.opener) {
    var cover = window.arguments ? window.arguments[0] : null;
    if (cover) {
      document.documentElement.setAttribute("hidechrome", "true");
      window.screenX = cover.boxObject.screenX;
      window.screenY = cover.boxObject.screenY;
      window.outerWidth = cover.boxObject.width;
      window.outerHeight = cover.boxObject.height;
    } else {
      // Position the dialog just after the toolbox.
      window.screenY = gToolbox.boxObject.screenY + gToolbox.boxObject.height;
    }
  }

  cacheCurrentState();

  // Build up the palette of other items.
  buildPalette();

  // Wrap all the items on the toolbar in toolbarpaletteitems.
  wrapToolbarItems();
}

function onAccept(aEvent)
{
  removeToolboxListeners();
  unwrapToolbarItems();
  persistCurrentSets();
  notifyParentComplete();
}

function onCancel(aEvent)
{
  removeToolboxListeners();
  reverToPreviousState();
  unwrapToolbarItems();
  notifyParentComplete();
}

function removeToolboxListeners()
{
  gToolbox.removeEventListener("draggesture", onToolbarDragGesture, false);
  gToolbox.removeEventListener("dragover", onToolbarDragOver, false);
  gToolbox.removeEventListener("dragexit", onToolbarDragExit, false);
  gToolbox.removeEventListener("dragdrop", onToolbarDragDrop, false);
}

/**
 * Invoke a callback on our parent window to notify it that
 * the dialog is done and going away.
 */
function notifyParentComplete()
{
  var parent = window.parent ? window.parent : window.opener;
  if (parent && "onToolbarCustomizeComplete" in parent.top)
    parent.top.onToolbarCustomizeComplete();
}

/**
 * Cache the current state of all customizable toolbars so
 * that we can undo changes.
 */
function cacheCurrentState()
{
  gPreviousMode = gToolbox.getAttribute("mode");
  gPreviousIconSize = gToolbox.getAttribute("iconsize");
  
  var currentIndex = 0;
  for (var i = 0; i < gToolbox.childNodes.length; ++i) {
    var toolbar = gToolbox.childNodes[i];
    if (isCustomizableToolbar(toolbar)) {
      var currentSet = toolbar.getAttribute("currentset");
      if (!currentSet)
        currentSet = toolbar.getAttribute("defaultset");
        
      gPreviousSets[toolbar.id] = currentSet;
    }
  }  
}

/**
 * Revert the currentset of each customizable toolbar to the value
 * that we cached when first opening the dialog and remove toolbars
 * that were added.
 */
function reverToPreviousState()
{
  updateIconSize(gPreviousIconSize);
  updateToolbarMode(gPreviousMode);
  
  if (!gToolboxChanged)
    return;

  var currentIndex = 0;
  for (var i = 0; i < gToolbox.childNodes.length; ++i) {
    var toolbar = gToolbox.childNodes[i];
    if (isCustomizableToolbar(toolbar)) {
      if (toolbar.id in gPreviousSets) {
        // Restore the currentset of previously existing toolbars.
        toolbar.currentSet = gPreviousSets[toolbar.id];
      } else {
        // Remove toolbars that have been added.
        gToolbox.removeChild(toolbar);
      }
    }
  }

  // Add back toolbars that have been removed.
  for (var toolbarId in gPreviousSets) {
    var toolbar = gToolboxDocument.getElementById(toolbarId);
    if (!toolbar)
      gToolbox.appendCustomToolbar("", gPreviousSets[toolbarId]);
  }
}

/**
 * Persist the current set of buttons in all customizable toolbars to
 * localstore.
 */
function persistCurrentSets()
{
  if (!gToolboxChanged)
    return;

  var customCount = 0;
  for (var i = 0; i < gToolbox.childNodes.length; ++i) {
    // Look for customizable toolbars that need to be persisted.
    var toolbar = gToolbox.childNodes[i];
    if (isCustomizableToolbar(toolbar)) {
      // Calculate currentset and store it in the attribute.
      var currentSet = toolbar.currentSet;
      toolbar.setAttribute("currentset", currentSet);
      
      var customIndex = toolbar.hasAttribute("customindex");
      if (customIndex) {
        if (!toolbar.firstChild) {
          // Remove custom toolbars whose contents have been removed.
          gToolbox.removeChild(toolbar);
          --i;
        } else {
          // Persist custom toolbar info on the <toolbarset/>
          gToolbox.toolbarset.setAttribute("toolbar"+(++customCount),
                                           toolbar.toolbarName + ":" + currentSet);
          gToolboxDocument.persist(gToolbox.toolbarset.id, "toolbar"+customCount);
        }
      }

      if (!customIndex) {
        // Persist the currentset attribute directly on hardcoded toolbars.
        gToolboxDocument.persist(toolbar.id, "currentset");
      }
    }
  }
  
  // Remove toolbarX attributes for removed toolbars.
  while (gToolbox.toolbarset.hasAttribute("toolbar"+(++customCount))) {
    dump(customCount + " gone\n");
    gToolbox.toolbarset.removeAttribute("toolbar"+customCount);
    gToolboxDocument.persist(gToolbox.toolbarset.id, "toolbar"+customCount);
  }
}

/**
 * Wraps all items in all customizable toolbars in a toolbox.
 */
function wrapToolbarItems()
{
  for (var i = 0; i < gToolbox.childNodes.length; ++i) {
    var toolbar = gToolbox.childNodes[i];
    if (isCustomizableToolbar(toolbar)) {
      for (var k = 0; k < toolbar.childNodes.length; ++k) {
        var item = toolbar.childNodes[k];
        if (isToolbarItem(item)) {
          var nextSibling = item.nextSibling;
          
          var wrapper = wrapToolbarItem(item);
          
          if (nextSibling)
            toolbar.insertBefore(wrapper, nextSibling);
          else
            toolbar.appendChild(wrapper);
        }
      }
    }
  }
}

/**
 * Unwraps all items in all customizable toolbars in a toolbox.
 */
function unwrapToolbarItems()
{
  var paletteItems = gToolbox.getElementsByTagName("toolbarpaletteitem");
  for (var i = 0; i < paletteItems.length; ++i) {
    var paletteItem = paletteItems[i];    
    var toolbarItem = paletteItem.firstChild;

    if (paletteItem.hasAttribute("itemdisabled"))
      toolbarItem.disabled = true;

    if (paletteItem.hasAttribute("itemcommand"))
      toolbarItem.setAttribute("command", paletteItem.getAttribute("itemcommand"));

    paletteItem.removeChild(toolbarItem);
    paletteItem.parentNode.replaceChild(toolbarItem, paletteItem);
  }
}

/**
 * Creates a wrapper that can be used to contain a toolbaritem and prevent
 * it from receiving UI events.
 */
function createWrapper(aId)
{
  var wrapper = document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
                                         "toolbarpaletteitem");

  wrapper.id = "wrapper-"+aId;  
  return wrapper;
}

/**
 * Wraps an item that has been cloned from a template and adds
 * it to the end of a row in the palette.
 */
function wrapPaletteItem(aPaletteItem, aCurrentRow, aSpacer)
{
  var wrapper = createWrapper(aPaletteItem.id);
  
  if (aPaletteItem.localName == "toolbarseparator") {
    wrapper.setAttribute("type", "separator");
  } else if (aPaletteItem.localName == "spacer") {
      wrapper.setAttribute("type", "spacer");
  } else {
    wrapper.setAttribute("align", "center");
  }
  
  wrapper.setAttribute("flex", 1);
  wrapper.setAttribute("pack", "center");
  wrapper.setAttribute("minheight", "0");
  wrapper.setAttribute("minwidth", "0");

  wrapper.appendChild(aPaletteItem);
  
  // XXX We need to call this AFTER the palette item has been appended
  // to the wrapper or else we crash dropping certain buttons on the 
  // palette due to removal of the command and disabled attributes - JRH
  cleanUpItemForPalette(aPaletteItem, wrapper);

  if (aSpacer)
    aCurrentRow.insertBefore(wrapper, aSpacer);
  else
    aCurrentRow.appendChild(wrapper);

}

/**
 * Wraps an item that is currently on a toolbar and replaces the item
 * with the wrapper.
 */
function wrapToolbarItem(aToolbarItem)
{
  var wrapper = createWrapper(aToolbarItem.id);
  
  cleanupItemForToolbar(aToolbarItem, wrapper);
  wrapper.flex = aToolbarItem.flex;

  if (aToolbarItem.parentNode)
    aToolbarItem.parentNode.removeChild(aToolbarItem);
  
  wrapper.appendChild(aToolbarItem);
  
  return wrapper;
}

/**
 * Get the list of ids for the current set of items on each toolbar.
 */
function getCurrentItemIds()
{
  var currentItems = {};
  for (var i = 0; i < gToolbox.childNodes.length; ++i) {
    var toolbar = gToolbox.childNodes[i];
    if (isCustomizableToolbar(toolbar)) {
      var currentSet = toolbar.getAttribute("currentset");
      if (currentSet == "__empty")
        continue;
      else if (!currentSet)
        currentSet = toolbar.getAttribute("defaultset");
      
      var ids = currentSet.split(",");
      for (var k = 0; k < ids.length; ++k)
        currentItems[ids[k]] = 1;
    }
  }
  return currentItems;
}

/**
 * Builds the palette of draggable items that are not yet in a toolbar.
 */
function buildPalette()
{
  // Empty the palette first.
  var paletteBox = document.getElementById("palette-box");
  while (paletteBox.firstChild)
    paletteBox.removeChild(paletteBox.firstChild);

  var currentRow = document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
                                            "hbox");
  currentRow.setAttribute("class", "paletteRow");

  // Add the toolbar separator item.
  var templateNode = document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
                                              "toolbarseparator");
  templateNode.id = "separator";
  wrapPaletteItem(templateNode, currentRow, null);

  // Add the toolbar spacer item.
  templateNode = document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
                                              "spacer");
  templateNode.id = "spacer";
  templateNode.flex = 1;
  wrapPaletteItem(templateNode, currentRow, null);

  var rowSlot = 2;

  var currentItems = getCurrentItemIds();
  templateNode = gToolbox.palette.firstChild;
  while (templateNode) {
    // Check if the item is already in a toolbar before adding it to the palette.
    if (!(templateNode.id in currentItems)) {
      var paletteItem = templateNode.cloneNode(true);

      if (rowSlot == kRowMax) {
        // Append the old row.
        paletteBox.appendChild(currentRow);

        // Make a new row.
        currentRow = document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
                                              "hbox");
        currentRow.setAttribute("class", "paletteRow");
        rowSlot = 0;
      }

      ++rowSlot;
      wrapPaletteItem(paletteItem, currentRow, null);
    }
    
    templateNode = templateNode.nextSibling;
  }

  if (currentRow) { 
    fillRowWithFlex(currentRow);
    paletteBox.appendChild(currentRow);
  }
}

/**
 * Creates a new palette item for a cloned template node and
 * adds it to the last slot in the palette.
 */
function appendPaletteItem(aItem)
{
  var paletteBox = document.getElementById("palette-box");
  var lastRow = paletteBox.lastChild;
  var lastSpacer = lastRow.lastChild;
   
  if (lastSpacer.localName != "spacer") {
    // The current row is full, so we have to create a new row.
    lastRow = document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
                                        "hbox");
    lastRow.setAttribute("class", "paletteRow");
    paletteBox.appendChild(lastRow);
    
    wrapPaletteItem(aItem, lastRow, null);

    fillRowWithFlex(lastRow);
  } else {
    // Decrement the flex of the last spacer or remove it entirely.
    var flex = lastSpacer.getAttribute("flex");
    if (flex == 1) {
      lastRow.removeChild(lastSpacer);
      lastSpacer = null;
    } else
      lastSpacer.setAttribute("flex", --flex);

    // Insert the wrapper where the last spacer was.
    wrapPaletteItem(aItem, lastRow, lastSpacer);
  }
}

function fillRowWithFlex(aRow)
{
  var remainingFlex = kRowMax - aRow.childNodes.length;
  if (remainingFlex > 0) {
    var spacer = document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
                                          "spacer");
    spacer.setAttribute("flex", remainingFlex);
    aRow.appendChild(spacer);
  }
}

/**
 * Makes sure that an item that has been cloned from a template
 * is stripped of all properties that may adversely affect it's
 * appearance in the palette.
 */
function cleanUpItemForPalette(aItem, aWrapper)
{
  // Mark special types of palette items so we can style them.
  if (aItem.localName == "toolbarseparator") {
    aWrapper.setAttribute("type", "separator");
  } else if (aItem.localName == "spacer") {
    aWrapper.setAttribute("type", "spacer");
  } else if (aItem.firstChild && aItem.firstChild.localName == "bookmarks-toolbar") {
    aWrapper.setAttribute("type", "bookmarks");
  }

  // Remove attributes that screw up our appearance.
  aItem.removeAttribute("command");
  aItem.removeAttribute("observes");
  aItem.removeAttribute("disabled");
  aItem.removeAttribute("type");
  
  if (aItem.localName == "toolbaritem" && aItem.firstChild) {
    aItem.firstChild.removeAttribute("observes");

    // So the throbber doesn't throb in the dialog,
    // cute as that may be...
    aItem.firstChild.removeAttribute("busy");
  }
}

/**
 * Makes sure that an item that has been cloned from a template
 * is stripped of all properties that may adversely affect it's
 * appearance in the toolbar.  Store critical properties on the 
 * wrapper so they can be put back on the item when we're done.
 */
function cleanupItemForToolbar(aItem, aWrapper)
{
  if (aItem.localName == "toolbarseparator") {
    aWrapper.setAttribute("type", "separator");
  } else if (aItem.localName == "spacer") {
    aWrapper.setAttribute("type", "spacer");
  }

  if (aItem.hasAttribute("command")) {
    aWrapper.setAttribute("itemcommand", aItem.getAttribute("command"));
    aItem.removeAttribute("command");
  }

  if (aItem.disabled) {
    aWrapper.setAttribute("itemdisabled", "true");
    aItem.disabled = false;
  }
}

function setDragActive(aItem, aValue)
{
  var node = aItem;
  var value = "left";
  if (aItem.localName == "toolbar") {
    node = aItem.lastChild;
    value = "right";
  }
  
  if (!node)
    return;
  
  if (aValue) {
    if (!node.hasAttribute("dragover"))
      node.setAttribute("dragover", value);
  } else {
    node.removeAttribute("dragover");
  }
}

function addNewToolbar()
{
  var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                                .getService(Components.interfaces.nsIPromptService);

  var stringBundle = document.getElementById("stringBundle");
  var message = stringBundle.getString("enterToolbarName");
  var title = stringBundle.getString("enterToolbarTitle");
  
  var name = {};
  while (1) {
    if (!promptService.prompt(window, title, message, name, null, {})) {
      return;
    } else {
      // Check for an existing toolbar with the same name and prompt again
      // if a conflict is found
      var nameToId = "__customToolbar_" + name.value.replace(" ", "");
      var existingToolbar = gToolboxDocument.getElementById(nameToId);
      if (existingToolbar) {
        message = stringBundle.getFormattedString("enterToolbarDup", [name.value]);
      } else {
        break;
      }
    }
  }
    
  gToolbox.appendCustomToolbar(name.value, "");
  gToolboxChanged = true;
}

/**
 * Restore the default set of buttons to fixed toolbars,
 * remove all custom toolbars, and rebuild the palette.
 */
function restoreDefaultSet()
{
  // Restore the defaultset for fixed toolbars.
  var toolbar = gToolbox.firstChild;
  while (toolbar) {
    if (isCustomizableToolbar(toolbar)) {
      if (!toolbar.hasAttribute("customindex")) {
        var defaultSet = toolbar.getAttribute("defaultset");
        if (defaultSet)
          toolbar.currentSet = defaultSet;
      }
    }
    toolbar = toolbar.nextSibling;
  }

  // Remove all of the customized toolbars.
  var child = gToolbox.lastChild;
  while (child) {
    if (child.hasAttribute("customindex")) {
      var thisChild = child;
      child = child.previousSibling;
      gToolbox.removeChild(thisChild);
    } else {
      child = child.previousSibling;
    }
  }
  
  // Now re-wrap the items on the toolbar.
  wrapToolbarItems();
  
  // Now rebuild the palette.
  buildPalette();
  
  gToolboxChanged = true;
}

function updateIconSize(aUseSmallIcons)
{
  var val = aUseSmallIcons ? "small" : null;
  
  setAttribute(gToolbox, "iconsize", val);
  gToolboxDocument.persist(gToolbox.id, "iconsize");
  
  for (var i = 0; i < gToolbox.childNodes.length; ++i) {
    var toolbar = gToolbox.childNodes[i];
    if (isCustomizableToolbar(toolbar)) {
      setAttribute(toolbar, "iconsize", val);
      gToolboxDocument.persist(toolbar.id, "iconsize");
    }
  }
}

function updateToolbarMode(aModeValue)
{
  setAttribute(gToolbox, "mode", aModeValue);
  gToolboxDocument.persist(gToolbox.id, "mode");

  for (var i = 0; i < gToolbox.childNodes.length; ++i) {
    var toolbar = gToolbox.childNodes[i];
    if (isCustomizableToolbar(toolbar)) {
      setAttribute(toolbar, "mode", aModeValue);
      gToolboxDocument.persist(toolbar.id, "mode");
    }
  }

  var iconSizeCheckbox = document.getElementById("smallicons");
  if (aModeValue == "text") {
    iconSizeCheckbox.disabled = true;
    iconSizeCheckbox.checked = false;
    updateIconSize(false);
  }
  else {
    iconSizeCheckbox.disabled = false;
  }
}


function setAttribute(aElt, aAttr, aVal)
{
 if (aVal)
    aElt.setAttribute(aAttr, aVal);
  else
    aElt.removeAttribute(aAttr);
}

function isCustomizableToolbar(aElt)
{
  return aElt.localName == "toolbar" &&
         aElt.getAttribute("customizable") == "true";
}

function isToolbarItem(aElt) {
  return aElt.localName == "toolbarbutton" ||
         aElt.localName == "toolbaritem" ||
         aElt.localName == "toolbarseparator" ||
         aElt.localName == "spacer";
}

///////////////////////////////////////////////////////////////////////////
//// Drag and Drop observers

function onToolbarDragGesture(aEvent)
{
  nsDragAndDrop.startDrag(aEvent, dragStartObserver);
}

function onToolbarDragOver(aEvent)
{
  nsDragAndDrop.dragOver(aEvent, toolbarDNDObserver);
}

function onToolbarDragDrop(aEvent)
{
  nsDragAndDrop.drop(aEvent, toolbarDNDObserver);
}

function onToolbarDragExit(aEvent)
{
  if (gCurrentDragOverItem)
    setDragActive(gCurrentDragOverItem, false);
}

var dragStartObserver =
{
  onDragStart: function (aEvent, aXferData, aDragAction) {
    var documentId = gToolboxDocument.documentElement.id;
    
    var item = aEvent.target;
    while (item && item.localName != "toolbarpaletteitem")
      item = item.parentNode;
    
    item.setAttribute("dragactive", "true");
    
    aXferData.data = new TransferDataSet();
    var data = new TransferData();
    data.addDataForFlavour("text/toolbarwrapper-id/"+documentId, item.firstChild.id);
    aXferData.data.push(data);
  }
}

var toolbarDNDObserver =
{
  onDragOver: function (aEvent, aFlavour, aDragSession)
  {
    var toolbar = aEvent.target;
    while (toolbar && toolbar.localName != "toolbar")
      toolbar = toolbar.parentNode;

    var previousDragItem = gCurrentDragOverItem;

    // Make sure we are dragging over a customizable toolbar.
    if (!isCustomizableToolbar(toolbar)) {
      gCurrentDragOverItem = null;
      return;
    }
    
    if (aEvent.target.localName == "toolbar") {
      gCurrentDragOverItem = aEvent.target;
    } else {
      var dropTargetWidth = aEvent.target.boxObject.width;
      var dropTargetX = aEvent.target.boxObject.x;

      gCurrentDragOverItem = null;
      if (aEvent.clientX > (dropTargetX + (dropTargetWidth / 2))) {
        gCurrentDragOverItem = aEvent.target.nextSibling;
        if (!gCurrentDragOverItem)
          gCurrentDragOverItem = toolbar;
      } else
        gCurrentDragOverItem = aEvent.target;
    }    

    if (previousDragItem && gCurrentDragOverItem != previousDragItem) {
      setDragActive(previousDragItem, false);
    }
    
    setDragActive(gCurrentDragOverItem, true);
    
    aDragSession.canDrop = true;
  },
  
  onDrop: function (aEvent, aXferData, aDragSession)
  {
    if (!gCurrentDragOverItem)
      return;
    
    setDragActive(gCurrentDragOverItem, false);

    var draggedItemId = aXferData.data;
    if (gCurrentDragOverItem.id == draggedItemId)
      return;

    var toolbar = aEvent.target;
    while (toolbar.localName != "toolbar")
      toolbar = toolbar.parentNode;

    var draggedPaletteWrapper = document.getElementById("wrapper-"+draggedItemId);       
    if (!draggedPaletteWrapper) {
      // The wrapper has been dragged from the toolbar.
      
      // Get the wrapper from the toolbar document and make sure that
      // it isn't being dropped on itself.
      var wrapper = gToolboxDocument.getElementById("wrapper-"+draggedItemId);
      if (wrapper == gCurrentDragOverItem)
        return;

      // Remove the item from it's place in the toolbar.
      wrapper.parentNode.removeChild(wrapper);

      // Determine which toolbar we are dropping on.
      var dropToolbar = null;
      if (gCurrentDragOverItem.localName == "toolbar")
        dropToolbar = gCurrentDragOverItem;
      else
        dropToolbar = gCurrentDragOverItem.parentNode;
      
      // Insert the item into the toolbar.
      if (gCurrentDragOverItem != dropToolbar)
        dropToolbar.insertBefore(wrapper, gCurrentDragOverItem);
      else
        dropToolbar.appendChild(wrapper);
    } else {
      // The item has been dragged from the palette
      
      // Create a new wrapper for the item. We don't know the id yet.
      var wrapper = createWrapper("");

      // Ask the toolbar to clone the item's template, place it inside the wrapper, and insert it in the toolbar.
      var newItem = toolbar.insertItem(draggedItemId, gCurrentDragOverItem == toolbar ? null : gCurrentDragOverItem, wrapper);
      
      // Prepare the item and wrapper to look good on the toolbar.
      cleanupItemForToolbar(newItem, wrapper);
      wrapper.id = "wrapper-"+newItem.id;
      wrapper.flex = newItem.flex;

      // Remove the wrapper from the palette.
      var currentRow = draggedPaletteWrapper.parentNode;
      if (draggedItemId != "separator" &&
          draggedItemId != "spacer")
      {
        currentRow.removeChild(draggedPaletteWrapper);

        while (currentRow) {
          // Pull the first child of the next row up
          // into this row.
          var nextRow = currentRow.nextSibling;
          
          if (!nextRow) {
            var last = currentRow.lastChild;
            var first = currentRow.firstChild;
            if (first == last) {
              // Kill the row.
              currentRow.parentNode.removeChild(currentRow);
              break;
            }

            if (last.localName == "spacer") {
              var flex = last.getAttribute("flex");
              last.setAttribute("flex", ++flex);
              // Reflow doesn't happen for some reason.  Trigger it with a hide/show. ICK! -dwh
              last.hidden = true;
              last.hidden = false;
              break;
            } else {
              // Make a spacer and give it a flex of 1.
              var spacer = document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
                                                    "spacer");
              spacer.setAttribute("flex", "1");
              currentRow.appendChild(spacer);
            }
            break;
          }
          
          currentRow.appendChild(nextRow.firstChild);
          currentRow = currentRow.nextSibling;
        }
      }
    }
    
    gToolboxChanged = true;
    gCurrentDragOverItem = null;
  },
  
  _flavourSet: null,
  
  getSupportedFlavours: function ()
  {
    if (!this._flavourSet) {
      this._flavourSet = new FlavourSet();
      var documentId = gToolboxDocument.documentElement.id;
      this._flavourSet.appendFlavour("text/toolbarwrapper-id/"+documentId);
    }
    return this._flavourSet;
  }
}

var paletteDNDObserver =
{
  onDragOver: function (aEvent, aFlavour, aDragSession)
  {
    aDragSession.canDrop = true;
  },
  
  onDrop: function(aEvent, aXferData, aDragSession)
  {
    var itemId = aXferData.data;
    
    var wrapper = gToolboxDocument.getElementById("wrapper-"+itemId);
    if (wrapper) {
      // The item was dragged out of the toolbar.
      wrapper.parentNode.removeChild(wrapper);
      
      var wrapperType = wrapper.getAttribute("type");
      if (wrapperType != "separator" && wrapperType != "spacer") {
        // Find the template node in the toolbox palette
        var templateNode = gToolbox.palette.firstChild;
        while (templateNode) {
          if (templateNode.id == itemId)
            break;
          templateNode = templateNode.nextSibling;
        }
        if (!templateNode)
          return;
        
        // Clone the template and add it to our palette.
        var paletteItem = templateNode.cloneNode(true);
        appendPaletteItem(paletteItem);
      }
    }
    
    gToolboxChanged = true;
  },
  
  _flavourSet: null,
  
  getSupportedFlavours: function ()
  {
    if (!this._flavourSet) {
      this._flavourSet = new FlavourSet();
      var documentId = gToolboxDocument.documentElement.id;
      this._flavourSet.appendFlavour("text/toolbarwrapper-id/"+documentId);
    }
    return this._flavourSet;
  }
}

