/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Contributor(s): smorrison@gte.com
 *   Terry Hayes <thayes@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const nsScriptableDateFormat_CONTRACTID = "@mozilla.org/intl/scriptabledateformat;1";
const nsIScriptableDateFormat = Components.interfaces.nsIScriptableDateFormat;

/* Overlays register init functions here.
 *   Add functions to call by invoking "onLoadRegistry.append(XXXLoadFunc);"
 *   The XXXLoadFunc should be unique to the overlay module, and will be
 *   invoked as "XXXLoadFunc();"
 */
var onLoadRegistry = [ ];

/* Called when PageInfo window is loaded.  Arguments are:
 *   window.arguments[0] - window (or frame) to use for source (may be null)
 *   window.arguments[1] - tab name to display first (may be null)
 */
function onLoadPageInfo()
{
  var strbundle = document.getElementById("pageinfobundle");

  var page;
  var docTitle;
  if ((window.arguments.length >= 1) && window.arguments[0])
  {
    page = window.arguments[0];
    docTitle = strbundle.getString("frameInfo.title");
  }
  else
  {
    page = window.opener.frames[0].document;
    docTitle = strbundle.getString("pageInfo.title");
  }

  window.document.title = docTitle;
  
  var root = document.getElementById("cont");

  makeDocument(page, root);

  var formTreeHolder = document.getElementById("formTreeContainer");
  var hasForm = makeFormTree(page, formTreeHolder);
  if (hasForm)
  {
    var formsTab = document.getElementById("formsTab");

    formsTab.removeAttribute("hidden");
    formTreeHolder.removeAttribute("collapsed");
  }

  var imageTreeHolder = document.getElementById("imageTreeContainer");
  var hasImages = makeImageTree(page, imageTreeHolder);
  if (hasImages)
  {
    var imagesTab = document.getElementById("imagesTab");

    imagesTab.removeAttribute("hidden");
    imageTreeHolder.removeAttribute("collapsed");
  }

  /* Call registered overlay init functions */
  for (x in onLoadRegistry)
  {
    onLoadRegistry[x]();
  }

  /* Selected the requested tab, if the name is specified */
  /*  if (window.arguments != null) { */
  if ("arguments" in window && window.arguments.length > 1) {
    var tabName = window.arguments[1];

    if (tabName)
    {
      var tabbox = document.getElementById("tabbox");
      var tab = document.getElementById(tabName);

      if (tabbox && tab) {
        tabbox.selectedTab = tab;
      }
    }
  }
}

function makeDocument(page, root)
{
  var title = page.title;
  var url   = page.URL;
  var lastModified;
  var lastMod = page.lastModified // get string of last modified date
  var lastModdate = Date.parse(lastMod)   // convert modified string to date
  if (lastModdate) {
    var date = new Date(lastModdate);
    try {
      var dateService = Components.classes[nsScriptableDateFormat_CONTRACTID]
        .getService(nsIScriptableDateFormat);
      lastModified =  dateService.FormatDateTime(
                              "", dateService.dateFormatLong,
                              dateService.timeFormatSeconds,
                              date.getFullYear(), date.getMonth()+1,
                              date.getDate(), date.getHours(),
                              date.getMinutes(), date.getSeconds());
    } catch(e) {
      lastModified = lastMod;
    }
  } else {
    try {
      var pageInfoBundle = document.getElementById("pageinfobundle");
      lastModified = pageInfoBundle.getString("unknown");
    } catch(e) {
      lastModified = "Unknown";
    }
  }

  document.getElementById("titletext").setAttribute("value", title);
  document.getElementById("urltext").setAttribute("value", url);
  document.getElementById("lastmodifiedtext").setAttribute("value", lastModified);
}

function makeFormTree(page, root)
{
  var formList = page.forms;
  if (formList.length == 0) return false;

  var treeChildren = document.getElementById("formChildren");

  for (var i = 0; i < formList.length; i++)
  {
    var treeItem = document.createElement("treeitem");
    var treeRowElem = treeItem.appendChild(document.createElement("treerow"));

    var treeCellElem = treeRowElem.appendChild(document.createElement("treecell"));
    treeCellElem.setAttribute("label", formList[i].action);

    treeCellElem = treeRowElem.appendChild(document.createElement("treecell"));
    treeCellElem.setAttribute("label", formList[i].method);

    treeCellElem = treeRowElem.appendChild(document.createElement("treecell"));
    treeCellElem.setAttribute("label", formList[i].name);

    treeChildren.appendChild(treeItem);
  }

  return true;
}

function makeImageTree(page, root)
{
  var imgList = page.images;
  if (imgList.length == 0) return false;

  var treeChildren = document.getElementById("imagesChildren");

  for (var i = 0; i < imgList.length; i++)
  {
    var treeItem = document.createElement("treeitem");
    treeItem.setAttribute("container", "true");
    treeItem.setAttribute("parent", "true");

    var treeRowElem = treeItem.appendChild(document.createElement("treerow"));

    var treeCellElem = treeRowElem.appendChild(document.createElement("treecell"));
    treeCellElem.setAttribute("label", imgList[i].src);

    treeCellElem = treeRowElem.appendChild(document.createElement("treecell"));
    treeCellElem.setAttribute("label", imgList[i].width);

    treeCellElem = treeRowElem.appendChild(document.createElement("treecell"));
    treeCellElem.setAttribute("label", imgList[i].height);

    treeCellElem = treeRowElem.appendChild(document.createElement("treecell"));
    treeCellElem.setAttribute("label", imgList[i].alt);

    treeChildren.appendChild(treeItem);
  }

  return true;
}

function onImageSelect()
{
  var imageFrame = document.getElementById("imageFrame");
  imageFrame.setAttribute("src", "about:blank");

  var tree = document.getElementById("imageTree");

  if (tree.selectedItems.length == 1)
  {
    var clickedRow = tree.selectedItems[0].firstChild;
    var firstCell = clickedRow.firstChild;
    var imageUrl = firstCell.getAttribute("label");

    /* The image has to be placed after a setTimeout because of bug 62517. */
    setTimeout(placeImage, 0, imageFrame, imageUrl);
  }
}

function placeImage(imageFrame, imageUrl)
{
  var imageDoc = imageFrame.contentDocument;
  var imageNode = imageDoc.createElement("img");
  imageNode.setAttribute("src", imageUrl);

  imageDoc.documentElement.appendChild(imageNode);
}

function BrowserClose()
{
  window.close();
}

