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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   smorrison@gte.com
 *   Terry Hayes <thayes@netscape.com>
 *   Daniel Brooks <db48x@yahoo.com>
 *   Florian QUEZE <f.qu@laposte.net>
 *   Erik Fabert <jerfa@yahoo.com>
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

//******** define a js object to implement nsITreeView
function pageInfoTreeView(columnids, copycol)
{
  // columnids is an array of strings indicating the names of the columns, in order
  this.columnids = columnids;
  this.colcount = columnids.length

  // copycol is the index number for the column that we want to add to 
  // the copy-n-paste buffer when the user hits accel-c
  this.copycol = copycol;
  this.rows = 0;
  this.tree = null;
  this.data = new Array;
  this.selection = null;
  this.sortcol = null;
  this.sortdir = 0;
}

pageInfoTreeView.prototype = {
  set rowCount(c) { throw "rowCount is a readonly property"; },
  get rowCount() { return this.rows; },

  setTree: function(tree) 
  {
    this.tree = tree;
  },

  getCellText: function(row, column)
  {
    // row can be null, but js arrays are 0-indexed.
    // colidx cannot be null, but can be larger than the number
    // of columns in the array (when column is a string not in 
    // this.columnids.) In this case it's the fault of
    // whoever typoed while calling this function.
    return this.data[row][column.index] || "";
  },

  setCellValue: function(row, column, value) 
  {
  },

  setCellText: function(row, column, value) 
  {
    this.data[row][column.index] = value;
  },

  addRow: function(row)
  {
    this.rows = this.data.push(row);
    this.rowCountChanged(this.rows - 1, 1);
  },

  addRows: function(rows)
  {
    var length = rows.length;
    for(var i = 0; i < length; i++)
      this.rows = this.data.push(rows[i]);
    this.rowCountChanged(this.rows - length, length);
  },

  rowCountChanged: function(index, count)
  {
    this.tree.rowCountChanged(index, count);
  },

  invalidate: function()
  {
    this.tree.invalidate();
  },

  clear: function()
  {
    this.data = new Array;
    this.rows = 0;
  },

  handleCopy: function(row)
  {
    return (row < 0 || this.copycol < 0) ? "" : (this.data[row][this.copycol] || "");
  },

  performActionOnRow: function(action, row)
  {
    if (action == "copy")
    {
      var data = this.handleCopy(row)
      this.tree.treeBody.parentNode.setAttribute("copybuffer", data);
    }
  },

  getRowProperties: function(row, prop) { },
  getCellProperties: function(row, column, prop) { },
  getColumnProperties: function(column, prop) { },
  isContainer: function(index) { return false; },
  isContainerOpen: function(index) { return false; },
  isSeparator: function(index) { return false; },
  isSorted: function() { },
  canDrop: function(index, orientation) { return false; },
  drop: function(row, orientation) { return false; },
  getParentIndex: function(index) { return 0; },
  hasNextSibling: function(index, after) { return false; },
  getLevel: function(index) { return 0; },
  getImageSrc: function(row, column) { },
  getProgressMode: function(row, column) { },
  getCellValue: function(row, column) { },
  toggleOpenState: function(index) { },
  cycleHeader: function(col) { },
  selectionChanged: function() { },
  cycleCell: function(row, column) { },
  isEditable: function(row, column) { return false; },
  isSelectable: function(row, column) { return false; },
  performAction: function(action) { },
  performActionOnCell: function(action, row, column) { }
};

// mmm, yummy. global variables.
var theWindow = null;
var theDocument = null;

// column number to copy from, second argument to pageInfoTreeView's constructor
const COPYCOL_NONE = -1;
const COPYCOL_META_CONTENT = 1;
const COPYCOL_FORM_ACTION = 2;
const COPYCOL_LINK_ADDRESS = 1;
const COPYCOL_IMAGE_ADDRESS = 0;

// one nsITreeView for each tree in the window
var metaView = new pageInfoTreeView(["meta-name","meta-content"], COPYCOL_META_CONTENT);
var formView = new pageInfoTreeView(["form-name","form-method","form-action","form-node"], COPYCOL_FORM_ACTION);
var fieldView = new pageInfoTreeView(["field-label","field-field","field-type","field-value"], COPYCOL_NONE);
var linkView = new pageInfoTreeView(["link-name","link-address","link-type","link-accesskey"], COPYCOL_LINK_ADDRESS);
var imageView = new pageInfoTreeView(["image-address","image-type","image-alt","image-count","image-node","image-bg"], COPYCOL_IMAGE_ADDRESS);

var imageHash = {};

var kmsPerSec = 1000;

// localized strings (will be filled in when the document is loaded)
// this isn't all of them, these are just the ones that would otherwise have been loaded inside a loop
var gStrings = {}
var theBundle;

const DRAGSERVICE_CONTRACTID    = "@mozilla.org/widget/dragservice;1";
const TRANSFERABLE_CONTRACTID   = "@mozilla.org/widget/transferable;1";
const ARRAY_CONTRACTID          = "@mozilla.org/supports-array;1";
const STRING_CONTRACTID         = "@mozilla.org/supports-string;1";

// a number of services I'll need later
// the cache services
const nsICacheService = Components.interfaces.nsICacheService;
const cacheService = Components.classes["@mozilla.org/network/cache-service;1"].getService(nsICacheService);
var httpCacheSession = cacheService.createSession("HTTP", 0, true);
httpCacheSession.doomEntriesIfExpired = false;
var ftpCacheSession = cacheService.createSession("FTP", 0, true);
ftpCacheSession.doomEntriesIfExpired = false;

// scriptable date formater, for pretty printing dates
const nsIScriptableDateFormat = Components.interfaces.nsIScriptableDateFormat;
var dateService = Components.classes["@mozilla.org/intl/scriptabledateformat;1"].getService(nsIScriptableDateFormat);

// clipboard helper
try
{
  const gClipboardHelper = Components.classes["@mozilla.org/widget/clipboardhelper;1"].getService(Components.interfaces.nsIClipboardHelper);
}
catch(e)
{
  // do nothing, later code will handle the error
}

// interfaces for the different html elements
const nsIAnchorElement   = Components.interfaces.nsIDOMHTMLAnchorElement
const nsIImageElement    = Components.interfaces.nsIDOMHTMLImageElement
const nsIAreaElement     = Components.interfaces.nsIDOMHTMLAreaElement
const nsILinkElement     = Components.interfaces.nsIDOMHTMLLinkElement
const nsIInputElement    = Components.interfaces.nsIDOMHTMLInputElement
const nsIFormElement     = Components.interfaces.nsIDOMHTMLFormElement
const nsIAppletElement   = Components.interfaces.nsIDOMHTMLAppletElement
const nsIObjectElement   = Components.interfaces.nsIDOMHTMLObjectElement
const nsIEmbedElement    = Components.interfaces.nsIDOMHTMLEmbedElement
const nsIButtonElement   = Components.interfaces.nsIDOMHTMLButtonElement
const nsISelectElement   = Components.interfaces.nsIDOMHTMLSelectElement
const nsITextareaElement = Components.interfaces.nsIDOMHTMLTextAreaElement

// Interface for image loading content
const nsIImageLoadingContent = Components.interfaces.nsIImageLoadingContent;

// namespaces, don't need all of these yet...
const XLinkNS  = "http://www.w3.org/1999/xlink";
const XULNS    = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const XMLNS    = "http://www.w3.org/XML/1998/namespace";
const XHTMLNS  = "http://www.w3.org/1999/xhtml";
const XHTML2NS = "http://www.w3.org/2002/06/xhtml2"

const XHTMLNSre  = "^http\:\/\/www\.w3\.org\/1999\/xhtml$";
const XHTML2NSre = "^http\:\/\/www\.w3\.org\/2002\/06\/xhtml2$";
const XHTMLre = RegExp(XHTMLNSre + "|" + XHTML2NSre, "");

/* Overlays register init functions here.
 *   Add functions to call by invoking "onLoadRegistry.append(XXXLoadFunc);"
 *   The XXXLoadFunc should be unique to the overlay module, and will be
 *   invoked as "XXXLoadFunc();"
 */
var onLoadRegistry = [ ];
 
/* Called when PageInfo window is loaded.  Arguments are:
 *  window.arguments[0] - (optional) an object consisting of
 *                         - doc: (optional) document to use for source. if not provided, 
 *                                the calling window's document will be used
 *                         - initialTab: (optional) id of the inital tab to display
 */
function onLoadPageInfo()
{
  //dump("===============================================================================\n");

  theBundle = document.getElementById("pageinfobundle");
  gStrings.unknown = theBundle.getString("unknown");
  gStrings.notSet = theBundle.getString("notset");
  gStrings.emptyString = theBundle.getString("emptystring");
  gStrings.noExpiration = theBundle.getString("generalNoExpiration");
  gStrings.linkAnchor = theBundle.getString("linkAnchor");
  gStrings.linkArea = theBundle.getString("linkArea");
  gStrings.linkSubmit = theBundle.getString("linkSubmit");
  gStrings.linkSubmission = theBundle.getString("linkSubmission");
  gStrings.linkRel = theBundle.getString("linkRel");
  gStrings.linkStylesheet = theBundle.getString("linkStylesheet");
  gStrings.linkRev = theBundle.getString("linkRev");
  gStrings.linkX = theBundle.getString("linkX");
  gStrings.mediaImg = theBundle.getString("mediaImg");
  gStrings.mediaBGImg = theBundle.getString("mediaBGImg");
  gStrings.mediaApplet = theBundle.getString("mediaApplet");
  gStrings.mediaObject = theBundle.getString("mediaObject");
  gStrings.mediaEmbed = theBundle.getString("mediaEmbed");
  gStrings.mediaLink = theBundle.getString("mediaLink");
  gStrings.mediaInput = theBundle.getString("mediaInput");

  var docTitle = "";
  if ("arguments" in window && window.arguments.length >= 1 &&
      window.arguments[0] && window.arguments[0].doc)
  {
    theWindow = null;
    theDocument = window.arguments[0].doc;
    docTitle = theBundle.getString("frameInfo.title");
  } 
  else 
  {
    if ("gBrowser" in window.opener)
      theWindow = window.opener.gBrowser.contentWindow;
    else
      theWindow = window.opener.frames[0];
    theDocument = theWindow.document;

    docTitle = theBundle.getString("pageInfo.title");
  }

  document.title = docTitle;

  // do the easy stuff first
  makeGeneralTab();

  // and then the hard stuff
  makeTabs(theDocument, theWindow);

  /* Call registered overlay init functions */
  for (x in onLoadRegistry)
  {
    onLoadRegistry[x]();
  }

  var tabControl = document.getElementById("tabbox");
  if (tabControl)
  {
    /* Select the requested tab, if the name is specified */
    if ("arguments" in window && window.arguments.length >= 1 &&
        window.arguments[0] && window.arguments[0].initialTab)
    {
      var tab = document.getElementById(window.arguments[0].initialTab);
      if (tab)
        tabControl.selectedTab = tab;
    }
    tabControl.selectedTab.focus();
  }
}

function doHelpButton() {
  var helpdoc;
  var tabControl = document.getElementById("tabbox");
  switch (tabControl.selectedTab.id) {
    case "generalTab":
      helpdoc = "pageinfo_general";
      break;
    case "formsTab":
      helpdoc = "pageinfo_forms";
      break;
    case "linksTab":
      helpdoc = "pageinfo_links";
      break;
    case "mediaTab":
      helpdoc = "pageinfo_media";
      break;
    case "securityTab":
      helpdoc = "pageinfo_security";
      break;
    case "p3pTab":
      helpdoc = "pageinfo_privacy";
      break;
    default:
      helpdoc = "pageinfo_general";
      break;
  }
  openHelp(helpdoc);  
}
 
function makeGeneralTab()
{
  var title = (theDocument.title) ? theBundle.getFormattedString("pageTitle", [theDocument.title]) : theBundle.getString("noPageTitle");
  document.getElementById("titletext").value = title;

  var url = theDocument.location.toString();
  setItemValue("urltext", url, gStrings.unknown);

  var mode = ("compatMode" in theDocument && theDocument.compatMode == "BackCompat") ? theBundle.getString("generalQuirksMode") : theBundle.getString("generalStrictMode");
  document.getElementById("modetext").value = mode;

  var referrer = ("referrer" in theDocument && theDocument.referrer);
  setItemValue("refertext", referrer);

  // find out the mime type
  var mimeType = theDocument.contentType;
  setItemValue("typetext", mimeType, gStrings.unknown);
  
  // get the meta tags
  var metaNodes = theDocument.getElementsByTagName("meta");
  var metaTree = document.getElementById("metatree");

  metaTree.treeBoxObject.view = metaView;

  var length = metaNodes.length;
  for (var i = 0; i < length; i++)
    metaView.addRow([metaNodes[i].name || metaNodes[i].httpEquiv, metaNodes[i].content]);

  // get the document characterset
  var encoding = theDocument.characterSet;
  document.getElementById("encodingtext").value = encoding;

  // get the date of last modification
  var modifiedText = formatDate(theDocument.lastModified, gStrings.notSet);
  document.getElementById("modifiedtext").value = modifiedText;
  
  // get cache info
  var sourceText;
  var expirationText;
  var sizeText;

  var pageSize = 0; 
  var kbSize = 0;
  var expirationTime = 0;

  var cacheKey = url.replace(/#.*$/, "");
  try
  {
    var cacheEntryDescriptor = httpCacheSession.openCacheEntry(cacheKey, Components.interfaces.nsICache.ACCESS_READ, false);
    if (cacheEntryDescriptor)
    { 
      switch(cacheEntryDescriptor.deviceID)
      {
        case "disk":
          sourceText = theBundle.getString("generalDiskCache");
          break;
        case "memory":
          sourceText = theBundle.getString("generalMemoryCache");
          break;
        default:
          sourceText = cacheEntryDescriptor.deviceID;
          break;
      }
    }
  }
  catch(ex)
  {
    try
    {
      cacheEntryDescriptor = ftpCacheSession.openCacheEntry(cacheKey, Components.interfaces.nsICache.ACCESS_READ, false);
      if (cacheEntryDescriptor)
      {
        switch(cacheEntryDescriptor.deviceID)
        {
          case "disk":
            sourceText = theBundle.getString("generalDiskCache");
            break;
          case "memory":
            sourceText = theBundle.getString("generalMemoryCache");
            break;
          default:
            sourceText = cacheEntryDescriptor.deviceID;
            break;
        }
      }
    }
    catch(ex2)
    {
    }
  }

  if (cacheEntryDescriptor)
  {
    pageSize = cacheEntryDescriptor.dataSize;
    kbSize = pageSize / 1024;
    sizeText = theBundle.getFormattedString("generalSize", [formatNumber(Math.round(kbSize*100)/100), formatNumber(pageSize)]);

    expirationText = formatDate(cacheEntryDescriptor.expirationTime*kmsPerSec, gStrings.notSet);
  }

  setItemValue("sourcetext", sourceText, theBundle.getString("generalNotCached"));
  setItemValue("expirestext", expirationText, gStrings.noExpiration);
  setItemValue("sizetext", sizeText, gStrings.unknown);
}

//******** Generic Build-a-tab
// Assumes the views are empty. Only called once to build the tabs, and
// does so by farming the task off to another thread via setTimeout().
// The actual work is done with a TreeWalker that calls doGrab() once for
// each element node in the document.

function makeTabs(aDocument, aWindow)
{
  if (aWindow && aWindow.frames.length > 0)
  {
    var num = aWindow.frames.length;
    for (var i = 0; i < num; i++)
      makeTabs(aWindow.frames[i].document, aWindow.frames[i]);  // recurse through the frames
  }

  var formTree = document.getElementById("formtree");
  var linkTree = document.getElementById("linktree");
  var imageTree = document.getElementById("imagetree");

  formTree.treeBoxObject.view = formView;
  linkTree.treeBoxObject.view = linkView;
  imageTree.treeBoxObject.view = imageView;
  
  var iterator = aDocument.createTreeWalker(aDocument, NodeFilter.SHOW_ELEMENT, grabAll, true);
  setTimeout(doGrab, 16, iterator);
}

function doGrab(iterator)
{
  for (var i = 0; i < 50; ++i)
    if (!iterator.nextNode())
      return;

  setTimeout(doGrab, 16, iterator);
}

function ensureSelection(view)
{
  // only select something if nothing is currently selected
  // and if there's anything to select
  if (view.selection.count == 0 && view.rowCount)
    view.selection.select(0);
}

function addImage(url, type, alt, elem, isBg)
{
  if (url == "")
    return;
  if (!(url in imageHash))
    imageHash[url] = {};
  if (!(type in imageHash[url]))
    imageHash[url][type] = {};
  if (!(alt in imageHash[url][type])) {
    imageHash[url][type][alt] = imageView.data.length;
    imageView.addRow([url, type, alt, 1, elem, isBg]);
  } else {
    var i = imageHash[url][type][alt];
    imageView.data[i][3]++;
  }
}

function grabAll(elem)
{
  // check for background images, any node may have one
  var ComputedStyle = elem.ownerDocument.defaultView.getComputedStyle(elem, "");
  var url = ComputedStyle && ComputedStyle.getPropertyCSSValue("background-image");
  if (url && url.primitiveType == CSSPrimitiveValue.CSS_URI)
    addImage(url.getStringValue(), gStrings.mediaBGImg, gStrings.notSet, elem, true);

  // one swi^H^H^Hif-else to rule them all
  if (elem instanceof nsIAnchorElement)
    linkView.addRow([getValueText(elem), elem.href, gStrings.linkAnchor, elem.target, elem.accessKey]);
  else if (elem instanceof nsIImageElement)
    addImage(elem.src, gStrings.mediaImg, (elem.hasAttribute("alt")) ? elem.alt : gStrings.notSet, elem, false);
  else if (elem instanceof nsIAreaElement)
    linkView.addRow([elem.alt, elem.href, gStrings.linkArea, elem.target]);
  else if (elem instanceof nsILinkElement)
  {
    if (elem.rel)
    {
      var rel = elem.rel;
      if (/\bicon\b/i.test(rel))
        addImage(elem.href, gStrings.mediaLink, "", elem, false);
      else if (/\bstylesheet\b/i.test(rel))
        linkView.addRow([elem.rel, elem.href, gStrings.linkStylesheet, elem.target]);
      else
        linkView.addRow([elem.rel, elem.href, gStrings.linkRel, elem.target]);
    }
    else
      linkView.addRow([elem.rev, elem.href, gStrings.linkRev, elem.target]);
  }
  else if (elem instanceof nsIInputElement || elem instanceof nsIButtonElement)
  {
    switch (elem.type.toLowerCase())
    {
      case "image":
        addImage(elem.src, gStrings.mediaInput, (elem.hasAttribute("alt")) ? elem.alt : gStrings.notSet, elem, false);
        // Fall through, <input type="image"> submits, too
      case "submit":
        // Form element properties can be hidden by child elements with the same name, so
        // we need to use a special access method, XPCNativeWrapper, to get their real values
        if ("form" in elem && elem.form)
        {
          linkView.addRow([elem.value || getValueText(elem) || gStrings.linkSubmit, elem.form.action, gStrings.linkSubmission, elem.form.target]);
        }
        else
          linkView.addRow([elem.value || getValueText(elem) || gStrings.linkSubmit, '', gStrings.linkSubmission, '']);
    }
  }
  else if (elem instanceof nsIFormElement)
  {
    formView.addRow([elem.name, elem.method, elem.action, elem]);
  }
  else if (elem instanceof nsIAppletElement)
  {
    //XXX When Java is enabled, the DOM model for <APPLET> is broken. Bug #59686.
    // Also, some reports of a crash with Java in Media tab (bug 136535), and mixed
    // content from two hosts (bug 136539) so just drop applets from Page Info when
    // Java is on. For the 1.0.1 branch; get a real fix on the trunk.
    if (!navigator.javaEnabled())
      addImage(elem.code || elem.object, gStrings.mediaApplet, "", elem, false);
  }
  else if (elem instanceof nsIObjectElement)
    addImage(elem.data, gStrings.mediaObject, getValueText(elem), elem, false);
  else if (elem instanceof nsIEmbedElement)
    addImage(elem.src, gStrings.mediaEmbed, "", elem, false);
  else if (elem.hasAttributeNS(XLinkNS, "href")) {
    var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                              .getService(Components.interfaces.nsIIOService);
    url = elem.getAttributeNS(XLinkNS, "href");
    try {
        var baseURI = ioService.newURI(elem.baseURI, elem.ownerDocument.characterSet, null);
        url = ioService.newURI(href, elem.ownerDocument.characterSet, baseURI).spec;
    } catch (e) {}
    linkView.addRow([getValueText(elem), url, gStrings.linkX, ""]);
  }
  return NodeFilter.FILTER_ACCEPT;
}

//******** Form Stuff
function onFormSelect()
{
  var formTree = document.getElementById("formtree");
  if (!formView.rowCount) return;

  if (formView.selection.count == 1)
  {
    var formPreview = document.getElementById("formpreview");
    fieldView.clear();
    formPreview.treeBoxObject.view = fieldView;

    var clickedRow = formView.selection.currentIndex;
    // form-node;
    var form = formView.data[clickedRow][3];

    var ft = null;
    if (form.name)
      ft = theBundle.getFormattedString("formTitle", [form.name]);

    setItemValue("formenctype", form.encoding, theBundle.getString("default"));
    setItemValue("formtarget", form.target, theBundle.getString("formDefaultTarget"));
    document.getElementById("formname").value = ft || theBundle.getString("formUntitled");

    var formfields = form.elements;

    var length = formfields.length;

    var checked = theBundle.getString("formChecked");
    var unchecked = theBundle.getString("formUnchecked");    

    for (var i = 0; i < length; i++)
    {
      var elem = formfields[i], val;

      if (elem instanceof nsIButtonElement)
        val = getValueText(elem);
      else
        val = (/^password$/i.test(elem.type)) ? theBundle.getString("formPassword") : elem.value;

      fieldView.addRow(["", elem.name, elem.type, val]);
    }

    var labels = form.getElementsByTagName("label");
    var llength = labels.length;
    var label;

    for (i = 0; i < llength; i++)
    {
      label = labels[i];
      var whatfor = label.hasAttribute("for") ?
        theDocument.getElementById(label.getAttribute("for")) :
        findFirstControl(label);

      if (whatfor && (whatfor.form == form)) 
      {
        var labeltext = getValueText(label);
        for (var j = 0; j < length; j++)
          if (formfields[j] == whatfor) {
            var col = formPreview.columns["field-label"];
            fieldView.setCellText(j, col, labeltext);
          }
      }
    }
  }
}

function FormControlFilter(node) 
{
  if (node instanceof nsIInputElement || node instanceof nsISelectElement ||
      node instanceof nsIButtonElement || node instanceof nsITextareaElement ||
      node instanceof nsIObjectElement)
      return NodeFilter.FILTER_ACCEPT;
    return NodeFilter.FILTER_SKIP;
}

function findFirstControl(node)
{
  var iterator = theDocument.createTreeWalker(node, NodeFilter.SHOW_ELEMENT, FormControlFilter, true);

  return iterator.nextNode();
}

//******** Link Stuff
function openURL(target)
{
  var url = target.parentNode.childNodes[2].value;
  window.open(url, "_blank", "chrome");
}

function onBeginLinkDrag(event,urlField,descField)
{
  if (event.originalTarget.localName != "treechildren")
    return;

  var tree = event.target;
  if (!("treeBoxObject" in tree))
    tree = tree.parentNode;

  var row = tree.treeBoxObject.getRowAt(event.clientX, event.clientY);
  if (row == -1)
    return;

  // Getting drag-system needed services
  var dragService = Components.classes[DRAGSERVICE_CONTRACTID].getService().QueryInterface(Components.interfaces.nsIDragService);
  var transArray = Components.classes[ARRAY_CONTRACTID].createInstance(Components.interfaces.nsISupportsArray);
  if (!transArray)
    return;
  var trans = Components.classes[TRANSFERABLE_CONTRACTID].createInstance(Components.interfaces.nsITransferable);
  if (!trans)
    return;
  
  // Adding URL flavor
  trans.addDataFlavor("text/x-moz-url");
  var col = tree.columns[urlField];
  var url = tree.view.getCellText(row, col);
  col = tree.columns[descField];
  var desc = tree.view.getCellText(row, col);
  var stringURL = Components.classes[STRING_CONTRACTID].createInstance(Components.interfaces.nsISupportsString);
  stringURL.data = url + "\n"+desc;
  trans.setTransferData("text/x-moz-url", stringURL, stringURL.data.length * 2 );
  transArray.AppendElement(trans.QueryInterface(Components.interfaces.nsISupports));

  dragService.invokeDragSession(event.target, transArray, null, dragService.DRAGDROP_ACTION_NONE);
}

//******** Image Stuff
function getSelectedImage(tree)
{
  if (!imageView.rowCount) return null;

  // Only works if only one item is selected
  var clickedRow = tree.currentIndex;
  // image-node
  return imageView.data[clickedRow][4];
}

function saveMedia()
{
  var tree = document.getElementById("imagetree");
  var item = getSelectedImage(tree);
  var url = imageView.data[tree.currentIndex][0];

  if (url)
    // XXX Mozilla specific
    saveURL(url, null, 'SaveImageTitle', false, makeURI(item.baseURI));
}

function onImageSelect()
{
  var tree = document.getElementById("imagetree");
  var saveAsButton = document.getElementById("imagesaveasbutton");

  if (tree.view.selection.count == 1)
  {
    makePreview(tree.view.selection.currentIndex);
    saveAsButton.setAttribute("disabled", "false");
  }
  else
    saveAsButton.setAttribute("disabled", "true");
}

function makePreview(row)
{
  var imageTree = document.getElementById("imagetree");
  var item = getSelectedImage(imageTree);
  var col = imageTree.columns["image-address"];
  var url = imageView.getCellText(row, col);
  // image-bg
  var isBG = imageView.data[row][5];

  setItemValue("imageurltext", url);

  if (item.hasAttribute("title"))
    setItemValue("imagetitletext", item.title, gStrings.emptyString);
  else
    setItemValue("imagetitletext", null);

  if (item.hasAttribute("longDesc"))
    setItemValue("imagelongdesctext", item.longDesc, gStrings.emptyString);
  else
    setItemValue("imagelongdesctext", null);

  if (item.hasAttribute("alt"))
    setItemValue("imagealttext", item.alt, gStrings.emptyString);
  else if (item instanceof nsIImageElement || isBG)
    setItemValue("imagealttext", null);
  else
    setItemValue("imagealttext", getValueText(item));

  // get cache info
  var sourceText = theBundle.getString("generalNotCached");
  var expirationText;
  var sizeText;

  var pageSize = 0; 
  var kbSize = 0;
  var expirationTime = 0;
  var expirationDate = null;

  document.getElementById("imagesourcetext").removeAttribute("disabled");

  var cacheKey = url.replace(/#.*$/, "");
  try
  {
    var cacheEntryDescriptor = httpCacheSession.openCacheEntry(cacheKey, Components.interfaces.nsICache.ACCESS_READ, false);   // open for READ, in non-blocking mode
    if (cacheEntryDescriptor)
    {
      switch(cacheEntryDescriptor.deviceID)
      {
        case "disk":
          sourceText = theBundle.getString("generalDiskCache");
          break;
        case "memory":
          sourceText = theBundle.getString("generalMemoryCache");
          break;
        default:
          sourceText = cacheEntryDescriptor.deviceID;
          break;
      }
    }
  }
  catch(ex)
  {
    try
    {
      cacheEntryDescriptor = ftpCacheSession.openCacheEntry(cacheKey, Components.interfaces.nsICache.ACCESS_READ, false);   // open for READ, in non-blocking mode
      if (cacheEntryDescriptor)
      {
        switch(cacheEntryDescriptor.deviceID)
        {
          case "disk":
            sourceText = theBundle.getString("generalDiskCache");
            break;
          case "memory":
            sourceText = theBundle.getString("generalMemoryCache");
            break;
          default:
            sourceText = cacheEntryDescriptor.deviceID;
            break;
        }
      }
    }
    catch(ex2)
    {
    }
  }

  // find out the file size and expiration date
  if (cacheEntryDescriptor)
  {
    pageSize = cacheEntryDescriptor.dataSize;
    kbSize = pageSize / 1024;
    sizeText = theBundle.getFormattedString("generalSize", [formatNumber(Math.round(kbSize*100)/100), formatNumber(pageSize)]);

    expirationText = formatDate(cacheEntryDescriptor.expirationTime*kmsPerSec, null);
  }

  setItemValue("imageexpirestext", expirationText, gStrings.noExpiration);
  setItemValue("imagesizetext", sizeText, gStrings.unknown);
  setItemValue("imagesourcetext", sourceText, theBundle.getString("generalNotCached"));

  var mimeType;
  if (item instanceof nsIObjectElement || item instanceof nsIEmbedElement || item instanceof nsILinkElement)
    mimeType = item.type;
  if (!mimeType)
    mimeType = getContentTypeFromImgRequest(item) ||
               getContentTypeFromHeaders(cacheEntryDescriptor);

  setItemValue("imagetypetext", mimeType, gStrings.unknown);

  var imageContainer = document.getElementById("theimagecontainer");
  var oldImage = document.getElementById("thepreviewimage");

  const regex = /^(https?|ftp|file|gopher|about|chrome|resource):/;
  var isProtocolAllowed = regex.test(url);
  if (/^data:/.test(url) && /^image\//.test(mimeType))
    isProtocolAllowed = true;

  var newImage = new Image();
  newImage.setAttribute("id", "thepreviewimage");
  var physWidth = 0, physHeight = 0;
  var width = 0, height = 0;

  if ((item instanceof nsILinkElement || item instanceof nsIInputElement || item instanceof nsIImageElement || 
      (item instanceof nsIObjectElement && /^image\//.test(mimeType)) || isBG) && isProtocolAllowed)
  {
    newImage.setAttribute("src", url);
    physWidth = newImage.width || 0;
    physHeight = newImage.height || 0;

    // "width" and "height" attributes must be set to newImage,
    // even if there is no "width" or "height attribute in item;
    // otherwise, the preview image cannot be displayed correctly.
    if (!isBG)
    {
      newImage.width = ("width" in item && item.width) || newImage.naturalWidth;
      newImage.height = ("height" in item && item.height) || newImage.naturalHeight;
    }
    else
    {
      // the Width and Height of an HTML tag should not be use for its background image
      // (for example, "table" can have "width" or "height" attributes)
      newImage.width = newImage.naturalWidth;
      newImage.height = newImage.naturalHeight;
    }

    width = newImage.width;
    height = newImage.height;

    document.getElementById("theimagecontainer").collapsed = false
    document.getElementById("brokenimagecontainer").collapsed = true;
  } 
  else 
  {
    // fallback image for protocols not allowed (e.g., data: or javascript:) 
    // or elements not [yet] handled (e.g., object, embed).
    document.getElementById("brokenimagecontainer").collapsed = false;
    document.getElementById("theimagecontainer").collapsed = true;
  }

  var imageSize = document.getElementById("imageSize");
  if (url)
  {
    imageSize.value = theBundle.getFormattedString("mediaSize", [formatNumber(width), formatNumber(height)]);
    imageSize.removeAttribute("disabled");
  }
  else
  {
    imageSize.value = gStrings.notSet;
    imageSize.setAttribute("disabled", "true");
  }

  var physRow = document.getElementById("physRow");
  if (width != physWidth || height != physHeight)
  {
    physRow.collapsed = false;
    document.getElementById("physSize").value = theBundle.getFormattedString("mediaSize", [formatNumber(physWidth), formatNumber(physHeight)]);
  }
  else
    physRow.collapsed = true;

  imageContainer.removeChild(oldImage);
  imageContainer.appendChild(newImage);
}

function getContentTypeFromHeaders(cacheEntryDescriptor)
{
  if (!cacheEntryDescriptor)
    return null;

  return (/^Content-Type:\s*(.*?)\s*(?:\;|$)/mi
          .exec(cacheEntryDescriptor.getMetaDataElement("response-head")))[1];
}

function getContentTypeFromImgRequest(item)
{
  var httpRequest;

  try
  {
    var imageItem = item.QueryInterface(nsIImageLoadingContent);
    var imageRequest = imageItem.getRequest(nsIImageLoadingContent.CURRENT_REQUEST);
    if (imageRequest) 
      httpRequest = imageRequest.mimeType;
  }
  catch (ex)
  {
    // This never happened.  ;)
  }

  return httpRequest;
}

//******** Other Misc Stuff
// Modified from the Links Panel v2.3, http://segment7.net/mozilla/links/links.html
// parse a node to extract the contents of the node
function getValueText(node)
{
  var valueText = "";

  // form input elements don't generally contain information that is useful to our callers, so return nothing
  if (node instanceof nsIInputElement || node instanceof nsISelectElement || node instanceof nsITextareaElement)
    return valueText;

  // otherwise recurse for each child
  var length = node.childNodes.length;
  for (var i = 0; i < length; i++)
  {
    var childNode = node.childNodes[i];
    var nodeType = childNode.nodeType;

    // text nodes are where the goods are
    if (nodeType == Node.TEXT_NODE)
      valueText += " " + childNode.nodeValue;
    // and elements can have more text inside them
    else if (nodeType == Node.ELEMENT_NODE)
    {
      // images are special, we want to capture the alt text as if the image weren't there
      if (childNode instanceof nsIImageElement)
        valueText += " " + getAltText(childNode);
      else
        valueText += " " + getValueText(childNode);
    }
  }

  return stripWS(valueText);
}

// Copied from the Links Panel v2.3, http://segment7.net/mozilla/links/links.html
// traverse the tree in search of an img or area element and grab its alt tag
function getAltText(node)
{
  var altText = "";
  
  if (node.alt)
    return node.alt;
  var length = node.childNodes.length;
  for (var i = 0; i < length; i++)
    if ((altText = getAltText(node.childNodes[i]) != undefined))  // stupid js warning...
      return altText;
  return "";
}

// Copied from the Links Panel v2.3, http://segment7.net/mozilla/links/links.html
// strip leading and trailing whitespace, and replace multiple consecutive whitespace characters with a single space
function stripWS(text)
{
  var middleRE = /\s+/g;
  var endRE = /(^\s+)|(\s+$)/g;

  text = text.replace(middleRE, " ");
  return text.replace(endRE, "");
}

function setItemValue(id, value, other)
{
  var item = document.getElementById(id);
  item.value = value || other || gStrings.notSet;
  if (value)
    item.removeAttribute("disabled");
  else
    item.setAttribute("disabled", "true");
}

function formatNumber(number)
{
  return (+number).toLocaleString();  // coerce number to a numeric value before calling toLocaleString()
}

function formatDate(datestr, unknown)
{
  var date = new Date(datestr);
  return (date.valueOf()) ? dateService.FormatDateTime("", dateService.dateFormatLong, dateService.timeFormatSeconds, date.getFullYear(), date.getMonth()+1, date.getDate(), date.getHours(), date.getMinutes(), date.getSeconds()) : unknown;
}

function doCopy()
{
  if (!gClipboardHelper) 
    return;

  var elem = document.commandDispatcher.focusedElement;

  if (elem && "treeBoxObject" in elem)
  {
    var view = elem.treeBoxObject.view;
    var selection = view.selection;
    var text = [], tmp = '';
    var min = {}, max = {};

    var count = selection.getRangeCount();

    for (var i = 0; i < count; i++)
    {
      selection.getRangeAt(i, min, max);

      for (var row = min.value; row <= max.value; row++)
      {
        view.performActionOnRow("copy", row);

        tmp = elem.getAttribute("copybuffer");
        if (tmp)
          text.push(tmp);
        elem.removeAttribute("copybuffer");
      }
    }
    gClipboardHelper.copyString(text.join("\n"));
  }
}

function doSelectAll()
{
  var elem = document.commandDispatcher.focusedElement;

  if (elem && "treeBoxObject" in elem)
    elem.treeBoxObject.view.selection.selectAll();
}

