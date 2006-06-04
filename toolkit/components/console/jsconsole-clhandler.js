/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:sw=4:sr:sta:et:sts: */
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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Martijn Pieters <mj@digicool.com>
 *   Benjamin Smedberg <benjamin@smedbergs.us>
 *   Simon Bünzli <zeniko@gmail.com>
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

/*
 * -jsconsole commandline handler; starts up the Error console.
 */

const nsISupports           = Components.interfaces.nsISupports;
const nsICategoryManager    = Components.interfaces.nsICategoryManager;
const nsIComponentRegistrar = Components.interfaces.nsIComponentRegistrar;
const nsICommandLine        = Components.interfaces.nsICommandLine;
const nsICommandLineHandler = Components.interfaces.nsICommandLineHandler;
const nsIFactory            = Components.interfaces.nsIFactory;
const nsIModule             = Components.interfaces.nsIModule;
const nsIWindowWatcher      = Components.interfaces.nsIWindowWatcher;
const nsIWindowMediator     = Components.interfaces.nsIWindowMediator;

/*
 * Classes
 */

const jsConsoleHandler = {
    /* nsISupports */
    QueryInterface : function clh_QI(iid) {
        if (iid.equals(nsICommandLineHandler) ||
            iid.equals(nsIFactory) ||
            iid.equals(nsISupports))
            return this;

        throw Components.results.NS_ERROR_NO_INTERFACE;
    },

    /* nsICommandLineHandler */

    handle : function clh_handle(cmdLine) {
        if (!cmdLine.handleFlag("jsconsole", false))
            return;

        var windowMediator = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                                       .getService(nsIWindowMediator);
        var console = windowMediator.getMostRecentWindow("global:console");
        if (!console) {
          var wwatch = Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
                                 .getService(nsIWindowWatcher);
          wwatch.openWindow(null, "chrome://global/content/console.xul", "_blank",
                            "chrome,dialog=no,all", cmdLine);
        } else {
          // the Error console was already open
          console.focus();
        }

        // note that since we don't prevent the default action, you'll get
        // an additional application window, unless you specified another
        // command line flag
    },

    helpInfo : "  -jsconsole           Open the Error console.\n",

    /* nsIFactory */

    createInstance : function clh_CI(outer, iid) {
        if (outer != null)
            throw Components.results.NS_ERROR_NO_AGGREGATION;

        return this.QueryInterface(iid);
    },

    lockFactory : function clh_lock(lock) {
        /* no-op */
    }
};

const clh_contractID = "@mozilla.org/toolkit/console-clh;1";
const clh_CID = Components.ID("{2cd0c310-e127-44d0-88fc-4435c9ab4d4b}");
const clh_category = "c-jsconsole";

const jsConsoleHandlerModule = {
    /* nsISupports */

    QueryInterface : function mod_QI(iid) {
        if (iid.equals(nsIModule) ||
            iid.equals(nsISupports))
            return this;

        throw Components.results.NS_ERROR_NO_INTERFACE;
    },

    /* nsIModule */

    getClassObject : function mod_gch(compMgr, cid, iid) {
        if (cid.equals(clh_CID))
            return jsConsoleHandler.QueryInterface(iid);

        throw Components.results.NS_ERROR_NOT_REGISTERED;
    },

    registerSelf : function mod_regself(compMgr, fileSpec, location, type) {
        compMgr.QueryInterface(nsIComponentRegistrar);

        compMgr.registerFactoryLocation(clh_CID,
                                        "jsConsoleHandler",
                                        clh_contractID,
                                        fileSpec,
                                        location,
                                        type);

        var catMan = Components.classes["@mozilla.org/categorymanager;1"]
                               .getService(nsICategoryManager);
        catMan.addCategoryEntry("command-line-handler",
                                clh_category,
                                clh_contractID, true, true);
    },

    unregisterSelf : function mod_unreg(compMgr, location, type) {
        compMgr.QueryInterface(nsIComponentRegistrar);

        compMgr.unregisterFactoryLocation(clh_CID, location);

        var catMan = Components.classes["@mozilla.org/categorymanager;1"]
                               .getService(nsICategoryManager);
        catMan.deleteCategoryEntry("command-line-handler", clh_category);
    },

    canUnload : function (compMgr) {
        return true;
    }
};

/* module initialisation */
function NSGetModule(comMgr, fileSpec) {
    return jsConsoleHandlerModule;
}
