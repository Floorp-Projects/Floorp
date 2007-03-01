/****** BEGIN LICENSE BLOCK *****
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
# ***** END LICENSE BLOCK *****/

const kRowMax = 3;
const kWindowWidth = 664;
const kWindowHeight = 480;
const kVSizeSlop = 5;

var gToolboxDocument = null;
var gToolbox = null;
var gCurrentDragOverItem = null;
var gToolboxChanged = false;
var gPreviousMode = null;
var gPreviousIconSize = null;

var gMboxTree = null;
var gMenuItemTree = null;
var gRecipientTree = null;

function doAdoptNode(pnode, node)
{
    try 
    {
        pnode.adoptNode(node);
    }
    catch(e)
    {
        // adoptNode is not supported in thunderbird 2.0
    }
}

function onLoad()
{
    gToolbox = window.arguments[0];
    gToolboxDocument = gToolbox.ownerDocument;
  
    gToolbox.addEventListener("draggesture", onToolbarDragGesture, false);
    gToolbox.addEventListener("dragover", onToolbarDragOver, false);
    gToolbox.addEventListener("dragexit", onToolbarDragExit, false);
    gToolbox.addEventListener("dragdrop", onToolbarDragDrop, false);
    repositionDialog();

    initDialog();
}

function onUnload(aEvent)
{
    removeToolboxListeners();
    unwrapToolbarItems(true);
    persistCurrentSets();
  
    notifyParentComplete();
}

function onCancel()
{
    // restore the saved toolbarset for each customizeable toolbar
    unwrapToolbarItems(false);
    var toolbar = gToolbox.firstChild;
    while (toolbar) 
    {
        if (isCustomizableToolbar(toolbar)) 
        {
            if (!toolbar.hasAttribute("customindex")) 
            {
                var previousset = toolbar.getAttribute("previousset");
                if (previousset)
                {
                    toolbar.currentSet = previousset;
                }
            }
        }
        toolbar = toolbar.nextSibling;
    }

    updateIconSize(gPreviousIconSize == "small");
    updateToolbarMode(gPreviousMode);

    repositionDialog();
    gToolboxChanged = true;

    return onAccept(); // we restored the default toolbar, act like a normal accept event now
}

function onAccept(aEvent)
{
    window.close();
}

function initDialog()
{
    gPreviousMode = gToolbox.getAttribute("mode");
    document.getElementById("modelist").value = gPreviousMode;
    gPreviousIconSize = gToolbox.getAttribute("iconsize");
    var smallIconsCheckbox = document.getElementById("smallicons");
    if (gPreviousMode == "text")
    {
        smallIconsCheckbox.disabled = true;
    }
    else
    {
        smallIconsCheckbox.checked = gPreviousIconSize == "small";
    }

    buildFormatItemPalette();
    buildExtrasPalette();

    // Wrap all the items on the toolbar in toolbarpaletteitems.
    wrapToolbarItems();

    gMenuItemTree = document.getElementById("menubartree");
    gMboxTree = document.getElementById("mailboxtree");
    gRecipientTree = document.getElementById("abResultsTree");
    var menu = gToolboxDocument.getElementsByTagName("menubar").item(0);

    createMenubarTree(menu, gMenuItemTree.getElementsByTagName("treechildren").item(0));

    // from abcommon.js
    InitCommonJS();
    gSearchInput = document.getElementById("searchInput");

    // Select personal addresses directory
    var treeBuilder = dirTree.builder.QueryInterface(Components.interfaces.nsIXULTreeBuilder);
    SelectFirstAddressBook();

    //SetupAbCommandUpdateHandlers();
}

function repositionDialog()
{
    // Position the dialog touching the bottom of the toolbox and centered with 
    // it. We must resize the window smaller first so that it is positioned 
    // properly. 
    var screenX = gToolbox.boxObject.screenX + ((gToolbox.boxObject.width - kWindowWidth) / 2);
    var screenY = gToolbox.boxObject.screenY + gToolbox.boxObject.height;

    var newHeight = kWindowHeight;
    if (newHeight >= screen.availHeight - screenY - kVSizeSlop) 
    {
        newHeight = screen.availHeight - screenY - kVSizeSlop;
    }

    window.resizeTo(kWindowWidth, newHeight);
    window.moveTo(screenX, screenY);
}

function removeToolboxListeners()
{
    gToolbox.removeEventListener("draggesture", onToolbarDragGesture, false);
    gToolbox.removeEventListener("dragover", onToolbarDragOver, false);
    gToolbox.removeEventListener("dragexit", onToolbarDragExit, false);
    gToolbox.removeEventListener("dragdrop", onToolbarDragDrop, false);
}

/**
 * Invoke a callback on the toolbox to notify it that the dialog is done
 * and going away.
 */
function notifyParentComplete()
{
    if ("customizeDone" in gToolbox)
    {
        gToolbox.customizeDone(gToolboxChanged);
    }
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
    if (!gToolboxChanged)
    {
       return;
    }

    var customCount = 0;
    for (var i = 0; i < gToolbox.childNodes.length; ++i) 
    {
        // Look for customizable toolbars that need to be persisted.
        var toolbar = getToolbarAt(i);
        if (isCustomizableToolbar(toolbar)) 
        {
            // Calculate currentset and store it in the attribute.
            var currentSet = toolbar.currentSet;
            toolbar.setAttribute("currentset", currentSet);
            
            var customIndex = toolbar.hasAttribute("customindex");
            if (customIndex) 
            {
                if (!toolbar.firstChild) 
                {
                    // Remove custom toolbars whose contents have been removed.
                    gToolbox.removeChild(toolbar);
                    --i;
                } 
                else 
                {
                    // Persist custom toolbar info on the <toolbarset/>
                    gToolbox.toolbarset.setAttribute("toolbar"+(++customCount),
                                                     toolbar.toolbarName + ":" + currentSet);
                    gToolboxDocument.persist(gToolbox.toolbarset.id, "toolbar"+customCount);
                }
            }

            if (!customIndex) 
            {
              // Persist the currentset attribute directly on hardcoded toolbars.
              gToolboxDocument.persist(toolbar.id, "currentset");
            }
        }
    }
    
    // Remove toolbarX attributes for removed toolbars.
    while (gToolbox.toolbarset.hasAttribute("toolbar"+(++customCount))) 
    {
        gToolbox.toolbarset.removeAttribute("toolbar"+customCount);
        gToolboxDocument.persist(gToolbox.toolbarset.id, "toolbar"+customCount);
    }
}

/**
 * Wraps all items in all customizable toolbars in a toolbox.
 */
function wrapToolbarItems()
{
    for (var i = 0; i < gToolbox.childNodes.length; ++i) 
    {
        var toolbar = getToolbarAt(i);
        if (isCustomizableToolbar(toolbar)) 
        {
            // save off the current set for each toolbar in case the user hits cancel
            toolbar.setAttribute('previousset', toolbar.currentSet);

            for (var k = 0; k < toolbar.childNodes.length; ++k) 
            {
                var item = toolbar.childNodes[k];
                if (isToolbarItem(item)) 
                {
                    var nextSibling = item.nextSibling;
                    
                    var wrapper = wrapToolbarItem(item);     
                    if (nextSibling)
                    {
                        toolbar.insertBefore(wrapper, nextSibling);
                    }
                    else
                    {
                        toolbar.appendChild(wrapper);
                    }
                }
            }
        }
    }
}

/**
 * Unwraps all items in all customizable toolbars in a toolbox.
 */
function unwrapToolbarItems(aRemovePreviousSet)
{
    for (var i = 0; i < gToolbox.childNodes.length; ++i) 
    {
        var toolbar = getToolbarAt(i);
        if (isCustomizableToolbar(toolbar)) 
        {
            if (aRemovePreviousSet)
            {
                toolbar.removeAttribute('previousset');
            }

            for (var k = 0; k < toolbar.childNodes.length; ++k) 
            {
                var paletteItem = toolbar.childNodes[k];
                var toolbarItem = paletteItem.firstChild;

                if (toolbarItem && isToolbarItem(toolbarItem)) 
                {
                    var nextSibling = paletteItem.nextSibling;

                    if (paletteItem.hasAttribute("itemcommand"))
                    {
                        toolbarItem.setAttribute("command", paletteItem.getAttribute("itemcommand"));
                    }
                    if (paletteItem.hasAttribute("itemobserves"))
                    {
                        toolbarItem.setAttribute("observes", paletteItem.getAttribute("itemobserves"));
                    }

                    paletteItem.removeChild(toolbarItem); 
                    paletteItem.parentNode.removeChild(paletteItem);

                    if (nextSibling)
                    {
                        toolbar.insertBefore(toolbarItem, nextSibling);
                    }
                    else
                    {
                        toolbar.appendChild(toolbarItem);
                    }
                }
            }
        }
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

    wrapper.setAttribute("flex", 1);
    wrapper.setAttribute("align", "center");
    wrapper.setAttribute("pack", "center");
    wrapper.setAttribute("minheight", "0");
    wrapper.setAttribute("minwidth", "0");

    doAdoptNode(document, aPaletteItem);
    wrapper.appendChild(aPaletteItem);
    
    // XXX We need to call this AFTER the palette item has been appended
    // to the wrapper or else we crash dropping certain buttons on the 
    // palette due to removal of the command and disabled attributes - JRH
    cleanUpItemForPalette(aPaletteItem, wrapper);

    if (aSpacer)
    {
        aCurrentRow.insertBefore(wrapper, aSpacer);
    }
    else
    {
        aCurrentRow.appendChild(wrapper);
    }
}

/**
 * Wraps an item that is currently on a toolbar and replaces the item
 * with the wrapper. This is not used when dropping items from the palette,
 * only when first starting the dialog and wrapping everything on the toolbars.
 */
function wrapToolbarItem(aToolbarItem)
{
    var wrapper = createWrapper(aToolbarItem.id);
    doAdoptNode(gToolboxDocument, wrapper);

    cleanupItemForToolbar(aToolbarItem, wrapper);
    wrapper.flex = aToolbarItem.flex;

    if (aToolbarItem.parentNode)
      aToolbarItem.parentNode.removeChild(aToolbarItem);
    
    wrapper.appendChild(aToolbarItem);
    
    return wrapper;
}

/**
 * Builds the palette of draggable items that are not yet in a toolbar.
 */
function buildPalette(mypalette, toolbaritem, descId)
{
    dump("in buildPalette "+toolbaritem+"\n");
    // Empty the palette first.
    var paletteBox = document.getElementById(mypalette);
    while (paletteBox.lastChild)
    {
        paletteBox.removeChild(paletteBox.lastChild);
    }

    var currentRow = document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
                                              "hbox");
    currentRow.setAttribute("class", "paletteRow");

    var rowSlot = 0;

    var items = gToolbox.palette.getElementsByAttribute('id', toolbaritem);
    if (items.length)
    {
        var paletteItem = items.item(0).cloneNode(true);

        wrapPaletteItem(paletteItem, currentRow, null);
        setDescText(descId, paletteItem.getAttribute("tooltiptext"));
    }
    else
    {
        notSupported(descId);
    }

    if (currentRow) 
    { 
        fillRowWithFlex(currentRow);
        paletteBox.appendChild(currentRow);
    }
    dump("leaving buildPalette "+toolbaritem+"\n");
}

/**
 * Builds the palette of draggable items that are not yet in a toolbar.
 */
function buildFormatItemPalette()
{
    dump("in buildFormatItemPalette\n");
    var descId = "formatItemDescription";

    // Empty the palette first.
    var paletteBox = document.getElementById("format-palette");
    while (paletteBox.lastChild)
    {
        paletteBox.removeChild(paletteBox.lastChild);
    }

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

    setDescText(descId, "Add a separator, flexible space, or space to the toolbar");

    if (currentRow) 
    { 
        fillRowWithFlex(currentRow);
        paletteBox.appendChild(currentRow);
    }
    dump("leaving buildFormatItemPalette\n");
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
     
    if (lastSpacer.localName != "spacer") 
    {
        // The current row is full, so we have to create a new row.
        lastRow = document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
                                            "hbox");
        lastRow.setAttribute("class", "paletteRow");
        paletteBox.appendChild(lastRow);
        
        wrapPaletteItem(aItem, lastRow, null);

        fillRowWithFlex(lastRow);
    } 
    else 
    {
        // Decrement the flex of the last spacer or remove it entirely.
        var flex = lastSpacer.getAttribute("flex");
        if (flex == 1) 
        {
            lastRow.removeChild(lastSpacer);
            lastSpacer = null;
        } 
        else
        {
            lastSpacer.setAttribute("flex", --flex);
        }

        // Insert the wrapper where the last spacer was.
        wrapPaletteItem(aItem, lastRow, lastSpacer);
    }
}

/**
 * Builds the palette of draggable items that are not yet in a toolbar.
 */
function buildMailboxPalette(uri, name, disableMoveto)
{
    dump("in buildMailboxPalette "+uri+"\n");
    var descId = "mailboxDescription";

    // Empty the palette first.
    var paletteBox = document.getElementById("mailbox-palette");
    while (paletteBox.lastChild)
    {
        paletteBox.removeChild(paletteBox.lastChild);
    }

    var currentRow = document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
                                              "hbox");
    currentRow.setAttribute("class", "paletteRow");

    var paletteItem = createFolderOpenButton(uri, name);
    wrapPaletteItem(paletteItem, currentRow, null);

    if (!disableMoveto)
    {
        var paletteItem = createFolderMovetoButton(uri, name);
        wrapPaletteItem(paletteItem, currentRow, null);

        setDescText(descId, "Open "+name+", or move mail to "+name);
    }
    else
    {
        setDescText(descId, "Open "+name);
    }

    if (currentRow) 
    { 
        fillRowWithFlex(currentRow);
        paletteBox.appendChild(currentRow);
    }
    dump("leaving buildMailboxPalette "+uri+"\n");
}

/**
 * Builds the palette of draggable items for recipients.
 */
function buildRecipientsPalette(email, name)
{
    dump("in buildRecipientsPalette\n");
    var descId = "recipientDescription";

    // Empty the palette first.
    var paletteBox = document.getElementById("recipient-palette");
    while (paletteBox.lastChild)
    {
        paletteBox.removeChild(paletteBox.lastChild);
    }

    var currentRow = document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
                                              "hbox");
    currentRow.setAttribute("class", "paletteRow");

    var node = gToolbox.palette.firstChild;
    var rowSlot = 1;

    // Add the create new message item.
    var newMailNode = createRecipientButton(email, name, "p-recipient-");
    newMailNode.flex = 1;
    wrapPaletteItem(newMailNode, currentRow, null);

    var forwardNode = createRecipientButton(email, name, "p-forward-");
    forwardNode.flex = 1;
    wrapPaletteItem(forwardNode, currentRow, null);

    var redirectNode = createRecipientButton(email, name, "p-redirect-");
    redirectNode.flex = 1;
    wrapPaletteItem(redirectNode, currentRow, null);

    var insertNode = createRecipientButton(email, name, "p-insert-");
    insertNode.flex = 1;
    wrapPaletteItem(insertNode, currentRow, null);

    setDescText(descId, "For recipient: create new mail, forward mail, redirect mail or insert as text");

    if (currentRow) 
    { 
        fillRowWithFlex(currentRow);
        paletteBox.appendChild(currentRow);
    }
    dump("leaving buildRecipientsPalette\n");
}

/**
 * Builds the palette of draggable items that are not yet in a toolbar.
 */
function buildExtrasPalette()
{
    dump("in buildExtrasPalette\n");

    // Empty the palette first.
    var paletteBox = document.getElementById("extras-palette");
    while (paletteBox.lastChild)
    {
        paletteBox.removeChild(paletteBox.lastChild);
    }

    var currentRow = document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
                                              "hbox");
    currentRow.setAttribute("class", "paletteRow");

    var node = gToolbox.palette.firstChild;
    var rowSlot = 1;
    var ignore;
    while (node)
    {
        ignore = true;
        switch(node.id)
        {
        case "button-getmsg":
        case "button-newmsg":
        case "button-reply":
        case "button-replyall":
        case "button-forward":
        case "button-file":
        case "button-goback":
        case "button-goforward":
        case "button-previous":
        case "button-next":
        case "button-junk":
        case "button-delete":
        case "button-print":
        case "button-mark":
        case "button-tag":
        case "button-address":
        case "throbber-box":
        case "button-stop":
        case "button-in":
        case "button-out":
        case "button-help":
        case "button-close":
        case "button-saveas":
        case "cut-button":
        case "copy-button":
        case "paste-button":
        case "undo-button":
        case "redo-button":
        case "button-redirect":
        case "button-resend":
        case "button-sendunsent":
        case "button-subscribe":
        case "button-emptytrash":
        case "button-offline":
        case "button-printsetup":
        case "button-nextunread":
        case "button-nextstarred":
        case "button-nextunreadthread":
        case "button-prevunread":
        case "button-prevstarred":
        case "button-prefs":
        case "priority-button":
        case "button-status":
            break;
        default:
            if ((node.id.substring(0,7) == "p-open-") ||
                (node.id.substring(0,9) == "p-moveto-") ||
                (node.id.substring(0,12) == "p-recipient-") ||
                (node.id.substring(0,10) == "p-forward-") ||
                (node.id.substring(0,11) == "p-redirect-") ||
                (node.id.substring(0,9)  == "p-insert-"))
            {
            }
            else
            {
                ignore = false;
            }
        }
        if (!ignore)
        {
            dump("Extra toolbar item: "+node.id+"\n");
            var paletteItem = node.cloneNode(true);
            if (rowSlot == kRowMax) 
            {
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
        node = node.nextSibling;
    }

    
    if (currentRow) 
    { 
        fillRowWithFlex(currentRow);
        paletteBox.appendChild(currentRow);
    }
    dump("leaving buildExtrasPalette\n");
}

function fillRowWithFlex(aRow)
{
    var remainingFlex = kRowMax - aRow.childNodes.length;
    if (remainingFlex > 0) 
    {
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
    dump("In cleanUpItemForPalette\n");
    aWrapper.setAttribute("place", "palette");
    setWrapperType(aItem, aWrapper);

    if (aItem.hasAttribute("title"))
    {
        aWrapper.setAttribute("title", aItem.getAttribute("title"));
    }
    else if (isSpecialItem(aItem)) 
    {
        var stringBundle = document.getElementById("stringBundle");
        var title = stringBundle.getString(aItem.id + "Title");
        aWrapper.setAttribute("title", title);
    }
    
    // Remove attributes that screw up our appearance.
    aItem.removeAttribute("command");
    aItem.removeAttribute("observes");
    aItem.removeAttribute("disabled");
    aItem.removeAttribute("type");
    
    if (aItem.localName == "toolbaritem" && aItem.firstChild) 
    {
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
    setWrapperType(aItem, aWrapper);
    aWrapper.setAttribute("place", "toolbar");

    if (aItem.hasAttribute("command")) 
    {
        aWrapper.setAttribute("itemcommand", aItem.getAttribute("command"));
        aItem.removeAttribute("command");
    }

    if (aItem.hasAttribute("observes"))
    {
        aWrapper.setAttribute("itemobserves", aItem.getAttribute("observes"));
        aItem.removeAttribute("observes");
    }

    if (aItem.disabled) 
    {
        aWrapper.setAttribute("itemdisabled", "true");
        aItem.disabled = false;
    }
}

function setWrapperType(aItem, aWrapper)
{
    dump("aItem.localName = "+aItem.localName+"\n");
    try
    {
        if (aItem.localName == "toolbarseparator") 
        {
            aWrapper.setAttribute("type", "separator");
        } 
        else if (aItem.localName == "toolbarspring") 
        {
            aWrapper.setAttribute("type", "spring");
        } 
        else if (aItem.localName == "toolbarspacer") 
        {
            aWrapper.setAttribute("type", "spacer");
        } 
        else if (aItem.localName == "toolbaritem" && aItem.firstChild) 
        {
            aWrapper.setAttribute("type", aItem.firstChild.localName);
        }
    }
    catch(e)
    {
        dump("Exception: aItem = "+aItem+"\n");
    }
}

function setDragActive(aItem, aValue)
{
    var node = aItem;
    var direction = window.getComputedStyle(aItem, null).direction;
    var value = direction == "ltr"? "left" : "right";
    if (aItem.localName == "toolbar") 
    {
        node = aItem.lastChild;
        value = direction == "ltr"? "right" : "left";
    }
    
    if (!node)
    {
        return;
    }
    
    if (aValue) 
    {
        if (!node.hasAttribute("dragover"))
        {
            node.setAttribute("dragover", value);
        }
    } 
    else 
    {
        node.removeAttribute("dragover");
    }
}

function updateIconSize(aUseSmallIcons)
{
    var val = aUseSmallIcons ? "small" : null;
    
    setAttribute(gToolbox, "iconsize", val);
    gToolboxDocument.persist(gToolbox.id, "iconsize");
    
    for (var i = 0; i < gToolbox.childNodes.length; ++i) 
    {
        var toolbar = getToolbarAt(i);
        if (isCustomizableToolbar(toolbar)) 
        {
            setAttribute(toolbar, "iconsize", val);
            gToolboxDocument.persist(toolbar.id, "iconsize");
        }
    }

    repositionDialog();
}

function updateToolbarMode(aModeValue)
{
    setAttribute(gToolbox, "mode", aModeValue);
    gToolboxDocument.persist(gToolbox.id, "mode");

    for (var i = 0; i < gToolbox.childNodes.length; ++i) 
    {
        var toolbar = getToolbarAt(i);
        if (isCustomizableToolbar(toolbar)) 
        {
            setAttribute(toolbar, "mode", aModeValue);
            gToolboxDocument.persist(toolbar.id, "mode");
        }
    }

    var iconSizeCheckbox = document.getElementById("smallicons");
    iconSizeCheckbox.disabled = aModeValue == "text";

    repositionDialog();
}

function setAttribute(aElt, aAttr, aVal)
{
    if (aVal)
    {
        aElt.setAttribute(aAttr, aVal);
    }
    else
    {
        aElt.removeAttribute(aAttr);
    }
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
    {
        setDragActive(gCurrentDragOverItem, false);
    }
}

var dragStartObserver =
{
    onDragStart: function (aEvent, aXferData, aDragAction) {
        var documentId = gToolboxDocument.documentElement.id;
        
        var item = aEvent.target;
        while (item && item.localName != "toolbarpaletteitem")
        {
            item = item.parentNode;
        }
        
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
        var dropTarget = aEvent.target;
        while (toolbar && toolbar.localName != "toolbar") 
        {
            dropTarget = toolbar;
            toolbar = toolbar.parentNode;
        }
        
        var previousDragItem = gCurrentDragOverItem;

        // Make sure we are dragging over a customizable toolbar.
        if (!isCustomizableToolbar(toolbar)) 
        {
            gCurrentDragOverItem = null;
            return;
        }
        
        if (dropTarget.localName == "toolbar") 
        {
            gCurrentDragOverItem = dropTarget;
        } 
        else 
        {
            gCurrentDragOverItem = null;

            var direction = window.getComputedStyle(dropTarget.parentNode, null).direction;
            var dropTargetCenter = dropTarget.boxObject.x + (dropTarget.boxObject.width / 2);
            if (direction == "ltr")
            {
                dragAfter = aEvent.clientX > dropTargetCenter;
            }
            else
            {
                dragAfter = aEvent.clientX < dropTargetCenter;
            }
              
            if (dragAfter) 
            {
                gCurrentDragOverItem = dropTarget.nextSibling;
                if (!gCurrentDragOverItem)
                {
                    gCurrentDragOverItem = toolbar;
                }
            } 
            else
            {
                gCurrentDragOverItem = dropTarget;
            }
        }    

        if (previousDragItem && gCurrentDragOverItem != previousDragItem) 
        {
            setDragActive(previousDragItem, false);
        }
        
        setDragActive(gCurrentDragOverItem, true);
        
        aDragSession.canDrop = true;
    },
    
    onDrop: function (aEvent, aXferData, aDragSession)
    {
        if (!gCurrentDragOverItem)
        {
            return;
        }
        
        setDragActive(gCurrentDragOverItem, false);

        var draggedItemId = aXferData.data;
        if (gCurrentDragOverItem.id == draggedItemId)
        {
            return;
        }

        var toolbar = aEvent.target;
        while (toolbar.localName != "toolbar")
        {
            toolbar = toolbar.parentNode;
        }

        var draggedPaletteWrapper = document.getElementById("wrapper-"+draggedItemId);       
        if (!draggedPaletteWrapper) 
        {
            // The wrapper has been dragged from the toolbar.
            
            // Get the wrapper from the toolbar document and make sure that
            // it isn't being dropped on itself.
            var wrapper = gToolboxDocument.getElementById("wrapper-"+draggedItemId);
            if (wrapper == gCurrentDragOverItem)
            {
                return;
            }

            // Don't allow static kids (e.g., the menubar) to move.
            if (wrapper.parentNode.firstPermanentChild && wrapper.parentNode.firstPermanentChild.id == wrapper.firstChild.id)
            {
                return;
            }
            if (wrapper.parentNode.lastPermanentChild && wrapper.parentNode.lastPermanentChild.id == wrapper.firstChild.id)
            {
                return;
            }

            // Remove the item from it's place in the toolbar.
            wrapper.parentNode.removeChild(wrapper);

            // Determine which toolbar we are dropping on.
            var dropToolbar = null;
            if (gCurrentDragOverItem.localName == "toolbar")
            {
                dropToolbar = gCurrentDragOverItem;
            }
            else
            {
                dropToolbar = gCurrentDragOverItem.parentNode;
            }
            
            // Insert the item into the toolbar.
            if (gCurrentDragOverItem != dropToolbar)
            {
                dropToolbar.insertBefore(wrapper, gCurrentDragOverItem);
            }
            else
            {
                dropToolbar.appendChild(wrapper);
            }
        } 
        else 
        {
            // The item has been dragged from the palette
            
            // Create a new wrapper for the item. We don't know the id yet.
            var wrapper = createWrapper("");
            doAdoptNode(gToolboxDocument, wrapper);
            var panel = document.getElementById("customizetabs").selectedPanel;

            if (draggedItemId != "separator" &&
                draggedItemId != "spring" &&
                draggedItemId != "spacer" &&
                gToolbox.palette.getElementsByAttribute("id", draggedItemId).length == 0)
            {

                if (panel.id == "mailboxestab")
                {
                    var paletteBox = document.getElementById("mailbox-palette");
                    var node = paletteBox.getElementsByAttribute("id", draggedItemId)[0];
                    var foldertype = node.getAttribute("foldertype");
                    var set = draggedItemId.split("/");
                    var name = set[set.length-1];

                    if (foldertype == "moveto-folder")
                    {
                        var newitem = createFolderMovetoButton(node.getAttribute("link"), name);
                        doAdoptNode(gToolboxDocument, newitem);
                        gToolbox.palette.appendChild(newitem);
                    }
                    else if (foldertype == "open-folder")
                    {
                        var newitem = createFolderOpenButton(node.getAttribute("link"), name);
                        doAdoptNode(gToolboxDocument, newitem);
                        gToolbox.palette.appendChild(newitem);
                    }
                }
                else if (panel.id == "recipientstab")
                {
                    var paletteBox = document.getElementById("recipient-palette");
                    var node = paletteBox.getElementsByAttribute("id", draggedItemId)[0];
                    var foldertype = node.getAttribute("foldertype");
                    var set = draggedItemId.split("/");
                    var name = node.getAttribute("label");
                    var email = node.getAttribute("link");
                    var buttontype = node.getAttribute("foldertype");

                    var newitem = createRecipientButton(email, name, buttontype);
                    doAdoptNode(gToolboxDocument, newitem);
                    gToolbox.palette.appendChild(newitem);
                }

            }

            // Ask the toolbar to clone the item's template, place it inside the wrapper, and insert it in the toolbar.
            var newItem = toolbar.insertItem(draggedItemId, gCurrentDragOverItem == toolbar ? null : gCurrentDragOverItem, wrapper);

            // Prepare the item and wrapper to look good on the toolbar.
            cleanupItemForToolbar(newItem, wrapper);
            wrapper.id = "wrapper-"+newItem.id;
            wrapper.flex = newItem.flex;
        }
        
        gCurrentDragOverItem = null;

        repositionDialog();
        gToolboxChanged = true;
    },
    
    _flavourSet: null,
    
    getSupportedFlavours: function ()
    {
        if (!this._flavourSet) 
        {
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
        if (wrapper) 
        {
            // Don't allow static kids (e.g., the menubar) to move.
            if (wrapper.parentNode.firstPermanentChild && wrapper.parentNode.firstPermanentChild.id == wrapper.firstChild.id)
            {
                return;
            }
            if (wrapper.parentNode.lastPermanentChild && wrapper.parentNode.lastPermanentChild.id == wrapper.firstChild.id)
            {
                return;
            }

            // The item was dragged out of the toolbar.
            wrapper.parentNode.removeChild(wrapper);
        }
        
        repositionDialog();
        gToolboxChanged = true;
    },
    
    _flavourSet: null,
    
    getSupportedFlavours: function ()
    {
        if (!this._flavourSet) 
        {
          this._flavourSet = new FlavourSet();
          var documentId = gToolboxDocument.documentElement.id;
          this._flavourSet.appendFlavour("text/toolbarwrapper-id/"+documentId);
        }
        return this._flavourSet;
    }
}

function createMenubarTree(node, curparent)
{
    if (node.hasChildNodes())
    {   
        var datasources = node.getAttribute("datasources");
        if (datasources)
        {
            // Not possible to resolve menu populated via templates and RDF
            return;
        }
        var label = node.getAttribute("label");
        var newparent;
        if (label)
        {
            var treeitem = document.createElement("treeitem");
            var treerow = document.createElement("treerow");
            var treecell = document.createElement("treecell");
            var treechildren = document.createElement("treechildren");
            treecell.setAttribute("label", label);
            treeitem.setAttribute("container", "true");
            treerow.appendChild(treecell);
            treeitem.appendChild(treerow);
            treeitem.appendChild(treechildren);
            curparent.appendChild(treeitem);
            newparent = treechildren;
        }
        else
        {
            newparent = curparent;
        }
        var hidden;
        var toolbaritem;
        var child = node.childNodes;
        for (var j=0; j < child.length; j++)
        {
            if (child[j].nodeType == Node.TEXT_NODE)
            {
                continue;
            }
            hidden = child[j].getAttribute("hidden");
            if (hidden == "true")
            {
                continue;
            }
            toolbaritem = child[j].getAttribute("toolbaritem");
            if (toolbaritem == "not-supported")
            {
                continue;
            }

            label = child[j].getAttribute("label");
            if (toolbaritem)
            {
                var treeitem = document.createElement("treeitem");
                var treerow = document.createElement("treerow");
                var treecell = document.createElement("treecell");
                treecell.setAttribute("label", label);
                treecell.setAttribute("toolbaritem", toolbaritem);
                treerow.appendChild(treecell);
                treeitem.appendChild(treerow);
                newparent.appendChild(treeitem);
            }
            else
            {
                createMenubarTree(child[j], newparent);
            }
        }
    }
}

function onMenuItemSelect()
{
    dump("In onMenuItemSelect\n");
    var elem = gMenuItemTree.view.getItemAtIndex(gMenuItemTree.currentIndex);
    var cell = elem.firstChild.firstChild;
    var label = cell.getAttribute("label");
    var toolbaritem = cell.getAttribute("toolbaritem");
    var descId = "menuItemDescription";

    if (toolbaritem)
    {
        buildPalette('general-palette', toolbaritem, descId);
    }
    else
    {
        notSupported(descId);
    }
}

function onMboxSelect()
{
    dump("In onMboxSelect\n");
    var elem = gMboxTree.view.getItemAtIndex(gMboxTree.currentIndex);
    var cell = elem.firstChild.firstChild;
    var label = cell.getAttribute("label");
    var allowFileOnServer = cell.getAttribute("allowFileOnServer");
    var allowFile = cell.getAttribute("allowFile");
    buildMailboxPalette(elem.id, label, (allowFileOnServer == "false" || allowFile=="false"));
}
 
function onRecipientSelect()
{
    dump("In onRecipientSelect\n");
    var index = GetSelectedCardIndex();
    var card = gAbView.getCardFromRow(index);
    var email = card.primaryEmail;
    var name = card.displayName;
    if (!name)
    {
        name=email;
    }
    buildRecipientsPalette(email, name);
}

function notSupported(descId)
{
    var linetext = document.createTextNode("No toolbar item supported");
    var desc = document.getElementById(descId);
    while (desc.firstChild)
    {
        desc.removeChild(desc.firstChild);
    }
    desc.appendChild(linetext);
}

function setDescText(descId, text)
{
    var linetext = document.createTextNode(text);
    var desc = document.getElementById(descId);
    while (desc.firstChild)
    {
        desc.removeChild(desc.firstChild);
    }
    desc.appendChild(linetext);
}

function goUpdateCommand(unused)
{
    dump("called goUpdateCommand: "+unused+"\n");
}

function ClearCardViewPane()
{
    // Stubb to prevent unresolved function
}

