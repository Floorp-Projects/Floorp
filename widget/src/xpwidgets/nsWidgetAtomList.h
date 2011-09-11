/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Original Author: Mike Pinkerton (pinkerton@netscape.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/******

  This file contains the list of all Widget nsIAtoms and their values
  
  It is designed to be used as inline input to nsWidgetAtoms.cpp *only*
  through the magic of C preprocessing.

  All entries must be enclosed in the macro WIDGET_ATOM which will have cruel
  and unusual things done to it

  It is recommended (but not strictly necessary) to keep all entries
  in alphabetical order

  The first argument to WIDGET_ATOM is the C++ identifier of the atom
  The second argument is the string value of the atom

 ******/

WIDGET_ATOM(accesskey, "accesskey") // The shortcut key for a menu or menu item
WIDGET_ATOM(active, "active")
WIDGET_ATOM(ascending, "ascending")
WIDGET_ATOM(autocheck, "autocheck")
WIDGET_ATOM(Back, "Back")
WIDGET_ATOM(Bookmarks, "Bookmarks")
WIDGET_ATOM(checkbox, "checkbox")
WIDGET_ATOM(checked, "checked")
WIDGET_ATOM(_class, "class")
WIDGET_ATOM(collapsed, "collapsed")
WIDGET_ATOM(comboboxControlFrame, "ComboboxControlFrame")
WIDGET_ATOM(command, "command")
WIDGET_ATOM(curpos, "curpos")
WIDGET_ATOM(_default, "default")
WIDGET_ATOM(descending, "descending")
WIDGET_ATOM(dir, "dir")
WIDGET_ATOM(disabled, "disabled")
WIDGET_ATOM(Clear, "Clear") // AppCommand to return to a previous state
WIDGET_ATOM(_false, "false")
WIDGET_ATOM(focused, "focused")
WIDGET_ATOM(Forward, "Forward")
WIDGET_ATOM(Home, "Home")
WIDGET_ATOM(hidden, "hidden")
WIDGET_ATOM(hidecolumnpicker, "hidecolumnpicker")
WIDGET_ATOM(horizontal, "horizontal")
WIDGET_ATOM(vertical, "vertical")
WIDGET_ATOM(id, "id")
WIDGET_ATOM(image, "image")
WIDGET_ATOM(input, "input")
WIDGET_ATOM(indeterminate, "indeterminate")
WIDGET_ATOM(key, "key") // The key element / attribute
WIDGET_ATOM(label, "label")
WIDGET_ATOM(max, "max")
WIDGET_ATOM(maxpos, "maxpos")
WIDGET_ATOM(Menu, "Menu") // AppCommand to open a menu
WIDGET_ATOM(menu, "menu") // Represents an XP menu
WIDGET_ATOM(menuitem, "menuitem") // Represents an XP menu item
WIDGET_ATOM(menupopup, "menupopup") // The XP menu's children.
WIDGET_ATOM(menuseparator, "menuseparator")  // Divider between menu items
WIDGET_ATOM(menuFrame, "MenuFrame")
WIDGET_ATOM(minpos, "minpos")
WIDGET_ATOM(mode, "mode")
WIDGET_ATOM(modifiers, "modifiers") // The modifiers attribute
WIDGET_ATOM(mozmenuactive, "_moz-menuactive")
WIDGET_ATOM(name, "name")
WIDGET_ATOM(onAppCommand, "onAppCommand")
WIDGET_ATOM(open, "open") // Whether or not a menu, tree, etc. is open
WIDGET_ATOM(orient, "orient")
WIDGET_ATOM(pageincrement, "pageincrement")
WIDGET_ATOM(parentfocused, "parentfocused")
WIDGET_ATOM(progress, "progress")
WIDGET_ATOM(radio, "radio")
WIDGET_ATOM(readonly, "readonly")
WIDGET_ATOM(Reload, "Reload")
WIDGET_ATOM(richlistitem, "richlistitem")
WIDGET_ATOM(sbattr, "sbattr")
WIDGET_ATOM(scrollFrame, "ScrollFrame")
WIDGET_ATOM(scrollbarFrame, "ScrollbarFrame")
WIDGET_ATOM(scrollbarDownBottom, "scrollbar-down-bottom")
WIDGET_ATOM(scrollbarDownTop, "scrollbar-down-top")
WIDGET_ATOM(scrollbarUpBottom, "scrollbar-up-bottom")
WIDGET_ATOM(scrollbarUpTop, "scrollbar-up-top")
WIDGET_ATOM(Search, "Search")
WIDGET_ATOM(selected, "selected")
WIDGET_ATOM(sortdirection, "sortDirection")
WIDGET_ATOM(state, "state")
WIDGET_ATOM(statusbar, "statusbar")
WIDGET_ATOM(Stop, "Stop")
WIDGET_ATOM(_true, "true")
WIDGET_ATOM(tab, "tab")
WIDGET_ATOM(toolbar, "toolbar")
WIDGET_ATOM(toolbox, "toolbox")
WIDGET_ATOM(tree, "tree")
WIDGET_ATOM(treecolpicker, "treecolpicker")
WIDGET_ATOM(type, "type")
WIDGET_ATOM(value, "value")
WIDGET_ATOM(VolumeUp, "VolumeUp")
WIDGET_ATOM(VolumeDown, "VolumeDown")

