/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This file contains the following chatzilla related components:
 * 1. Command line handler service, for responding to the -chat command line
 *    option. (CLineHandler)
 * 2. Content handlers for responding to content of type x-application-irc[s]
 *    (IRCContentHandler)
 * 3. Protocol handler for supplying a channel to the browser when an irc://
 *    or ircs:// link is clicked. (IRCProtocolHandler)
 * 4. A (nearly empty) implementation of nsIChannel for telling the browser
 *    that irc:// links have the content type x-application-irc[s] (BogusChannel)
 */

/* components defined in this file */
const CLINE_SERVICE_CONTRACTID =
    "@mozilla.org/commandlinehandler/general-startup;1?type=chat";
const CLINE_SERVICE_CID =
    Components.ID("{38a95514-1dd2-11b2-97e7-9da958640f2c}");
const IRCPROT_HANDLER_CONTRACTID =
    "@mozilla.org/network/protocol;1?name=irc";
const IRCSPROT_HANDLER_CONTRACTID =
    "@mozilla.org/network/protocol;1?name=ircs";
const IRCPROT_HANDLER_CID =
    Components.ID("{f21c35f4-1dd1-11b2-a503-9bf8a539ea39}");
const IRCSPROT_HANDLER_CID =
    Components.ID("{f21c35f4-1dd1-11b2-a503-9bf8a539ea3a}");

const IRC_MIMETYPE = "application/x-irc";
const IRCS_MIMETYPE = "application/x-ircs";


/* components used in this file */
const MEDIATOR_CONTRACTID =
    "@mozilla.org/appshell/window-mediator;1";
const STANDARDURL_CONTRACTID =
    "@mozilla.org/network/standard-url;1";
const IOSERVICE_CONTRACTID =
    "@mozilla.org/network/io-service;1";
const ASS_CONTRACTID =
    "@mozilla.org/appshell/appShellService;1";
const RDFS_CONTRACTID =
    "@mozilla.org/rdf/rdf-service;1";

//XXXgijs: Because necko is annoying and doesn't expose this error flag, we
//         define our own constant for it. Throwing something else will show
//         ugly errors instead of seeminly doing nothing.
const NS_ERROR_MODULE_NETWORK_BASE = 0x804b0000;
const NS_ERROR_NO_CONTENT = NS_ERROR_MODULE_NETWORK_BASE + 17;

/* interfaces used in this file */
const nsIWindowMediator  = Components.interfaces.nsIWindowMediator;
const nsICmdLineHandler  = Components.interfaces.nsICmdLineHandler;
const nsICategoryManager = Components.interfaces.nsICategoryManager;
const nsIURIContentListener = Components.interfaces.nsIURIContentListener;
const nsIURILoader       = Components.interfaces.nsIURILoader;
const nsIProtocolHandler = Components.interfaces.nsIProtocolHandler;
const nsIURI             = Components.interfaces.nsIURI;
const nsIStandardURL     = Components.interfaces.nsIStandardURL;
const nsIChannel         = Components.interfaces.nsIChannel;
const nsIRequest         = Components.interfaces.nsIRequest;
const nsIIOService       = Components.interfaces.nsIIOService;
const nsIAppShellService = Components.interfaces.nsIAppShellService;
const nsISupports        = Components.interfaces.nsISupports;
const nsISupportsWeakReference = Components.interfaces.nsISupportsWeakReference;
const nsIRDFService      = Components.interfaces.nsIRDFService;
const nsICommandLineHandler = Components.interfaces.nsICommandLineHandler;
const nsICommandLine     = Components.interfaces.nsICommandLine;


// ChatZilla launcher method
function spawnChatZilla(uri, count)
{
    var e;

    var wmClass = Components.classes[MEDIATOR_CONTRACTID];
    var windowManager = wmClass.getService(nsIWindowMediator);

    var assClass = Components.classes[ASS_CONTRACTID];
    var ass = assClass.getService(nsIAppShellService);
    hiddenWin = ass.hiddenDOMWindow;

    // Ok, not starting currently, so check if we've got existing windows.
    var w = windowManager.getMostRecentWindow("irc:chatzilla");

    // Claiming that a ChatZilla window is loading.
    if ("ChatZillaStarting" in hiddenWin)
    {
        dump("cz-service: ChatZilla claiming to be starting.\n");
        if (w && ("client" in w) && ("initialized" in w.client) &&
            w.client.initialized)
        {
            dump("cz-service: It lied. It's finished starting.\n");
            // It's actually loaded ok.
            delete hiddenWin.ChatZillaStarting;
        }
    }

    if ("ChatZillaStarting" in hiddenWin)
    {
        count = count || 0;

        if ((new Date() - hiddenWin.ChatZillaStarting) > 10000)
        {
            dump("cz-service: Continuing to be unable to talk to existing window!\n");
        }
        else
        {
            //dump("cz-service: **** Try: " + count + ", delay: " + (new Date() - hiddenWin.ChatZillaStarting) + "\n");

            // We have a ChatZilla window, but we're still loading.
            hiddenWin.setTimeout(function wrapper(count) {
                    spawnChatZilla(uri, count + 1);
                }, 250, count);
            return true;
        }
    }

    // We have a window.
    if (w)
    {
        dump("cz-service: Existing, fully loaded window. Using.\n");
        // Window is working and initialized ok. Use it.
        w.focus();
        w.gotoIRCURL(uri);
        return true;
    }

    dump("cz-service: No windows, starting new one.\n");
    // Ok, no available window, loading or otherwise, so start ChatZilla.
    var args = new Object();
    args.url = uri;

    hiddenWin.ChatZillaStarting = new Date();
    hiddenWin.openDialog("chrome://chatzilla_qa/content/chatzilla.xul", "_blank",
                 "chrome,menubar,toolbar,status,resizable,dialog=no",
                 args);

    return true;
}


/* Command Line handler service */
function CLineService()
{}

/* nsISupports */
CLineService.prototype.QueryInterface =
function handler_QI(iid)
{
    if (iid.equals(nsISupports))
        return this;

    if (nsICmdLineHandler && iid.equals(nsICmdLineHandler))
        return this;

    if (nsICommandLineHandler && iid.equals(nsICommandLineHandler))
        return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
}

/* nsICmdLineHandler */
CLineService.prototype.commandLineArgument = "-chat";
CLineService.prototype.prefNameForStartup = "general.startup.chat";
CLineService.prototype.chromeUrlForTask = "chrome://chatzilla_qa/content";
CLineService.prototype.helpText = "Start with an IRC chat client";
CLineService.prototype.handlesArgs = true;
CLineService.prototype.defaultArgs = "";
CLineService.prototype.openWindowWithArgs = true;

/* nsICommandLineHandler */
CLineService.prototype.handle =
function handler_handle(cmdLine)
{
    var args;
    try
    {
        var uristr = cmdLine.handleFlagWithParam("chat", false);
        if (uristr)
        {
            args = new Object();
            args.url = uristr;
        }
    }
    catch (e)
    {
    }

    if (args || cmdLine.handleFlag("chat", false))
    {
        var assClass = Components.classes[ASS_CONTRACTID];
        var ass = assClass.getService(nsIAppShellService);
        var hWin = ass.hiddenDOMWindow;
        hWin.openDialog("chrome://chatzilla_qa/content/", "_blank",
                        "chrome,menubar,toolbar,status,resizable,dialog=no",
                        args);
        cmdLine.preventDefault = true;
    }
}

CLineService.prototype.helpInfo =
 "  -chat [<ircurl>]  Start with an IRC chat client.\n"

/* factory for command line handler service (CLineService) */
var CLineFactory = new Object();

CLineFactory.createInstance =
function (outer, iid)
{
    if (outer != null)
        throw Components.results.NS_ERROR_NO_AGGREGATION;

    return new CLineService().QueryInterface(iid);
}

/* irc protocol handler component */
function IRCProtocolHandler(isSecure)
{
    this.isSecure = isSecure;
}

IRCProtocolHandler.prototype.protocolFlags =
                   nsIProtocolHandler.URI_NORELATIVE |
                   nsIProtocolHandler.ALLOWS_PROXY;
if ("URI_DANGEROUS_TO_LOAD" in nsIProtocolHandler) {
  IRCProtocolHandler.prototype.protocolFlags |=
      nsIProtocolHandler.URI_LOADABLE_BY_ANYONE;
}

IRCProtocolHandler.prototype.allowPort =
function ircph_allowPort(port, scheme)
{
    return false;
}

IRCProtocolHandler.prototype.newURI =
function ircph_newURI(spec, charset, baseURI)
{
    var cls = Components.classes[STANDARDURL_CONTRACTID];
    var url = cls.createInstance(nsIStandardURL);
    url.init(nsIStandardURL.URLTYPE_STANDARD, (this.isSecure ? 9999 : 6667), spec, charset, baseURI);

    return url.QueryInterface(nsIURI);
}

IRCProtocolHandler.prototype.newChannel =
function ircph_newChannel(URI)
{
    ios = Components.classes[IOSERVICE_CONTRACTID].getService(nsIIOService);
    if (!ios.allowPort(URI.port, URI.scheme))
        throw Components.results.NS_ERROR_FAILURE;

    return new BogusChannel(URI, this.isSecure);
}

/* protocol handler factory object (IRCProtocolHandler) */
var IRCProtocolHandlerFactory = new Object();

IRCProtocolHandlerFactory.createInstance =
function ircphf_createInstance(outer, iid)
{
    if (outer != null)
        throw Components.results.NS_ERROR_NO_AGGREGATION;

    if (!iid.equals(nsIProtocolHandler) && !iid.equals(nsISupports))
        throw Components.results.NS_ERROR_INVALID_ARG;

    var protHandler = new IRCProtocolHandler(false);
    protHandler.scheme = "irc";
    protHandler.defaultPort = 6667;
    return protHandler;
}

/* secure protocol handler factory object (IRCProtocolHandler) */
var IRCSProtocolHandlerFactory = new Object();

IRCSProtocolHandlerFactory.createInstance =
function ircphf_createInstance(outer, iid)
{
    if (outer != null)
        throw Components.results.NS_ERROR_NO_AGGREGATION;

    if (!iid.equals(nsIProtocolHandler) && !iid.equals(nsISupports))
        throw Components.results.NS_ERROR_INVALID_ARG;

    var protHandler = new IRCProtocolHandler(true);
    protHandler.scheme = "ircs";
    protHandler.defaultPort = 9999;
    return protHandler;
}

/* bogus IRC channel used by the IRCProtocolHandler */
function BogusChannel(URI, isSecure)
{
    this.URI = URI;
    this.originalURI = URI;
    this.isSecure = isSecure;
    this.contentType = (this.isSecure ? IRCS_MIMETYPE : IRC_MIMETYPE);
}

BogusChannel.prototype.QueryInterface =
function bc_QueryInterface(iid)
{
    if (!iid.equals(nsIChannel) && !iid.equals(nsIRequest) &&
        !iid.equals(nsISupports))
        throw Components.results.NS_ERROR_NO_INTERFACE;

    return this;
}

/* nsIChannel */
BogusChannel.prototype.loadAttributes = null;
BogusChannel.prototype.contentLength = 0;
BogusChannel.prototype.owner = null;
BogusChannel.prototype.loadGroup = null;
BogusChannel.prototype.notificationCallbacks = null;
BogusChannel.prototype.securityInfo = null;

BogusChannel.prototype.open =
BogusChannel.prototype.asyncOpen =
function bc_open(observer, ctxt)
{
    spawnChatZilla(this.URI.spec);
    // We don't throw this (a number, not a real 'resultcode') because it
    // upsets xpconnect if we do (error in the js console).
    Components.returnCode = NS_ERROR_NO_CONTENT;
}

BogusChannel.prototype.asyncRead =
function bc_asyncRead(listener, ctxt)
{
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIRequest */
BogusChannel.prototype.isPending =
function bc_isPending()
{
    return true;
}

BogusChannel.prototype.status = Components.results.NS_OK;

BogusChannel.prototype.cancel =
function bc_cancel(status)
{
    this.status = status;
}

BogusChannel.prototype.suspend =
BogusChannel.prototype.resume =
function bc_suspres()
{
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
}

var ChatzillaModule = new Object();

ChatzillaModule.registerSelf =
function cz_mod_registerSelf(compMgr, fileSpec, location, type)
{
    compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);
    var catman = Components.classes["@mozilla.org/categorymanager;1"]
        .getService(nsICategoryManager);

    debug("*** Registering -chat handler.\n");
    compMgr.registerFactoryLocation(CLINE_SERVICE_CID,
                                    "Chatzilla CommandLine Service",
                                    CLINE_SERVICE_CONTRACTID,
                                    fileSpec, location, type);
    catman.addCategoryEntry("command-line-argument-handlers",
                            "chatzilla command line handler",
                            CLINE_SERVICE_CONTRACTID, true, true);
    catman.addCategoryEntry("command-line-handler",
                            "m-irc",
                            CLINE_SERVICE_CONTRACTID, true, true);

    debug("*** Registering irc protocol handler.\n");
    compMgr.registerFactoryLocation(IRCPROT_HANDLER_CID,
                                    "IRC protocol handler",
                                    IRCPROT_HANDLER_CONTRACTID,
                                    fileSpec, location, type);

    debug("*** Registering ircs protocol handler.\n");
    compMgr.registerFactoryLocation(IRCSPROT_HANDLER_CID,
                                    "IRCS protocol handler",
                                    IRCSPROT_HANDLER_CONTRACTID,
                                    fileSpec, location, type);

    debug("*** Registering done.\n");
}

ChatzillaModule.unregisterSelf =
function cz_mod_unregisterSelf(compMgr, fileSpec, location)
{
    compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);

    var catman = Components.classes["@mozilla.org/categorymanager;1"]
        .getService(nsICategoryManager);
    catman.deleteCategoryEntry("command-line-argument-handlers",
                               "chatzilla command line handler", true);
    catman.deleteCategoryEntry("command-line-handler",
                               "m-irc", true);
}

ChatzillaModule.getClassObject =
function cz_mod_getClassObject(compMgr, cid, iid)
{
    // Checking if we're disabled in the Chrome Registry.
    var rv;
    try {
        var rdfSvc = Components.classes[RDFS_CONTRACTID].getService(nsIRDFService);
        var rdfDS = rdfSvc.GetDataSource("rdf:chrome");
        var resSelf = rdfSvc.GetResource("urn:mozilla:package:chatzilla");
        var resDisabled = rdfSvc.GetResource("http://www.mozilla.org/rdf/chrome#disabled");
        rv = rdfDS.GetTarget(resSelf, resDisabled, true);
    } catch (e) {
    }
    if (rv)
        throw Components.results.NS_ERROR_NO_INTERFACE;

    if (cid.equals(CLINE_SERVICE_CID))
        return CLineFactory;

    if (cid.equals(IRCPROT_HANDLER_CID))
        return IRCProtocolHandlerFactory;

    if (cid.equals(IRCSPROT_HANDLER_CID))
        return IRCSProtocolHandlerFactory;

    if (!iid.equals(Components.interfaces.nsIFactory))
        throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

    throw Components.results.NS_ERROR_NO_INTERFACE;
}

ChatzillaModule.canUnload =
function cz_mod_canUnload(compMgr)
{
    return true;
}

/* entrypoint */
function NSGetModule(compMgr, fileSpec)
{
    return ChatzillaModule;
}
