# -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is mozilla.org code.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) Netscape Communications Corporation.  All
# Rights Reserved.
# 
# Contributor(s): 
#    Doron Rosenberg (doronr@naboonline.com) 
#    Neil Rashbrook (neil@parkwaycc.co.uk)
#

const pageLoaderIface = Components.interfaces.nsIWebPageDescriptor;
const nsISelectionPrivate = Components.interfaces.nsISelectionPrivate;
const nsISelectionController = Components.interfaces.nsISelectionController;
var gBrowser = null;
var gViewSourceBundle = null;
var gPrefs = null;

var gLastLineFound = '';
var gGoToLine = 0;

try {
  var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                              .getService(Components.interfaces.nsIPrefService);
  gPrefs = prefService.getBranch(null);
} catch (ex) {
}

var gSelectionListener = {
  timeout: 0,
  notifySelectionChanged: function(doc, sel, reason)
  {
    // Coalesce notifications within 100ms intervals.
    if (!this.timeout)
      this.timeout = setTimeout(updateStatusBar, 100);
  }
}

function onLoadViewSource() 
{
  viewSource(window.arguments[0]);
  document.commandDispatcher.focusedWindow = content;
}

function getBrowser()
{
  if (!gBrowser)
    gBrowser = document.getElementById("content");
  return gBrowser;
}

function getSelectionController()
{
  return getBrowser().docShell
    .QueryInterface(Components.interfaces.nsIInterfaceRequestor)
    .getInterface(Components.interfaces.nsISelectionDisplay)
    .QueryInterface(nsISelectionController);

}

function getViewSourceBundle()
{
  if (!gViewSourceBundle)
    gViewSourceBundle = document.getElementById("viewSourceBundle");
  return gViewSourceBundle;
}

function viewSource(url)
{
  if (!url)
    return false; // throw Components.results.NS_ERROR_FAILURE;

  getBrowser().addEventListener("unload", onUnloadContent, true);
  getBrowser().addEventListener("load", onLoadContent, true);

  var loadFromURL = true;
  //
  // Parse the 'arguments' supplied with the dialog.
  //    arg[0] - URL string.
  //    arg[1] - Charset value in the form 'charset=xxx'.
  //    arg[2] - Page descriptor used to load content from the cache.
  //    arg[3] - Line number to go to.
  //
  if ("arguments" in window) {
    var arg;
    //
    // Set the charset of the viewsource window...
    //
    if (window.arguments.length >= 2) {
      arg = window.arguments[1];

      try {
        if (typeof(arg) == "string" && arg.indexOf('charset=') != -1) {
          var arrayArgComponents = arg.split('=');
          if (arrayArgComponents) {
            //we should "inherit" the charset menu setting in a new window
            getMarkupDocumentViewer().defaultCharacterSet = arrayArgComponents[1];
          } 
        }
      } catch (ex) {
        // Ignore the failure and keep processing arguments...
      }
    }
    //
    // Get any specified line to jump to.
    //
    if (window.arguments.length >= 4) {
      arg = window.arguments[3];
      gGoToLine = parseInt(arg);
    }
    //
    // Use the page descriptor to load the content from the cache (if
    // available).
    //
    if (window.arguments.length >= 3) {
      arg = window.arguments[2];

      try {
        if (typeof(arg) == "object" && arg != null) {
          var PageLoader = getBrowser().webNavigation.QueryInterface(pageLoaderIface);

          //
          // Load the page using the page descriptor rather than the URL.
          // This allows the content to be fetched from the cache (if
          // possible) rather than the network...
          //
          PageLoader.loadPage(arg, pageLoaderIface.DISPLAY_AS_SOURCE);
          // The content was successfully loaded from the page cookie.
          loadFromURL = false;
        }
      } catch(ex) {
        // Ignore the failure.  The content will be loaded via the URL
        // that was supplied in arg[0].
      }
    }
  }

  if (loadFromURL) {
    //
    // Currently, an exception is thrown if the URL load fails...
    //
    var loadFlags = Components.interfaces.nsIWebNavigation.LOAD_FLAGS_NONE;
    var viewSrcUrl = "view-source:" + url;
    getBrowser().webNavigation.loadURI(viewSrcUrl, loadFlags, null, null, null);
  }

  //check the view_source.wrap_long_lines pref and set the menuitem's checked attribute accordingly
  if (gPrefs) {
    try {
      var wraplonglinesPrefValue = gPrefs.getBoolPref("view_source.wrap_long_lines");

      if (wraplonglinesPrefValue)
        document.getElementById('menu_wrapLongLines').setAttribute("checked", "true");
    } catch (ex) {
    }
    try {
      document.getElementById("menu_highlightSyntax").setAttribute("checked", gPrefs.getBoolPref("view_source.syntax_highlight"));
    } catch (ex) {
    }
  } else {
    document.getElementById("menu_highlightSyntax").setAttribute("hidden", "true");
  }

  window._content.focus();

  return true;
}

function onLoadContent()
{
  //
  // If the view source was opened with a "go to line" argument.
  //
  if (gGoToLine > 0) {
    goToLine(gGoToLine);
    gGoToLine = 0;
  }
  document.getElementById('cmd_goToLine').removeAttribute('disabled');

  // Register a listener so that we can show the caret position on the status bar.
  window._content.getSelection()
   .QueryInterface(nsISelectionPrivate)
   .addSelectionListener(gSelectionListener);
}

function onUnloadContent()
{
  //
  // Disable "go to line" while reloading due to e.g. change of charset
  // or toggling of syntax highlighting.
  //
  document.getElementById('cmd_goToLine').setAttribute('disabled', 'true');
}

function ViewSourceClose()
{
  window.close();
}

function BrowserReload()
{
  // Reload will always reload from cache which is probably not what's wanted
  BrowserReloadSkipCache();
}

function BrowserReloadSkipCache()
{
  const webNavigation = getBrowser().webNavigation;
  webNavigation.reload(webNavigation.LOAD_FLAGS_BYPASS_PROXY | webNavigation.LOAD_FLAGS_BYPASS_CACHE);
}

// Strips the |view-source:| for editPage()
function ViewSourceEditPage()
{
  editPage(window.content.location.href.substring(12), window, false);
}

// Strips the |view-source:| for saveURL()
function ViewSourceSavePage()
{
  saveURL(window.content.location.href.substring(12), null, "SaveLinkTitle");
}

function onEnterPP()
{
  var toolbox = document.getElementById("viewSource-toolbox");
  toolbox.hidden = true;
}

function onExitPP()
{
  var toolbox = document.getElementById("viewSource-toolbox");
  toolbox.hidden = false;
}

function ViewSourceGoToLine()
{
  var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
        .getService(Components.interfaces.nsIPromptService);
  var viewSourceBundle = getViewSourceBundle();

  var input = {value:gLastLineFound};
  for (;;) {
    var ok = promptService.prompt(
        window,
        viewSourceBundle.getString("goToLineTitle"),
        viewSourceBundle.getString("goToLineText"),
        input,
        null,
        {value:0});

    if (!ok) return;

    var line = parseInt(input.value);
 
    if (!(line > 0)) {
      promptService.alert(window,
          viewSourceBundle.getString("invalidInputTitle"),
          viewSourceBundle.getString("invalidInputText"));
  
      continue;
    }

    var found = goToLine(line);

    if (found) {
      break;
    }

    promptService.alert(window,
        viewSourceBundle.getString("outOfRangeTitle"),
        viewSourceBundle.getString("outOfRangeText"));
  }
}

function goToLine(line)
{
  var viewsource = window._content.document.body;

  //
  // The source document is made up of a number of pre elements with
  // id attributes in the format <pre id="line123">, meaning that
  // the first line in the pre element is number 123.
  // Do binary search to find the pre element containing the line.
  //
  var pre;
  for (var lbound = 0, ubound = viewsource.childNodes.length; ; ) {
    var middle = (lbound + ubound) >> 1;
    pre = viewsource.childNodes[middle];

    var firstLine = parseInt(pre.id.substring(4));

    if (lbound == ubound - 1) {
      break;
    }

    if (line >= firstLine) {
      lbound = middle;
    } else {
      ubound = middle;
    }
  }

  var result = {};
  var found = findLocation(pre, line, null, -1, false, result);

  if (!found) {
    return false;
  }

  var selection = window._content.getSelection();
  selection.removeAllRanges();

  // In our case, the range's startOffset is after "\n" on the previous line.
  // Tune the selection at the beginning of the next line and do some tweaking
  // to position the focusNode and the caret at the beginning of the line.

  selection.QueryInterface(nsISelectionPrivate)
    .interlinePosition = true;	

  selection.addRange(result.range);

  if (!selection.isCollapsed) {
    selection.collapseToEnd();

    var offset = result.range.startOffset;
    var node = result.range.startContainer;
    if (offset < node.data.length) {
      // The same text node spans across the "\n", just focus where we were.
      selection.extend(node, offset);
    }
    else {
      // There is another tag just after the "\n", hook there. We need
      // to focus a safe point because there are edgy cases such as
      // <span>...\n</span><span>...</span> vs.
      // <span>...\n<span>...</span></span><span>...</span>
      node = node.nextSibling ? node.nextSibling : node.parentNode.nextSibling;
      selection.extend(node, 0);
    }
  }

  var selCon = getSelectionController();
  selCon.setDisplaySelection(nsISelectionController.SELECTION_ON);
  selCon.setCaretEnabled(true);
  selCon.setCaretVisibilityDuringSelection(true);

  // Scroll the beginning of the line into view.
  selCon.scrollSelectionIntoView(
    nsISelectionController.SELECTION_NORMAL,
    nsISelectionController.SELECTION_FOCUS_REGION,
    true);

  gLastLineFound = line;

  //pch: don't update the status bar for now
  //document.getElementById("statusbar-line-col").label = getViewSourceBundle()
  //    .getFormattedString("statusBarLineCol", [line, 1]);

  return true;
}

function updateStatusBar()
{
  // Reset the coalesce flag.
  gSelectionListener.timeout = 0;

  var statusBarField = document.getElementById("statusbar-line-col");

  var selection = window._content.getSelection();
  if (!selection.focusNode) {
    statusBarField.label = '';
    return;
  }
  if (selection.focusNode.nodeType != Node.TEXT_NODE) {
    return;
  }

  var selCon = getSelectionController();
  selCon.setDisplaySelection(nsISelectionController.SELECTION_ON);
  selCon.setCaretEnabled(true);
  selCon.setCaretVisibilityDuringSelection(true);

  var interlinePosition = selection
      .QueryInterface(nsISelectionPrivate).interlinePosition;

  var result = {};
  findLocation(null, -1, 
      selection.focusNode, selection.focusOffset, interlinePosition, result);

  //pch no status bar for now
  return;
  statusBarField.label = getViewSourceBundle()
      .getFormattedString("statusBarLineCol", [result.line, result.col]);
}

//
// Loops through the text lines in the pre element. The arguments are either
// (pre, line) or (node, offset, interlinePosition). result is an out
// argument. If (pre, line) are specified (and node == null), result.range is
// a range spanning the specified line. If the (node, offset,
// interlinePosition) are specified, result.line and result.col are the line
// and column number of the specified offset in the specified node relative to
// the whole file.
//
function findLocation(pre, line, node, offset, interlinePosition, result)
{
  if (node && !pre) {
    //
    // Look upwards to find the current pre element.
    //
    for (pre = node;
         pre.nodeName != "PRE";
         pre = pre.parentNode);
  }

  //
  // The source document is made up of a number of pre elements with
  // id attributes in the format <pre id="line123">, meaning that
  // the first line in the pre element is number 123.
  //
  var curLine = parseInt(pre.id.substring(4));

  //
  // Walk through each of the text nodes and count newlines.
  //
  var treewalker = window._content.document
      .createTreeWalker(pre, NodeFilter.SHOW_TEXT, null, false);

  //
  // The column number of the first character in the current text node.
  //
  var firstCol = 1;

  var found = false;
  for (var textNode = treewalker.firstChild();
       textNode && !found;
       textNode = treewalker.nextNode()) {

    //
    // \r is not a valid character in the DOM, so we only check for \n.
    //
    var lineArray = textNode.data.split(/\n/);
    var lastLineInNode = curLine + lineArray.length - 1;

    //
    // Check if we can skip the text node without further inspection.
    //
    if (node ? (textNode != node) : (lastLineInNode < line)) {
      if (lineArray.length > 1) {
        firstCol = 1;
      }
      firstCol += lineArray[lineArray.length - 1].length;
      curLine = lastLineInNode;
      continue;
    }

    //
    // curPos is the offset within the current text node of the first
    // character in the current line.
    //
    for (var i = 0, curPos = 0;
         i < lineArray.length;
         curPos += lineArray[i++].length + 1) {

      if (i > 0) {
        curLine++;
      }

      if (node) {
        if (offset >= curPos && offset <= curPos + lineArray[i].length) {
          //
          // If we are right after the \n of a line and interlinePosition is
          // false, the caret looks as if it were at the end of the previous
          // line, so we display that line and column instead.
          //
          if (i > 0 && offset == curPos && !interlinePosition) {
            result.line = curLine - 1;
            var prevPos = curPos - lineArray[i - 1].length;
            result.col = (i == 1 ? firstCol : 1) + offset - prevPos;

          } else {
            result.line = curLine;
            result.col = (i == 0 ? firstCol : 1) + offset - curPos;
          }
          found = true;

          break;
        }

      } else {
        if (curLine == line && !("range" in result)) {
          result.range = document.createRange();
          result.range.setStart(textNode, curPos);

          //
          // This will always be overridden later, except when we look for
          // the very last line in the file (this is the only line that does
          // not end with \n).
          //
          result.range.setEndAfter(pre.lastChild);

        } else if (curLine == line + 1) {
          result.range.setEnd(textNode, curPos - 1);
          found = true;
          break;
        }
      }
    }
  }

  return found;
}

//function to toggle long-line wrapping and set the view_source.wrap_long_lines 
//pref to persist the last state
function wrapLongLines()
{
  var myWrap = window._content.document.body;

  if (myWrap.className == '')
    myWrap.className = 'wrap';
  else myWrap.className = '';

  //since multiple viewsource windows are possible, another window could have 
  //affected the pref, so instead of determining the new pref value via the current
  //pref value, we use myWrap.className  
  if (gPrefs){
    try {
      if (myWrap.className == '') {
        gPrefs.setBoolPref("view_source.wrap_long_lines", false);
      }
      else {
        gPrefs.setBoolPref("view_source.wrap_long_lines", true);
      }
    } catch (ex) {
    }
  }
}

//function to toggle syntax highlighting and set the view_source.syntax_highlight
//pref to persist the last state
function highlightSyntax()
{
  var highlightSyntaxMenu = document.getElementById("menu_highlightSyntax");
  var highlightSyntax = (highlightSyntaxMenu.getAttribute("checked") == "true");
  gPrefs.setBoolPref("view_source.syntax_highlight", highlightSyntax);

  var PageLoader = getBrowser().webNavigation.QueryInterface(pageLoaderIface);
  PageLoader.loadPage(PageLoader.currentDescriptor, pageLoaderIface.DISPLAY_NORMAL);
}

// Fix for bug 136322: this function overrides the function in
// browser.js to call PageLoader.loadPage() instead of BrowserReloadWithFlags()
function BrowserSetForcedCharacterSet(aCharset)
{
  var docCharset = getBrowser().docShell.QueryInterface(
                            Components.interfaces.nsIDocCharset);
  docCharset.charset = aCharset;
  var PageLoader = getBrowser().webNavigation.QueryInterface(pageLoaderIface);
  PageLoader.loadPage(PageLoader.currentDescriptor, pageLoaderIface.DISPLAY_NORMAL);
}

function getMarkupDocumentViewer()
{
  return gBrowser.markupDocumentViewer;
}
