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
 * The Original Code is DOM Inspector.
 *
 * The Initial Developer of the Original Code is
 * Christopher A. Aillon <christopher@aillon.com>.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Christopher A. Aillon <christopher@aillon.com>
 *   L. David Baron, Mozilla Corporation <dbaron@dbaron.org> (modified for reftest)
 *   Vladimir Vukicevic, Mozilla Corporation <dbaron@dbaron.org> (modified for tp)
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

// This only implements nsICommandLineHandler, since it needs
// to handle multiple arguments.

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "categoryManager",
                                   "@mozilla.org/categorymanager;1",
                                   "nsICategoryManager");

const Cm = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);

const CATMAN_CONTRACTID         = "@mozilla.org/categorymanager;1";

const CATEGORY_NAME = "command-line-handler";
const CATEGORY_ENTRY = "m-tp";

function PageLoaderCmdLine() {}
PageLoaderCmdLine.prototype =
{
  factory: XPCOMUtils._getFactory(PageLoaderCmdLine),
  classDescription: "Loads pages. Tests them.",
  classID:          Components.ID("{8AF052F5-8EFE-4359-8266-E16498A82E8B}"),
  contractID:       "@mozilla.org/commandlinehandler/general-startup;1?type=tp",
  QueryInterface:   ChromeUtils.generateQI([Ci.nsICommandLineHandler]),

  register() {
    Cm.registerFactory(this.classID, this.classDescription,
                       this.contractID, this.factory);

    categoryManager.addCategoryEntry(CATEGORY_NAME, CATEGORY_ENTRY,
                                     this.contractID, false, true);
  },

  unregister() {
    categoryManager.deleteCategoryEntry(CATEGORY_NAME, CATEGORY_ENTRY,
                                        this.contractID, false);

    Cm.unregisterFactory(this.classID, this.factory);
  },

  /* nsICommandLineHandler */
  handle: function handler_handle(cmdLine) {
    var args = {};

    var tpmanifest = Services.prefs.getCharPref("talos.tpmanifest", null);
    if (tpmanifest == null) {
      // pageloader tests as well as startup tests use this pageloader add-on; pageloader tests
      // pass in a 'tpmanifest' which has been set to the talos.tpmanifest pref; startup tests
      // don't use 'tpmanifest' but pass in the test page which is forwarded onto the Firefox cmd
      // line. If this is a startup test exit here as we don't need a pageloader window etc.
      return;
    }

    let chromeURL = "chrome://pageloader/content/pageloader.xul";

    args.wrappedJSObject = args;

    Services.ww.openWindow(null, chromeURL, "_blank",
                            "chrome,dialog=no,all", args);

    // Don't pass command line to the default app processor
    cmdLine.preventDefault = true;
  },
};

function startup(data, reason) {
  PageLoaderCmdLine.prototype.register();
}

function shutdown(data, reason) {
  PageLoaderCmdLine.prototype.unregister();
}

function install(data, reason) {}
function uninstall(data, reason) {}
