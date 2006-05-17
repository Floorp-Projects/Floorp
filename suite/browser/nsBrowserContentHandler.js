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
 * The Original Code is the Mozilla Firefox browser.
 *
 * The Initial Developer of the Original Code is
 * Benjamin Smedberg <benjamin@smedbergs.us>
 *
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Neil Rashbook <neil@parkwaycc.co.uk>
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

const nsISupports            = Components.interfaces.nsISupports;
const nsIBrowserDOMWindow    = Components.interfaces.nsIBrowserDOMWindow;
const nsIBrowserHistory      = Components.interfaces.nsIBrowserHistory;
const nsIChannel             = Components.interfaces.nsIChannel;
const nsICommandLine         = Components.interfaces.nsICommandLine;
const nsICommandLineHandler  = Components.interfaces.nsICommandLineHandler;
const nsIComponentRegistrar  = Components.interfaces.nsIComponentRegistrar;
const nsICategoryManager     = Components.interfaces.nsICategoryManager;
const nsIContentHandler      = Components.interfaces.nsIContentHandler;
const nsIDOMWindow           = Components.interfaces.nsIDOMWindow;
const nsIFactory             = Components.interfaces.nsIFactory;
const nsIFileURL             = Components.interfaces.nsIFileURL;
const nsIHttpProtocolHandler = Components.interfaces.nsIHttpProtocolHandler;
const nsIPrefService         = Components.interfaces.nsIPrefService;
const nsIPrefBranch          = Components.interfaces.nsIPrefBranch;
const nsIPrefLocalizedString = Components.interfaces.nsIPrefLocalizedString;
const nsISupportsString      = Components.interfaces.nsISupportsString;
const nsIURIFixup            = Components.interfaces.nsIURIFixup;
const nsIWindowMediator      = Components.interfaces.nsIWindowMediator;
const nsIWindowWatcher       = Components.interfaces.nsIWindowWatcher;
const nsIWebNavigationInfo   = Components.interfaces.nsIWebNavigationInfo;

const NS_BINDING_ABORTED = 0x804b0002;
const NS_ERROR_WONT_HANDLE_CONTENT = 0x805d0001;

const NS_GENERAL_STARTUP_PREFIX = "@mozilla.org/commandlinehandler/general-startup;1?type=";

function shouldLoadURI(aURI)
{
  if (aURI && !aURI.schemeIs("chrome"))
    return true;

  dump("*** Preventing external load of chrome: URI into browser window\n");
  dump("    Use -chrome <uri> instead\n");
  return false;
}

function resolveURIInternal(aCmdLine, aArgument)
{
  try {
    var uri = aCmdLine.resolveURI(aArgument);
    if (!(uri instanceof nsIFileURL) || uri.file.exists())
      return uri;
  } catch (e) {
    Components.utils.reportError(e);
  }

  // We have interpreted the argument as a relative file URI, but the file
  // doesn't exist. Try URI fixup heuristics: see bug 290782.
 
  try {
    var urifixup = Components.classes["@mozilla.org/docshell/urifixup;1"]
                             .getService(nsIURIFixup);

    uri = urifixup.createFixupURI(aArgument, 0);
  } catch (e) {
    Components.utils.reportError(e);
  }

  return uri;
}

function getHomePageGroup(prefs)
{
  var homePage = prefs.getComplexValue("browser.startup.homepage",
                                       nsIPrefLocalizedString).data;

  var count = 0;
  try {
    count = prefs.getIntPref("browser.startup.homepage.count");
  } catch (e) {
  }

  for (var i = 1; i < count; ++i) {
    try {
      homePage += '\n' + prefs.getComplexValue("browser.startup.homepage." + i,
                                               nsISupportsString).data;
    } catch (e) {
    }
  }
  return homePage;
}

function needHomePageOverride(prefs)
{
  var savedmstone = null;
  try {
    savedmstone = prefs.getCharPref("browser.startup.homepage_override.mstone");
    if (savedmstone == "ignore")
      return false;
  } catch (e) {
  }

  var mstone = Components.classes["@mozilla.org/network/protocol;1?name=http"]
                         .getService(nsIHttpProtocolHandler).misc;

  if (mstone == savedmstone)
    return false;

  prefs.setCharPref("browser.startup.homepage_override.mstone", mstone);

  return true;
}

function getURLToLoad()
{
  var prefs = Components.classes["@mozilla.org/preferences-service;1"]
                        .getService(nsIPrefBranch);

  if (needHomePageOverride(prefs)) {
    try {
      return prefs.getComplexValue("startup.homepage_override_url",
                                   nsIPrefLocalizedString).data;
    } catch (e) {
    }
  }

  try {
    switch (prefs.getIntPref("browser.startup.page")) {
    case 1:
      return getHomePageGroup(prefs);

    case 2:
      return Components.classes["@mozilla.org/browser/global-history;2"]
                       .getService(nsIBrowserHistory).lastPageVisited;
    }
  } catch (e) {
  } 

  return "about:blank";
}

function openWindow(parent, url, features, arg)
{
  var wwatch = Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
                         .getService(nsIWindowWatcher);
  var argstring = Components.classes["@mozilla.org/supports-string;1"]
                            .createInstance(nsISupportsString);
  argstring.data = arg;
  return wwatch.openWindow(parent, url, "", features, argstring);
}

function openPreferences()
{
  var win = getMostRecentWindow("mozilla:preferences");
  if (win)
    win.focus();
  else
    openWindow(null, "chrome://communicator/content/pref/pref.xul", "chrome,titlebar,dialog=no", "");
}

function getMostRecentWindow(aType)
{
  var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                     .getService(nsIWindowMediator);
  return wm.getMostRecentWindow(aType);
}

function getBrowserURL()
{
  try {
    return Components.classes["@mozilla.org/preferences-service;1"]
                     .getService(nsIPrefBranch)
                     .getCharPref("browser.chromeURL");
  } catch (e) {
  }
  return "chrome://navigator/content/naviagator.xul";
}

function handURIToExistingBrowser(uri, location, features)
{
  if (!shouldLoadURI(uri))
    return;

  var navWin = getMostRecentWindow("navigator:browser");
  if (navWin)
    navWin.browserDOMWindow.openURI(uri, null, location,
                                    nsIBrowserDOMWindow.OPEN_EXTERNAL);
  else
    openWindow(null, getBrowserURL(), features, uri.spec);
}

var nsBrowserContentHandler = {
  get wrappedJSObject() {
    return this;
  },

  /* nsISupports */
  QueryInterface: function QueryInterface(iid) {
    if (iid.equals(nsISupports) ||
        iid.equals(nsICommandLineHandler) ||
        iid.equals(nsICommandLine) ||
        iid.equals(nsIContentHandler) ||
        iid.equals(nsIFactory))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  /* nsICommandLineHandler */
  handle: function handle(cmdLine) {
    var features = "chrome,all,dialog=no";
    try {
      var width = cmdLine.handleFlagWithParam("width", false);
      if (width != null)
        features += ",width=" + width;
    } catch (e) {
    }
    try {
      var height = cmdLine.handleFlagWithParam("height", false);
      if (height != null)
        features += ",height=" + height;
    } catch (e) {
    }

    try {
      var remote = cmdLine.handleFlagWithParam("remote", true);
      if (/^\s*(\w+)\s*\(\s*([^\s,]+)\s*,?\s*([^\s]*)\s*\)\s*$/.test(remote)) {
        switch (RegExp.$1.toLowerCase()) {
        case "openurl":
        case "openfile":
          // openURL(<url>)
          // openURL(<url>,new-window)
          // openURL(<url>,new-tab)

          var uri = resolveURIInternal(cmdLine, RegExp.$2);

          var location = nsIBrowserDOMWindow.OPEN_DEFAULTWINDOW;
          if (RegExp.$3 == "new-window")
            location = nsIBrowserDOMWindow.OPEN_NEWWINDOW;
          else if (RegExp.$3 == "new-tab")
            location = nsIBrowserDOMWindow.OPEN_NEWTAB;

          handURIToExistingBrowser(uri, location, features);
          break;

        case "mailto":
          openWindow(null, "chrome://messenger/content/messengercompose/messengercompose.xul", features, RegExp.$2);
          break;

        case "xfedocommand":
          switch (RegExp.$2.toLowerCase()) {
          case "openbrowser":
            openWindow(null, getBrowserURL(), features, RegExp.$3 || getURLToLoad());
            break;
          
          case "openinbox":
            openWindow(null, "chrome://messenger/content", features);
            break;

          case "composemessage":
            openWindow(null, "chrome://messenger/content/messengercompose/messengercompose.xul", features, RegExp.$3);
            break;

          default:
            throw Components.results.NS_ERROR_ABORT;
          }
          break;

        default:
          // Somebody sent us a remote command we don't know how to process:
          // just abort.
          throw Components.results.NS_ERROR_ABORT;
        }

        cmdLine.preventDefault = true;
      }
    } catch (e) {
      // If we had a -remote flag but failed to process it, throw
      // NS_ERROR_ABORT so that the xremote code knows to return a failure
      // back to the handling code.
      throw Components.results.NS_ERROR_ABORT;
    }

    try {
      var browserParam = cmdLine.handleFlagWithParam("browser", false);
      if (browserParam) {
        openWindow(null, getBrowserURL(), features, browserParam);
        cmdLine.preventDefault = true;
      }
    } catch (e) {
      if (cmdLine.handleFlag("browser", false)) {
        openWindow(null, getBrowserURL(), features, getURLToLoad());
        cmdLine.preventDefault = true;
      }
    }

    try {
      var urlParam = cmdLine.handleFlagWithParam("url", false);
      if (urlParam) {
        try {
          urlParam = resolveURIInternal(cmdLine, urlParam);
          handURIToExistingBrowser(urlParam, nsIBrowserDOMWindow.OPEN_DEFAULTWINDOW, features);
          cmdLine.preventDefault = true;
        } catch (e) {
        }
      }
    } catch (e) {
    }

    try {
      var chromeParam = cmdLine.handleFlagWithParam("chrome", false);
      if (chromeParam) {
        openWindow(null, chromeParam, features);
        cmdLine.preventDefault = true;
      }
    } catch (e) {
    }

    if (cmdLine.handleFlag("preferences", false)) {
      openPreferences();
      cmdLine.preventDefault = true;
    }

    if (cmdLine.handleFlag("silent", false))
      cmdLine.preventDefault = true;

    if (!cmdLine.preventDefault && cmdLine.length) {
      var arg = cmdLine.getArgument(0);
      if (!/^-/.test(arg)) {
        try {
          arg = resolveURIInternal(cmdLine, arg);
          handURIToExistingBrowser(arg, nsIBrowserDOMWindow.OPEN_DEFAULTWINDOW, features);
          cmdLine.preventDefault = true;
        } catch (e) {
        }
      }
    }

    if (!cmdLine.preventDefault) {
      this.realCmdLine = cmdLine;

      var prefBranch = Components.classes["@mozilla.org/preferences-service;1"]
                                 .getService(nsIPrefService)
                                 .getBranch("general.startup.");

      var startupArray = prefBranch.getChildList("", {});

      for (var i = 0; i < startupArray.length; ++i) {
        this.currentArgument = startupArray[i];
        var contract = NS_GENERAL_STARTUP_PREFIX + this.currentArgument;
        if (contract in Components.classes) {
          // Ignore any exceptions - we can't do anything about them here.
          try {
            if (prefBranch.getBoolPref(this.currentArgument)) {
              var handler = Components.classes[contract].getService(nsICommandLineHandler);
              if (handler.wrappedJSObject)
                handler.wrappedJSObject.handle(this);
              else
                handler.handle(this);
            }
          } catch (e) {
            Components.utils.reportError(e);
          }
        }
      }
    }

    if (!cmdLine.preventDefault) {
      var homePage = getURLToLoad();
      if (!/\n/.test(homePage)) {
        try {
          var urifixup = Components.classes["@mozilla.org/docshell/urifixup;1"]
                                   .getService(nsIURIFixup);
          var uri = urifixup.createFixupURI(homePage, 0);
          handURIToExistingBrowser(uri, nsIBrowserDOMWindow.OPEN_DEFAULTWINDOW, features);
          cmdLine.preventDefault = true;
        } catch (e) {
        }
      }

      if (!cmdLine.preventDefault) {
        openWindow(null, getBrowserURL(), features, homePage);
        cmdLine.preventDefault = true;
      }
    }

  },

  helpInfo: "  -browser <url>       Open a browser window.\n" +
            "  -url <url>           Open the specified url.\n" +
            "  -chrome <url>        Open the specified chrome.\n",

  /* nsICommandLine */
  length: 1,

  getArgument: function getArgument(index) {
    if (index == 0)
      return this.currentArgument;

    throw Components.results.NS_ERROR_INVALID_ARG;
  },

  findFlag: function findFlag(flag, caseSensitive) {
    if (caseSensitive)
      return flag == this.currentArgument ? 0 : -1;
    return flag.toLowerCase() == this.currentArgument.toLowerCase() ? 0 : -1;
  },

  removeArguments: function removeArguments(start, end) {
    // do nothing
  },

  handleFlag: function handleFlag(flag, caseSensitive) {
    if (caseSensitive)
      return flag == this.currentArgument;
    return flag.toLowerCase() == this.currentArgument.toLowerCase();
  },

  handleFlagWithParam : function handleFlagWithParam(flag, caseSensitive) {
    if (this.handleFlag(flag, caseSensitive))
      throw Components.results.NS_ERROR_INVALID_ARG;
  },

  get state() {
    return this.realCmdLine.state;
  },

  get preventDefault() {
    return this.realCmdLine.preventDefault;
  },

  set preventDefault(preventDefault) {
    return this.realCmdLine.preventDefault = preventDefault;
  },

  get workingDirectory() {
    return this.realCmdLine.workingDirectory;
  },

  get windowContext() {
    return this.realCmdLine.windowContext;
  },

  resolveFile: function resolveFile(arg) {
    return this.realCmdLine.resolveFile(arg);
  },

  resolveURI: function resolveURI(arg) {
    return this.realCmdLine.resolveURI(arg);
  },

  /* nsIContentHandler */
  handleContent: function handleContent(contentType, context, request) {
    var webNavInfo = Components.classes["@mozilla.org/webnavigation-info;1"]
                               .getService(nsIWebNavigationInfo);
    if (!webNavInfo.isTypeSupported(contentType, null))
      throw NS_ERROR_WONT_HANDLE_CONTENT;

    var parentWin;
    try {
      parentWin = context.getInterface(nsIDOMWindow);
    } catch (e) {
    }

    request.QueryInterface(nsIChannel);
    openWindow(parentWin, request.URI, null, null);
    request.cancel(NS_BINDING_ABORTED);
  },

  /* nsIFactory */
  createInstance: function createInstance(outer, iid) {
    if (outer != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;

    return this.QueryInterface(iid);
  },
    
  lockFactory: function lockFactory(lock) {
    /* no-op */
  }
};

const CONTRACTID_PREFIX = "@mozilla.org/uriloader/content-handler;1?type=";
const BROWSER_CONTRACTID = NS_GENERAL_STARTUP_PREFIX + "browser";
const BROWSER_CID = Components.ID("{c2343730-dc2c-11d3-98b3-001083010e9b}");

var Module = {
  /* nsISupports */
  QueryInterface: function QueryInterface(iid) {
    if (iid.equals(Components.interfaces.nsIModule) ||
        iid.equals(Components.interfaces.nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  /* nsIModule */
  getClassObject: function getClassObject(compMgr, cid, iid) {
    if (cid.equals(BROWSER_CID))
      return nsBrowserContentHandler.QueryInterface(iid);

    throw Components.results.NS_ERROR_FACTORY_NOT_REGISTERED;
  },
    
  registerSelf: function registerSelf(compMgr, fileSpec, location, type) {
    var compReg = compMgr.QueryInterface(nsIComponentRegistrar);

    compReg.registerFactoryLocation(BROWSER_CID,
                                    "nsBrowserContentHandler",
                                    BROWSER_CONTRACTID,
                                    fileSpec,
                                    location,
                                    type);

    function registerType(contentType) {
      compReg.registerFactoryLocation(BROWSER_CID,
				      "Browser Content Handler",
				      CONTRACTID_PREFIX + contentType,
				      fileSpec,
				      location,
				      type);
    }

    registerType("text/html");
    registerType("application/vnd.mozilla.xul+xml");
    registerType("image/svg+xml");
    registerType("text/rdf");
    registerType("text/xml");
    registerType("application/xhtml+xml");
    registerType("text/css");
    registerType("text/plain");
    registerType("image/gif");
    registerType("image/jpeg");
    registerType("image/jpg");
    registerType("image/png");
    registerType("image/bmp");
    registerType("image/x-icon");
    registerType("image/vnd.microsoft.icon");
    registerType("image/x-xbitmap");
    registerType("application/http-index-format");

    var catMan = Components.classes["@mozilla.org/categorymanager;1"]
                           .getService(nsICategoryManager);

    catMan.addCategoryEntry("command-line-handler", "x-default",
                            BROWSER_CONTRACTID, true, true);
  },
    
  unregisterSelf: function unregisterSelf(compMgr, location, type) {
    var compReg = compMgr.QueryInterface(nsIComponentRegistrar);
    compReg.unregisterFactoryLocation(BROWSER_CID, location);

    var catMan = Components.classes["@mozilla.org/categorymanager;1"]
                           .getService(nsICategoryManager);

    catMan.deleteCategoryEntry("command-line-handler",
                               "x-default", true);
  },

  canUnload: function canUnload(compMgr) {
    return true;
  }
};

// NSGetModule: Return the nsIModule object.
function NSGetModule(compMgr, fileSpec) {
  return Module;
}
