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
   Script for Proxy Auto Config in the new world order.
       - Gagan Saksena 04/24/00 
*/

const kPAC_PROGID = "component://mozilla/network/proxy_autoconfig";
const kPAC_CID = Components.ID("{63ac8c66-1dd2-11b2-b070-84d00d3eaece}");
const nsIProxyAutoConfig = Components.interfaces.nsIProxyAutoConfig;

function debug(msg)
{
    dump(msg + "\n");
}

// implementor of nsIProxyAutoConfig
function nsProxyAutoConfig() {};

nsProxyAutoConfig.prototype = {

    ProxyForURL: function(url, host, port, type) {
        uri = url.QueryInterface(Components.interfaces.nsIURI);
        // Call the original function-
        var proxy = FindProxyForURL(uri.spec, uri.host);
        debug("Proxy = " + proxy);
        // TODO add code here that parses stupid PAC return strings
        // to host and port pair... 
        // TODO warn about SOCKS
        // test dummy for now...
        host.value = "localhost";
        port.value = 4444;
		type.value = "http"; //proxy (http, socks, direct, etc)
    }
}

var pacModule = new Object();

pacModule.registerSelf =
    function (compMgr, fileSpec, location, type) {
        dump("*** Registering Proxy Auto Config (a Javascript module!) \n");
        compMgr.registerComponentWithType(kPAC_CID,
            "Proxy Auto Config",
            kPAC_PROGID,
            fileSpec, location, 
            true, true, type);
    }

pacModule.getClassObject =
function (compMgr, cid, iid) {
        if (!cid.equals(kPAC_CID))
            throw Components.results.NS_ERROR_NO_INTERFACE;

        if (!iid.equals(Components.interfaces.nsIFactory))
            throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

        return pacFactory;
    }

pacModule.canUnload =
    function (compMgr) {
        dump("*** Unloading Proxy Auto Config...\n");
        return true;
    }

var pacFactory = new Object();
pacFactory.CreateInstance =
    function (outer, iid) {
        if (outer != null)
            throw Components.results.NS_ERROR_NO_AGGREGATION;

        if (!iid.equals(nsIProxyAutoConfig) &&
                !iid.equals(Components.interfaces.nsISupports)) {
            // shouldn't this be NO_INTERFACE?
            throw Components.results.NS_ERROR_INVALID_ARG;
        }
        return PacMan;
    }

function NSGetModule(compMgr, fileSpec) {
    return pacModule;
}

var PacMan = new nsProxyAutoConfig() ;

// dumb hacks for "special" functions that should have long been 
// taken out and shot, and shot again... most of them.

var date = new Date();

function dateRange(dmy) {
    var intval = parseInt(dmy, 10);
    if (isNaN(intval)) { // it's a month
        return (date.toString().toLowerCase().search(dmy.toLowerCase()) != -1);
    }
    if (intval <= 31) {  // it's a day
        return (date.getDate() == intval);
    }
    else { // it's an year
        return (date.getFullYear() == intval);
    }
}

function dateRange(dmy1, dmy2) {
    return false;
}

function dateRange(d1, m1, y1, d2, m2, y2) {
    var date1 = new Date(y1,m1,d1);
    var date2 = new Date(y2,m2,d2);

    return (date >= date1) && (date <= date2);
}

function dnsDomainIs(host, domain) {
    return (host.search("/" + domain + "$/") != -1);
}

function dnsDomainLevels(host) {
    return host.split('.').length-1;
}

function dnsResolve(host) {
    // call back into IOService... TODO
    return "127.0.0.1";
}

function isInNet(host, pattern, mask) {
    return false;
}

function isPlainHostName(host) {
    return (host.search("\.") == -1);
}

function isResolvable(host) {
    // call back into IOService and get the result... TODO
    return false;
}

function localHostOrDomainIs(host, hostdom) {
    if (isPlainHostName(host)) {
        return (hostdom.search("/^" + host + "/") != -1);
    }
    else {
        return (host == hostdom); //TODO check 
    }
}

function myIpAddress() {
    // call into IOService ... TODO
    return "127.0.0.1";
}

function shExpMatch(str, shexp) {
    return false;
}

function timeRange(hour) {
    return false;
}

function timeRange(h1, h2) {
    return false;
}

function timeRange(h1, m1, h2, m2) {
    return false;
}

function timeRange(h1, m1, s1, h2, m2, s2) {
    return false;
}

function timeRange(h1, m1, s1, h2, m2, s2, gmt) {
    return false;
}

function weekdayRange(wd1, wd2, gmt) {
    return false;
}

///--------- replace everything below with your existing pac file ...

// Sample implementation ...
function FindProxyForURL(url, host) 
{
    if (isPlainHostName(host) || 
        dnsDomainIs(host, ".mozilla.org") || 
        isResolvable(host))
        return "DIRECT";
    else
        return "PROXY tegu.mozilla.org:3130; DIRECT";
}
