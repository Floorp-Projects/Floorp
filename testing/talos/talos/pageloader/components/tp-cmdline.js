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

const TP_CMDLINE_CONTRACTID     = "@mozilla.org/commandlinehandler/general-startup;1?type=tp";
const TP_CMDLINE_CLSID          = Components.ID("{8AF052F5-8EFE-4359-8266-E16498A82E8B}");
const CATMAN_CONTRACTID         = "@mozilla.org/categorymanager;1";
const nsISupports               = Ci.nsISupports;

const nsICategoryManager        = Ci.nsICategoryManager;
const nsICommandLine            = Ci.nsICommandLine;
const nsICommandLineHandler     = Ci.nsICommandLineHandler;
const nsIComponentRegistrar     = Ci.nsIComponentRegistrar;
const nsISupportsString         = Ci.nsISupportsString;
const nsIWindowWatcher          = Ci.nsIWindowWatcher;

ChromeUtils.import("resource://gre/modules/Services.jsm");

function PageLoaderCmdLineHandler() {}
PageLoaderCmdLineHandler.prototype =
{
  /* nsISupports */
  QueryInterface: function handler_QI(iid) {
    if (iid.equals(nsISupports))
      return this;

    if (nsICommandLineHandler && iid.equals(nsICommandLineHandler))
      return this;

    throw Cr.NS_ERROR_NO_INTERFACE;
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


var PageLoaderCmdLineFactory =
{
  createInstance(outer, iid) {
    if (outer != null) {
      throw Cr.NS_ERROR_NO_AGGREGATION;
    }

    return new PageLoaderCmdLineHandler().QueryInterface(iid);
  }
};

function NSGetFactory(cid) {
  if (!cid.equals(TP_CMDLINE_CLSID))
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;

  return PageLoaderCmdLineFactory;
}

var PageLoaderCmdLineModule =
{
  registerSelf(compMgr, fileSpec, location, type) {
    compMgr = compMgr.QueryInterface(nsIComponentRegistrar);

    compMgr.registerFactoryLocation(TP_CMDLINE_CLSID,
                                    "PageLoader CommandLine Service",
                                    TP_CMDLINE_CONTRACTID,
                                    fileSpec,
                                    location,
                                    type);

    var catman = Cc[CATMAN_CONTRACTID].getService(nsICategoryManager);
    catman.addCategoryEntry("command-line-handler",
                            "m-tp",
                            TP_CMDLINE_CONTRACTID, true, true);
  },

  unregisterSelf(compMgr, fileSpec, location) {
    compMgr = compMgr.QueryInterface(nsIComponentRegistrar);

    compMgr.unregisterFactoryLocation(TP_CMDLINE_CLSID, fileSpec);
    var catman = Cc[CATMAN_CONTRACTID].getService(nsICategoryManager);
    catman.deleteCategoryEntry("command-line-handler",
                               "m-tp", true);
  },

  getClassObject(compMgr, cid, iid) {
    return NSGetFactory(cid);
  },

  canUnload(compMgr) {
    return true;
  }
};


function NSGetModule(compMgr, fileSpec) {
  return PageLoaderCmdLineModule;
}
