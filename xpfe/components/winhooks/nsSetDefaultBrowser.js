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
 * The Original Code is Mozilla Default Browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bill Law  <law@netscape.com>
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

/* This file implements the nsICmdLineHandler interface.  See nsICmdLineHandler.idl
 * at http://lxr.mozilla.org/seamonkey/source/xpfe/appshell/public/nsICmdLineHandler.idl.
 *
 * This component handles the startup command line argument of the form:
 *   -setDefaultBrowser
 * by making the current executable the "default browser."  It accomplishes
 * that via use of the nsIWindowsHooks interface (see implementation below).
 *
 * The module is registered under the contractid
 *   "@mozilla.org/commandlinehandler/general-startup;1?type=setDefaultBrowser"
 *
 * The implementation consists of a JavaScript "class" named nsKillAll,
 * comprised of:
 *   - a JS constructor function
 *   - a prototype providing all the interface methods and implementation stuff
 *
 * In addition, this file implements an nsIModule object that registers the
 * nsSetDefaultBrowser component.
 */

/* ctor
 */
function nsSetDefaultBrowser() {
}

nsSetDefaultBrowser.prototype = {

    // nsICmdLineHandler interface
    get commandLineArgument() { throw Components.results.NS_ERROR_NOT_IMPLEMENTED; },
    get prefNameForStartup()  { throw Components.results.NS_ERROR_NOT_IMPLEMENTED; },

    get chromeUrlForTask()    {
      // First, get winhooks service.
      var winHooks = Components.classes[ "@mozilla.org/winhooks;1" ]
                        .getService( Components.interfaces.nsIWindowsHooks );

      // Next, extract current settings (these are what the user
      // had previously checked on the Advanced/System pref panel).
      var settings = winHooks.settings;

      // Now, turn on all "default browser" settings.
      settings.isHandlingHTTP  = true;
      settings.isHandlingHTTPS = true;
      settings.isHandlingFTP   = true;
      settings.isHandlingHTML  = true;
      settings.isHandlingXHTML = true;
      settings.isHandlingXML   = true;

      // Finally, apply the (new) settings.
      winHooks.settings = settings;

      // Now, get the cmd line service.
      var cmdLineService = Components.classes[ "@mozilla.org/appshell/commandLineService;1" ]
                              .getService( Components.interfaces.nsICmdLineService );

      // See if "-setDefaultBrowser" was specified.  The value will be "1" if
      // -setDefaultBrowser is in the service's list of cmd line arguments, and
      // null otherwise.  -setDefaultBrowser will only be in the service's
      // arg list if the application was not already running.  That's because
      // if it was already running, then the service reflects the arguments
      // that were specified when *that* process was started, *not* the ones
      // passed via IPC from the second instance.
      var option = cmdLineService.getCmdLineValue( "-setDefaultBrowser" );
      if (!option) {
        // Already running, so we don't have to worry about opening
        // another window, etc.
        throw Components.results.NS_ERROR_NOT_AVAILABLE;
      }

      // Return URL for dummy window that will auto-close.  We do this rather
      // than throw NS_ERROR_NOT_AVAILABLE, which *should* work, because it
      // seems that if we don't open a window, we get a crash when trying to
      // release this (or some other) JS component during XPCOM shutdown.
      return "chrome://global/content/dummyWindow.xul";
    },

    get helpText()            { throw Components.results.NS_ERROR_NOT_IMPLEMENTED; },
    get handlesArgs()         { return false; },
    get defaultArgs()         { throw Components.results.NS_ERROR_NOT_IMPLEMENTED; },
    get openWindowWithArgs()  { throw Components.results.NS_ERROR_NOT_IMPLEMENTED; },

    // nsISupports interface

    // This "class" supports nsICmdLineHandler and nsISupports.
    QueryInterface: function (iid) {
        if (iid.equals(Components.interfaces.nsICmdLineHandler) ||
            iid.equals(Components.interfaces.nsISupports))
            return this;

        Components.returnCode = Components.results.NS_ERROR_NO_INTERFACE;
        return null;
    },

    // This Component's module implementation.  All the code below is used to get this
    // component registered and accessible via XPCOM.
    module: {
        // registerSelf: Register this component.
        registerSelf: function (compMgr, fileSpec, location, type) {
            var compReg = compMgr.QueryInterface( Components.interfaces.nsIComponentRegistrar );
            compReg.registerFactoryLocation( this.cid,
                                             "Default Browser Component",
                                             this.contractId,
                                             fileSpec,
                                             location,
                                             type );
        },

        // getClassObject: Return this component's factory object.
        getClassObject: function (compMgr, cid, iid) {
            if (!cid.equals(this.cid))
                throw Components.results.NS_ERROR_NO_INTERFACE;

            if (!iid.equals(Components.interfaces.nsIFactory))
                throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

            return this.factory;
        },

        /* CID for this class */
        cid: Components.ID("{C66E05DC-509C-4972-A1F2-EE5AC34B9800}"),

        /* Contract ID for this class */
        contractId: "@mozilla.org/commandlinehandler/general-startup;1?type=setDefaultBrowser",

        /* factory object */
        factory: {
            // createInstance: Return a new nsSetDefaultBrowser object.
            createInstance: function (outer, iid) {
                if (outer != null)
                    throw Components.results.NS_ERROR_NO_AGGREGATION;

                return (new nsSetDefaultBrowser()).QueryInterface(iid);
            }
        },

        // canUnload: n/a (returns true)
        canUnload: function(compMgr) {
            return true;
        }
    }
}

// NSGetModule: Return the nsIModule object.
function NSGetModule(compMgr, fileSpec) {
    return nsSetDefaultBrowser.prototype.module;
}
