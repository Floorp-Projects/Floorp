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
        const kIScriptableTimer = Components.interfaces.nsIScriptableTimer;
        mTimer = Components.classes["@mozilla.org/timer;1"].
          createInstance(kIScriptableTimer);
        mTimer.init(this, kUpdateCheckDelay, kIScriptableTimer.PRIORITY_NORMAL,
          kIScriptableTimer.TYPE_ONE_SHOT);

        // we are no longer interested in the ``domwindowopened'' topic
        var observerService = Components.
          classes["@mozilla.org/observer-service;1"].
          getService(Components.interfaces.nsIObserverService);
        observerService.removeObserver(this, "domwindowopened");
      }
      catch (ex)
      {
        debug("Exception init'ing timer: " + ex);
      }
    }
    else if (aTopic == "timer-callback")
    {
      mTimer = null; // free up timer so it can be gc'ed
      this.checkForUpdate();
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
    if (!aIID.equals(Components.interfaces.nsIObserver) &&
        !aIID.equals(Components.interfaces.nsIProfileStartupListener) &&
        !aIID.equals(Components.interfaces.nsISupports))
      throw Components.results.NS_ERROR_NO_INTERFACE;

    return this;
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

    // XXX Once InstallTrigger is a component we will be able to
    //     get at it without needing to reference it from hiddenDOMWindow.
    //     This will enable us to |compareVersion()|s even when 
    //     XPInstall is disabled but update notifications are enabled.
    //     See <http://bugzilla.mozilla.org/show_bug.cgi?id=121506>.
    //
    //     Also, once |compareVersion()| is changed to return -5 when
    //     the component was not registered/was removed we should change
    //     this code to handle the situation and inform users that an
    //     update is available.
    //     See <http://bugzilla.mozilla.org/show_bug.cgi?id=119370>.
    var ass = Components.classes["@mozilla.org/appshell/appShellService;1"].
      getService(Components.interfaces.nsIAppShellService);
    var diffLevel = ass.hiddenDOMWindow.InstallTrigger.compareVersion(
                      aUpdateInfo.registryName, aUpdateInfo.version);
    return (diffLevel < 0);
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

    // XXX The new version of nsIComponentManager (once completed)
    //     will obviate the need to QI to nsIComponentManagerObsolete.
    //     See <http://bugzilla.mozilla.org/show_bug.cgi?id=115853>.
    aCompMgr = aCompMgr.QueryInterface(
                 Components.interfaces.nsIComponentManagerObsolete);
    aCompMgr.registerComponentWithType(this.mClassID, this.mClassName,
      this.mContractID, aFileSpec, aLocation, true, true, aType);

    // receive startup notification from the profile manager
    // (we get |createInstance()|d at startup-notification time)
    this.getCategoryManager().addCategoryEntry("profile-startup-category", 
      this.mContractID, "", true, true);
  },

  unregisterSelf: function(aCompMgr, aFileSpec, aLocation)
  {
    // XXX The new version of nsIComponentManager (once completed)
    //     will obviate the need to QI to nsIComponentManagerObsolete.
    //     See <http://bugzilla.mozilla.org/show_bug.cgi?id=115853>.
    aCompMgr = aCompMgr.QueryInterface(
                 Components.interfaces.nsIComponentManagerObsolete);
    aCompMgr.unregisterComponentSpec(this.mClassID, aFileSpec);

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
