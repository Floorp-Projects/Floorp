/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Conrad Carlen <ccarlen@netscape.com>
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


/* This file implements the nsCloseAllWindows component which implements
 * the nsICloseAllWindows interface. Use this component when all
 * windows need to be closed and confirmation to save changes is required.
 */


/* ctor
 */
function nsCloseAllWindows() {
}

nsCloseAllWindows.prototype = {

    // This "class" supports nsICloseAllWindows, and nsISupports.
    QueryInterface: function (iid) {
        if (!iid.equals(Components.interfaces.nsICloseAllWindows) &&
            !iid.equals(Components.interfaces.nsISupports)) {
            throw Components.results.NS_ERROR_NO_INTERFACE;
        }
        return this;
    },

    // ---------- nsICloseAllWindows methods ----------

    // closeAll: Close all open windows
    closeAll: function(aAskToSave)  {
    
        var windowMediator = Components.classes['@mozilla.org/appshell/window-mediator;1'].
                                getService(Components.interfaces.nsIWindowMediator);
        var enumerator = windowMediator.getEnumerator(null);

        while (enumerator.hasMoreElements()) {
           var domWindow = enumerator.getNext();
           if (aAskToSave && ("tryToClose" in domWindow)) {
               if (!domWindow.tryToClose())
                   return false;
           }
           domWindow.close();
        };
    
        return true;
    }
}

// This Component's module implementation.  All the code below is used to get this
// component registered and accessible via XPCOM.
var module = {
    firstTime: true,

    // registerSelf: Register this component.
    registerSelf: function (compMgr, fileSpec, location, type) {
        if (this.firstTime) {
            this.firstTime = false;
            throw Components.results.NS_ERROR_FACTORY_REGISTER_AGAIN;
        }
        compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);
        compMgr.registerFactoryLocation( this.cid,
                                         "Close All Windows",
                                         this.contractId,
                                         fileSpec,
                                         location,
                                         type );
    },

    // getClassObject: Return this component's factory object.
    getClassObject: function (compMgr, cid, iid) {
        if (!cid.equals(this.cid)) {
            throw Components.results.NS_ERROR_NO_INTERFACE;
        }

        if (!iid.equals(Components.interfaces.nsIFactory)) {
            throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
        }

        return this.factory;
    },

    /* CID for this class */
    cid: Components.ID("{2f977d48-5485-11d4-87e2-0010a4e75ef2}"),

    /* Contract ID for this class */
    contractId: "@mozilla.org/appshell/closeallwindows;1",

    /* factory object */
    factory: {
        // createInstance: Return a new nsCloseAllWindows object.
        createInstance: function (outer, iid) {
            if (outer != null)
                throw Components.results.NS_ERROR_NO_AGGREGATION;

            return (new nsCloseAllWindows()).QueryInterface(iid);
        }
    },

    // canUnload: n/a (returns true)
    canUnload: function(compMgr) {
        return true;
    }
};

// NSGetModule: Return the nsIModule object.
function NSGetModule(compMgr, fileSpec) {
    return module;
}
