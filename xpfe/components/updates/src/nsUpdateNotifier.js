/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Update Notifier.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Samir Gehani <sgehani@netscape.com> (Original Author)
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

const kDebug               = false;
const kUpdateCheckDelay    = 5 * 60 * 1000; // 5 minutes
const kUNEnabledPref       = "update_notifications.enabled";
const kUNDatasourceURIPref = "update_notifications.provider.0.datasource";
const kUNFrequencyPref     = "update_notifications.provider.0.frequency";
const kUNLastCheckedPref   = "update_notifications.provider.0.last_checked";
const kUNBundleURI         =
  "chrome://communicator/locale/update-notifications.properties";

////////////////////////////////////////////////////////////////////////
//
//   nsUpdateNotifier : nsIProfileStartupListener, nsIObserver
//
//   Checks for updates of the client by polling a distributor's website
//   for the latest available version of the software and comparing it
//   with the version of the running client.
//
////////////////////////////////////////////////////////////////////////

var nsUpdateNotifier =
{
  onProfileStartup: function(aProfileName)
  {
    debug("onProfileStartup");

    // now wait for the first app window to open
    var observerService = Components.
      classes["@mozilla.org/observer-service;1"].
      getService(Components.interfaces.nsIObserverService);
    observerService.addObserver(this, "domwindowopened", false);
  },

  mTimer: null, // need to hold on to timer ref

  observe: function(aSubject, aTopic, aData)
  {
    debug("observe: " + aTopic);

    if (aTopic == "domwindowopened")
    {
      try
      {
        const kITimer = Components.interfaces.nsITimer;
        this.mTimer = Components.classes["@mozilla.org/timer;1"].
          createInstance(kITimer);
        this.mTimer.init(this, kUpdateCheckDelay, kITimer.TYPE_ONE_SHOT);

        // we are no longer interested in the ``domwindowopened'' topic
        var observerService = Components.
          classes["@mozilla.org/observer-service;1"].
          getService(Components.interfaces.nsIObserverService);
        observerService.removeObserver(this, "domwindowopened");

        // but we are interested in removing our timer reference on XPCOM
        // shutdown so as not to leak.
        observerService.addObserver(this, "xpcom-shutdown", false);
      }
      catch (ex)
      {
        debug("Exception init'ing timer: " + ex);
      }
    }
    else if (aTopic == "timer-callback")
    {
      this.mTimer = null; // free up timer so it can be gc'ed
      this.checkForUpdate();
    }
    else if (aTopic == "xpcom-shutdown")
    {
      /*
       * We need to drop our timer reference here to avoid a leak
       * since the timer keeps a reference to the observer.
       */
      this.mTimer = null;
    }
  },

  checkForUpdate: function()
  {
    debug("checkForUpdate");

    if (this.shouldCheckForUpdate())
    {
      try
      {
        // get update ds URI from prefs
        var prefs = Components.classes["@mozilla.org/preferences-service;1"].
          getService(Components.interfaces.nsIPrefBranch);
        var updateDatasourceURI = prefs.
          getComplexValue(kUNDatasourceURIPref,
          Components.interfaces.nsIPrefLocalizedString).data;

        var rdf = Components.classes["@mozilla.org/rdf/rdf-service;1"].
          getService(Components.interfaces.nsIRDFService);
        var ds = rdf.GetDataSource(updateDatasourceURI);

        ds = ds.QueryInterface(Components.interfaces.nsIRDFXMLSink);
        ds.addXMLSinkObserver(nsUpdateDatasourceObserver);
      }
      catch (ex)
      {
        debug("Exception getting updates.rdf: " + ex);
      }
    }
  },

  shouldCheckForUpdate: function()
  {
    debug("shouldCheckForUpdate");

    var shouldCheck = false;

    try
    {
      var prefs = Components.classes["@mozilla.org/preferences-service;1"].
        getService(Components.interfaces.nsIPrefBranch);

      if (prefs.getBoolPref(kUNEnabledPref))
      {
        var freq = prefs.getIntPref(kUNFrequencyPref) * (24 * 60 * 60); // secs
        var now = (new Date().valueOf())/1000; // secs

        if (!prefs.prefHasUserValue(kUNLastCheckedPref))
        {
          // setting last_checked pref first time so must randomize in
          // order that servers don't get flooded with updates.rdf checks
          // (and eventually downloads of new clients) all at the same time

          var randomizedLastChecked = now + freq * (1 + Math.random());
          prefs.setIntPref(kUNLastCheckedPref, randomizedLastChecked);

          return false;
        }

        var lastChecked = prefs.getIntPref(kUNLastCheckedPref);
        if ((lastChecked + freq) > now)
          return false;

        prefs.setIntPref(kUNLastCheckedPref, now);
        prefs = prefs.QueryInterface(Components.interfaces.nsIPrefService);
        prefs.savePrefFile(null); // flush prefs now

        shouldCheck = true;
      }
    }
    catch (ex)
    {
      shouldCheck = false;
      debug("Exception in shouldCheckForUpdate: " + ex);
    }

    return shouldCheck;
  },

  QueryInterface: function(aIID)
  {
    if (aIID.equals(Components.interfaces.nsIObserver) ||
        aIID.equals(Components.interfaces.nsIProfileStartupListener) ||
        aIID.equals(Components.interfaces.nsISupports))
      return this;

    Components.returnCode = Components.results.NS_ERROR_NO_INTERFACE;
    return null;
  }
}

////////////////////////////////////////////////////////////////////////
//
//   nsUpdateDatasourceObserver : nsIRDFXMLSinkObserver
//
//   Gets relevant info on latest available update after the updates.rdf
//   datasource has completed loading asynchronously.
//
////////////////////////////////////////////////////////////////////////

var nsUpdateDatasourceObserver =
{
  onBeginLoad: function(aSink)
  {
  },

  onInterrupt: function(aSink)
  {
  },

  onResume: function(aSink)
  {
  },

  onEndLoad: function(aSink)
  {
    debug("onEndLoad");

    aSink.removeXMLSinkObserver(this);

    var ds = aSink.QueryInterface(Components.interfaces.nsIRDFDataSource);
    var updateInfo = this.getUpdateInfo(ds);
    if (updateInfo && this.newerVersionAvailable(updateInfo))
    {
      var promptService = Components.
        classes["@mozilla.org/embedcomp/prompt-service;1"].
        getService(Components.interfaces.nsIPromptService);
      var winWatcher = Components.
        classes["@mozilla.org/embedcomp/window-watcher;1"].
        getService(Components.interfaces.nsIWindowWatcher);

      var unBundle = this.getBundle(kUNBundleURI);
      if (!unBundle)
        return;

      var title = unBundle.formatStringFromName("title",
        [updateInfo.productName], 1);
      var desc = unBundle.formatStringFromName("desc",
        [updateInfo.productName], 1);
      var button0Text = unBundle.GetStringFromName("getItNow");
      var button1Text = unBundle.GetStringFromName("noThanks");
      var checkMsg = unBundle.GetStringFromName("dontAskAgain");
      var checkVal = {value:0};

      var result = promptService.confirmEx(winWatcher.activeWindow, title, desc,
        (promptService.BUTTON_POS_0 * promptService.BUTTON_TITLE_IS_STRING) +
        (promptService.BUTTON_POS_1 * promptService.BUTTON_TITLE_IS_STRING),
        button0Text, button1Text, null, checkMsg, checkVal);

      // user wants update now so open new window
      // (result => 0 is button0)
      if (result == 0)
        winWatcher.openWindow(winWatcher.activeWindow, updateInfo.URL,
          "_blank", "", null);

      // if "Don't ask again" was checked disable update notifications
      if (checkVal.value)
      {
        var prefs = Components.classes["@mozilla.org/preferences-service;1"].
          getService(Components.interfaces.nsIPrefBranch);
        prefs.setBoolPref(kUNEnabledPref, false);
      }
    }
  },

  onError: function(aSink, aStatus, aErrorMsg)
  {
    debug("Error " + aStatus + ": " + aErrorMsg);
    aSink.removeXMLSinkObserver(this);
  },

  getUpdateInfo: function(aDS)
  {
    var info = null;

    try
    {
      var rdf = Components.classes["@mozilla.org/rdf/rdf-service;1"].
        getService(Components.interfaces.nsIRDFService);
      var src = "urn:updates:latest";

      info = new Object;
      info.registryName = this.getTarget(rdf, aDS, src, "registryName");
      info.version = this.getTarget(rdf, aDS, src, "version");
      info.URL = this.getTarget(rdf, aDS, src, "URL");
      info.productName = this.getTarget(rdf, aDS, src, "productName");
    }
    catch (ex)
    {
      info = null;
      debug("Exception getting update info: " + ex);

      // NOTE: If the (possibly remote) datasource doesn't exist
      //       or fails to load the first |GetTarget()| call will fail
      //       bringing us to this exception handler.  In turn, we
      //       will fail silently.  Testing has revealed that for a
      //       non-existent datasource (invalid URI) the
      //       |nsIRDFXMLSinkObserver.onEndLoad()| is called instead of
      //       |nsIRDFXMLSinkObserver.onError()| as one may expect.  In
      //       addition, if we QI the aSink parameter of |onEndLoad()|
      //       to an |nsIRDFRemoteDataSource| and check the |loaded|
      //       boolean, it reflects true so we can't use that.  The
      //       safe way to know we have failed to load the datasource
      //       is by handling the first exception as we are doing now.
    }

    return info;
  },

  getTarget: function(aRDF, aDS, aSrc, aProp)
  {
    var src = aRDF.GetResource(aSrc);
    var arc = aRDF.GetResource("http://home.netscape.com/NC-rdf#" + aProp);
    var target = aDS.GetTarget(src, arc, true);
    return target.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
  },

  newerVersionAvailable: function(aUpdateInfo)
  {
    // sanity check
    if (!aUpdateInfo.registryName || !aUpdateInfo.version)
    {
      debug("Sanity check failed: aUpdateInfo is invalid!");
      return false;
    }

    // when we know we are updating the ``Browser'' component
    // we can rely on Necko to give us the app version

    if (aUpdateInfo.registryName == "Browser")
      return this.neckoHaveNewer(aUpdateInfo);

    return this.xpinstallHaveNewer(aUpdateInfo);
  },

  neckoHaveNewer: function(aUpdateInfo)
  {
    try
    {
      var httpHandler = Components.
        classes["@mozilla.org/network/protocol;1?name=http"].
        getService(Components.interfaces.nsIHttpProtocolHandler);
      var synthesized = this.synthesizeVersion(httpHandler.misc,
        httpHandler.productSub);
      var local = new nsVersion(synthesized);
      var server = new nsVersion(aUpdateInfo.version);

      return (server.isNewerThan(local));
    }
    catch (ex)
    {
      // fail silently
      debug("Exception getting httpHandler: " + ex);
      return false;
    }

    return false; // return value expected from this function
  },

  xpinstallHaveNewer: function(aUpdateInfo)
  {
    // XXX Once InstallTrigger is a component we will be able to
    //     get at it without needing to reference it from hiddenDOMWindow.
    //     This will enable us to |compareVersion()|s even when
    //     XPInstall is disabled but update notifications are enabled.
    //     See <http://bugzilla.mozilla.org/show_bug.cgi?id=121506>.
    var ass = Components.classes["@mozilla.org/appshell/appShellService;1"].
      getService(Components.interfaces.nsIAppShellService);
    var trigger = ass.hiddenDOMWindow.InstallTrigger;
    var diffLevel = trigger.compareVersion(aUpdateInfo.registryName,
      aUpdateInfo.version);
    if (diffLevel < trigger.EQUAL && diffLevel != trigger.NOT_FOUND)
      return true;
    return false; // already have newer version or
                  // fail silently if old version not found on disk
  },

  synthesizeVersion: function(aMisc, aProductSub)
  {
    // Strip out portion of nsIHttpProtocolHandler.misc that
    // contains version info and stuff all ``missing'' portions
    // with a default 0 value.  We are interested in the first 3
    // numbers delimited by periods.  The 4th comes from aProductSub.
    // e.g., x => x.0.0, x.1 => x.1.0, x.1.2 => x.1.2, x.1.2.3 => x.1.2

    var synthesized = "0.0.0.";

    // match only digits and periods after "rv:" in the misc
    var onlyVer = /rv:([0-9.]+)/.exec(aMisc);

    // original string in onlyVer[0], matched substring in onlyVer[1]
    if (onlyVer && onlyVer.length >= 2)
    {
      var parts = onlyVer[1].split('.');
      var len = parts.length;
      if (len > 0)
      {
        synthesized = "";

        // extract first 3 dot delimited numbers in misc (after "rv:")
        for (var i = 0; i < 3; ++i)
        {
          synthesized += ((len >= i+1) ? parts[i] : "0") + ".";
        }
      }
    }

    // tack on productSub for nsVersion.mBuild field if available
    synthesized += aProductSub ? aProductSub : "0";

    return synthesized;
  },

  getBundle: function(aURI)
  {
    if (!aURI)
      return null;

    var bundle = null;
    try
    {
      var strBundleService = Components.
        classes["@mozilla.org/intl/stringbundle;1"].
        getService(Components.interfaces.nsIStringBundleService);
      bundle = strBundleService.createBundle(aURI);
    }
    catch (ex)
    {
      bundle = null;
      debug("Exception getting bundle " + aURI + ": " + ex);
    }

    return bundle;
  }
}

////////////////////////////////////////////////////////////////////////
//
//   nsVersion
//
//   Constructs a version object given a string representation.  This
//   constructor populates the mMajor, mMinor, mRelease, and mBuild
//   fields regardless of whether string contains all the fields.
//   The default for all unspecified fields is 0.
//
////////////////////////////////////////////////////////////////////////

function nsVersion(aStringVersion)
{
  var parts = aStringVersion.split('.');
  var len = parts.length;

  this.mMajor   = (len >= 1) ? this.getValidInt(parts[0]) : 0;
  this.mMinor   = (len >= 2) ? this.getValidInt(parts[1]) : 0;
  this.mRelease = (len >= 3) ? this.getValidInt(parts[2]) : 0;
  this.mBuild   = (len >= 4) ? this.getValidInt(parts[3]) : 0;
}

nsVersion.prototype =
{
  isNewerThan: function(aOther)
  {
    if (this.mMajor == aOther.mMajor)
    {
      if (this.mMinor == aOther.mMinor)
      {
        if (this.mRelease == aOther.mRelease)
        {
          if (this.mBuild <= aOther.mBuild)
            return false;
          else
            return true; // build is newer
        }
        else if (this.mRelease < aOther.mRelease)
          return false;
        else
          return true; // release is newer
      }
      else if (this.mMinor < aOther.mMinor)
        return false;
      else
        return true; // minor is newer
    }
    else if (this.mMajor < aOther.mMajor)
      return false;
    else
      return true; // major is newer

    return false;
  },

  getValidInt: function(aString)
  {
    var integer = parseInt(aString);
    if (isNaN(integer))
      return 0;
    return integer;
  }
}

////////////////////////////////////////////////////////////////////////
//
//   nsUpdateNotifierModule : nsIModule
//
////////////////////////////////////////////////////////////////////////

var nsUpdateNotifierModule =
{
  mClassName:     "Update Notifier",
  mContractID:    "@mozilla.org/update-notifier;1",
  mClassID:       Components.ID("8b6dcf5e-3b5a-4fff-bff5-65a8fa9d71b2"),

  getClassObject: function(aCompMgr, aCID, aIID)
  {
    if (!aCID.equals(this.mClassID))
      throw Components.results.NS_ERROR_NO_INTERFACE;
    if (!aIID.equals(Components.interfaces.nsIFactory))
      throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

    return this.mFactory;
  },

  registerSelf: function(aCompMgr, aFileSpec, aLocation, aType)
  {
    if (kDebug)
      dump("*** Registering nsUpdateNotifier (a JavaScript Module)\n");

    aCompMgr = aCompMgr.QueryInterface(
                 Components.interfaces.nsIComponentRegistrar);
    aCompMgr.registerFactoryLocation(this.mClassID, this.mClassName,
      this.mContractID, aFileSpec, aLocation, aType);

    // receive startup notification from the profile manager
    // (we get |createInstance()|d at startup-notification time)
    this.getCategoryManager().addCategoryEntry("profile-startup-category",
      this.mContractID, "", true, true);
  },

  unregisterSelf: function(aCompMgr, aFileSpec, aLocation)
  {
    aCompMgr = aCompMgr.QueryInterface(
                 Components.interfaces.nsIComponentRegistrar);
    aCompMgr.unregisterFactoryLocation(this.mClassID, aFileSpec);

    this.getCategoryManager().deleteCategoryEntry("profile-startup-category",
      this.mContractID, true);
  },

  canUnload: function(aCompMgr)
  {
    return true;
  },

  getCategoryManager: function()
  {
    return Components.classes["@mozilla.org/categorymanager;1"].
      getService(Components.interfaces.nsICategoryManager);
  },

  //////////////////////////////////////////////////////////////////////
  //
  //   mFactory : nsIFactory
  //
  //////////////////////////////////////////////////////////////////////
  mFactory:
  {
    createInstance: function(aOuter, aIID)
    {
      if (aOuter != null)
        throw Components.results.NS_ERROR_NO_AGGREGATION;
      if (!aIID.equals(Components.interfaces.nsIObserver) &&
          !aIID.equals(Components.interfaces.nsIProfileStartupListener) &&
          !aIID.equals(Components.interfaces.nsISupports))
        throw Components.results.NS_ERROR_INVALID_ARG;

      // return the singleton
      return nsUpdateNotifier.QueryInterface(aIID);
    },

    lockFactory: function(aLock)
    {
      // quiten warnings
    }
  }
};

function NSGetModule(aCompMgr, aFileSpec)
{
  return nsUpdateNotifierModule;
}

////////////////////////////////////////////////////////////////////////
//
//   Debug helper
//
////////////////////////////////////////////////////////////////////////
if (!kDebug)
  debug = function(m) {};
else
  debug = function(m) {dump("\t *** nsUpdateNotifier: " + m + "\n");};
