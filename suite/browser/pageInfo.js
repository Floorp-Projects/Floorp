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

function onLoadPageInfo() {
  var page = window.opener.frames[0].document;
  //var page = window.frames.content.document;
  var root = document.getElementById("cont");

  makeDocument(page, root);
  var hasimages = makeImageTree(page, root);
  makeFormTree(page, root);

  if(hasimages) {
    var iframe = document.createElement("iframe");
    iframe.setAttribute("name", "view");
    iframe.setAttribute("src", "about:blank");
    iframe.setAttribute("flex", "4");
    root.appendChild(iframe);
  }
}

function makeDocument(page, root) {
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

  document.getElementById("titletext").appendChild(document.createTextNode(title));
  document.getElementById("urltext").appendChild(document.createTextNode(url));
  document.getElementById("lastmodifiedtext").appendChild(document.createTextNode(lastmodified));
}

function makeFormTree(page, root) {
  var form_list = page.forms;
  for(i=0; i < form_list.length; i++) {
    if(i == 0) {
      var tree = document.createElement("tree");
      tree.setAttribute("flex", "4");
      var treehead = tree.appendChild(document.createElement("treehead"));
      var treerow = treehead.appendChild(document.createElement("treerow"));

      var treecolgroup = tree.appendChild(document.createElement("treecolgroup"));
      var treecol = treecolgroup.appendChild(document.createElement("treecol"));
      treecol.setAttribute("width", "2");
      var treecol = treecolgroup.appendChild(document.createElement("treecol"));
      treecol.setAttribute("width", "1");
      var treecol = treecolgroup.appendChild(document.createElement("treecol"));
      treecol.setAttribute("width", "1");

      var treecell = treerow.appendChild(document.createElement("treecell"));
      treecell.setAttribute("value", "Form Action");
      treecell = treerow.appendChild(document.createElement("treecell"));
      treecell.setAttribute("value", "Method");
      treecell = treerow.appendChild(document.createElement("treecell"));
      treecell.setAttribute("value", "Name");

      var treechildren = document.createElement("treechildren");
      tree.appendChild(treechildren);

      root.appendChild(tree);
    }
  var treeitem = treechildren.appendChild(document.createElement("treeitem"));
  var treerow_elem = treeitem.appendChild(document.createElement("treerow"));

  var treecell_elem = treerow_elem.appendChild(document.createElement("treecell"));
  treecell_elem.setAttribute("value", form_list[i].action);

  treecell_elem = treerow_elem.appendChild(document.createElement("treecell"));
  treecell_elem.setAttribute("value", form_list[i].method);

  treecell_elem = treerow_elem.appendChild(document.createElement("treecell"));
  treecell_elem.setAttribute("value", form_list[i].name);
  tree.appendChild(treechildren);

  }
}

function makeImageTree(page, root) {
  var img_list = page.images;
  var ret = false;

  for(i=0; i < img_list.length; i++) {
    if(i == 0) {
      var tree = document.createElement("tree");
      tree.setAttribute("flex", "1");

      var treehead = tree.appendChild(document.createElement("treehead"));
      var treerow = treehead.appendChild(document.createElement("treerow"));

      var treecolgroup = tree.appendChild(document.createElement("treecolgroup"));
      var treecol = treecolgroup.appendChild(document.createElement("treecol"));
      treecol.setAttribute("width", "8");
      var treecol = treecolgroup.appendChild(document.createElement("treecol"));
      treecol.setAttribute("width", "2");
      var treecol = treecolgroup.appendChild(document.createElement("treecol"));
      treecol.setAttribute("width", "2");
      var treecol = treecolgroup.appendChild(document.createElement("treecol"));
      treecol.setAttribute("width", "4");

      var treecell = treerow.appendChild(document.createElement("treecell"));
      treecell.setAttribute("value", "Image URLs");
      treecell = treerow.appendChild(document.createElement("treecell"));
      treecell.setAttribute("value", "Width");
      treecell = treerow.appendChild(document.createElement("treecell"));
      treecell.setAttribute("value", "Height");
      treecell = treerow.appendChild(document.createElement("treecell"));
      treecell.setAttribute("value", "Alt Text");

      var treechildren = document.createElement("treechildren");
      tree.appendChild(treechildren);

      root.appendChild(tree);
  }
  var treeitem = treechildren.appendChild(document.createElement("treeitem"));
  var treerow_elem = treeitem.appendChild(document.createElement("treerow"));

  var treecell_elem = treerow_elem.appendChild(document.createElement("treecell"));
  treecell_elem.setAttribute("value", img_list[i].src);
  treecell_elem.addEventListener("click", openImage, true);

  treecell_elem = treerow_elem.appendChild(document.createElement("treecell"));
  treecell_elem.setAttribute("value", img_list[i].width);

  treecell_elem = treerow_elem.appendChild(document.createElement("treecell"));
  treecell_elem.setAttribute("value", img_list[i].height);

  treecell_elem = treerow_elem.appendChild(document.createElement("treecell"));
  treecell_elem.setAttribute("value", img_list[i].alt);

  tree.appendChild(treechildren);
  ret = true;
  }

  return ret;
}

function openImage(e) { window.frames.view.document.location.href = e.target.getAttribute("value"); }


