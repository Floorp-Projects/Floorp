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

  var formTreeHolder = document.getElementById("formtreecontainer");
  var hasForm = makeFormTree(page, formTreeHolder);
  if (hasForm)
  {
    var formBox = document.getElementById("formtreecontainer");
    formBox.removeAttribute("collapsed");
  }

  var imageTreeHolder = document.getElementById("imagetreecontainer");
  var hasimages = makeImageTree(page, imageTreeHolder);
  if (hasimages)
  {
    var imageBox = document.getElementById("image_items");
    imageBox.removeAttribute("collapsed");
  }
}

function makeDocument(page, root)
{
  var title = page.title;
  var url   = page.URL;
  var lastmodified;
  var lastmod = page.lastModified // get string of last modified date
  var lastmoddate = Date.parse(lastmod)   // convert modified string to date
  if(lastmoddate == 0){               // unknown date (or January 1, 1970 GMT)
    lastmodified = "Unknown";
    } else {
    lastmodified = lastmod;
  }

  document.getElementById("titletext").setAttribute("value", title);
  document.getElementById("urltext").setAttribute("value", url);
  document.getElementById("lastmodifiedtext").setAttribute("value", lastmodified);
}

function makeFormTree(page, root)
{
  var form_list = page.forms;
  if (form_list.length == 0) return false;

  var treechildren = document.getElementById("formchildren");
  
  for (var i = 0; i < form_list.length; i++)
  {
	  var treeitem = document.createElement("treeitem");
	  var treerow_elem = treeitem.appendChild(document.createElement("treerow"));

	  var treecell_elem = treerow_elem.appendChild(document.createElement("treecell"));
	  treecell_elem.setAttribute("value", form_list[i].action);

	  treecell_elem = treerow_elem.appendChild(document.createElement("treecell"));
	  treecell_elem.setAttribute("value", form_list[i].method);

	  treecell_elem = treerow_elem.appendChild(document.createElement("treecell"));
	  treecell_elem.setAttribute("value", form_list[i].name);
	  
	  treechildren.appendChild(treeitem);
  }
  
  return true;
}

function makeImageTree(page, root)
{
  var img_list = page.images;
  if (img_list.length == 0) return false;

  var treechildren = document.getElementById("imageschildren");

  for (var i = 0; i < img_list.length; i++) 
  {
	  var treeitem = document.createElement("treeitem");
	  treeitem.setAttribute("container", "true");
	  treeitem.setAttribute("parent", "true");
	  
	  var treerow_elem = treeitem.appendChild(document.createElement("treerow"));

	  var treecell_elem = treerow_elem.appendChild(document.createElement("treecell"));
	  treecell_elem.setAttribute("value", img_list[i].src);

	  treecell_elem = treerow_elem.appendChild(document.createElement("treecell"));
	  treecell_elem.setAttribute("value", img_list[i].width);

	  treecell_elem = treerow_elem.appendChild(document.createElement("treecell"));
	  treecell_elem.setAttribute("value", img_list[i].height);

	  treecell_elem = treerow_elem.appendChild(document.createElement("treecell"));
	  treecell_elem.setAttribute("value", img_list[i].alt);
	  
	  treechildren.appendChild(treeitem);
  }

  return true;
}

function onImageSelect()
{
  var tree = document.getElementById("images_tree");
  var imageFrame = document.getElementById("image_frame");
  
  if (tree.selectedItems.length == 1)
  {
    var  clickedRow = tree.selectedItems[0].firstChild;
    var  firstCell = clickedRow.firstChild;
    var  imageUrl = firstCell.getAttribute("value");
    imageFrame.setAttribute("src", imageUrl);
  }
}


function BrowserClose()
{
  window.close();
}
