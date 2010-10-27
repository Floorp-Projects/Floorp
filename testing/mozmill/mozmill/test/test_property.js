/* * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is MozMill Test code.
 *
 * The Initial Developer of the Original Code is Fidesfit.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  M.-A. Darche  (Original Author)
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
 * **** END LICENSE BLOCK ***** */

Components.utils.import('resource://mozmill/modules/jum.js');

var setupModule = function(module) {
  controller = mozmill.getBrowserController();
};

var testProperty = function() {
  var res;

  var menu_item = new elementslib.ID(controller.window.document, 'file-menu');
  controller.click(menu_item);

  var new_tab_menu_item = new elementslib.ID(controller.window.document,
                                             'menu_newNavigatorTab');
  res = controller.assertProperty(new_tab_menu_item, 'command', 'cmd_newNavigatorTab');
  assertEquals(true, res);

  res = controller.assertProperty(new_tab_menu_item, 'command', '');
  assertEquals(false, res);
};

var testPropertyNotEquals = function() {
  var res;

  var menu_item = new elementslib.ID(controller.window.document, 'file-menu');
  controller.click(menu_item);

  var new_tab_menu_item = new elementslib.ID(controller.window.document,
                                             'menu_newNavigatorTab');
  res = controller.assertPropertyNotEquals(new_tab_menu_item, 'command', 'cmd_newNavigatorTab');
  assertEquals(false, res);

  res = controller.assertPropertyNotEquals(new_tab_menu_item, 'command', '');
  assertEquals(true, res);
};

