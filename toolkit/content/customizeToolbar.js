# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
#
# The Initial Developer of the Original Code is
# David Hyatt.
# Portions created by the Initial Developer are Copyright (C) 2002
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   David Hyatt (hyatt@apple.com)
#   Blake Ross (blaker@netscape.com)
#   Joe Hewitt (hewitt@netscape.com)
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

const kRowMax = 4;

var gToolboxDocument = null;
var gToolbox = null;
var gCurrentDragOverItem = null;
var gToolboxChanged = false;
var gToolboxIconSize = false;

function onLoad()
{
  InitWithToolbox(window.arguments[0]);
  repositionDialog();
}

function InitWithToolbox(aToolbox)
{
  gToolbox = aToolbox;
  gToolboxDocument = gToolbox.ownerDocument;
  gToolbox.customizing = true;

  gToolbox.addEventListener("dragstart", onToolbarDragStart, false);
  gToolbox.addEventListener("dragover", onToolbarDragOver, false);
  gToolbox.addEventListener("dragleave", onToolbarDragLeave, false);
  gToolbox.addEventListener("drop", onToolbarDrop, false);

  initDialog();

  notifyParentInitialized();
}

function finishToolbarCustomization()
{
  gToolbox.customizing = false;
  removeToolboxListeners();
  unwrapToolbarItems();
  persistCurrentSets();
  
  notifyParentComplete();
}

function initDialog()
{
  var mode = gToolbox.getAttribute("mode");
  document.getElementById("modelist").value = mode;
  gToolboxIconSize = gToolbox.getAttribute("iconsize");
  var smallIconsCheckbox = document.getElementById("smallicons");
  smallIconsCheckbox.checked = gToolboxIconSize == "small";
  if (mode == "text")
    smallIconsCheckbox.disabled = true;

  // Build up the palette of other items.
  buildPalette();

  // Wrap all the items on the toolbar in toolbarpaletteitems.
  wrapToolbarItems();
}

function repositionDialog()
{
  // Position the dialog touching the bottom of the toolbox and centered with 
  // it.
  var width;
  if (document.documentElement.hasAttribute("width"))
    width = document.documentElement.getAttribute("width");
  else
    width = parseInt(document.documentElement.style.width);
  var screenX = gToolbox.boxObject.screenX 
                + ((gToolbox.boxObject.width - width) / 2);
  var screenY = gToolbox.boxObject.screenY + gToolbox.boxObject.height;

  window.moveTo(screenX, screenY);
}

function removeToolboxListeners()
{
  gToolbox.removeEventListener("dragstart", onToolbarDragStart, false);
  gToolbox.removeEventListener("dragover", onToolbarDragOver, false);
  gToolbox.removeEventListener("dragleave", onToolbarDragLeave, false);
  gToolbox.removeEventListener("drop", onToolbarDrop, false);
}

/**
 * Invoke a callback on the toolbox to notify it that the dialog is done
 * and going away.
 */
function notifyParentComplete()
{
  if ("customizeDone" in gToolbox)
    gToolbox.customizeDone(gToolboxChanged);
}

/**
 * Invoke a callback on the toolbox to notify it that the dialog is fully
 * initialized.
 */
function notifyParentInitialized()
{
  if ("customizeInitialized" in gToolbox)
    gToolbox.customizeInitialized();
}

function toolboxChanged()
{
  gToolboxChanged = true;
  if ("customizeChange" in gToolbox)
    gToolbox.customizeChange();
}

function getToolbarAt(i)
{
  return gToolbox.childNodes[i];
}

/**
 * Persist the current set of buttons in all customizable toolbars to
 * localstore.
 */
function persistCurrentSets()
{
  if (!gToolboxChanged || gToolboxDocument.defaultView.closed)
    return;

  var customCount = 0;
  for (var i = 0; i < gToolbox.childNodes.length; ++i) {
    // Look for customizable toolbars that need to be persisted.
    var toolbar = getToolbarAt(i);
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
    var toolbar = getToolbarAt(i);
    if (isCustomizableToolbar(toolbar)) {
      for (var k = 0; k < toolbar.childNodes.length; ++k) {
        var item = toolbar.childNodes[k];

#ifdef XP_MACOSX
        if (item.firstChild && item.firstChild.localName == "menubar")
          continue;
#endif

        if (isToolbarItem(item)) {
          var wrapper = wrapToolbarItem(item);
          cleanupItemForToolbar(item, wrapper);
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
  var paletteItem;
  while ((paletteItem = paletteItems.item(0)) != null) {
    var toolbarItem = paletteItem.firstChild;

    if (paletteItem.hasAttribute("itemdisabled"))
      toolbarItem.disabled = true;

    if (paletteItem.hasAttribute("itemcommand")) {
      let commandID = paletteItem.getAttribute("itemcommand");
      toolbarItem.setAttribute("command", commandID);
      toolbarItem.disabled = gToolboxDocument.getElementById(commandID).disabled;
    }

    paletteItem.parentNode.replaceChild(toolbarItem, paletteItem);
  }
}

/**
 * Creates a wrapper that can be used to contain a toolbaritem and prevent
 * it from receiving UI events.
 */
function createWrapper(aId, aDocument)
{
  var wrapper = aDocument.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
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
  var wrapper = createWrapper(aPaletteItem.id, document);

  wrapper.setAttribute("flex", 1);
  wrapper.setAttribute("align", "center");
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
 * with the wrapper. This is not used when dropping items from the palette,
 * only when first starting the dialog and wrapping everything on the toolbars.
 */
function wrapToolbarItem(aToolbarItem)
{
  var wrapper = createWrapper(aToolbarItem.id, gToolboxDocument);

  wrapper.flex = aToolbarItem.flex;

  aToolbarItem.parentNode.replaceChild(wrapper, aToolbarItem);
  
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
    var toolbar = getToolbarAt(i);
    if (isCustomizableToolbar(toolbar)) {
      var child = toolbar.firstChild;
      while (child) {
        if (isToolbarItem(child))
          currentItems[child.id] = 1;
        child = child.nextSibling;
      }
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
  while (paletteBox.lastChild)
    paletteBox.removeChild(paletteBox.lastChild);

  var currentRow = document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
                                            "hbox");
  currentRow.setAttribute("class", "paletteRow");

  // Add the toolbar separator item.
  var templateNode = document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
                                              "toolbarseparator");
  templateNode.id = "separator";
  wrapPaletteItem(templateNode, currentRow, null);

  // Add the toolbar spring item.
  templateNode = document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
                                              "toolbarspring");
  templateNode.id = "spring";
  templateNode.flex = 1;
  wrapPaletteItem(templateNode, currentRow, null);

  // Add the toolbar spacer item.
  templateNode = document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
                                              "toolbarspacer");
  templateNode.id = "spacer";
  templateNode.flex = 1;
  wrapPaletteItem(templateNode, currentRow, null);

  var rowSlot = 3;

  var currentItems = getCurrentItemIds();
  templateNode = gToolbox.palette.firstChild;
  while (templateNode) {
    // Check if the item is already in a toolbar before adding it to the palette.
    if (!(templateNode.id in currentItems)) {
      var paletteItem = document.importNode(templateNode, true);

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
 * is stripped of all properties that may adversely affect its
 * appearance in the palette.
 */
function cleanUpItemForPalette(aItem, aWrapper)
{
  aWrapper.setAttribute("place", "palette");
  setWrapperType(aItem, aWrapper);

  if (aItem.hasAttribute("title"))
    aWrapper.setAttribute("title", aItem.getAttribute("title"));
  else if (isSpecialItem(aItem)) {
    var stringBundle = document.getElementById("stringBundle");
    // Remove the common "toolbar" prefix to generate the string name.
    var title = stringBundle.getString(aItem.localName.slice(7) + "Title");
    aWrapper.setAttribute("title", title);
  }
  
  // Remove attributes that screw up our appearance.
  aItem.removeAttribute("command");
  aItem.removeAttribute("observes");
  aItem.removeAttribute("disabled");
  aItem.removeAttribute("type");
  aItem.removeAttribute("width");
  
  if (aItem.localName == "toolbaritem" && aItem.firstChild) {
    aItem.firstChild.removeAttribute("observes");

    // So the throbber doesn't throb in the dialog,
    // cute as that may be...
    aItem.firstChild.removeAttribute("busy");
  }
}

/**
 * Makes sure that an item that has been cloned from a template
 * is stripped of all properties that may adversely affect its
 * appearance in the toolbar.  Store critical properties on the 
 * wrapper so they can be put back on the item when we're done.
 */
function cleanupItemForToolbar(aItem, aWrapper)
{
  setWrapperType(aItem, aWrapper);
  aWrapper.setAttribute("place", "toolbar");

  if (aItem.hasAttribute("command")) {
    aWrapper.setAttribute("itemcommand", aItem.getAttribute("command"));
    aItem.removeAttribute("command");
  }

  if (aItem.disabled) {
    aWrapper.setAttribute("itemdisabled", "true");
    aItem.disabled = false;
  }
}

function setWrapperType(aItem, aWrapper)
{
  if (aItem.localName == "toolbarseparator") {
    aWrapper.setAttribute("type", "separator");
  } else if (aItem.localName == "toolbarspring") {
    aWrapper.setAttribute("type", "spring");
  } else if (aItem.localName == "toolbarspacer") {
    aWrapper.setAttribute("type", "spacer");
  } else if (aItem.localName == "toolbaritem" && aItem.firstChild) {
    aWrapper.setAttribute("type", aItem.firstChild.localName);
  }
}

function setDragActive(aItem, aValue)
{
  var node = aItem;
  var direction = window.getComputedStyle(aItem, null).direction;
  var value = direction == "ltr"? "left" : "right";
  if (aItem.localName == "toolbar") {
    node = aItem.lastChild;
    value = direction == "ltr"? "right" : "left";
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

  // Quitting from the toolbar dialog while the new toolbar prompt is up
  // can cause things to become unresponsive on the Mac. Until dialog modality
  // is fixed (395465), disable the "Done" button explicitly.
  var doneButton = document.getElementById("donebutton");
  doneButton.disabled = true;

  while (true) {

    if (!promptService.prompt(window, title, message, name, null, {})) {
      doneButton.disabled = false;
      return;
    }
    
    if (!name.value) {
      message = stringBundle.getFormattedString("enterToolbarBlank", [name.value]);
      continue;
    }

    var dupeFound = false;

     // Check for an existing toolbar with the same display name
    for (i = 0; i < gToolbox.childNodes.length; ++i) {
      var toolbar = gToolbox.childNodes[i];
      var toolbarName = toolbar.getAttribute("toolbarname");

      if (toolbarName == name.value &&
          toolbar.getAttribute("type") != "menubar" &&
          toolbar.nodeName == 'toolbar') {
        dupeFound = true;
        break;
      }
    }

    if (!dupeFound)
      break;

    message = stringBundle.getFormattedString("enterToolbarDup", [name.value]);
  }
    
  gToolbox.appendCustomToolbar(name.value, "");
  
  toolboxChanged();

  doneButton.disabled = false;
}

/**
 * Restore the default set of buttons to fixed toolbars,
 * remove all custom toolbars, and rebuild the palette.
 */
function restoreDefaultSet()
{
  // Unwrap the items on the toolbar.
  unwrapToolbarItems();

  // Remove all of the customized toolbars.
  var child = gToolbox.lastChild;
  while (child) {
    if (child.hasAttribute("customindex")) {
      var thisChild = child;
      child = child.previousSibling;
      thisChild.currentSet = "__empty";
      gToolbox.removeChild(thisChild);
    } else {
      child = child.previousSibling;
    }
  }

  // Restore the defaultset for fixed toolbars.
  var toolbar = gToolbox.firstChild;
  while (toolbar) {
    if (isCustomizableToolbar(toolbar)) {
      var defaultSet = toolbar.getAttribute("defaultset");
      if (defaultSet)
        toolbar.currentSet = defaultSet;
    }
    toolbar = toolbar.nextSibling;
  }

  // Restore the default icon size and mode.
  var defaultMode = gToolbox.getAttribute("defaultmode");
  var defaultIconsSmall = gToolbox.getAttribute("defaulticonsize") == "small";

  updateIconSize(defaultIconsSmall, true);
  document.getElementById("smallicons").checked = defaultIconsSmall;
  updateToolbarMode(defaultMode, true);
  document.getElementById("modelist").value = defaultMode;

  // Now rebuild the palette.
  buildPalette();

  // Now re-wrap the items on the toolbar.
  wrapToolbarItems();

  toolboxChanged();
}

function updateIconSize(aUseSmallIcons, localDefault)
{
  gToolboxIconSize = aUseSmallIcons ? "small" : "large";
  
  setAttribute(gToolbox, "iconsize", gToolboxIconSize);
  gToolboxDocument.persist(gToolbox.id, "iconsize");
  
  for (var i = 0; i < gToolbox.childNodes.length; ++i) {
    var toolbar = getToolbarAt(i);
    if (isCustomizableToolbar(toolbar)) {
      var toolbarIconSize = (localDefault && toolbar.hasAttribute("defaulticonsize")) ?
                            toolbar.getAttribute("defaulticonsize") :
                            gToolboxIconSize;
      setAttribute(toolbar, "iconsize", toolbarIconSize);
      gToolboxDocument.persist(toolbar.id, "iconsize");
    }
  }
}

function updateToolbarMode(aModeValue, localDefault)
{
  setAttribute(gToolbox, "mode", aModeValue);
  gToolboxDocument.persist(gToolbox.id, "mode");

  for (var i = 0; i < gToolbox.childNodes.length; ++i) {
    var toolbar = getToolbarAt(i);
    if (isCustomizableToolbar(toolbar)) {
      var toolbarMode = (localDefault && toolbar.hasAttribute("defaultmode")) ?
                        toolbar.getAttribute("defaultmode") :
                        aModeValue;
      setAttribute(toolbar, "mode", toolbarMode);
      gToolboxDocument.persist(toolbar.id, "mode");
    }
  }

  var iconSizeCheckbox = document.getElementById("smallicons");
  iconSizeCheckbox.disabled = aModeValue == "text";
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

function isSpecialItem(aElt)
{
  return aElt.localName == "toolbarseparator" ||
         aElt.localName == "toolbarspring" ||
         aElt.localName == "toolbarspacer";
}

function isToolbarItem(aElt)
{
  return aElt.localName == "toolbarbutton" ||
         aElt.localName == "toolbaritem" ||
         aElt.localName == "toolbarseparator" ||
         aElt.localName == "toolbarspring" ||
         aElt.localName == "toolbarspacer";
}

///////////////////////////////////////////////////////////////////////////
//// Drag and Drop observers

function onToolbarDragStart(aEvent)
{
  nsDragAndDrop.startDrag(aEvent, dragStartObserver);
}

function onToolbarDragOver(aEvent)
{
  nsDragAndDrop.dragOver(aEvent, toolbarDNDObserver);
}

function onToolbarDrop(aEvent)
{
  nsDragAndDrop.drop(aEvent, toolbarDNDObserver);
}

function onToolbarDragLeave(aEvent)
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
    aDragAction.action = Components.interfaces.nsIDragService.DRAGDROP_ACTION_MOVE;
  }
}

var toolbarDNDObserver =
{
  onDragOver: function (aEvent, aFlavour, aDragSession)
  {
    var toolbar = aEvent.target;
    var dropTarget = aEvent.target;
    while (toolbar && toolbar.localName != "toolbar") {
      dropTarget = toolbar;
      toolbar = toolbar.parentNode;
    }
    
    var previousDragItem = gCurrentDragOverItem;

    // Make sure we are dragging over a customizable toolbar.
    if (!isCustomizableToolbar(toolbar)) {
      gCurrentDragOverItem = null;
      return;
    }
    
    if (dropTarget.localName == "toolbar") {
      gCurrentDragOverItem = dropTarget;
    } else {
      gCurrentDragOverItem = null;

      var direction = window.getComputedStyle(dropTarget.parentNode, null).direction;
      var dropTargetCenter = dropTarget.boxObject.x + (dropTarget.boxObject.width / 2);
      if (direction == "ltr")
        dragAfter = aEvent.clientX > dropTargetCenter;
      else
        dragAfter = aEvent.clientX < dropTargetCenter;
        
      if (dragAfter) {
        gCurrentDragOverItem = dropTarget.nextSibling;
        if (!gCurrentDragOverItem)
          gCurrentDragOverItem = toolbar;
      } else
        gCurrentDragOverItem = dropTarget;
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

      // Don't allow static kids (e.g., the menubar) to move.
      if (wrapper.parentNode.firstPermanentChild && wrapper.parentNode.firstPermanentChild.id == wrapper.firstChild.id)
        return;
      if (wrapper.parentNode.lastPermanentChild && wrapper.parentNode.lastPermanentChild.id == wrapper.firstChild.id)
        return;

      // Remove the item from its place in the toolbar.
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
      var wrapper = createWrapper("", gToolboxDocument);

      // Ask the toolbar to clone the item's template, place it inside the wrapper, and insert it in the toolbar.
      var newItem = toolbar.insertItem(draggedItemId, gCurrentDragOverItem == toolbar ? null : gCurrentDragOverItem, wrapper);
      
      // Prepare the item and wrapper to look good on the toolbar.
      cleanupItemForToolbar(newItem, wrapper);
      wrapper.id = "wrapper-"+newItem.id;
      wrapper.flex = newItem.flex;

      // Remove the wrapper from the palette.
      var currentRow = draggedPaletteWrapper.parentNode;
      if (draggedItemId != "separator" &&
          draggedItemId != "spring" &&
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
    
    gCurrentDragOverItem = null;

    toolboxChanged();
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
      // Don't allow static kids (e.g., the menubar) to move.
      if (wrapper.parentNode.firstPermanentChild && wrapper.parentNode.firstPermanentChild.id == wrapper.firstChild.id)
        return;
      if (wrapper.parentNode.lastPermanentChild && wrapper.parentNode.lastPermanentChild.id == wrapper.firstChild.id)
        return;

      var wrapperType = wrapper.getAttribute("type");
      if (wrapperType != "separator" &&
          wrapperType != "spacer" &&
          wrapperType != "spring") {
        appendPaletteItem(document.importNode(wrapper.firstChild, true));
        gToolbox.palette.appendChild(wrapper.firstChild);
      }

      // The item was dragged out of the toolbar.
      wrapper.parentNode.removeChild(wrapper);
    }
    
    toolboxChanged();
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

