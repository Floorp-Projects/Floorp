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
 *   Akhil Arora <akhil.arora@sun.com>
 *   Tomi Leppikangas <Tomi.Leppikangas@oulu.fi>
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
const nsIDNSService = Components.interfaces.nsIDNSService;
const nsIRequest = Components.interfaces.nsIRequest;

// implementor of nsIProxyAutoConfig
function nsProxyAutoConfig() {};

// global variable that will hold the downloaded js 
var pac = null;
//hold PAC's URL, used in evalAsCodebase()
var pacURL;
// ptr to eval'ed FindProxyForURL function
var LocalFindProxyForURL=null;
// sendbox in which we eval loaded autoconfig js file
var ProxySandBox = null;

nsProxyAutoConfig.prototype = {
    sis: null,
    done: false,

    ProxyForURL: function(url, host, port, type) {
        /* If we're not done loading the pac yet, wait (ideally). For
           now, just return DIRECT to avoid loops. A simple mutex
           between ProxyForURL and LoadPACFromURL locks-up the
           browser. */
        if (!this.done) {
            host.value = null;
            type.value = "direct";
            return;
        }

        var uri = url.QueryInterface(Components.interfaces.nsIURI);
        // Call the original function-
        var proxy = LocalFindProxyForURL(uri.spec, uri.host);
        if (proxy == "DIRECT") {
            host.value = null;
            type.value = "direct";
        }
        else {
            // TODO warn about SOCKS

            // we ignore everything else past the first proxy. 
            // we could theoretically check isResolvable now and continue 
            // parsing. but for now...
            var hostport = /^PROXY ([^:]+):(\d+)/(proxy);
            host.value = hostport[1];
            port.value = hostport[2];
            type.value = "http"; //proxy (http, socks, direct, etc)
        }
    },

    LoadPACFromURL: function(uri, ioService) {
        this.done = false;
        var channel = ioService.newChannelFromURI(uri);
        // don't cache the PAC content
        channel.loadFlags |= nsIRequest.LOAD_BYPASS_CACHE;
        pacURL = uri.spec;
        channel.asyncOpen(this, null);
    },

    // nsIStreamListener interface
    onStartRequest: function(request, ctxt) { 
        pac = '';
        LocalFindProxyForURL=null;
        this.sis = 
        Components.Constructor('@mozilla.org/scriptableinputstream;1',
                               'nsIScriptableInputStream', 
                               'init');
    },

    onStopRequest: function(request, ctxt, status, errorMsg) {
        if(!ProxySandBox) {
           ProxySandBox = new Sandbox();
        }
        // add predefined functions to pac
        var mypac = pacUtils + pac;
        // evaluate loded js file
        evalInSandbox(mypac, ProxySandBox, pacURL);
        try {
            ProxySandBox.myIP = dns.myIPAddress;
        } catch (e) {
            // Well, theres nothing better.
            // see bugs 80363 and 92516.
            ProxySandBox.myIP = "127.0.0.1";
        }
        ProxySandBox.dnsResolve = dnsResolve;
        LocalFindProxyForURL=ProxySandBox.FindProxyForURL;
        this.done = true;
    },

    onDataAvailable: function(request, ctxt, inStream, sourceOffset, count) {
        var ins = new this.sis(inStream);
        pac += ins.read(count);
    }
}

// Synchronous calls to nsDNSService::Resolve ignore the cache! (bug 97097)
// Keep a simple one of our own.
var dnsResolveCachedHost = null;
var dnsResolveCachedIp = null;

// wrapper for dns.resolve to catch exception on failure
function dnsResolve(host) {
    if (host == dnsResolveCachedHost) {
        return dnsResolveCachedIp;
    }
    dnsResolveCachedHost = host;
    try {
        dnsResolveCachedIp = dns.resolve(host);
    }
    catch (e) {
        dnsResolveCachedIp = null;
    }
    return dnsResolveCachedIp;
}

var pacModule = new Object();

pacModule.registerSelf =
    function (compMgr, fileSpec, location, type) {
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
        return true;
    }

var pacFactory = new Object();
pacFactory.createInstance =
    function (outer, iid) {
        if (outer != null)
            throw Components.results.NS_ERROR_NO_AGGREGATION;

        if (!iid.equals(nsIProxyAutoConfig) &&
            !!iid.equals(Components.interfaces.nsIStreamListener) &&
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

var ios = Components.classes[kIOSERVICE_CONTRACTID].getService(nsIIOService);
var dns = Components.classes[kDNS_CONTRACTID].getService(nsIDNSService);

var pacUtils = 
"function dnsDomainIs(host, domain) {\n" +
"    return (host.length >= domain.length &&\n" +
"            host.substring(host.length - domain.length) == domain);\n" +
"}\n" +

"function dnsDomainLevels(host) {\n" +
"    return host.split('.').length-1;\n" +
"}\n" +

"function convert_addr(ipchars) {\n"+
"    var bytes = ipchars.split('.');\n"+
"    var result = ((bytes[0] & 0xff) << 24) |\n"+
"                 ((bytes[1] & 0xff) << 16) |\n"+
"                 ((bytes[2] & 0xff) <<  8) |\n"+
"                  (bytes[3] & 0xff);\n"+
"    return result;\n"+
"}\n"+

"function isInNet(ipaddr, pattern, maskstr) {\n"+
"    var test = /^(\\d{1,4})\\.(\\d{1,4})\\.(\\d{1,4})\\.(\\d{1,4})$/(ipaddr);\n"+
"    if (test == null) {\n"+
"        ipaddr = dnsResolve(ipaddr);\n"+
"        if (ipaddr == null)\n"+
"            return false;\n"+
"    } else if (test[1] > 255 || test[2] > 255 || \n"+
"               test[3] > 255 || test[4] > 255) {\n"+
"        return false;    // not an IP address\n"+
"    }\n"+
"    var host = convert_addr(ipaddr);\n"+
"    var pat  = convert_addr(pattern);\n"+
"    var mask = convert_addr(maskstr);\n"+
"    return ((host & mask) == (pat & mask));\n"+
"    \n"+
"}\n"+

"function isPlainHostName(host) {\n" +
"    return (host.search('\\\\.') == -1);\n" +
"}\n" +

"function isResolvable(host) {\n" +
"    var ip = dnsResolve(host);\n" +
"    return (ip != null);\n" +
"}\n" +

"function localHostOrDomainIs(host, hostdom) {\n" +
"    if (isPlainHostName(host)) {\n" +
"        return (hostdom.search('/^' + host + '/') != -1);\n" +
"    }\n" +
"    else {\n" +
"        return (host == hostdom); //TODO check \n" +
"    }\n" +
"}\n" +

" var myIP;\n" +
"function myIpAddress() {\n" +
"    return (myIP) ? myIP : '127.0.0.1';\n" +
"}\n" +

"function shExpMatch(url, pattern) {\n" +
"   pattern = pattern.replace(/\\./g, '\\\\.');\n" +
"   pattern = pattern.replace(/\\*/g, '.*');\n" +
"   pattern = pattern.replace(/\\?/g, '.');\n" +
"   var newRe = new RegExp(pattern);\n" +
"   return newRe.test(url);\n" +
"}\n" +

"var wdays = new Array('SUN', 'MON', 'TUE', 'WED', 'THU', 'FRI', 'SAT');\n" +

"var monthes = new Array('JAN', 'FEB', 'MAR', 'APR', 'MAY', 'JUN', 'JUL', 'AUG', 'SEP', 'OCT', 'NOV', 'DEC');\n"+

"function weekdayRange() {\n" +
"    function getDay(weekday) {\n" +
"        for (var i = 0; i < 6; i++) {\n" +
"            if (weekday == wdays[i]) \n" +
"                return i;\n" +
"        }\n" +
"        return -1;\n" +
"    }\n" +
"    var date = new Date();\n" +
"    var argc = arguments.length;\n" +
"    var wday;\n" +
"    if (argc < 1)\n" +
"        return false;\n" +
"    if (arguments[argc - 1] == 'GMT') {\n" +
"        argc--;\n" +
"        wday = date.getUTCDay();\n" +
"    } else {\n" +
"        wday = date.getDay();\n" +
"    }\n" +
"    var wd1 = getDay(arguments[0]);\n" +
"    var wd2 = (argc == 2) ? getDay(arguments[1]) : wd1;\n" +
"    return (wd1 == -1 || wd2 == -1) ? false\n" +
"                                    : (wd1 <= wday && wday <= wd2);\n" +
"}\n" +

"function dateRange() {\n" +
"    function getMonth(name) {\n" +
"        for (var i = 0; i < 6; i++) {\n" +
"            if (name == monthes[i])\n" +
"                return i;\n" +
"        }\n" +
"        return -1;\n" +
"    }\n" +
"    var date = new Date();\n" +
"    var argc = arguments.length;\n" +
"    if (argc < 1) {\n" +
"        return false;\n" +
"    }\n" +
"    var isGMT = (arguments[argc - 1] == 'GMT');\n" +
"\n" +
"    if (isGMT) {\n" +
"        argc--;\n" +
"    }\n" +
"    // function will work even without explict handling of this case\n" +
"    if (argc == 1) {\n" +
"        var tmp = parseInt(arguments[0]);\n" +
"        if (isNaN(tmp)) {\n" +
"            return ((isGMT ? date.getUTCMonth() : date.getMonth()) ==\n" +
"getMonth(arguments[0]));\n" +
"        } else if (tmp < 32) {\n" +
"            return ((isGMT ? date.getUTCDate() : date.getDate()) == tmp);\n" +
"        } else { \n" +
"            return ((isGMT ? date.getUTCFullYear() : date.getFullYear()) ==\n" +
"tmp);\n" +
"        }\n" +
"    }\n" +
"    var year = date.getFullYear();\n" +
"    var date1, date2;\n" +
"    date1 = new Date(year,  0,  1,  0,  0,  0);\n" +
"    date2 = new Date(year, 11, 31, 23, 59, 59);\n" +
"    var adjustMonth = false;\n" +
"    for (var i = 0; i < (argc >> 1); i++) {\n" +
"        var tmp = parseInt(arguments[i]);\n" +
"        if (isNaN(tmp)) {\n" +
"            var mon = getMonth(arguments[i]);\n" +
"            date1.setMonth(mon);\n" +
"        } else if (tmp < 32) {\n" +
"            adjustMonth = (argc <= 2);\n" +
"            date1.setDate(tmp);\n" +
"        } else {\n" +
"            date1.setFullYear(tmp);\n" +
"        }\n" +
"    }\n" +
"    for (var i = (argc >> 1); i < argc; i++) {\n" +
"        var tmp = parseInt(arguments[i]);\n" +
"        if (isNaN(tmp)) {\n" +
"            var mon = getMonth(arguments[i]);\n" +
"            date2.setMonth(mon);\n" +
"        } else if (tmp < 32) {\n" +
"            date2.setDate(tmp);\n" +
"        } else {\n" +
"            date2.setFullYear(tmp);\n" +
"        }\n" +
"    }\n" +
"    if (adjustMonth) {\n" +
"        date1.setMonth(date.getMonth());\n" +
"        date2.setMonth(date.getMonth());\n" +
"    }\n" +
"    if (isGMT) {\n" +
"    var tmp = date;\n" +
"        tmp.setFullYear(date.getUTCFullYear());\n" +
"        tmp.setMonth(date.getUTCMonth());\n" +
"        tmp.setDate(date.getUTCDate());\n" +
"        tmp.setHours(date.getUTCHours());\n" +
"        tmp.setMinutes(date.getUTCMinutes());\n" +
"        tmp.setSeconds(date.getUTCSeconds());\n" +
"        date = tmp;\n" +
"    }\n" +
"    return ((date1 <= date) && (date <= date2));\n" +
"}\n" +

"function timeRange() {\n" +
"    var argc = arguments.length;\n" +
"    var date = new Date();\n" +
"    var isGMT= false;\n"+
"\n" +
"    if (argc < 1) {\n" +
"        return false;\n" +
"    }\n" +
"    if (arguments[argc - 1] == 'GMT') {\n" +
"        isGMT = true;\n" +
"        argc--;\n" +
"    }\n" +
"\n" +
"    var hour = isGMT ? date.getUTCHours() : date.getHours();\n" +
"    var date1, date2;\n" +
"    date1 = new Date();\n" +
"    date2 = new Date();\n" +
"\n" +
"    if (argc == 1) {\n" +
"        return (hour == arguments[0]);\n" +
"    } else if (argc == 2) {\n" +
"        return ((arguments[0] <= hour) && (hour <= arguments[1]));\n" +
"    } else {\n" +
"        switch (argc) {\n" +
"        case 6:\n" +
"            date1.setSeconds(arguments[2]);\n" +
"            date2.setSeconds(arguments[5]);\n" +
"        case 4:\n" +
"            var middle = argc >> 1;\n" +
"            date1.setHours(arguments[0]);\n" +
"            date1.setMinutes(arguments[1]);\n" +
"            date2.setHours(arguments[middle]);\n" +
"            date2.setMinutes(arguments[middle + 1]);\n" +
"            if (middle == 2) {\n" +
"                date2.setSeconds(59);\n" +
"            }\n" +
"            break;\n" +
"        default:\n" +
"          throw 'timeRange: bad number of arguments'\n" +
"        }\n" +
"    }\n" +
"\n" +
"    if (isGMT) {\n" +
"        date.setFullYear(date.getUTCFullYear());\n" +
"        date.setMonth(date.getUTCMonth());\n" +
"        date.setDate(date.getUTCDate());\n" +
"        date.setHours(date.getUTCHours());\n" +
"        date.setMinutes(date.getUTCMinutes());\n" +
"        date.setSeconds(date.getUTCSeconds());\n" +
"    }\n" +
"    return ((date1 <= date) && (date <= date2));\n" +
"}\n"

