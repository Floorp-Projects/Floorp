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
 * The Original Code is MozMill Test code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
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
 * ***** END LICENSE BLOCK ***** */

var RELATIVE_ROOT = '../../shared-modules';
var MODULE_REQUIRES = ['SessionStoreAPI'];

const gDelay = 0;
const gTimeout = 5000;

var setupModule = function(module)
{
  module.controller = mozmill.getBrowserController();

  module.session = new SessionStoreAPI.aboutSessionRestore(controller);
}

var testAboutSessionRestoreErrorPage = function()
{
  controller.open("about:sessionrestore");
  controller.sleep(400);

  // Test the list
  var list = session.getElement({type: "tabList"});
  var windows = session.getWindows();

  for (var ii = 0; ii < windows.length; ii++) {
    var window = windows[ii];
    var tabs = session.getTabs(window);

    for (var jj = 0; jj < tabs.length; jj++) {
      var tab = tabs[jj];

      if (jj == 0) {
        session.toggleRestoreState(tab);
      }
    }
  }

  // Test the buttons
  var button = session.getElement({type: "button_restoreSession"});
  controller.assertJS("subject.getAttribute('oncommand') == 'restoreSession();'", button.getNode());

  var button = session.getElement({type: "button_newSession"});
  controller.assertJS("subject.getAttribute('oncommand') == 'startNewSession();'", button.getNode());

  controller.keypress(null, "t", {accelKey: true});
  controller.open("http://www.google.com");
  controller.waitForPageLoad();
  controller.keypress(null, "w", {accelKey: true});

  SessionStoreAPI.undoClosedTab(controller, {type: "shortcut"});
  
}
