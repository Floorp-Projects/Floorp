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

const kPAC_CONTRACTID = "@mozilla.org/network/proxy_autoconfig;1";
const kIOSERVICE_CONTRACTID = "@mozilla.org/network/io-service;1";
const kDNS_CONTRACTID = "@mozilla.org/network/dns-service;1";
const kPAC_CID = Components.ID("{63ac8c66-1dd2-11b2-b070-84d00d3eaece}");
const nsIProxyAutoConfig = Components.interfaces.nsIProxyAutoConfig;
const nsIIOService = Components.interfaces['nsIIOService'];
const nsIDNSService = Components.interfaces['@mozilla.org/js/xpc/ID;1NSService'];

function debug(msg)
{
    dump("\nPAC:" + msg + "\n\n");
}

// implementor of nsIProxyAutoConfig
function nsProxyAutoConfig() {};

nsProxyAutoConfig.prototype = {

    ProxyForURL: function(url, host, port, type) {
        uri = url.QueryInterface(Components.interfaces.nsIURI);
        // Call the original function-
        var proxy = FindProxyForURL(uri.spec, uri.host);
        debug("Proxy = " + proxy);
        if (proxy == "DIRECT") {
            host.value = null;
            type.value = "direct";
        }
        else {
            // TODO warn about SOCKS
            
            // we ignore everything else past the first proxy. 
            // we could theoretically check isResolvable now and continue 
            // parsing. but for now...
            var re = /PROXY (.+):(\d+);.*/; 
            hostport = proxy.match(re);
            host.value = hostport[1];
            port.value = hostport[2];
            type.value = "http"; //proxy (http, socks, direct, etc)
        }
    }
}

var pacModule = new Object();

pacModule.registerSelf =
    function (compMgr, fileSpec, location, type) {
        dump("*** Registering Proxy Auto Config (a Javascript module!) \n");
        compMgr.registerComponentWithType(kPAC_CID,
            "Proxy Auto Config",
            kPAC_CONTRACTID,
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
pacFactory.createInstance =
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
// deprecated. 

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
    dump("nsProxyAutoConfig.js: dateRange function deprecated or not implemented\n");
    return false;
}

function dateRange(d1, m1, y1, d2, m2, y2) {
    var date1 = new Date(y1,m1,d1);
    var date2 = new Date(y2,m2,d2);

    return (date >= date1) && (date <= date2);
}

function dnsDomainIs(host, domain) {
    //TODO fix this later!
    return (host.search(domain) != -1);
}

function dnsDomainLevels(host) {
    return host.split('.').length-1;
}

var ios = Components.classes[kIOSERVICE_CONTRACTID].getService(nsIIOService);
var dns = Components.classes[kDNS_CONTRACTID].getService(nsIDNSService);

function dnsResolve(host) {
    try {
        var addr =  dns.resolve(host);
        //debug(addr);
        return addr;
    }
    catch (ex) {
        debug (ex);
        // ugh... return error!
        return null;
    }
}

// This function could be done here instead of in nsDNSService...
function isInNet(ipaddr, pattern, mask) {
    var result = dns.isInNet(ipaddr, pattern, mask);
    //debug(result);
    return result;
}

function isPlainHostName(host) {
    return (host.search("\.") == -1);
}

function isResolvable(host) {
    var ip = dnsResolve(host);
    return (ip != null) ? true: false;
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
    try {
        // for now...
        var ip = dnsResolve("localhost");
        return ip;
    }
    catch (ex) {
        debug(ex);
    }
    return null;
}

function shExpMatch(str, shexp) {
    dump("nsProxyAutoConfig.js: shExpMatch function deprecated or not implemented\n");
    // this may be a tricky one to implement in JS. 
    return false;
}

function timeRange(hour) {
    dump("nsProxyAutoConfig.js: timeRange function deprecated or not implemented\n");
    return false;
}

function timeRange(h1, h2) {
    dump("nsProxyAutoConfig.js: timeRange function deprecated or not implemented\n");
    return false;
}

function timeRange(h1, m1, h2, m2) {
    dump("nsProxyAutoConfig.js: timeRange function deprecated or not implemented\n");
    return false;
}

function timeRange(h1, m1, s1, h2, m2, s2) {
    dump("nsProxyAutoConfig.js: timeRange function deprecated or not implemented\n");
    return false;
}

function timeRange(h1, m1, s1, h2, m2, s2, gmt) {
    dump("nsProxyAutoConfig.js: timeRange function deprecated or not implemented\n");
    return false;
}

function weekdayRange(wd1, wd2, gmt) {
    dump("nsProxyAutoConfig.js: weekdayRange function deprecated or not implemented\n");
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
