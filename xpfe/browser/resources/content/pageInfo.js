/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): smorrison@gte.com
 *   Terry Hayes <thayes@netscape.com>
 */

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
  var page;
  if ((window.arguments.length >= 1) && window.arguments[0])
    page = window.arguments[0];
  else
    page = window.opener.frames[0].document;
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
  if ("arguments" in window) {
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

  lastModified = (lastModdate) ? lastMod : "Unknown";  // unknown date (or January 1, 1970 GMT)

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

