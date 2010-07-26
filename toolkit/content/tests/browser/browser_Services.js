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
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Gavin Sharp <gavin@gavinsharp.com> (original author)
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

function test() {
  /** Tests for Services.jsm (Bug 512784) **/
  ok(Services, "Services object exists");
  checkServices();
}

function checkService(service, interface) {
  ok(service in Services, "Services." + service + " exists");
  ok(Services[service] instanceof interface, "Services." + service + " is an " + interface);
}

function checkServices() {
  checkService("prefs", Ci.nsIPrefBranch2);
  checkService("prefs", Ci.nsIPrefService);
  checkService("contentPrefs", Ci.nsIContentPrefService);
  checkService("wm", Ci.nsIWindowMediator);
  checkService("perms", Ci.nsIPermissionManager);
  checkService("io", Ci.nsIIOService);
  checkService("io", Ci.nsIIOService2);
  checkService("appinfo", Ci.nsIXULAppInfo);
  checkService("appinfo", Ci.nsIXULRuntime);
  checkService("dirsvc", Ci.nsIDirectoryService);
  checkService("dirsvc", Ci.nsIProperties);
  checkService("prompt", Ci.nsIPromptService);
  if ("nsIBrowserSearchService" in Ci)
    checkService("search", Ci.nsIBrowserSearchService);
  checkService("storage", Ci.mozIStorageService);
  checkService("vc", Ci.nsIVersionComparator);
  checkService("locale", Ci.nsILocaleService);
  checkService("scriptloader", Ci.mozIJSSubScriptLoader);
  checkService("ww", Ci.nsIWindowWatcher);
  checkService("tm", Ci.nsIThreadManager);
  checkService("strings", Ci.nsIStringBundleService);
  checkService("urlFormatter", Ci.nsIURLFormatter);
}
