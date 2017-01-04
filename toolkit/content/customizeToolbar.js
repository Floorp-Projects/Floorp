/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var gToolboxDocument = null;
var gToolbox = null;
var gCurrentDragOverItem = null;
var gToolboxChanged = false;
var gToolboxSheet = false;
var gPaletteBox = null;

Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/AppConstants.jsm");

function onLoad() {
  if ("arguments" in window && window.arguments[0]) {
    InitWithToolbox(window.arguments[0]);
    repositionDialog(window);
  } else if (window.frameElement &&
           "toolbox" in window.frameElement) {
    gToolboxSheet = true;
    InitWithToolbox(window.frameElement.toolbox);
    repositionDialog(window.frameElement.panel);
  }
}

function InitWithToolbox(aToolbox) {
  gToolbox = aToolbox;
  dispatchCustomizationEvent("beforecustomization");
  gToolboxDocument = gToolbox.ownerDocument;
  gToolbox.customizing = true;
  forEachCustomizableToolbar(function(toolbar) {
    toolbar.setAttribute("customizing", "true");
  });
  gPaletteBox = document.getElementById("palette-box");

  var elts = getRootElements();
  for (let i = 0; i < elts.length; i++) {
    elts[i].addEventListener("dragstart", onToolbarDragStart, true);
    elts[i].addEventListener("dragover", onToolbarDragOver, true);
    elts[i].addEventListener("dragexit", onToolbarDragExit, true);
    elts[i].addEventListener("drop", onToolbarDrop, true);
  }

  initDialog();
}

function onClose() {
  if (!gToolboxSheet)
    window.close();
  else
    finishToolbarCustomization();
}

function onUnload() {
  if (!gToolboxSheet)
    finishToolbarCustomization();
}

function finishToolbarCustomization() {
  removeToolboxListeners();
  unwrapToolbarItems();
  persistCurrentSets();
  gToolbox.customizing = false;
  forEachCustomizableToolbar(function(toolbar) {
    toolbar.removeAttribute("customizing");
  });

  notifyParentComplete();
}

function initDialog() {
  if (!gToolbox.toolbarset) {
    document.getElementById("newtoolbar").hidden = true;
  }

  var mode = gToolbox.getAttribute("mode");
  document.getElementById("modelist").value = mode;
  var smallIconsCheckbox = document.getElementById("smallicons");
  smallIconsCheckbox.checked = gToolbox.getAttribute("iconsize") == "small";
  if (mode == "text")
    smallIconsCheckbox.disabled = true;

  // Build up the palette of other items.
  buildPalette();

  // Wrap all the items on the toolbar in toolbarpaletteitems.
  wrapToolbarItems();
}

function repositionDialog(aWindow) {
  // Position the dialog touching the bottom of the toolbox and centered with
  // it.
  if (!aWindow)
    return;

  var width;
  if (aWindow != window)
    width = aWindow.getBoundingClientRect().width;
  else if (document.documentElement.hasAttribute("width"))
    width = document.documentElement.getAttribute("width");
  else
    width = parseInt(document.documentElement.style.width);
  var screenX = gToolbox.boxObject.screenX
                + ((gToolbox.boxObject.width - width) / 2);
  var screenY = gToolbox.boxObject.screenY + gToolbox.boxObject.height;

  aWindow.moveTo(screenX, screenY);
}

function removeToolboxListeners() {
  var elts = getRootElements();
  for (let i = 0; i < elts.length; i++) {
    elts[i].removeEventListener("dragstart", onToolbarDragStart, true);
    elts[i].removeEventListener("dragover", onToolbarDragOver, true);
    elts[i].removeEventListener("dragexit", onToolbarDragExit, true);
    elts[i].removeEventListener("drop", onToolbarDrop, true);
  }
}

/**
 * Invoke a callback on the toolbox to notify it that the dialog is done
 * and going away.
 */
function notifyParentComplete() {
  if ("customizeDone" in gToolbox)
    gToolbox.customizeDone(gToolboxChanged);
  dispatchCustomizationEvent("aftercustomization");
}

function toolboxChanged(aType) {
  gToolboxChanged = true;
  if ("customizeChange" in gToolbox)
    gToolbox.customizeChange(aType);
  dispatchCustomizationEvent("customizationchange");
}

function dispatchCustomizationEvent(aEventName) {
  var evt = document.createEvent("Events");
  evt.initEvent(aEventName, true, true);
  gToolbox.dispatchEvent(evt);
}

/**
 * Persist the current set of buttons in all customizable toolbars to
 * localstore.
 */
function persistCurrentSets() {
  if (!gToolboxChanged || gToolboxDocument.defaultView.closed)
    return;

  var customCount = 0;
  forEachCustomizableToolbar(function(toolbar) {
    // Calculate currentset and store it in the attribute.
    var currentSet = toolbar.currentSet;
    toolbar.setAttribute("currentset", currentSet);

    var customIndex = toolbar.hasAttribute("customindex");
    if (customIndex) {
      if (!toolbar.hasChildNodes()) {
        // Remove custom toolbars whose contents have been removed.
        gToolbox.removeChild(toolbar);
      } else if (gToolbox.toolbarset) {
        // Persist custom toolbar info on the <toolbarset/>
        gToolbox.toolbarset.setAttribute("toolbar" + (++customCount),
                                         toolbar.toolbarName + ":" + currentSet);
        gToolboxDocument.persist(gToolbox.toolbarset.id, "toolbar" + customCount);
      }
    }

    if (!customIndex) {
      // Persist the currentset attribute directly on hardcoded toolbars.
      gToolboxDocument.persist(toolbar.id, "currentset");
    }
  });

  // Remove toolbarX attributes for removed toolbars.
  while (gToolbox.toolbarset && gToolbox.toolbarset.hasAttribute("toolbar" + (++customCount))) {
    gToolbox.toolbarset.removeAttribute("toolbar" + customCount);
    gToolboxDocument.persist(gToolbox.toolbarset.id, "toolbar" + customCount);
  }
}

/**
 * Wraps all items in all customizable toolbars in a toolbox.
 */
function wrapToolbarItems() {
  forEachCustomizableToolbar(function(toolbar) {
    Array.forEach(toolbar.childNodes, function(item) {
      if (AppConstants.platform == "macosx") {
        if (item.firstChild && item.firstChild.localName == "menubar")
          return;
      }
      if (isToolbarItem(item)) {
        let wrapper = wrapToolbarItem(item);
        cleanupItemForToolbar(item, wrapper);
      }
    });
  });
}

function getRootElements() {
  return [gToolbox].concat(gToolbox.externalToolbars);
}

/**
 * Unwraps all items in all customizable toolbars in a toolbox.
 */
function unwrapToolbarItems() {
  let elts = getRootElements();
  for (let i = 0; i < elts.length; i++) {
    let paletteItems = elts[i].getElementsByTagName("toolbarpaletteitem");
    let paletteItem;
    while ((paletteItem = paletteItems.item(0)) != null) {
      let toolbarItem = paletteItem.firstChild;
      restoreItemForToolbar(toolbarItem, paletteItem);
      paletteItem.parentNode.replaceChild(toolbarItem, paletteItem);
    }
  }
}

/**
 * Creates a wrapper that can be used to contain a toolbaritem and prevent
 * it from receiving UI events.
 */
function createWrapper(aId, aDocument) {
  var wrapper = aDocument.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
                                         "toolbarpaletteitem");

  wrapper.id = "wrapper-" + aId;
  return wrapper;
}

/**
 * Wraps an item that has been cloned from a template and adds
 * it to the end of the palette.
 */
function wrapPaletteItem(aPaletteItem) {
  var wrapper = createWrapper(aPaletteItem.id, document);

  wrapper.appendChild(aPaletteItem);

  // XXX We need to call this AFTER the palette item has been appended
  // to the wrapper or else we crash dropping certain buttons on the
  // palette due to removal of the command and disabled attributes - JRH
  cleanUpItemForPalette(aPaletteItem, wrapper);

  gPaletteBox.appendChild(wrapper);
}

/**
 * Wraps an item that is currently on a toolbar and replaces the item
 * with the wrapper. This is not used when dropping items from the palette,
 * only when first starting the dialog and wrapping everything on the toolbars.
 */
function wrapToolbarItem(aToolbarItem) {
  var wrapper = createWrapper(aToolbarItem.id, gToolboxDocument);

  wrapper.flex = aToolbarItem.flex;

  aToolbarItem.parentNode.replaceChild(wrapper, aToolbarItem);

  wrapper.appendChild(aToolbarItem);

  return wrapper;
}

/**
 * Get the list of ids for the current set of items on each toolbar.
 */
function getCurrentItemIds() {
  var currentItems = {};
  forEachCustomizableToolbar(function(toolbar) {
    var child = toolbar.firstChild;
    while (child) {
      if (isToolbarItem(child))
        currentItems[child.id] = 1;
      child = child.nextSibling;
    }
  });
  return currentItems;
}

/**
 * Builds the palette of draggable items that are not yet in a toolbar.
 */
function buildPalette() {
  // Empty the palette first.
  while (gPaletteBox.lastChild)
    gPaletteBox.removeChild(gPaletteBox.lastChild);

  // Add the toolbar separator item.
  var templateNode = document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
                                              "toolbarseparator");
  templateNode.id = "separator";
  wrapPaletteItem(templateNode);

  // Add the toolbar spring item.
  templateNode = document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
                                              "toolbarspring");
  templateNode.id = "spring";
  templateNode.flex = 1;
  wrapPaletteItem(templateNode);

  // Add the toolbar spacer item.
  templateNode = document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
                                              "toolbarspacer");
  templateNode.id = "spacer";
  templateNode.flex = 1;
  wrapPaletteItem(templateNode);

  var currentItems = getCurrentItemIds();
  templateNode = gToolbox.palette.firstChild;
  while (templateNode) {
    // Check if the item is already in a toolbar before adding it to the palette.
    if (!(templateNode.id in currentItems)) {
      var paletteItem = document.importNode(templateNode, true);
      wrapPaletteItem(paletteItem);
    }

    templateNode = templateNode.nextSibling;
  }
}

/**
 * Makes sure that an item that has been cloned from a template
 * is stripped of any attributes that may adversely affect its
 * appearance in the palette.
 */
function cleanUpItemForPalette(aItem, aWrapper) {
  aWrapper.setAttribute("place", "palette");
  setWrapperType(aItem, aWrapper);

  if (aItem.hasAttribute("title"))
    aWrapper.setAttribute("title", aItem.getAttribute("title"));
  else if (aItem.hasAttribute("label"))
    aWrapper.setAttribute("title", aItem.getAttribute("label"));
  else if (isSpecialItem(aItem)) {
    var stringBundle = document.getElementById("stringBundle");
    // Remove the common "toolbar" prefix to generate the string name.
    var title = stringBundle.getString(aItem.localName.slice(7) + "Title");
    aWrapper.setAttribute("title", title);
  }
  aWrapper.setAttribute("tooltiptext", aWrapper.getAttribute("title"));

  // Remove attributes that screw up our appearance.
  aItem.removeAttribute("command");
  aItem.removeAttribute("observes");
  aItem.removeAttribute("type");
  aItem.removeAttribute("width");

  Array.forEach(aWrapper.querySelectorAll("[disabled]"), function(aNode) {
    aNode.removeAttribute("disabled");
  });
}

/**
 * Makes sure that an item that has been cloned from a template
 * is stripped of all properties that may adversely affect its
 * appearance in the toolbar.  Store critical properties on the
 * wrapper so they can be put back on the item when we're done.
 */
function cleanupItemForToolbar(aItem, aWrapper) {
  setWrapperType(aItem, aWrapper);
  aWrapper.setAttribute("place", "toolbar");

  if (aItem.hasAttribute("command")) {
    aWrapper.setAttribute("itemcommand", aItem.getAttribute("command"));
    aItem.removeAttribute("command");
  }

  if (aItem.checked) {
    aWrapper.setAttribute("itemchecked", "true");
    aItem.checked = false;
  }

  if (aItem.disabled) {
    aWrapper.setAttribute("itemdisabled", "true");
    aItem.disabled = false;
  }
}

/**
 * Restore all the properties that we stripped off above.
 */
function restoreItemForToolbar(aItem, aWrapper) {
  if (aWrapper.hasAttribute("itemdisabled"))
    aItem.disabled = true;

  if (aWrapper.hasAttribute("itemchecked"))
    aItem.checked = true;

  if (aWrapper.hasAttribute("itemcommand")) {
    let commandID = aWrapper.getAttribute("itemcommand");
    aItem.setAttribute("command", commandID);

    // XXX Bug 309953 - toolbarbuttons aren't in sync with their commands after customizing
    let command = gToolboxDocument.getElementById(commandID);
    if (command && command.hasAttribute("disabled"))
      aItem.setAttribute("disabled", command.getAttribute("disabled"));
  }
}

function setWrapperType(aItem, aWrapper) {
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

function setDragActive(aItem, aValue) {
  var node = aItem;
  var direction = window.getComputedStyle(aItem, null).direction;
  var value = direction == "ltr" ? "left" : "right";
  if (aItem.localName == "toolbar") {
    node = aItem.lastChild;
    value = direction == "ltr" ? "right" : "left";
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

function addNewToolbar() {
  var promptService = Services.prompt;
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
    for (let i = 0; i < gToolbox.childNodes.length; ++i) {
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
function restoreDefaultSet() {
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
  forEachCustomizableToolbar(function(toolbar) {
    var defaultSet = toolbar.getAttribute("defaultset");
    if (defaultSet)
      toolbar.currentSet = defaultSet;
  });

  // Restore the default icon size and mode.
  document.getElementById("smallicons").checked = (updateIconSize() == "small");
  document.getElementById("modelist").value = updateToolbarMode();

  // Now rebuild the palette.
  buildPalette();

  // Now re-wrap the items on the toolbar.
  wrapToolbarItems();

  toolboxChanged("reset");
}

function updateIconSize(aSize) {
  return updateToolboxProperty("iconsize", aSize, "large");
}

function updateToolbarMode(aModeValue) {
  var mode = updateToolboxProperty("mode", aModeValue, "icons");

  var iconSizeCheckbox = document.getElementById("smallicons");
  iconSizeCheckbox.disabled = mode == "text";

  return mode;
}

function updateToolboxProperty(aProp, aValue, aToolkitDefault) {
  var toolboxDefault = gToolbox.getAttribute("default" + aProp) ||
                       aToolkitDefault;

  gToolbox.setAttribute(aProp, aValue || toolboxDefault);
  gToolboxDocument.persist(gToolbox.id, aProp);

  forEachCustomizableToolbar(function(toolbar) {
    var toolbarDefault = toolbar.getAttribute("default" + aProp) ||
                         toolboxDefault;
    if (toolbar.getAttribute("lock" + aProp) == "true" &&
        toolbar.getAttribute(aProp) == toolbarDefault)
      return;

    toolbar.setAttribute(aProp, aValue || toolbarDefault);
    gToolboxDocument.persist(toolbar.id, aProp);
  });

  toolboxChanged(aProp);

  return aValue || toolboxDefault;
}

function forEachCustomizableToolbar(callback) {
  Array.filter(gToolbox.childNodes, isCustomizableToolbar).forEach(callback);
  Array.filter(gToolbox.externalToolbars, isCustomizableToolbar).forEach(callback);
}

function isCustomizableToolbar(aElt) {
  return aElt.localName == "toolbar" &&
         aElt.getAttribute("customizable") == "true";
}

function isSpecialItem(aElt) {
  return aElt.localName == "toolbarseparator" ||
         aElt.localName == "toolbarspring" ||
         aElt.localName == "toolbarspacer";
}

function isToolbarItem(aElt) {
  return aElt.localName == "toolbarbutton" ||
         aElt.localName == "toolbaritem" ||
         aElt.localName == "toolbarseparator" ||
         aElt.localName == "toolbarspring" ||
         aElt.localName == "toolbarspacer";
}

// Drag and Drop observers

function onToolbarDragExit(aEvent) {
  if (isUnwantedDragEvent(aEvent)) {
    return;
  }

  if (gCurrentDragOverItem)
    setDragActive(gCurrentDragOverItem, false);
}

function onToolbarDragStart(aEvent) {
  var item = aEvent.target;
  while (item && item.localName != "toolbarpaletteitem") {
    if (item.localName == "toolbar")
      return;
    item = item.parentNode;
  }

  item.setAttribute("dragactive", "true");

  var dt = aEvent.dataTransfer;
  var documentId = gToolboxDocument.documentElement.id;
  dt.setData("text/toolbarwrapper-id/" + documentId, item.firstChild.id);
  dt.effectAllowed = "move";
}

function onToolbarDragOver(aEvent) {
  if (isUnwantedDragEvent(aEvent)) {
    return;
  }

  var documentId = gToolboxDocument.documentElement.id;
  if (!aEvent.dataTransfer.types.includes("text/toolbarwrapper-id/" + documentId.toLowerCase()))
    return;

  var toolbar = aEvent.target;
  var dropTarget = aEvent.target;
  while (toolbar && toolbar.localName != "toolbar") {
    dropTarget = toolbar;
    toolbar = toolbar.parentNode;
  }

  // Make sure we are dragging over a customizable toolbar.
  if (!toolbar || !isCustomizableToolbar(toolbar)) {
    gCurrentDragOverItem = null;
    return;
  }

  var previousDragItem = gCurrentDragOverItem;

  if (dropTarget.localName == "toolbar") {
    gCurrentDragOverItem = dropTarget;
  } else {
    gCurrentDragOverItem = null;

    var direction = window.getComputedStyle(dropTarget.parentNode, null).direction;
    var dropTargetCenter = dropTarget.boxObject.x + (dropTarget.boxObject.width / 2);
    var dragAfter;
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

  aEvent.preventDefault();
  aEvent.stopPropagation();
}

function onToolbarDrop(aEvent) {
  if (isUnwantedDragEvent(aEvent)) {
    return;
  }

  if (!gCurrentDragOverItem)
    return;

  setDragActive(gCurrentDragOverItem, false);

  var documentId = gToolboxDocument.documentElement.id;
  var draggedItemId = aEvent.dataTransfer.getData("text/toolbarwrapper-id/" + documentId);
  if (gCurrentDragOverItem.id == draggedItemId)
    return;

  var toolbar = aEvent.target;
  while (toolbar.localName != "toolbar")
    toolbar = toolbar.parentNode;

  var draggedPaletteWrapper = document.getElementById("wrapper-" + draggedItemId);
  if (!draggedPaletteWrapper) {
    // The wrapper has been dragged from the toolbar.
    // Get the wrapper from the toolbar document and make sure that
    // it isn't being dropped on itself.
    let wrapper = gToolboxDocument.getElementById("wrapper-" + draggedItemId);
    if (wrapper == gCurrentDragOverItem)
       return;

    // Don't allow non-removable kids (e.g., the menubar) to move.
    if (wrapper.firstChild.getAttribute("removable") != "true")
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
    let wrapper = createWrapper("", gToolboxDocument);

    // Ask the toolbar to clone the item's template, place it inside the wrapper, and insert it in the toolbar.
    var newItem = toolbar.insertItem(draggedItemId, gCurrentDragOverItem == toolbar ? null : gCurrentDragOverItem, wrapper);

    // Prepare the item and wrapper to look good on the toolbar.
    cleanupItemForToolbar(newItem, wrapper);
    wrapper.id = "wrapper-" + newItem.id;
    wrapper.flex = newItem.flex;

    // Remove the wrapper from the palette.
    if (draggedItemId != "separator" &&
        draggedItemId != "spring" &&
        draggedItemId != "spacer")
      gPaletteBox.removeChild(draggedPaletteWrapper);
  }

  gCurrentDragOverItem = null;

  toolboxChanged();
}

function onPaletteDragOver(aEvent) {
  if (isUnwantedDragEvent(aEvent)) {
    return;
  }
  var documentId = gToolboxDocument.documentElement.id;
  if (aEvent.dataTransfer.types.includes("text/toolbarwrapper-id/" + documentId.toLowerCase()))
    aEvent.preventDefault();
}

function onPaletteDrop(aEvent) {
  if (isUnwantedDragEvent(aEvent)) {
    return;
  }
  var documentId = gToolboxDocument.documentElement.id;
  var itemId = aEvent.dataTransfer.getData("text/toolbarwrapper-id/" + documentId);

  var wrapper = gToolboxDocument.getElementById("wrapper-" + itemId);
  if (wrapper) {
    // Don't allow non-removable kids (e.g., the menubar) to move.
    if (wrapper.firstChild.getAttribute("removable") != "true")
      return;

    var wrapperType = wrapper.getAttribute("type");
    if (wrapperType != "separator" &&
        wrapperType != "spacer" &&
        wrapperType != "spring") {
      restoreItemForToolbar(wrapper.firstChild, wrapper);
      wrapPaletteItem(document.importNode(wrapper.firstChild, true));
      gToolbox.palette.appendChild(wrapper.firstChild);
    }

    // The item was dragged out of the toolbar.
    wrapper.parentNode.removeChild(wrapper);
  }

  toolboxChanged();
}


function isUnwantedDragEvent(aEvent) {
  try {
    if (Services.prefs.getBoolPref("toolkit.customization.unsafe_drag_events")) {
      return false;
    }
  } catch (ex) {}

  /* Discard drag events that originated from a separate window to
     prevent content->chrome privilege escalations. */
  let mozSourceNode = aEvent.dataTransfer.mozSourceNode;
  // mozSourceNode is null in the dragStart event handler or if
  // the drag event originated in an external application.
  if (!mozSourceNode) {
    return true;
  }
  let sourceWindow = mozSourceNode.ownerDocument.defaultView;
  return sourceWindow != window && sourceWindow != gToolboxDocument.defaultView;
}

