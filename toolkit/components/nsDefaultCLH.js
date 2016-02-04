/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

const nsISupports              = Components.interfaces.nsISupports;

const nsICommandLine           = Components.interfaces.nsICommandLine;
const nsICommandLineHandler    = Components.interfaces.nsICommandLineHandler;
const nsIPrefBranch            = Components.interfaces.nsIPrefBranch;
const nsISupportsString        = Components.interfaces.nsISupportsString;
const nsIWindowWatcher         = Components.interfaces.nsIWindowWatcher;
const nsIProperties            = Components.interfaces.nsIProperties;
const nsIFile                  = Components.interfaces.nsIFile;
const nsISimpleEnumerator      = Components.interfaces.nsISimpleEnumerator;

/**
 * This file provides a generic default command-line handler.
 *
 * It opens the chrome window specified by the pref "toolkit.defaultChromeURI"
 * with the flags specified by the pref "toolkit.defaultChromeFeatures"
 * or "chrome,dialog=no,all" is it is not available.
 * The arguments passed to the window are the nsICommandLine instance.
 *
 * It doesn't do anything if the pref "toolkit.defaultChromeURI" is unset.
 */

function getDirectoryService()
{
  return Components.classes["@mozilla.org/file/directory_service;1"]
                   .getService(nsIProperties);
}

function nsDefaultCLH() { }
nsDefaultCLH.prototype = {
  classID: Components.ID("{6ebc941a-f2ff-4d56-b3b6-f7d0b9d73344}"),

  /* nsISupports */

  QueryInterface : XPCOMUtils.generateQI([nsICommandLineHandler]),

  /* nsICommandLineHandler */

  handle : function clh_handle(cmdLine) {
    var printDir;
    while ((printDir = cmdLine.handleFlagWithParam("print-xpcom-dir", false))) {
      var out = "print-xpcom-dir(\"" + printDir + "\"): ";
      try {
        out += getDirectoryService().get(printDir, nsIFile).path;
      }
      catch (e) {
        out += "<Not Provided>";
      }

      dump(out + "\n");
      Components.utils.reportError(out);
    }

    var printDirList;
    while ((printDirList = cmdLine.handleFlagWithParam("print-xpcom-dirlist",
                                                       false))) {
      out = "print-xpcom-dirlist(\"" + printDirList + "\"): ";
      try {
        var list = getDirectoryService().get(printDirList,
                                             nsISimpleEnumerator);
        while (list.hasMoreElements())
          out += list.getNext().QueryInterface(nsIFile).path + ";";
      }
      catch (e) {
        out += "<Not Provided>";
      }

      dump(out + "\n");
      Components.utils.reportError(out);
    }

    if (cmdLine.handleFlag("silent", false)) {
      cmdLine.preventDefault = true;
    }

    if (cmdLine.preventDefault)
      return;

    var prefs = Components.classes["@mozilla.org/preferences-service;1"]
                          .getService(nsIPrefBranch);

    try {
      var singletonWindowType =
                              prefs.getCharPref("toolkit.singletonWindowType");
      var windowMediator =
                Components.classes["@mozilla.org/appshell/window-mediator;1"]
                          .getService(Components.interfaces.nsIWindowMediator);

      var win = windowMediator.getMostRecentWindow(singletonWindowType);
      if (win) {
        win.focus();
        cmdLine.preventDefault = true;
        return;
      }
    }
    catch (e) { }

    // if the pref is missing, ignore the exception
    try {
      var chromeURI = prefs.getCharPref("toolkit.defaultChromeURI");

      var flags = "chrome,dialog=no,all";
      try {
        flags = prefs.getCharPref("toolkit.defaultChromeFeatures");
      }
      catch (e) { }

      var wwatch = Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
                            .getService(nsIWindowWatcher);
      wwatch.openWindow(null, chromeURI, "_blank",
                        flags, cmdLine);
    }
    catch (e) { }
  },

  helpInfo : "",
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([nsDefaultCLH]);
