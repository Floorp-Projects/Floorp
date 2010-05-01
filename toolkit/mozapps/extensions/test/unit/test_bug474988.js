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
 * Dave Townsend <dtownsend@oxymoronical.com>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
 * the terms of any one of the MPL, the GPL or the LGPL
 *
 * ***** END LICENSE BLOCK *****
 */

var installListenerA = {};
var installListenerB = {};
var installListenerC = {};
var installListenerD = {};
var installListenerE = {};

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "2", "1.9");
  startupEM();

  // Add some listeners
  do_check_eq(gEM.addInstallListener(installListenerA), 0);
  do_check_eq(gEM.addInstallListener(installListenerB), 1);
  do_check_eq(gEM.addInstallListener(installListenerC), 2);
  // Now stack is ABC

  // Should get back the same id if adding the same listener twice
  do_check_eq(gEM.addInstallListener(installListenerA), 0);
  do_check_eq(gEM.addInstallListener(installListenerB), 1);
  do_check_eq(gEM.addInstallListener(installListenerC), 2);
  // Stack is still ABC

  // Removing a listener shouldn't affect the id of the others
  gEM.removeInstallListenerAt(0);
  do_check_eq(gEM.addInstallListener(installListenerB), 1);
  do_check_eq(gEM.addInstallListener(installListenerC), 2);
  // Stack is .BC

  // Adding a new listener should still give a higher id than previously
  do_check_eq(gEM.addInstallListener(installListenerD), 3);
  // Stack is .BCD

  // Removing and re-adding the highest listener should give the same result.
  gEM.removeInstallListenerAt(3);
  do_check_eq(gEM.addInstallListener(installListenerE), 3);
  // Stack is .BCE

  do_check_eq(gEM.addInstallListener(installListenerD), 4);
  gEM.removeInstallListenerAt(3);
  do_check_eq(gEM.addInstallListener(installListenerE), 5);
  // Stack is .BC.DE

  // Stack should shorten down to the last listener
  gEM.removeInstallListenerAt(4);
  gEM.removeInstallListenerAt(5);
  do_check_eq(gEM.addInstallListener(installListenerD), 3);
  // Stack is .BCD

  // Stack should get down to 0
  gEM.removeInstallListenerAt(1);
  gEM.removeInstallListenerAt(2);
  gEM.removeInstallListenerAt(3);
  do_check_eq(gEM.addInstallListener(installListenerE), 0);
  gEM.removeInstallListenerAt(0);
  // Stack is empty
}
