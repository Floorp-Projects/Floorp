/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Alec Flett <alecf@netscape.com>
 */

var elements = [];
var numItems;
var list;

function selectDialogOnLoad() {
  dump("selectDialogOnLoad \n");
  doSetOKCancel( commonDialogOnOK, commonDialogOnCancel );

  param = window.arguments[0].QueryInterface( Components.interfaces.nsIDialogParamBlock  );
  if( !param )
  dump( " error getting param block interface\n" );

  var messageText = param.GetString( 1 );
  dump("message: "+ messageText +"\n");
  {
    var messageFragment;

    // Let the caller use "\n" to cause breaks
    // Translate these into <br> tags
    var messageParent = (document.getElementById("info.txt"));
    done = false;
    while (!done) {
    breakIndex =   messageText.indexOf('\n');
      if (breakIndex == 0) {
        // Ignore break at the first character
        messageText = messageText.slice(1);
        dump("Found break at begining\n");
        messageFragment = "";
      } else if (breakIndex > 0) {
        // The fragment up to the break
        messageFragment = messageText.slice(0, breakIndex);

        // Chop off fragment we just found from remaining string
        messageText = messageText.slice(breakIndex+1);
      } else {
        // "\n" not found. We're done
        done = true;
        messageFragment = messageText;
      }
      messageParent.setAttribute("value", messageFragment);
    }
  }

  var windowTitle = param.GetString( 0 );
  dump("title: "+ windowTitle +"\n");
  window.title = windowTitle;

  list = document.getElementById("list");
  numItems = param.GetInt( 2 )

  for ( i = 2; i <= numItems+1; i++ ) {
    var newString = param.GetString( i );
    if (newString == "") {
      newString = "<>";
    }
    dump("setting string "+newString+"\n");
    elements[i-2] = AppendStringToTreelist(list, newString);
  }
  list.selectItem(elements[0]);

  // resize the window to the content
  window.sizeToContent();

  // Move to the right location
  moveToAlertPosition();
  param.SetInt(0, 1 );
  centerWindowOnScreen();
}

function commonDialogOnOK() {
  dump("commonDialogOnOK \n");
  for (var i=0; i<numItems; i++) {
    if (elements[i] == list.selectedItems[0]) {
      param.SetInt(2, i );
      break;
    }
  }
  param.SetInt(0, 0 );
  return true;
}

function commonDialogOnCancel() {
  dump("commonDialogOnCancel \n");
  for (var i=0; i<numItems; i++) {
    if (elements[i]) {
      param.SetInt(2, i );
      break;
    }
  }
  param.SetInt(0, 1 );
  return true;
}

// following routine should really be in a global utilities package

function AppendStringToTreelist(tree, string)
{
  if (tree)
  {
    var treechildren = document.getElementById('child');
    
    var treeitem = document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul", "treeitem");
    var treerow = document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul", "treerow");
    var treecell = document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul", "treecell");
    if (treeitem && treerow && treecell)
    {
      treecell.setAttribute("value", string);
      treerow.appendChild(treecell);
      treeitem.appendChild(treerow);
      treechildren.appendChild(treeitem)
      var len = Number(tree.getAttribute("length"));
      if (!len) len = -1;
      tree.setAttribute("length",len+1);
      return treeitem;
    }
  }
  return null;
}