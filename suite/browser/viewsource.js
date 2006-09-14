/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
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
 * Copyright (C) Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *    Doron Rosenberg (doronr@naboonline.com) 
 *    Neil Rashbrook (neil@parkwaycc.co.uk)
 */

const pageLoaderIface = Components.interfaces.nsIWebPageDescriptor;
var gBrowser = null;
var gPrefs = null;

var gLastLineFound = '';
var gGoToLine = 0;

try {
  var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                              .getService(Components.interfaces.nsIPrefService);
  gPrefs = prefService.getBranch(null);
} catch (ex) {
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
          PageLoader.LoadPage(arg, pageLoaderIface.DISPLAY_AS_SOURCE);
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

// Strips the |view-source:| for editPage()
function ViewSourceEditPage()
{
  var url = window._content.location.href;
  url = url.substring(12,url.length);
  editPage(url,window, false);
}

// Strips the |view-source:| for saveURL()
function ViewSourceSavePage()
{
  var url = window._content.document.location.href;
  url = url.substring(12,url.length);

  saveURL(url, null, "SaveLinkTitle");
}

function ViewSourceGoToLine()
{
  var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
        .getService(Components.interfaces.nsIPromptService);
  var viewSourceBundle = document.getElementById('viewSourceBundle');

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
  var pre, curLine;
  for (var lbound = 0, ubound = viewsource.childNodes.length; ; ) {
    var middle = (lbound + ubound) >> 1;
    pre = viewsource.childNodes[middle];

    curLine = parseInt(pre.id.substring(4));

    if (lbound == ubound - 1) {
      break;
    }

    if (line >= curLine) {
      lbound = middle;
    } else {
      ubound = middle;
    }
  }

  var range = null;

  //
  // Walk through each of the text nodes and count newlines.
  //
  var treewalker = document.createTreeWalker(pre, NodeFilter.SHOW_TEXT, null, false);

  for (var textNode = treewalker.firstChild();
       textNode && curLine <= line + 1;
       textNode = treewalker.nextNode()) {    

    //
    // \r is not a valid character in the DOM, so we only check for \n.
    //
    var lineArray = textNode.data.split(/\n/);
    var lastLineInNode = curLine + lineArray.length - 1;
    if (lastLineInNode < line) {
      curLine = lastLineInNode;
      continue;
    }

    for (var i = 0, curPos = 0; 
         i < lineArray.length;
         curPos += lineArray[i++].length + 1) {

      if (i > 0) {
        curLine++;
      }

      if (curLine == line && !range) {
        range = document.createRange();
        range.setStart(textNode, curPos);

        //
        // This will always be overridden later, except when we look for
        // the very last line in the file (this is the only line that does 
        // not end with \n).
        //
        range.setEndAfter(pre.lastChild);

      } else if (curLine == line + 1) {
        range.setEnd(textNode, curPos);
        curLine++;
        break;
      }
    }
  }

  if (!range) {
    return false;
  }

  var selection = window._content.getSelection();
  selection.removeAllRanges();

  var selCon = getBrowser().docShell
    .QueryInterface(Components.interfaces.nsIInterfaceRequestor)
    .getInterface(Components.interfaces.nsISelectionDisplay)
    .QueryInterface(Components.interfaces.nsISelectionController);

  selCon.setDisplaySelection(
    Components.interfaces.nsISelectionController.SELECTION_ON);

  selCon.setCaretEnabled(true);

  // In our case, the range's startOffset is after "\n" on the previous line.
  // Set "hintright" to tune the selection at the beginning of the next line.
  selection.QueryInterface(Components.interfaces.nsISelectionPrivate)
    .interlinePosition = true;	

  selection.addRange(range);

  // If it is a blank line, collapse to make the caret show up.
  // (work-around to bug 156175)
  if (range.endContainer == range.startContainer &&
      range.endOffset - range.startOffset == 1) {
    // note: by construction, there is just a "\n" in-bewteen
    selection.collapseToStart();
  }

  // Scroll the beginning of the line into view.
  selCon.scrollSelectionIntoView(
    Components.interfaces.nsISelectionController.SELECTION_NORMAL,
    Components.interfaces.nsISelectionController.SELECTION_ANCHOR_REGION,
    true);

  gLastLineFound = line;

  return true;
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
  PageLoader.LoadPage(PageLoader.currentDescriptor, pageLoaderIface.DISPLAY_NORMAL);
}

// Fix for bug 136322: this function overrides the function in
// browser.js to call PageLoader.LoadPage() instead of BrowserReloadWithFlags()
function BrowserSetForcedCharacterSet(aCharset)
{
  var docCharset = getBrowser().docShell.QueryInterface(
                            Components.interfaces.nsIDocCharset);
  docCharset.charset = aCharset;
  var PageLoader = getBrowser().webNavigation.QueryInterface(pageLoaderIface);
  PageLoader.LoadPage(PageLoader.currentDescriptor, pageLoaderIface.DISPLAY_NORMAL);
}
