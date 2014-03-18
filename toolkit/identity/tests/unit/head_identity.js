/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://testing-common/httpd.js");

// XXX until bug 937114 is fixed
Cu.importGlobalProperties(["atob"]);

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

const TEST_MESSAGE_MANAGER = "Mr McFeeley";
const TEST_URL = "https://myfavoritebacon.com";
const TEST_URL2 = "https://myfavoritebaconinacan.com";
const TEST_USER = "user@mozilla.com";
const TEST_PRIVKEY = "fake-privkey";
const TEST_CERT = "fake-cert";
const TEST_ASSERTION = "fake-assertion";
const TEST_IDPPARAMS = {
  domain: "myfavoriteflan.com",
  authentication: "/foo/authenticate.html",
  provisioning: "/foo/provision.html"
};

// The following are utility functions for Identity testing

function log(...aMessageArgs) {
  Logger.log.apply(Logger, ["test"].concat(aMessageArgs));
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

function base64UrlDecode(s) {
  s = s.replace(/-/g, "+");
  s = s.replace(/_/g, "/");

  // Replace padding if it was stripped by the sender.
  // See http://tools.ietf.org/html/rfc4648#section-4
  switch (s.length % 4) {
    case 0:
      break; // No pad chars in this case
    case 2:
      s += "==";
      break; // Two pad chars
    case 3:
      s += "=";
      break; // One pad char
    default:
      throw new InputException("Illegal base64url string!");
  }

  // With correct padding restored, apply the standard base64 decoder
  return atob(s);
}

// create a mock "doc" object, which the Identity Service
// uses as a pointer back into the doc object
function mock_doc(aIdentity, aOrigin, aDoFunc) {
  let mockedDoc = {};
  mockedDoc.id = uuid();
  mockedDoc.loggedInUser = aIdentity;
  mockedDoc.origin = aOrigin;
  mockedDoc["do"] = aDoFunc;
  mockedDoc._mm = TEST_MESSAGE_MANAGER;
  mockedDoc.doReady = partial(aDoFunc, "ready");
  mockedDoc.doLogin = partial(aDoFunc, "login");
  mockedDoc.doLogout = partial(aDoFunc, "logout");
  mockedDoc.doError = partial(aDoFunc, "error");
  mockedDoc.doCancel = partial(aDoFunc, "cancel");
  mockedDoc.doCoffee = partial(aDoFunc, "coffee");
  mockedDoc.childProcessShutdown = partial(aDoFunc, "child-process-shutdown");

  mockedDoc.RP = mockedDoc;

  return mockedDoc;
}

function mock_fxa_rp(aIdentity, aOrigin, aDoFunc) {
  let mockedDoc = {};
  mockedDoc.id = uuid();
  mockedDoc.emailHint = aIdentity;
  mockedDoc.origin = aOrigin;
  mockedDoc.wantIssuer = "firefox-accounts";
  mockedDoc._mm = TEST_MESSAGE_MANAGER;

  mockedDoc.doReady = partial(aDoFunc, "ready");
  mockedDoc.doLogin = partial(aDoFunc, "login");
  mockedDoc.doLogout = partial(aDoFunc, "logout");
  mockedDoc.doError = partial(aDoFunc, "error");
  mockedDoc.doCancel = partial(aDoFunc, "cancel");
  mockedDoc.childProcessShutdown = partial(aDoFunc, "child-process-shutdown");

  mockedDoc.RP = mockedDoc;

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
let initialPrefDebugValue = false;
try {
  initialPrefDebugValue = Services.prefs.getBoolPref("toolkit.identity.debug");
} catch(noPref) {}
Services.prefs.setBoolPref("toolkit.identity.debug", true);

// Switch on firefox accounts
let initialPrefFXAValue = false;
try {
  initialPrefFXAValue = Services.prefs.getBoolPref("identity.fxaccounts.enabled");
} catch(noPref) {}
Services.prefs.setBoolPref("identity.fxaccounts.enabled", true);

// after execution, restore prefs
do_register_cleanup(function() {
  log("restoring prefs to their initial values");
  Services.prefs.setBoolPref("toolkit.identity.debug", initialPrefDebugValue);
  Services.prefs.setBoolPref("identity.fxaccounts.enabled", initialPrefFXAValue);
});


