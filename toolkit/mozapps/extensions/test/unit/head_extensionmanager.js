/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/*
 * These are intended to be a set of base functions enabling unit testing
 * for the Extension Manager component.
 */

const PREFIX_NS_EM                    = "http://www.mozilla.org/2004/em-rdf#";
const PREFIX_NS_CHROME                = "http://www.mozilla.org/rdf/chrome#";
const PREFIX_ITEM_URI                 = "urn:mozilla:item:";

const NS_INSTALL_LOCATION_APPPROFILE = "app-profile";

const XULAPPINFO_CONTRACTID = "@mozilla.org/xre/app-info;1";
const XULAPPINFO_CID = Components.ID("{c763b610-9d49-455a-bbd2-ede71682a1ac}");

var gXULAppInfo = null;
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
  return do_get_file("addons/" + name + ".xpi");
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
  gXULAppInfo = {
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
    invalidateCachesOnRestart: function invalidateCachesOnRestart() {},

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
      return gXULAppInfo.QueryInterface(iid);
    }
  };
  var registrar = Components.manager.QueryInterface(Components.interfaces.nsIComponentRegistrar);
  registrar.registerFactory(XULAPPINFO_CID, "XULAppInfo",
                            XULAPPINFO_CONTRACTID, XULAppInfoFactory);
}

function startEM(upgraded) {
  var needsRestart = false;
  if (upgraded) {
    try {
      needsRestart = gEM.checkForMismatches();
    }
    catch (e) {
      do_throw("checkForMismatches threw an exception: " + e + "\n");
      needsRestart = false;
      upgraded = false;
    }
  }

  if (!upgraded || !needsRestart)
    needsRestart = gEM.start();

  if (needsRestart)
    restartEM();
}

/**
 * This simulates an application startup. Since we will be starting from an
 * empty profile we follow that path.
 */
function startupEM()
{
  gEM = Components.classes["@mozilla.org/extensions/manager;1"]
                  .getService(Components.interfaces.nsIExtensionManager);

  gEM.QueryInterface(Components.interfaces.nsIObserver);
  gEM.observe(null, "profile-after-change", "startup");

  // First run is a new profile which nsAppRunner would consider as an update
  // (no existing compatibility.ini)
  startEM(true);
}

/**
 * Simple function to simulate the termination of an app to the EM.
 * This harness does not support creating a new EM after calling this.
 */
function shutdownEM()
{
  // xpcshell calls xpcom-shutdown so we don't actually do anything here.
  gEM = null;
}

/**
 * Many operations require restarts to take effect. This function should
 * perform all that is necessary for this to happen.
 */
function restartEM(newVersion)
{
  if (newVersion) {
    gXULAppInfo.version = newVersion;
    startEM(true);
  }
  else {
    startEM(false);
  }
}

var gDirSvc = Components.classes["@mozilla.org/file/directory_service;1"]
                        .getService(Components.interfaces.nsIProperties);

// Need to create and register a profile folder.
var gProfD = do_get_profile();

var gPrefs = Components.classes["@mozilla.org/preferences-service;1"]
                   .getService(Components.interfaces.nsIPrefBranch);
// Enable more extensive EM logging
gPrefs.setBoolPref("extensions.logging.enabled", true);
