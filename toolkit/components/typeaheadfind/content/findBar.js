# -*- Mode: HTML -*-
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
# The Original Code is mozilla.org viewsource frontend.
# 
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 2003
# the Initial Developer.  All Rights Reserved.
# 
# Contributor(s):
#     Blake Ross     <blake@cs.stanford.edu> (Original Author)
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the LGPL or the GPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
#***** END LICENSE BLOCK ***** -->

const FIND_NORMAL = 0;
const FIND_TYPEAHEAD = 1;
const FIND_LINKS = 2;

// Global find variables
var gFindMode = FIND_NORMAL;
var gQuickFindTimeout = null;
var gQuickFindTimeoutLength = 0;
var gHighlightTimeout = null;
var gUseTypeAheadFind = false;
var gWrappedToTopStr = "";
var gWrappedToBottomStr = "";
var gNotFoundStr = "";
var gTypeAheadFindBuffer = "";
var gIsBack = true;
var gBackProtectBuffer = 3;
var gFlashFindBar = 0;
var gFlashFindBarCount = 6;
var gFlashFindBarTimeout = null;
var gLastHighlightString = "";
var gTypeAheadLinksOnly = false;

// DOMRange used during highlighting
var searchRange;
var startPt;
var endPt;

var gTypeAheadFind = {
  useTAFPref: "accessibility.typeaheadfind",
  searchLinksPref: "accessibility.typeaheadfind.linksonly",
  
  observe: function(aSubject, aTopic, aPrefName)
  {
    if (aTopic != "nsPref:changed" || (aPrefName != this.useTAFPref && aPrefName != this.searchLinksPref))
      return;

    var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                                .getService(Components.interfaces.nsIPrefBranch);
      
    gUseTypeAheadFind = prefService.getBoolPref("accessibility.typeaheadfind");
    gTypeAheadLinksOnly = gPrefService.getBoolPref("accessibility.typeaheadfind.linksonly");
  }
};

function initFindBar()
{
  getBrowser().addEventListener("keypress", onBrowserKeyPress, false);
  getBrowser().addEventListener("mousedown", onBrowserMouseDown, false);
  
  var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                              .getService(Components.interfaces.nsIPrefBranch);

  var pbi = prefService.QueryInterface(Components.interfaces.nsIPrefBranchInternal);

  gQuickFindTimeoutLength = prefService.getIntPref("accessibility.typeaheadfind.timeout");
  gFlashFindBar = prefService.getIntPref("accessibility.typeaheadfind.flashBar");

  pbi.addObserver(gTypeAheadFind.useTAFPref, gTypeAheadFind, false);
  pbi.addObserver(gTypeAheadFind.searchLinksPref, gTypeAheadFind, false);
  gUseTypeAheadFind = prefService.getBoolPref("accessibility.typeaheadfind");
  gTypeAheadLinksOnly = prefService.getBoolPref("accessibility.typeaheadfind.linksonly");
}

function uninitFindBar()
{
   var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                               .getService(Components.interfaces.nsIPrefBranch);

   var pbi = prefService.QueryInterface(Components.interfaces.nsIPrefBranchInternal);
   pbi.removeObserver(gTypeAheadFind.useTAFPref, gTypeAheadFind);
   pbi.removeObserver(gTypeAheadFind.searchLinksPref, gTypeAheadFind);

   getBrowser().removeEventListener("keypress", onBrowserKeyPress, false);
   getBrowser().removeEventListener("mousedown", onBrowserMouseDown, false);
}

function toggleHighlight(aHighlight)
{
  var word = document.getElementById("find-field").value;
  if (aHighlight) {
    highlightDoc('yellow', word);
  } else {
    highlightDoc(null, null);
    gLastHighlightString = null;
  }
}

function highlightDoc(color, word, win)
{
  if (!win)
    win = window._content; 

  for (var i = 0; win.frames && i < win.frames.length; i++) {
    highlightDoc(color, word, win.frames[i]);
  }

  var doc = win.document;
  if (!document)
    return;

  if (!("body" in doc))
    return;

  var body = doc.body;
  
  var count = body.childNodes.length;
  searchRange = doc.createRange();
  startPt = doc.createRange();
  endPt = doc.createRange();

  searchRange.setStart(body, 0);
  searchRange.setEnd(body, count);

  startPt.setStart(body, 0);
  startPt.setEnd(body, 0);
  endPt.setStart(body, count);
  endPt.setEnd(body, count);

  if (!color) {
    // Remove highlighting.  We use the find API again rather than
    // searching for our span elements by id so that we gain access to the
    // anonymous content that nsIFind searches.

    if (!gLastHighlightString)
      return;

    var retRange = null;
    var finder = Components.classes["@mozilla.org/embedcomp/rangefind;1"].createInstance()
                         .QueryInterface(Components.interfaces.nsIFind);

    while ((retRange = finder.Find(gLastHighlightString, searchRange,startPt, endPt))) {
      var startContainer = retRange.startContainer;
      var elem = null;
      try {
        elem = startContainer.parentNode;
      }
      catch (e) { }

      if (elem && elem.getAttribute("id") == "__firefox-findbar-search-id") {
        var child = null;
        var docfrag = doc.createDocumentFragment();
        var next = elem.nextSibling;
        var parent = elem.parentNode;

        while ((child = elem.firstChild)) {
          docfrag.appendChild(child);
        }

        startPt = doc.createRange();
        startPt.setStartAfter(elem);

        parent.removeChild(elem);
        parent.insertBefore(docfrag, next);
      }
      else {
        // Somehow we didn't highlight this instance; just skip it.
        startPt = doc.createRange();
        startPt.setStart(retRange.endContainer, retRange.endOffset);
      }

      startPt.collapse(true);
    }
    return;
  }

  var baseNode = doc.createElement("span");
  baseNode.setAttribute("style", "background-color: " + color + ";");
  baseNode.setAttribute("id", "__firefox-findbar-search-id");

  highlightText(word, baseNode);
}

function highlightText(word, baseNode)
{
  var retRange = null;
  var finder = Components.classes["@mozilla.org/embedcomp/rangefind;1"].createInstance()
                         .QueryInterface(Components.interfaces.nsIFind);

  finder.caseSensitive = document.getElementById("find-case-sensitive").checked;

  while((retRange = finder.Find(word, searchRange, startPt, endPt))) {
    // Highlight
    var nodeSurround = baseNode.cloneNode(true);
    var node = highlight(retRange, nodeSurround);
    startPt = node.ownerDocument.createRange();
    startPt.setStart(node, node.childNodes.length);
    startPt.setEnd(node, node.childNodes.length);
  }
  
  gLastHighlightString = word;
}

function highlight(range, node)
{
  var startContainer = range.startContainer;
  var startOffset = range.startOffset;
  var endOffset = range.endOffset;
  var docfrag = range.extractContents();
  var before = startContainer.splitText(startOffset);
  var parent = before.parentNode;
  node.appendChild(docfrag);
  parent.insertBefore(node, before);
  return node;
}

function getSelectionControllerForFindToolbar(ds)
{
  var display = ds.QueryInterface(Components.interfaces.nsIInterfaceRequestor).getInterface(Components.interfaces.nsISelectionDisplay);
  if (!display)
    return null;
  return display.QueryInterface(Components.interfaces.nsISelectionController);
}

function toggleCaseSensitivity(aCaseSensitive)
{
  var fastFind = getBrowser().fastFind;
  fastFind.caseSensitive = aCaseSensitive;
  
  if (gFindMode != FIND_NORMAL)
    setFindCloseTimeout();
}
  
function changeSelectionColor(aAttention)
{
  var ds = getBrowser().docShell;
  var dsEnum = ds.getDocShellEnumerator(Components.interfaces.nsIDocShellTreeItem.typeContent,
                                        Components.interfaces.nsIDocShell.ENUMERATE_FORWARDS);
  while (dsEnum.hasMoreElements()) {
    ds = dsEnum.getNext().QueryInterface(Components.interfaces.nsIDocShell);
    var controller = getSelectionControllerForFindToolbar(ds);
    if (!controller)
      continue;
    const selCon = Components.interfaces.nsISelectionController;
    controller.setDisplaySelection(aAttention? selCon.SELECTION_ATTENTION : selCon.SELECTION_ON);
  }
}

function openFindBar()
{
  if (!gNotFoundStr || !gWrappedToTopStr || !gWrappedToBottomStr) {
    var bundle = document.getElementById("bundle_findBar");
    gNotFoundStr = bundle.getString("NotFound");
    gWrappedToTopStr = bundle.getString("WrappedToTop");
    gWrappedToBottomStr = bundle.getString("WrappedToBottom");
  }

  var findToolbar = document.getElementById("FindToolbar");
  if (findToolbar.hidden) {
    findToolbar.hidden = false;
  
    var statusIcon = document.getElementById("find-status-icon");
    var statusText = document.getElementById("find-status");
    var findField = document.getElementById("find-field");
    findField.removeAttribute("status");
    statusIcon.removeAttribute("status");
    statusText.value = "";

    return true;
  }
  return false;
}

function focusFindBar()
{
  var findField = document.getElementById("find-field");
  findField.focus();    
}

function selectFindBar()
{
  var findField = document.getElementById("find-field");
  findField.select();    
}

function closeFindBar()
{
  var findField = document.getElementById("find-field");
  var ww = Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
                     .getService(Components.interfaces.nsIWindowWatcher);
  if (window == ww.activeWindow && document.commandDispatcher.focusedElement &&
      document.commandDispatcher.focusedElement.parentNode.parentNode == findField) {
    _content.focus();
  }

  var findToolbar = document.getElementById("FindToolbar");
  findToolbar.hidden = true;
  gTypeAheadFindBuffer = "";
  changeSelectionColor(false);
  if (gQuickFindTimeout) {
    clearTimeout(gQuickFindTimeout);
    gQuickFindTimeout = null;    
  } 
}

function shouldFastFind(evt)
{
  if (evt.ctrlKey || evt.altKey || evt.metaKey || evt.getPreventDefault())
    return false;
    
  var elt = document.commandDispatcher.focusedElement;
  if (elt) {
    var ln = elt.localName.toLowerCase();
    if (ln == "input" || ln == "textarea" || ln == "select" || ln == "button" || ln == "isindex")
      return false;
  }
  
  var win = document.commandDispatcher.focusedWindow;
  if (win && win.document.designMode == "on")
    return false;
  
  return true;
}

function onFindBarFocus()
{
  toggleLinkFocus(false);
}

function onFindBarBlur()
{
  toggleLinkFocus(true);
  changeSelectionColor(false);
}

function onBrowserMouseDown(evt)
{
  var findToolbar = document.getElementById("FindToolbar");
  if (!findToolbar.hidden && gFindMode != FIND_NORMAL)
    closeFindBar();
}

function onBrowserKeyPress(evt)
{  
  // Check focused elt
  if (!shouldFastFind(evt))
    return;
  
  var findField = document.getElementById("find-field");
  if (gFindMode != FIND_NORMAL && gQuickFindTimeout) {    
    if (evt.keyCode == 8) { // Backspace
      if (findField.value) {
        findField.value = findField.value.substr(0, findField.value.length - 1);
        gIsBack = true;   
        gBackProtectBuffer = 3;
      }
      else if (gBackProtectBuffer > 0) {
        gBackProtectBuffer--;
      }
      
      if (gIsBack || gBackProtectBuffer > 0)
        evt.preventDefault();
        
      find(findField.value);
    }
    else if (evt.keyCode == 27) { // Escape
      closeFindBar();
      evt.preventDefault();
    }
    else if (evt.charCode) {
      if (evt.charCode == 32) // Space
        evt.preventDefault();
        
      findField.value += String.fromCharCode(evt.charCode);
      find(findField.value);
    }
    return;
  }
  
  if (evt.charCode == 39 /* ' */ || evt.charCode == 47 /* / */ || (gUseTypeAheadFind && evt.charCode && evt.charCode != 32)) {   
    gFindMode = (evt.charCode == 39 || (gTypeAheadLinksOnly && evt.charCode != 47)) ? FIND_LINKS : FIND_TYPEAHEAD;
    toggleLinkFocus(true);
    if (openFindBar()) {      
      setFindCloseTimeout();      
      if (gUseTypeAheadFind && evt.charCode != 39 && evt.charCode != 47) {
        gTypeAheadFindBuffer += String.fromCharCode(evt.charCode);        
        findField.value = gTypeAheadFindBuffer;
        find(findField.value);
      }
      else {
        findField.value = "";
      }
    }
    else {
      if (gFindMode == FIND_NORMAL) {
        selectFindBar();      
        focusFindBar();
      }
      else {
        findField.value = String.fromCharCode(evt.charCode);
        find(findField.value);
      }
    }        
  }
}

function toggleLinkFocus(aFocusLinks)
{
  var fastFind = getBrowser().fastFind;
  fastFind.focusLinks = aFocusLinks;
}

function onBrowserKeyUp(evt)
{
  if (evt.keyCode == 8)
    gIsBack = false;
}

function onFindBarKeyPress(evt)
{
  if (evt.keyCode == KeyEvent.DOM_VK_RETURN) {
    var findString = document.getElementById("find-field");
    if (!findString.value)
      return;
      
    if (evt.ctrlKey) {
      document.getElementById("highlight").click();
      return;
    }
    
    if (evt.shiftKey)
      findPrevious();
    else
      findNext();
  }
  else if (evt.keyCode == KeyEvent.DOM_VK_ESCAPE) {
    closeFindBar();
    evt.preventDefault();
  } 
  else if (evt.keyCode == KeyEvent.DOM_VK_PAGE_UP) {
    window.top._content.scrollByPages(-1);
    evt.preventDefault();
  }
  else if (evt.keyCode == KeyEvent.DOM_VK_PAGE_DOWN) {
    window.top._content.scrollByPages(1);
    evt.preventDefault();
  }
  else if (evt.keyCode == KeyEvent.DOM_VK_UP) {
    window.top._content.scrollByLines(-1);
    evt.preventDefault();
  }
  else if (evt.keyCode == KeyEvent.DOM_VK_DOWN) {
    window.top._content.scrollByLines(1);
    evt.preventDefault();
  }

} 

function enableFindButtons(aEnable)
{
  var findNext = document.getElementById("find-next");
  var findPrev = document.getElementById("find-previous");  
  var highlight = document.getElementById("highlight");
  findNext.disabled = findPrev.disabled = highlight.disabled = !aEnable;  
}

function find(val)
{
  if (!val)
    val = document.getElementById("find-field").value;
    
  enableFindButtons(val);
 
  var highlightBtn = document.getElementById("highlight");
  if (highlightBtn.checked)
    setHighlightTimeout();
        
  changeSelectionColor(true);
  var fastFind = getBrowser().fastFind;  
  var res = fastFind.find(val, gFindMode == FIND_LINKS);
  updateStatus(res, true);
  
  if (gFindMode != FIND_NORMAL)
    setFindCloseTimeout();
}

function flashFindBar()
{
  var findToolbar = document.getElementById("FindToolbar");
  if (gFlashFindBarCount-- == 0) {
    clearInterval(gFlashFindBarTimeout);
    findToolbar.removeAttribute("flash");
    gFlashFindBarCount = 6;
    return true;
  }
 
  findToolbar.setAttribute("flash", (gFlashFindBarCount % 2 == 0) ? "false" : "true");
}

function onFindCmd()
{
  gFindMode = FIND_NORMAL;
  openFindBar();
  if (gFlashFindBar) {
    gFlashFindBarTimeout = setInterval(flashFindBar, 500);
    var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                                .getService(Components.interfaces.nsIPrefBranch);

    prefService.setIntPref("accessibility.typeaheadfind.flashBar", --gFlashFindBar);
  }
  selectFindBar();
  focusFindBar();
}

function onFindAgainCmd()
{
  var findString = getBrowser().findString;
  if (!findString)
    return onFindCmd();

  var res = findNext();
  if (res == Components.interfaces.nsITypeAheadFind.FIND_NOTFOUND) {
    if (openFindBar()) {
      focusFindBar();
      selectFindBar();
      if (gFindMode != FIND_NORMAL)
        setFindCloseTimeout();
      
      updateStatus(res, true);
    }
  }
}

function onFindPreviousCmd()
{
  var findString = getBrowser().findString;
  if (!findString)
    return onFindCmd();
 
  var res = findPrevious();
  if (res == Components.interfaces.nsITypeAheadFind.FIND_NOTFOUND) {
    if (openFindBar()) {
      focusFindBar();
      selectFindBar();
      if (gFindMode != FIND_NORMAL)
        setFindCloseTimeout();
      
      updateStatus(res, false);
    }
  }
}

function setHighlightTimeout()
{
  if (gHighlightTimeout)
    clearTimeout(gHighlightTimeout);
  gHighlightTimeout = setTimeout(function() { toggleHighlight(false); toggleHighlight(true); }, 500);  
}

function isFindBarVisible()
{
  var findBar = document.getElementById("FindToolbar");
  return !findBar.hidden;
}

function findNext()
{
  changeSelectionColor(true);
  var fastFind = getBrowser().fastFind; 
  var res = fastFind.findNext();  
  updateStatus(res, true);
    
  if (gFindMode != FIND_NORMAL && isFindBarVisible())
    setFindCloseTimeout();
  
  return res;
}

function findPrevious()
{
  changeSelectionColor(true);
  var fastFind = getBrowser().fastFind;
  var res = fastFind.findPrevious();
  updateStatus(res, false);
  
  if (gFindMode != FIND_NORMAL && isFindBarVisible())
    setFindCloseTimeout();
  
  return res;
}

function updateStatus(res, findNext)
{
  var findBar = document.getElementById("FindToolbar");
  var field = document.getElementById("find-field");
  var statusIcon = document.getElementById("find-status-icon");
  var statusText = document.getElementById("find-status");
  switch(res) {
    case Components.interfaces.nsITypeAheadFind.FIND_WRAPPED:
      statusIcon.setAttribute("status", "wrapped");      
      statusText.value = findNext ? gWrappedToTopStr : gWrappedToBottomStr;
      break;
    case Components.interfaces.nsITypeAheadFind.FIND_NOTFOUND:
      statusIcon.setAttribute("status", "notfound");
      statusText.value = gNotFoundStr;
      field.setAttribute("status", "notfound");      
      break;
    case Components.interfaces.nsITypeAheadFind.FIND_FOUND:
    default:
      statusIcon.removeAttribute("status");      
      statusText.value = "";
      field.removeAttribute("status");
      break;
  }
}

function setFindCloseTimeout()
{
  if (gQuickFindTimeout)
    clearTimeout(gQuickFindTimeout);
  gQuickFindTimeout = setTimeout(function() { if (gFindMode != FIND_NORMAL) closeFindBar(); }, gQuickFindTimeoutLength);
}
