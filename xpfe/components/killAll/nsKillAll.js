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
 * The Original Code is Mozilla Kill All.
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
 *   -killAll
 * by closing all windows and shutting down turbo mode.
 *
 * The module is registered under the contractid
 *   "@mozilla.org/commandlinehandler/general-startup;1?type=killAll"
 *
 * The implementation consists of a JavaScript "class" named nsKillAll,
 * comprised of:
 *   - a JS constructor function
 *   - a prototype providing all the interface methods and implementation stuff
 *
 * In addition, this file implements an nsIModule object that registers the
 * nsKillAll component.
 */

/* ctor
 */
function nsKillAll() {
}

nsKillAll.prototype = {

    // nsICmdLineHandler interface
    get commandLineArgument() { throw Components.results.NS_ERROR_NOT_IMPLEMENTED; },
    get prefNameForStartup()  { throw Components.results.NS_ERROR_NOT_IMPLEMENTED; },

    get chromeUrlForTask()    {

      // turn off server mode

      var wasMozillaAlreadyRunning = false;
      var appShellService = Components.classes["@mozilla.org/appshell/appShellService;1"]
                           .getService(Components.interfaces.nsIAppShellService);
      var nativeAppSupport = {isServerMode: false};
      try {
        nativeAppSupport = appShellService.nativeAppSupport;
      } catch ( ex ) {
      }

      var originalServerMode = false;
      if (nativeAppSupport.isServerMode) {
        originalServerMode = true;
        wasMozillaAlreadyRunning = true;
        nativeAppSupport.isServerMode = false;
      }

      // stop all applications

      var gObserverService = Components.classes["@mozilla.org/observer-service;1"]
                             .getService(Components.interfaces.nsIObserverService);
      if (gObserverService) {
        try {
          gObserverService.notifyObservers(null, "quit-application", null);
        } catch (ex) {
          // dump("no observer found \n");
        }
      }

      // close down all windows

      var windowManager =
        Components.classes['@mozilla.org/appshell/window-mediator;1']
        .getService(Components.interfaces.nsIWindowMediator);
      var enumerator = windowManager.getEnumerator(null);
      while(enumerator.hasMoreElements()) {
        wasMozillaAlreadyRunning = true;
        var domWindow = enumerator.getNext();
        if (("tryToClose" in domWindow) && !domWindow.tryToClose()) {
          // user pressed cancel in response to dialog for closing an unsaved window
          nativeAppSupport.isServerMode = originalServerMode
          // Return NS_ERROR_ABORT to inform caller of the "cancel."
          throw Components.results.NS_ERROR_ABORT;
        }
        domWindow.close();
      }

      // exit without opening a new window

      if (wasMozillaAlreadyRunning) {
        // Need to exit appshell in this case.
        appShellService.quit(Components.interfaces.nsIAppShellService.eAttemptQuit);
      }

      // We throw NS_ERROR_NOT_AVAILABLE which will be interpreted by the caller
      // to mean that the command line handler is indicating that this should be a
      // silent command and not cause any windows to open.
      throw Components.results.NS_ERROR_NOT_AVAILABLE;
    },

    get helpText()            { throw Components.results.NS_ERROR_NOT_IMPLEMENTED; },
    get handlesArgs()         { throw Components.results.NS_ERROR_NOT_IMPLEMENTED; },
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
                                             "Kill All Component",
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
        cid: Components.ID("{F1F25940-4C8F-11d6-A651-0010A401EB10}"),

        /* Contract ID for this class */
        contractId: "@mozilla.org/commandlinehandler/general-startup;1?type=killAll",

        /* factory object */
        factory: {
            // createInstance: Return a new nsKillAll object.
            createInstance: function (outer, iid) {
                if (outer != null)
                    throw Components.results.NS_ERROR_NO_AGGREGATION;

                return (new nsKillAll()).QueryInterface(iid);
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
    return nsKillAll.prototype.module;
}
