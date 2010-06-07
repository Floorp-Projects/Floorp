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
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Henrik Skupin <hskupin@mozilla.com>
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

var RELATIVE_ROOT = '../../shared-modules';
var MODULE_REQUIRES = ['PrivateBrowsingAPI', 'ModalDialogAPI'];

const gTimeout = 5000;

var setupModule = function(module)
{
  controller = mozmill.getBrowserController();
}

/**
* Test that we do not crash when removing a cookie
*/
var testCrashRemoveCookieAfterPrivateBrowsingMode = function()
{
  var pb = new PrivateBrowsingAPI.privateBrowsing(controller);

  pb.enabled = false;
  pb.showPrompt = false;

  pb.start();
  pb.stop();

  pb.showPrompt = true;
  pb.enabled = false;

  // Call clear recent history dialog and clear all form history
  var md = new ModalDialogAPI.modalDialog(clearHistoryHandler);
  md.start();

  controller.click(new elementslib.Elem(controller.menus["tools-menu"].sanitizeItem));
}

/**
 * Accesses the clear recent history dialog and accepts the default options to clear
 *
 * @param {MozMillController} controller
 *        MozMillController of the window to operate on
 */
var clearHistoryHandler = function(controller)
{
  var clearButton = new elementslib.Lookup(controller.window.document, '/id("SanitizeDialog")/anon({"anonid":"dlg-buttons"})/{"dlgtype":"accept"}');
  controller.waitThenClick(clearButton);
}
