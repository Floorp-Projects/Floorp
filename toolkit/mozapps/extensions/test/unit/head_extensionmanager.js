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
 *      Dave Townsend <dtownsend@oxymoronical.com>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
 * ***** END LICENSE BLOCK *****
 */

/*
 * These are intended to be a set of base functions enabling unit testing
 * for the Extension Manager component.
 */

const PREFIX_NS_EM                    = "http://www.mozilla.org/2004/em-rdf#";
const PREFIX_NS_CHROME                = "http://www.mozilla.org/rdf/chrome#";
const PREFIX_ITEM_URI                 = "urn:mozilla:item:";

const NS_APP_USER_PROFILE_50_DIR      = "ProfD";
const NS_APP_PROFILE_DIR_STARTUP      = "ProfDS";
const NS_OS_TEMP_DIR                  = "TmpD";

const NS_INSTALL_LOCATION_APPPROFILE = "app-profile";

const XULAPPINFO_CONTRACTID = "@mozilla.org/xre/app-info;1";
const XULAPPINFO_CID = Components.ID("{c763b610-9d49-455a-bbd2-ede71682a1ac}");

var gEM = null;
var gRDF = Components.classes["@mozilla.org/rdf/rdf-service;1"]
                     .getService(Components.interfaces.nsIRDFService);

/**
 * Utility functions
 */
function EM_NS(property) {
  return PREFIX_NS_EM + property;
}

function CHROME_NS(property) {
  return PREFIX_NS_CHROME + property;
}

function EM_R(property) {
  return gRDF.GetResource(EM_NS(property));
}

function EM_L(literal) {
  return gRDF.GetLiteral(literal);
}

function EM_I(integer) {
  return gRDF.GetIntLiteral(integer);
}

function EM_D(integer) {
  return gRDF.GetDateLiteral(integer);
}

/**
 * Extract the string value from a RDF Literal or Resource
 * @param   literalOrResource
 *          RDF String Literal or Resource
 * @returns String value of the literal or resource, or undefined if the object
 *          supplied is not a RDF string literal or resource.
 */
function stringData(literalOrResource) {
  if (literalOrResource instanceof Components.interfaces.nsIRDFLiteral)
    return literalOrResource.Value;
  if (literalOrResource instanceof Components.interfaces.nsIRDFResource)
    return literalOrResource.Value;
  return undefined;
}

/**
 * Extract the integer value of a RDF Literal
 * @param   literal
 *          nsIRDFInt literal
 * @return  integer value of the literal
 */
function intData(literal) {
  if (literal instanceof Components.interfaces.nsIRDFInt)
    return literal.Value;
  return undefined;
}

/**
 * Gets a RDF Resource for item with the given ID
 * @param   id
 *          The GUID of the item to construct a RDF resource to the 
 *          active item for
 * @returns The RDF Resource to the Active item. 
 */
function getResourceForID(id) {
  return gRDF.GetResource(PREFIX_ITEM_URI + id);
}

/**
 * Extract a string property for an add-on
 * @param   id
 *          ID of the add-on to retrieve a property for
 * @param   property
 *          The localname of the property
 * @returns String value of the property or undefined if the property
 *          does not exist
 */
function getManifestProperty(id, property) {
  return stringData(gEM.datasource.GetTarget(getResourceForID(id), EM_R(property), true));
}

/**
 * Returns a testcase xpi
 * @param   name
 *          The name of the testcase (without extension)
 * @returns an nsILocalFile pointing to the testcase xpi
 */
function do_get_addon(name)
{
  var lf = gTestRoot.clone();
  lf.append("unit");
  lf.append("addons");
  lf.append(name + ".xpi");
  if (!lf.exists())
    do_throw("Addon "+name+" does not exist.");

  return lf;
}

/**
 * Creates an nsIXULAppInfo
 * @param   id
 *          The ID of the test application
 * @param   name
 *          A name for the test application
 * @param   version
 *          The version of the application
 * @param   platformVersion
 *          The gecko version of the application
 */
function createAppInfo(id, name, version, platformVersion)
{
  var XULAppInfo = {
    vendor: "Mozilla",
    name: name,
    ID: id,
    version: version,
    appBuildID: "2007010101",
    platformVersion: platformVersion,
    platformBuildID: "2007010101",
    inSafeMode: false,
    logConsoleErrors: true,
    OS: "XPCShell",
    XPCOMABI: "noarch-spidermonkey",
    
    QueryInterface: function QueryInterface(iid) {
      if (iid.equals(Components.interfaces.nsIXULAppInfo)
       || iid.equals(Components.interfaces.nsIXULRuntime)
       || iid.equals(Components.interfaces.nsISupports))
        return this;
    
      throw Components.results.NS_ERROR_NO_INTERFACE;
    }
  };
  
  var XULAppInfoFactory = {
    createInstance: function (outer, iid) {
      if (outer != null)
        throw Components.results.NS_ERROR_NO_AGGREGATION;
      return XULAppInfo.QueryInterface(iid);
    }
  };
  var registrar = Components.manager.QueryInterface(Components.interfaces.nsIComponentRegistrar);
  registrar.registerFactory(XULAPPINFO_CID, "XULAppInfo",
                            XULAPPINFO_CONTRACTID, XULAppInfoFactory);
}

/**
 * This simulates an application startup. Since we will be starting from an
 * empty profile we follow that path.
 */
function startupEM()
{
  // Make sure the update service is initialised.
  var updateSvc = Components.classes["@mozilla.org/updates/update-service;1"]
                            .getService(Components.interfaces.nsISupports);
  
  gEM = Components.classes["@mozilla.org/extensions/manager;1"]
                  .getService(Components.interfaces.nsIExtensionManager);
  
  gEM.QueryInterface(Components.interfaces.nsIObserver);
  gEM.observe(null, "profile-after-change", "startup");

  // First run is a new profile which nsAppRunner would consider as an update
  // (no existing compatibility.ini)
  var upgraded = true;
  var needsRestart = false;
  try {
    needsRestart = gEM.checkForMismatches();
  }
  catch (e) {
    dump("checkForMismatches threw an exception: " + e + "\n");
    needsRestart = false;
    upgraded = false;
  }

  if (!upgraded || !needsRestart)
    needsRestart = gEM.start(null);
}

/**
 * Simple function to simulate the termination of an app to the EM.
 * This harness does not support creating a new EM after calling this.
 */
function shutdownEM()
{
  // xpcshell calls xpcom-shutdown so we don't actually do anything here.
  gDirSvc.unregisterProvider(dirProvider);
  gEM = null;
}

/**
 * Many operations require restarts to take effect. This function should
 * perform all that is necessary for this to happen.
 */
function restartEM()
{
  var needsRestart = gEM.start(null);
  if (needsRestart)
    gEM.start(null);
}

var gDirSvc = Components.classes["@mozilla.org/file/directory_service;1"]
                        .getService(Components.interfaces.nsIProperties);
var gTestRoot = gDirSvc.get("CurProcD", Components.interfaces.nsILocalFile);
gTestRoot = gTestRoot.parent.parent;
gTestRoot.append("_tests");
gTestRoot.append("xpcshell-simple");
gTestRoot.append("test_extensionmanager");
gTestRoot.normalize();

// Need to create and register a profile folder.
var gProfD = gTestRoot.clone();
gProfD.append("profile");
if (gProfD.exists())
  gProfD.remove(true);
gProfD.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0755);

var dirProvider = {
  getFile: function(prop, persistent) {
    persistent.value = true;
    if (prop == NS_APP_USER_PROFILE_50_DIR ||
        prop == NS_APP_PROFILE_DIR_STARTUP)
      return gProfD.clone();
    return null;
  },
  QueryInterface: function(iid) {
    if (iid.equals(Components.interfaces.nsIDirectoryServiceProvider) ||
        iid.equals(Components.interfaces.nsISupports)) {
      return this;
    }
    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
};
gDirSvc.QueryInterface(Components.interfaces.nsIDirectoryService)
       .registerProvider(dirProvider);

var gPrefs = Components.classes["@mozilla.org/preferences;1"]
                   .getService(Components.interfaces.nsIPrefBranch);
// Enable more extensive EM logging
gPrefs.setBoolPref("extensions.logging.enabled", true);
