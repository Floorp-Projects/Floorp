// -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/Services.jsm");

var gDebug = 0;
var gLineCount = 0;
var gStartTargetLine = 0;
var gEndTargetLine = 0;
var gTargetNode = null;

var gEntityConverter = null;
var gWrapLongLines = false;
const gViewSourceCSS = 'resource://gre-resources/viewsource.css';
const NS_XHTML = 'http://www.w3.org/1999/xhtml';

// These are markers used to delimit the selection during processing. They
// are removed from the final rendering, but we pick space-like characters for
// safety (and futhermore, these are known to be mapped to a 0-length string
// in transliterate.properties). It is okay to set start=end, we use findNext()
// U+200B ZERO WIDTH SPACE
const MARK_SELECTION_START = '\u200B\u200B\u200B\u200B\u200B';
const MARK_SELECTION_END = '\u200B\u200B\u200B\u200B\u200B';

function onLoadViewPartialSource()
{
  // check the view_source.wrap_long_lines pref
  // and set the menuitem's checked attribute accordingly
  gWrapLongLines = Services.prefs.getBoolPref("view_source.wrap_long_lines");
  document.getElementById("menu_wrapLongLines").setAttribute("checked", gWrapLongLines);
  document.getElementById("menu_highlightSyntax")
          .setAttribute("checked",
                        Services.prefs.getBoolPref("view_source.syntax_highlight"));

  if (window.arguments[3] == 'selection')
    viewPartialSourceForSelection(window.arguments[2]);
  else
    viewPartialSourceForFragment(window.arguments[2], window.arguments[3]);

  gBrowser.droppedLinkHandler = function (event, url, name) {
    viewSource(url)
    event.preventDefault();
  }

  window.content.focus();
}

////////////////////////////////////////////////////////////////////////////////
// view-source of a selection with the special effect of remapping the selection
// to the underlying view-source output
function viewPartialSourceForSelection(selection)
{
  var range = selection.getRangeAt(0);
  var ancestorContainer = range.commonAncestorContainer;
  var doc = ancestorContainer.ownerDocument;

  var startContainer = range.startContainer;
  var endContainer = range.endContainer;
  var startOffset = range.startOffset;
  var endOffset = range.endOffset;

  // let the ancestor be an element
  if (ancestorContainer.nodeType == Node.TEXT_NODE ||
      ancestorContainer.nodeType == Node.CDATA_SECTION_NODE)
    ancestorContainer = ancestorContainer.parentNode;

  // for selectAll, let's use the entire document, including <html>...</html>
  // @see nsDocumentViewer::SelectAll() for how selectAll is implemented
  try {
    if (ancestorContainer == doc.body)
      ancestorContainer = doc.documentElement;
  } catch (e) { }

  // each path is a "child sequence" (a.k.a. "tumbler") that
  // descends from the ancestor down to the boundary point
  var startPath = getPath(ancestorContainer, startContainer);
  var endPath = getPath(ancestorContainer, endContainer);

  // clone the fragment of interest and reset everything to be relative to it
  // note: it is with the clone that we operate/munge from now on.  Also note
  // that we clone into a data document to prevent images in the fragment from
  // loading and the like.  The use of importNode here, as opposed to adoptNode,
  // is _very_ important.
  // XXXbz wish there were a less hacky way to create an untrusted document here
  var isHTML = (doc.createElement("div").tagName == "DIV");
  var dataDoc = isHTML ? 
    ancestorContainer.ownerDocument.implementation.createHTMLDocument("") :
    ancestorContainer.ownerDocument.implementation.createDocument("", "", null);
  ancestorContainer = dataDoc.importNode(ancestorContainer, true);
  startContainer = ancestorContainer;
  endContainer = ancestorContainer;

  // Only bother with the selection if it can be remapped. Don't mess with
  // leaf elements (such as <isindex>) that secretly use anynomous content
  // for their display appearance.
  var canDrawSelection = ancestorContainer.hasChildNodes();
  if (canDrawSelection) {
    var i;
    for (i = startPath ? startPath.length-1 : -1; i >= 0; i--) {
      startContainer = startContainer.childNodes.item(startPath[i]);
    }
    for (i = endPath ? endPath.length-1 : -1; i >= 0; i--) {
      endContainer = endContainer.childNodes.item(endPath[i]);
    }

    // add special markers to record the extent of the selection
    // note: |startOffset| and |endOffset| are interpreted either as
    // offsets in the text data or as child indices (see the Range spec)
    // (here, munging the end point first to keep the start point safe...)
    var tmpNode;
    if (endContainer.nodeType == Node.TEXT_NODE ||
        endContainer.nodeType == Node.CDATA_SECTION_NODE) {
      // do some extra tweaks to try to avoid the view-source output to look like
      // ...<tag>]... or ...]</tag>... (where ']' marks the end of the selection).
      // To get a neat output, the idea here is to remap the end point from:
      // 1. ...<tag>]...   to   ...]<tag>...
      // 2. ...]</tag>...  to   ...</tag>]...
      if ((endOffset > 0 && endOffset < endContainer.data.length) ||
          !endContainer.parentNode || !endContainer.parentNode.parentNode)
        endContainer.insertData(endOffset, MARK_SELECTION_END);
      else {
        tmpNode = dataDoc.createTextNode(MARK_SELECTION_END);
        endContainer = endContainer.parentNode;
        if (endOffset == 0)
          endContainer.parentNode.insertBefore(tmpNode, endContainer);
        else
          endContainer.parentNode.insertBefore(tmpNode, endContainer.nextSibling);
      }
    }
    else {
      tmpNode = dataDoc.createTextNode(MARK_SELECTION_END);
      endContainer.insertBefore(tmpNode, endContainer.childNodes.item(endOffset));
    }

    if (startContainer.nodeType == Node.TEXT_NODE ||
        startContainer.nodeType == Node.CDATA_SECTION_NODE) {
      // do some extra tweaks to try to avoid the view-source output to look like
      // ...<tag>[... or ...[</tag>... (where '[' marks the start of the selection).
      // To get a neat output, the idea here is to remap the start point from:
      // 1. ...<tag>[...   to   ...[<tag>...
      // 2. ...[</tag>...  to   ...</tag>[...
      if ((startOffset > 0 && startOffset < startContainer.data.length) ||
          !startContainer.parentNode || !startContainer.parentNode.parentNode ||
          startContainer != startContainer.parentNode.lastChild)
        startContainer.insertData(startOffset, MARK_SELECTION_START);
      else {
        tmpNode = dataDoc.createTextNode(MARK_SELECTION_START);
        startContainer = startContainer.parentNode;
        if (startOffset == 0)
          startContainer.parentNode.insertBefore(tmpNode, startContainer);
        else
          startContainer.parentNode.insertBefore(tmpNode, startContainer.nextSibling);
      }
    }
    else {
      tmpNode = dataDoc.createTextNode(MARK_SELECTION_START);
      startContainer.insertBefore(tmpNode, startContainer.childNodes.item(startOffset));
    }
  }

  // now extract and display the syntax highlighted source
  tmpNode = dataDoc.createElementNS(NS_XHTML, 'div');
  tmpNode.appendChild(ancestorContainer);

  // the load is aynchronous and so we will wait until the view-source DOM is done
  // before drawing the selection.
  if (canDrawSelection) {
    window.document.getElementById("content").addEventListener("load", drawSelection, true);
  }

  // all our content is held by the data:URI and URIs are internally stored as utf-8 (see nsIURI.idl)
  var loadFlags = Components.interfaces.nsIWebNavigation.LOAD_FLAGS_NONE;
  getWebNavigation().loadURIWithBase((isHTML ?
                                      "view-source:data:text/html;charset=utf-8," :
                                      "view-source:data:application/xml;charset=utf-8,")
                                     + encodeURIComponent(tmpNode.innerHTML),
                                     loadFlags, null, null, null,
                                     Services.io.newURI(doc.baseURI, null, null));
}

////////////////////////////////////////////////////////////////////////////////
// helper to get a path like FIXptr, but with an array instead of the "tumbler" notation
// see FIXptr: http://lists.w3.org/Archives/Public/www-xml-linking-comments/2001AprJun/att-0074/01-NOTE-FIXptr-20010425.htm
function getPath(ancestor, node)
{
  var n = node;
  var p = n.parentNode;
  if (n == ancestor || !p)
    return null;
  var path = new Array();
  if (!path)
    return null;
  do {
    for (var i = 0; i < p.childNodes.length; i++) {
      if (p.childNodes.item(i) == n) {
        path.push(i);
        break;
      }
    }
    n = p;
    p = n.parentNode;
  } while (n != ancestor && p);
  return path;
}

////////////////////////////////////////////////////////////////////////////////
// using special markers left in the serialized source, this helper makes the
// underlying markup of the selected fragment to automatically appear as selected
// on the inflated view-source DOM
function drawSelection()
{
  gBrowser.contentDocument.title =
    gViewSourceBundle.getString("viewSelectionSourceTitle");

  // find the special selection markers that we added earlier, and
  // draw the selection between the two...
  var findService = null;
  try {
    // get the find service which stores the global find state
    findService = Components.classes["@mozilla.org/find/find_service;1"]
                            .getService(Components.interfaces.nsIFindService);
  } catch(e) { }
  if (!findService)
    return;

  // cache the current global find state
  var matchCase     = findService.matchCase;
  var entireWord    = findService.entireWord;
  var wrapFind      = findService.wrapFind;
  var findBackwards = findService.findBackwards;
  var searchString  = findService.searchString;
  var replaceString = findService.replaceString;

  // setup our find instance
  var findInst = gBrowser.webBrowserFind;
  findInst.matchCase = true;
  findInst.entireWord = false;
  findInst.wrapFind = true;
  findInst.findBackwards = false;

  // ...lookup the start mark
  findInst.searchString = MARK_SELECTION_START;
  var startLength = MARK_SELECTION_START.length;
  findInst.findNext();

  var selection = content.getSelection();
  if (!selection.rangeCount)
    return;

  var range = selection.getRangeAt(0);

  var startContainer = range.startContainer;
  var startOffset = range.startOffset;

  // ...lookup the end mark
  findInst.searchString = MARK_SELECTION_END;
  var endLength = MARK_SELECTION_END.length;
  findInst.findNext();

  var endContainer = selection.anchorNode;
  var endOffset = selection.anchorOffset;

  // reset the selection that find has left
  selection.removeAllRanges();

  // delete the special markers now...
  endContainer.deleteData(endOffset, endLength);
  startContainer.deleteData(startOffset, startLength);
  if (startContainer == endContainer)
    endOffset -= startLength; // has shrunk if on same text node...
  range.setEnd(endContainer, endOffset);

  // show the selection and scroll it into view
  selection.addRange(range);
  // the default behavior of the selection is to scroll at the end of
  // the selection, whereas in this situation, it is more user-friendly
  // to scroll at the beginning. So we override the default behavior here
  try {
    getSelectionController().scrollSelectionIntoView(
                               Ci.nsISelectionController.SELECTION_NORMAL,
                               Ci.nsISelectionController.SELECTION_ANCHOR_REGION,
                               true);
  }
  catch(e) { }

  // restore the current find state
  findService.matchCase     = matchCase;
  findService.entireWord    = entireWord;
  findService.wrapFind      = wrapFind;
  findService.findBackwards = findBackwards;
  findService.searchString  = searchString;
  findService.replaceString = replaceString;

  findInst.matchCase     = matchCase;
  findInst.entireWord    = entireWord;
  findInst.wrapFind      = wrapFind;
  findInst.findBackwards = findBackwards;
  findInst.searchString  = searchString;
}

////////////////////////////////////////////////////////////////////////////////
// special handler for markups such as MathML where reformatting the output is
// helpful
function viewPartialSourceForFragment(node, context)
{
  gTargetNode = node;
  if (gTargetNode && gTargetNode.nodeType == Node.TEXT_NODE)
    gTargetNode = gTargetNode.parentNode;

  // walk up the tree to the top-level element (e.g., <math>, <svg>)
  var topTag;
  if (context == 'mathml')
    topTag = 'math';
  else
    throw 'not reached';
  var topNode = gTargetNode;
  while (topNode && topNode.localName != topTag)
    topNode = topNode.parentNode;
  if (!topNode)
    return;

  // serialize
  var title = gViewSourceBundle.getString("viewMathMLSourceTitle");
  var wrapClass = gWrapLongLines ? ' class="wrap"' : '';
  var source =
    '<!DOCTYPE html>'
  + '<html>'
  + '<head><title>' + title + '</title>'
  + '<link rel="stylesheet" type="text/css" href="' + gViewSourceCSS + '">'
  + '<style type="text/css">'
  + '#target { border: dashed 1px; background-color: lightyellow; }'
  + '</style>'
  + '</head>'
  + '<body id="viewsource"' + wrapClass
  +        ' onload="document.title=\''+title+'\';document.getElementById(\'target\').scrollIntoView(true)">'
  + '<pre>'
  + getOuterMarkup(topNode, 0)
  + '</pre></body></html>'
  ; // end

  // display
  gBrowser.loadURI("data:text/html;charset=utf-8," + encodeURIComponent(source));
}

////////////////////////////////////////////////////////////////////////////////
function getInnerMarkup(node, indent) {
  var str = '';
  for (var i = 0; i < node.childNodes.length; i++) {
    str += getOuterMarkup(node.childNodes.item(i), indent);
  }
  return str;
}

////////////////////////////////////////////////////////////////////////////////
function getOuterMarkup(node, indent) {
  var newline = '';
  var padding = '';
  var str = '';
  if (node == gTargetNode) {
    gStartTargetLine = gLineCount;
    str += '</pre><pre id="target">';
  }

  switch (node.nodeType) {
  case Node.ELEMENT_NODE: // Element
    // to avoid the wide gap problem, '\n' is not emitted on the first
    // line and the lines before & after the <pre id="target">...</pre>
    if (gLineCount > 0 &&
        gLineCount != gStartTargetLine &&
        gLineCount != gEndTargetLine) {
      newline = '\n';
    }
    gLineCount++;
    if (gDebug) {
      newline += gLineCount;
    }
    for (var k = 0; k < indent; k++) {
      padding += ' ';
    }
    str += newline + padding
        +  '&lt;<span class="start-tag">' + node.nodeName + '</span>';
    for (var i = 0; i < node.attributes.length; i++) {
      var attr = node.attributes.item(i);
      if (!gDebug && attr.nodeName.match(/^[-_]moz/)) {
        continue;
      }
      str += ' <span class="attribute-name">'
          +  attr.nodeName
          +  '</span>=<span class="attribute-value">"'
          +  unicodeTOentity(attr.nodeValue)
          +  '"</span>';
    }
    if (!node.hasChildNodes()) {
      str += '/&gt;';
    }
    else {
      str += '&gt;';
      var oldLine = gLineCount;
      str += getInnerMarkup(node, indent + 2);
      if (oldLine == gLineCount) {
        newline = '';
        padding = '';
      }
      else {
        newline = (gLineCount == gEndTargetLine) ? '' : '\n';
        gLineCount++;
        if (gDebug) {
          newline += gLineCount;
        }
      }
      str += newline + padding
          +  '&lt;/<span class="end-tag">' + node.nodeName + '</span>&gt;';
    }
    break;
  case Node.TEXT_NODE: // Text
    var tmp = node.nodeValue;
    tmp = tmp.replace(/(\n|\r|\t)+/g, " ");
    tmp = tmp.replace(/^ +/, "");
    tmp = tmp.replace(/ +$/, "");
    if (tmp.length != 0) {
      str += '<span class="text">' + unicodeTOentity(tmp) + '</span>';
    }
    break;
  default:
    break;
  }

  if (node == gTargetNode) {
    gEndTargetLine = gLineCount;
    str += '</pre><pre>';
  }
  return str;
}

////////////////////////////////////////////////////////////////////////////////
function unicodeTOentity(text)
{
  const charTable = {
    '&': '&amp;<span class="entity">amp;</span>',
    '<': '&amp;<span class="entity">lt;</span>',
    '>': '&amp;<span class="entity">gt;</span>',
    '"': '&amp;<span class="entity">quot;</span>'
  };

  function charTableLookup(letter) {
    return charTable[letter];
  }

  function convertEntity(letter) {
    try {
      var unichar = gEntityConverter.ConvertToEntity(letter, entityVersion);
      var entity = unichar.substring(1); // extract '&'
      return '&amp;<span class="entity">' + entity + '</span>';
    } catch (ex) {
      return letter;
    }
  }

  if (!gEntityConverter) {
    try {
      gEntityConverter =
        Components.classes["@mozilla.org/intl/entityconverter;1"]
                  .createInstance(Components.interfaces.nsIEntityConverter);
    } catch(e) { }
  }

  const entityVersion = Components.interfaces.nsIEntityConverter.entityW3C;

  var str = text;

  // replace chars in our charTable
  str = str.replace(/[<>&"]/g, charTableLookup);

  // replace chars > 0x7f via nsIEntityConverter
  str = str.replace(/[^\0-\u007f]/g, convertEntity);

  return str;
}
