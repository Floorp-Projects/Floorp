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
 * The Original Code is Mozilla Pref Resetter.
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
 *   -resetPref pref1,pref2,pref3
 * by resetting the user prefs pref1, pref2, and pref3 (an arbitrary list of pref names
 * separated by commas).
 *
 * The module is registered under the contractid
 *   "@mozilla.org/commandlinehandler/general-startup;1?type=resetPref"
 *
 * The implementation consists of a JavaScript "class" named nsResetPref,
 * comprised of:
 *   - a JS constructor function
 *   - a prototype providing all the interface methods and implementation stuff
 *
 * In addition, this file implements an nsIModule object that registers the
 * nsResetPref component.
 */

/* ctor
 */
function nsResetPref() {
}

nsResetPref.prototype = {
    // Turn this on to get debugging messages.
    debug: false,

    // nsICmdLineHandler interface
    get commandLineArgument() { throw Components.results.NS_ERROR_NOT_IMPLEMENTED; },
    get prefNameForStartup()  { throw Components.results.NS_ERROR_NOT_IMPLEMENTED; },
    get chromeUrlForTask()    {
        try {
            // We trust that this has been called during command-line handling during
            // startup from nsAppRunner.cpp.

            // We get the command line service and from that the -resetPref argument.
            var cmdLine  = Components.classes[ "@mozilla.org/appshell/commandLineService;1" ]
                             .getService( Components.interfaces.nsICmdLineService );
            var prefList = cmdLine.getCmdLineValue( "-resetPref" ).split( "," );

            // Get pref service.
            var prefs    = Components.classes[ "@mozilla.org/preferences-service;1" ]
                             .getService( Components.interfaces.nsIPrefService );

            // For each pref specified on the cmd line, reset the user pref value.
            for ( i in prefList ) {
                var pref = prefs.getBranch( prefList[ i ] );
                try {
                    pref.clearUserPref( "" );
                } catch( e ) {
                }
            }
        } catch( e ) {
            this.dump( "exception: " + e );
        }

        // Return an error (so nsAppRunner doesn't think we've opened a window).
        throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
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

    // Dump text (if debug is on).
    dump: function( text ) {
        if ( this.debug ) {
            dump( "nsResetPref: " + text + "\n" );
        }
    },

    // This Component's module implementation.  All the code below is used to get this
    // component registered and accessible via XPCOM.
    module: {
        // registerSelf: Register this component.
        registerSelf: function (compMgr, fileSpec, location, type) {
            var compReg = compMgr.QueryInterface( Components.interfaces.nsIComponentRegistrar );
            compReg.registerFactoryLocation( this.cid,
                                             "Pref Reset Component",
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
        cid: Components.ID("{15ABFAF7-AD4F-4450-899B-0373EE9FAD95}"),

        /* Contract ID for this class */
        contractId: "@mozilla.org/commandlinehandler/general-startup;1?type=resetPref",

        /* factory object */
        factory: {
            // createInstance: Return a new nsResetPref object.
            createInstance: function (outer, iid) {
                if (outer != null)
                    throw Components.results.NS_ERROR_NO_AGGREGATION;

                return (new nsResetPref()).QueryInterface(iid);
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
    return nsResetPref.prototype.module;
}
