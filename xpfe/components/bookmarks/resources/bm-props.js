/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*

  Script for the bookmarks properties window

*/

var node;
var form_ids;
var bm_attrs;

function onLoad() {
  node = new Object;
  node = document.getElementById("properties_node");

  form_ids = new Array("name", "url", "shortcut",    "description");
  bm_attrs = new Array("Name", "url", "ShortcutURL", "Description");

  var element;
  var value;
  for (var ii=0; ii != form_ids.length; ii++) {
    element = document.getElementById(form_ids[ii]);
    value = node.getAttribute(bm_attrs[ii]);
    element.setAttribute("value", value);
  }
}
function commit() {
  var element;
  var value;
  for (var ii=0; ii != form_ids.length; ii++) {
    element = document.getElementById(form_ids[ii]);
    node.setAttribute(bm_attrs[ii], element.value);
  }
  closeDialog();
}

function cancel() {
  closeDialog();
}
function closeDialog() {
  // XXX This needs to be replaced with window.close()!
  var toolkitCore = XPAppCoresManager.Find("toolkitCore");
  if (!toolkitCore) {
    toolkitCore = new ToolkitCore();
    if (toolkitCore) {
      toolkitCore.Init("toolkitCore");
    }
  }
  if (toolkitCore) {
    toolkitCore.CloseWindow(window);
  }
}
