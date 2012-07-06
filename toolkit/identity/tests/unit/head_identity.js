/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This has the side-effect of populating Cc, Ci, Cu, Cr. It's best not to
// ask questions and just accept it.
do_load_httpd_js();

// The following boilerplate makes sure that XPCom calls
// that use the profile directory work.

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "jwcrypto",
                                  "resource://gre/modules/identity/jwcrypto.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "IDService",
                                  "resource://gre/modules/identity/Identity.jsm",
                                  "IdentityService");

XPCOMUtils.defineLazyModuleGetter(this,
                                  "IdentityStore",
                                  "resource://gre/modules/identity/IdentityStore.jsm");

XPCOMUtils.defineLazyModuleGetter(this,
                                  "Logger",
                                  "resource://gre/modules/identity/LogUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this,
                                   "uuidGenerator",
                                   "@mozilla.org/uuid-generator;1",
                                   "nsIUUIDGenerator");

const TEST_URL = "https://myfavoritebacon.com";
const TEST_URL2 = "https://myfavoritebaconinacan.com";
const TEST_USER = "user@mozilla.com";
const TEST_PRIVKEY = "fake-privkey";
const TEST_CERT = "fake-cert";
const TEST_IDPPARAMS = {
  domain: "myfavoriteflan.com",
  authentication: "/foo/authenticate.html",
  provisioning: "/foo/provision.html"
};

let XULAppInfo = {
  vendor: "Mozilla",
  name: "XPCShell",
  ID: "xpcshell@tests.mozilla.org",
  version: "1",
  appBuildID: "20100621",
  platformVersion: "",
  platformBuildID: "20100621",
  inSafeMode: false,
  logConsoleErrors: true,
  OS: "XPCShell",
  XPCOMABI: "noarch-spidermonkey",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIXULAppInfo, Ci.nsIXULRuntime]),
  invalidateCachesOnRestart: function invalidateCachesOnRestart() { }
};

let XULAppInfoFactory = {
  createInstance: function (outer, iid) {
    if (outer != null)
      throw Cr.NS_ERROR_NO_AGGREGATION;
    return XULAppInfo.QueryInterface(iid);
  }
};

let registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
registrar.registerFactory(Components.ID("{fbfae60b-64a4-44ef-a911-08ceb70b9f31}"),
                          "XULAppInfo", "@mozilla.org/xre/app-info;1",
                          XULAppInfoFactory);

// The following are utility functions for Identity testing

function log(...aMessageArgs) {
  Logger.log(["test"].concat(aMessageArgs));
}

function get_idstore() {
  return IdentityStore;
}

function partial(fn) {
  let args = Array.prototype.slice.call(arguments, 1);
  return function() {
    return fn.apply(this, args.concat(Array.prototype.slice.call(arguments)));
  };
}

function uuid() {
  return uuidGenerator.generateUUID().toString();
}

// create a mock "doc" object, which the Identity Service
// uses as a pointer back into the doc object
function mock_doc(aIdentity, aOrigin, aDoFunc) {
  let mockedDoc = {};
  mockedDoc.id = uuid();
  mockedDoc.loggedInEmail = aIdentity;
  mockedDoc.origin = aOrigin;
  mockedDoc['do'] = aDoFunc;
  mockedDoc.doReady = partial(aDoFunc, 'ready');
  mockedDoc.doLogin = partial(aDoFunc, 'login');
  mockedDoc.doLogout = partial(aDoFunc, 'logout');
  mockedDoc.doError = partial(aDoFunc, 'error');
  mockedDoc.doCancel = partial(aDoFunc, 'cancel');
  mockedDoc.doCoffee = partial(aDoFunc, 'coffee');

  return mockedDoc;
}

// mimicking callback funtionality for ease of testing
// this observer auto-removes itself after the observe function
// is called, so this is meant to observe only ONE event.
function makeObserver(aObserveTopic, aObserveFunc) {
  let observer = {
    // nsISupports provides type management in C++
    // nsIObserver is to be an observer
    QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports, Ci.nsIObserver]),

    observe: function (aSubject, aTopic, aData) {
      if (aTopic == aObserveTopic) {
        aObserveFunc(aSubject, aTopic, aData);
        Services.obs.removeObserver(observer, aObserveTopic);
      }
    }
  };

  Services.obs.addObserver(observer, aObserveTopic, false);
}

// set up the ID service with an identity with keypair and all
// when ready, invoke callback with the identity
function setup_test_identity(identity, cert, cb) {
  // set up the store so that we're supposed to be logged in
  let store = get_idstore();

  function keyGenerated(err, kpo) {
    store.addIdentity(identity, kpo, cert);
    cb();
  };

  jwcrypto.generateKeyPair("DS160", keyGenerated);
}

// takes a list of functions and returns a function that
// when called the first time, calls the first func,
// then the next time the second, etc.
function call_sequentially() {
  let numCalls = 0;
  let funcs = arguments;

  return function() {
    if (!funcs[numCalls]) {
      let argString = Array.prototype.slice.call(arguments).join(",");
      do_throw("Too many calls: " + argString);
      return;
    }
    funcs[numCalls].apply(funcs[numCalls],arguments);
    numCalls += 1;
  };
}

/*
 * Setup a provisioning workflow with appropriate callbacks
 *
 * identity is the email we're provisioning.
 *
 * afterSetupCallback is required.
 *
 * doneProvisioningCallback is optional, if the caller
 * wants to be notified when the whole provisioning workflow is done
 *
 * frameCallbacks is optional, contains the callbacks that the sandbox
 * frame would provide in response to DOM calls.
 */
function setup_provisioning(identity, afterSetupCallback, doneProvisioningCallback, callerCallbacks) {
  IDService.reset();

  let provId = uuid();
  IDService.IDP._provisionFlows[provId] = {
    identity : identity,
    idpParams: TEST_IDPPARAMS,
    callback: function(err) {
      if (doneProvisioningCallback)
        doneProvisioningCallback(err);
    },
    sandbox: {
	// Emulate the free() method on the iframe sandbox
	free: function() {}
    }
  };

  let caller = {};
  caller.id = provId;
  caller.doBeginProvisioningCallback = function(id, duration_s) {
    if (callerCallbacks && callerCallbacks.beginProvisioningCallback)
      callerCallbacks.beginProvisioningCallback(id, duration_s);
  };
  caller.doGenKeyPairCallback = function(pk) {
    if (callerCallbacks && callerCallbacks.genKeyPairCallback)
      callerCallbacks.genKeyPairCallback(pk);
  };

  afterSetupCallback(caller);
}

// Switch debug messages on by default
Services.prefs.setBoolPref("toolkit.identity.debug", true);
