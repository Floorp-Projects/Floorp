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
 * The Original Code is the nsTryToClose component.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Michael Wu <flamingice@sourmilk.net>  (original author)
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

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

function TryToClose() {}

TryToClose.prototype = {
  observe: function (aSubject, aTopic, aData) {
    switch (aTopic) {
    case "app-startup":
      var obsService = Cc["@mozilla.org/observer-service;1"].
                       getService(Ci.nsIObserverService);
      obsService.addObserver(this, "quit-application-requested", true);
      break;
    case "quit-application-requested":
      var windowMediator = Cc['@mozilla.org/appshell/window-mediator;1'].
                           getService(Ci.nsIWindowMediator);
      var enumerator = windowMediator.getEnumerator(null);
      while (enumerator.hasMoreElements()) {
        var domWindow = enumerator.getNext();
        if (("tryToClose" in domWindow) && !domWindow.tryToClose()) {
          aSubject.QueryInterface(Ci.nsISupportsPRBool);
          aSubject.data = true;
          break;
        }
      }
      break;
    }
  },

  classDescription: "tryToClose Service",
  contractID: "@mozilla.org/appshell/trytoclose;1",
  classID: Components.ID("{b69155f4-a8bf-453d-8653-91d1456e1d3d}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),
  _xpcom_categories: [{ category: "app-startup", service: true }]
};

function NSGetModule(aCompMgr, aFileSpec) {
  return XPCOMUtils.generateModule([TryToClose]);
}
