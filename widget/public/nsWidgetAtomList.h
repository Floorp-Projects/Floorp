/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author: Mike Pinkerton (pinkerton@netscape.com)
 *
 * Contributor(s): 
 */

/******

  This file contains the list of all Widget nsIAtoms and their values
  
  It is designed to be used as inline input to nsWidgetAtoms.cpp *only*
  through the magic of C preprocessing.

  All entires must be enclosed in the macro WIDGET_ATOM which will have cruel
  and unusual things done to it

  It is recommended (but not strictly necessary) to keep all entries
  in alphabetical order

  The first argument to WIDGET_ATOM is the C++ identifier of the atom
  The second argument is the string value of the atom

 ******/

WIDGET_ATOM(collapsed, "collapsed")
WIDGET_ATOM(menuseparator, "menuseparator")  // Divider between menu items
WIDGET_ATOM(modifiers, "modifiers") // The modifiers attribute
WIDGET_ATOM(key, "key") // The key element / attribute
WIDGET_ATOM(command, "command")
WIDGET_ATOM(menu, "menu") // Represents an XP menu
WIDGET_ATOM(menuitem, "menuitem") // Represents an XP menu item
WIDGET_ATOM(open, "open") // Whether or not a menu, tree, etc. is open
WIDGET_ATOM(menupopup, "menupopup") // The XP menu's children.
WIDGET_ATOM(id, "id")
WIDGET_ATOM(accesskey, "accesskey") // The shortcut key for a menu or menu item

WIDGET_ATOM(name, "name")
WIDGET_ATOM(type, "type")
WIDGET_ATOM(checked, "checked")
WIDGET_ATOM(disabled, "disabled")
WIDGET_ATOM(label, "label")
WIDGET_ATOM(hidden, "hidden")

