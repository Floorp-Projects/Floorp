/* -*- Mode: Java; tab-width: 4; c-basic-offset: 4; -*-
 * 
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/*
   Script for the proxy auto config in the new world order. 
       - Gagan Saksena 04/24/00 
*/

var pac_progid= "component://mozilla/network/proxy_autoconfig";

function debug(msg)
{
    dump(msg);
}

function testSelf() {

    try {
        var pacMan = 
            Components.classes['component://mozilla/network/proxy_autoconfig']
                .createInstance();
        //pacMan.ProxyForURL();
    }
    catch (e) {
        debug("oh oh...");
        debug(e);
    }
}

function Init()
{
    debug("nsProxyAutoConfig.js: Init()\n");
    PacMan = new nsProxyAutoConfig();
    //testSelf();
}

function nsProxyAutoConfig() {};

nsProxyAutoConfig.prototype = {

    ProxyForURL: function(url, host, port) {
        uri = url.QueryInterface(Components.interfaces.nsIURI);
        print("PAC.js uri= " + uri.spec);
        // test dummy for now...
        host = "localhost";
        port = 4321;
    }
}

var pacModule = {
    firstTime: true,

    registerSelf: function (compMgr, fileSpec, location, type) {
        if (this.firstTime) {
            dump("*** Deferring registration of Proxy Auto Config\n");
            this.firstTime = false;
            throw Components.results.NS_ERROR_FACTORY_REGISTER_AGAIN;
        }
        dump("*** Registering Proxy Auto Config\n");
        compMgr.registerComponentWithType(this.pacCID, 
            "Proxy Auto Config",
            "component://mozilla/network/proxy_autoconfig",
            fileSpec,
            location, 
            true, 
            true,
            type);
    }, 

    getClassObject: function (compMgr, cid, iid) {
        if (!cid.equals(this.pacCID))
            throw Components.results.NS_ERROR_NO_INTERFACE;

        if (!iid.equals(Components.interfaces.nsIFactory))
            throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

        return this.pacFactory;
    },

    pacCID: Components.ID("{63ac8c66-1dd2-11b2-b070-84d00d3eaece}"),

    pacFactory: {
        CreateInstance: function (outer, iid) {
            if (outer != null)
                throw Components.results.NS_ERROR_NO_AGGREGATION;

            if (!iid.equals(Components.interfaces.nsIProxyAutoConfig) &&
                !iid.equals(Components.interfaces.nsISupports)) {
                // shouldn't this be NO_INTERFACE?
                throw Components.results.NS_ERROR_INVALID_ARG;
            }
            return PacMan;
        }
    }, 

    canUnload: function (compMgr) {
        dump("*** Unloading Proxy Auto Config...\n");
        return true;
    }
}

function NSGetModule(compMgr, fileSpec) {
    return pacModule;
}

var PacMan;
Init();

