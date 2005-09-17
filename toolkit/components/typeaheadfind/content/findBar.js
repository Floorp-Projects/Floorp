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
#     Masayuki Nakano <masayuki@d-toybox.com>
#     Ben Basson <contact@cusser.net>
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

const CHAR_CODE_SPACE = " ".charCodeAt(0);
const CHAR_CODE_SLASH = "/".charCodeAt(0);
const CHAR_CODE_APOSTROPHE = "'".charCodeAt(0);

// Global find variables
var gFindMode = FIND_NORMAL;
var gFoundLink = null;
var gTmpOutline = null;
var gTmpOutlineOffset = "0";
var gDrawOutline = false;
var gQuickFindTimeout = null;
var gQuickFindTimeoutLength = 0;
var gHighlightTimeout = null;
var gUseTypeAheadFind = false;
var gWrappedToTopStr = "";
var gWrappedToBottomStr = "";
var gNotFoundStr = "";
var gFlashFindBar = 0;
var gFlashFindBarCount = 6;
var gFlashFindBarTimeout = null;
var gLastHighlightString = "";
var gTypeAheadLinksOnly = false;
var gIsIMEComposing = false;

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
  getBrowser().addEventListener("mouseup", onBrowserMouseUp, false);
  
  var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                              .getService(Components.interfaces.nsIPrefBranch);

  var pbi = prefService.QueryInterface(Components.interfaces.nsIPrefBranch2);

  gQuickFindTimeoutLength = prefService.getIntPref("accessibility.typeaheadfind.timeout");
  gFlashFindBar = prefService.getIntPref("accessibility.typeaheadfind.flashBar");

  pbi.addObserver(gTypeAheadFind.useTAFPref, gTypeAheadFind, false);
  pbi.addObserver(gTypeAheadFind.searchLinksPref, gTypeAheadFind, false);
  gUseTypeAheadFind = prefService.getBoolPref("accessibility.typeaheadfind");
  gTypeAheadLinksOnly = prefService.getBoolPref("accessibility.typeaheadfind.linksonly");

  var fastFind = getBrowser().fastFind;
  fastFind.focusLinks = true;
}

function uninitFindBar()
{
   var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                               .getService(Components.interfaces.nsIPrefBranch);

   var pbi = prefService.QueryInterface(Components.interfaces.nsIPrefBranch2);
   pbi.removeObserver(gTypeAheadFind.useTAFPref, gTypeAheadFind);
   pbi.removeObserver(gTypeAheadFind.searchLinksPref, gTypeAheadFind);

   getBrowser().removeEventListener("keypress", onBrowserKeyPress, false);
   getBrowser().removeEventListener("mouseup", onBrowserMouseUp, false);
}

function toggleHighlight(aHighlight)
{
  var word = document.getElementById("find-field").value;
  if (aHighlight) {
    highlightDoc('yellow', 'black', word);
  } else {
    highlightDoc(null, null, null);
    gLastHighlightString = null;
  }
}

function highlightDoc(highBackColor, highTextColor, word, win)
{
  if (!win)
    win = window._content; 

  for (var i = 0; win.frames && i < win.frames.length; i++) {
    highlightDoc(highBackColor, highTextColor, word, win.frames[i]);
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

  if (!highBackColor) {
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
        parent.normalize();
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
  baseNode.setAttribute("style", "background-color: " + highBackColor + "; color: " + highTextColor + ";"
                                   + "display: inline; font-size: inherit; padding: 0;");
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
  try {
    var display = ds.QueryInterface(Components.interfaces.nsIInterfaceRequestor).getInterface(Components.interfaces.nsISelectionDisplay);
  }
  catch (e) { return null; }
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
  try {
    var ds = getBrowser().docShell;
  } catch(e) {
    // If we throw here, the browser we were in is already destroyed.
    // See bug 273200.
    return;
  }
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
  // ensure the dom is ready...
  setTimeout(delayedCloseFindBar, 0);
}

function fireKeypressEvent(target, evt)
{
  if (!target)
    return;
  var event = document.createEvent("KeyEvents");
  event.initKeyEvent(evt.type, evt.canBubble, evt.cancelable,
                     evt.view, evt.ctrlKey, evt.altKey, evt.shiftKey,
                     evt.metaKey, evt.keyCode, evt.charCode);
  target.dispatchEvent(event);
}

function setFoundLink(foundLink)
{
  if (gFoundLink == foundLink)
    return;

  if (gFoundLink && gDrawOutline) {
    // restore original outline
    gFoundLink.style.outline = gTmpOutline;
    gFoundLink.style.outlineOffset = gTmpOutlineOffset;
  }
  gDrawOutline = (foundLink && gFindMode != FIND_NORMAL);
  if (gDrawOutline) {
    // backup original outline
    gTmpOutline = foundLink.style.outline;
    gTmpOutlineOffset = foundLink.style.outlineOffset;
    // draw pseudo focus rect
    // XXX Should we change the following style for FAYT pseudo focus?
    // XXX Shouldn't we change default design if outline is visible already?
    foundLink.style.outline = "1px dotted invert";
    foundLink.style.outlineOffset = "0";
  }

  gFoundLink = foundLink;
}

function finishFAYT(aKeypressEvent)
{
  if (!gFoundLink)
    return false;

  gFoundLink.focus(); // In this function, gFoundLink is set null.

  if (aKeypressEvent)
    aKeypressEvent.preventDefault();

  closeFindBar();
  return true;
}

function delayedCloseFindBar()
{
  var findField = document.getElementById("find-field");
  var findToolbar = document.getElementById("FindToolbar");
  var ww = Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
                     .getService(Components.interfaces.nsIWindowWatcher);

  if (window == ww.activeWindow) { 
    var focusedElement = document.commandDispatcher.focusedElement;
    if (focusedElement && (focusedElement.parentNode == findToolbar ||
                           focusedElement.parentNode.parentNode == findField)) {
      if (gFoundLink)
        gFoundLink.focus();
      else
        window.content.focus();
    }
  }

  findToolbar.hidden = true;
  setFindMode(FIND_NORMAL);
  setFoundLink(null);
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
    if (elt instanceof HTMLInputElement) {
      // block FAYT when an <input> textfield element is focused
      var inputType = elt.type;
      switch (inputType) {
        case "text":
        case "password":
        case "file":
          return false;
      }
    }
    else if (elt instanceof HTMLTextAreaElement ||
             elt instanceof HTMLSelectElement ||
             elt instanceof HTMLIsIndexElement)
      return false;
  }

  // disable FAYT in about:config and about:blank to prevent FAYT opening
  // unexpectedly - to fix bugs 264562, 267150, 269712
  var url = getBrowser().currentURI.spec;
  if (url == "about:blank" || url == "about:config")
    return false;

  var win = document.commandDispatcher.focusedWindow;
  if (win && win.document.designMode == "on")
    return false;
  
  return true;
}

function onFindBarBlur()
{
  changeSelectionColor(false);
  setFoundLink(null);
}

function onBrowserMouseUp(evt)
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
    if (!evt.charCode)
      return;
    selectFindBar();
    focusFindBar();
    fireKeypressEvent(findField.inputField, evt);
    evt.preventDefault();
    return;
  }

  if (evt.charCode == CHAR_CODE_APOSTROPHE || evt.charCode == CHAR_CODE_SLASH ||
      (gUseTypeAheadFind && evt.charCode && evt.charCode != CHAR_CODE_SPACE)) {
    var findMode = (evt.charCode == CHAR_CODE_APOSTROPHE ||
                    (gTypeAheadLinksOnly && evt.charCode != CHAR_CODE_SLASH))
                   ? FIND_LINKS : FIND_TYPEAHEAD;
    setFindMode(findMode);
    if (openFindBar()) {
      setFindCloseTimeout();
      selectFindBar();
      focusFindBar();
      findField.value = "";
      if (gUseTypeAheadFind &&
          evt.charCode != CHAR_CODE_APOSTROPHE &&
          evt.charCode != CHAR_CODE_SLASH)
        fireKeypressEvent(findField.inputField, evt);
      evt.preventDefault();
    }
    else {
      selectFindBar();
      focusFindBar();
      if (gFindMode != FIND_NORMAL) {
        findField.value = "";
        if (evt.charCode != CHAR_CODE_APOSTROPHE &&
            evt.charCode != CHAR_CODE_SLASH)
          fireKeypressEvent(findField.inputField, evt);
        evt.preventDefault();
      }
    }
  }
}

function onFindBarKeyPress(evt)
{
  if (evt.keyCode == KeyEvent.DOM_VK_RETURN) {
    if (gFindMode == FIND_NORMAL) {
      var findString = document.getElementById("find-field");
      if (!findString.value)
        return;

#ifdef XP_MACOSX
      if (evt.metaKey) {
#else
      if (evt.ctrlKey) {
#endif
        document.getElementById("highlight").click();
        return;
      }

      if (evt.shiftKey)
        findPrevious();
      else
        findNext();
    }
    else {
      if (gFoundLink) {
        var tmpLink = gFoundLink;
        if (finishFAYT(evt))
          fireKeypressEvent(tmpLink, evt);
      }
    }
  }
  else if (evt.keyCode == KeyEvent.DOM_VK_TAB) {
    var shouldHandle = !evt.altKey && !evt.ctrlKey && !evt.metaKey;
    if (shouldHandle && gFindMode != FIND_NORMAL &&
        gFoundLink && finishFAYT(evt)) {
      if (evt.shiftKey)
        document.commandDispatcher.rewindFocus();
      else
        document.commandDispatcher.advanceFocus();
    }
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

function updateFoundLink(res)
{
  var val = document.getElementById("find-field").value;
  if (res == Components.interfaces.nsITypeAheadFind.FIND_NOTFOUND || !val)
    setFoundLink(null);
  else
    setFoundLink(getBrowser().fastFind.foundLink);
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
  updateFoundLink(res);
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
    return;
  }
 
  findToolbar.setAttribute("flash", (gFlashFindBarCount % 2 == 0) ? "false" : "true");
}

function onFindCmd()
{
  setFindMode(FIND_NORMAL);
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
  if (!findString) {
    onFindCmd();
    return;
  }

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
  if (!findString) {
    onFindCmd();
    return;
  }
 
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
  updateFoundLink(res);
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
  updateFoundLink(res);
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

  // Don't close the find toolbar while IME is composing.
  if (gIsIMEComposing) {
    gQuickFindTimeout = null;
    return;
  }

  gQuickFindTimeout =
    setTimeout(function() { if (gFindMode != FIND_NORMAL) closeFindBar(); },
               gQuickFindTimeoutLength);
}

function onFindBarCompositionStart(evt)
{
  gIsIMEComposing = true;
  // Don't close the find toolbar while IME is composing.
  if (gQuickFindTimeout) {
    clearTimeout(gQuickFindTimeout);
    gQuickFindTimeout = null;
  }
}

function onFindBarCompositionEnd(evt)
{
  gIsIMEComposing = false;
  if (gFindMode != FIND_NORMAL && isFindBarVisible())
    setFindCloseTimeout();
}

function setFindMode(mode)
{
  if (mode == gFindMode)
    return;

  gFindMode = mode;
}
