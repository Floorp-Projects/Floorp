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
 */

function onLoadPageInfo()
{
  var page = window.opener.frames[0].document;
  //var page = window.frames.content.document;
  var root = document.getElementById("cont");

  makeDocument(page, root);

  var formTreeHolder = document.getElementById("formTreeContainer");
  var hasForm = makeFormTree(page, formTreeHolder);
  if (hasForm)
  {
    formTreeHolder.removeAttribute("collapsed");
  }

  var imageTreeHolder = document.getElementById("imageTreeContainer");
  var hasImages = makeImageTree(page, imageTreeHolder);
  if (hasImages)
  {
    imageTreeHolder.removeAttribute("collapsed");
  }

  if (hasForm && hasImages)
  {
    document.getElementById("formImageSplitter").removeAttribute("hidden");
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
    treeCellElem.setAttribute("value", formList[i].action);

    treeCellElem = treeRowElem.appendChild(document.createElement("treecell"));
    treeCellElem.setAttribute("value", formList[i].method);

    treeCellElem = treeRowElem.appendChild(document.createElement("treecell"));
    treeCellElem.setAttribute("value", formList[i].name);
	  
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
    treeCellElem.setAttribute("value", imgList[i].src);

    treeCellElem = treeRowElem.appendChild(document.createElement("treecell"));
    treeCellElem.setAttribute("value", imgList[i].width);

    treeCellElem = treeRowElem.appendChild(document.createElement("treecell"));
    treeCellElem.setAttribute("value", imgList[i].height);

    treeCellElem = treeRowElem.appendChild(document.createElement("treecell"));
    treeCellElem.setAttribute("value", imgList[i].alt);
	  
    treeChildren.appendChild(treeItem);
  }

  return true;
}

function onImageSelect()
{
  var tree = document.getElementById("imageTree");
  var imageFrame = document.getElementById("imageFrame");
  
  if (tree.selectedItems.length == 1)
  {
    var clickedRow = tree.selectedItems[0].firstChild;
    var firstCell = clickedRow.firstChild;
    var imageUrl = firstCell.getAttribute("value");
    imageFrame.setAttribute("src", imageUrl);
  }
}

function BrowserClose()
{
  window.close();
}

