/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let SocialService = Components.utils.import("resource://gre/modules/SocialService.jsm", {}).SocialService;

// A helper to run a suite of tests.
// The "test object" should be an object with function names as keys and a
// function as the value.  The functions will be called with a "cbnext" param
// which should be called when the test is complete.
// eg:
// test = {
//   foo: function(cbnext) {... cbnext();}
// }
function runTests(tests, cbPreTest, cbPostTest, cbFinish) {
  let testIter = Iterator(tests);

  if (cbPreTest === undefined) {
    cbPreTest = function(cb) {cb()};
  }
  if (cbPostTest === undefined) {
    cbPostTest = function(cb) {cb()};
  }

  function runNextTest() {
    let name, func;
    try {
      [name, func] = testIter.next();
    } catch (err if err instanceof StopIteration) {
      // out of items:
      (cbFinish || finish)();
      return;
    }
    // We run on a timeout as the frameworker also makes use of timeouts, so
    // this helps keep the debug messages sane.
    executeSoon(function() {
      function cleanupAndRunNextTest() {
        info("sub-test " + name + " complete");
        cbPostTest(runNextTest);
      }
      cbPreTest(function() {
        info("sub-test " + name + " starting");
        try {
          func.call(tests, cleanupAndRunNextTest);
        } catch (ex) {
          ok(false, "sub-test " + name + " failed: " + ex.toString() +"\n"+ex.stack);
          cleanupAndRunNextTest();
        }
      })
    });
  }
  runNextTest();
}

// A mock notifications server.  Based on:
// dom/tests/mochitest/notification/notification_common.js
const FAKE_CID = Cc["@mozilla.org/uuid-generator;1"].
    getService(Ci.nsIUUIDGenerator).generateUUID();

const ALERTS_SERVICE_CONTRACT_ID = "@mozilla.org/alerts-service;1";
const ALERTS_SERVICE_CID = Components.ID(Cc[ALERTS_SERVICE_CONTRACT_ID].number);

function MockAlertsService() {}

MockAlertsService.prototype = {

    showAlertNotification: function(imageUrl, title, text, textClickable,
                                    cookie, alertListener, name) {
        let obData = JSON.stringify({
          imageUrl: imageUrl,
          title: title,
          text:text,
          textClickable: textClickable,
          cookie: cookie,
          name: name
        });
        Services.obs.notifyObservers(null, "social-test:notification-alert", obData);

        if (textClickable) {
          // probably should do this async....
          alertListener.observe(null, "alertclickcallback", cookie);
        }

        alertListener.observe(null, "alertfinished", cookie);
    },

    QueryInterface: function(aIID) {
        if (aIID.equals(Ci.nsISupports) ||
            aIID.equals(Ci.nsIAlertsService))
            return this;
        throw Cr.NS_ERROR_NO_INTERFACE;
    }
};

var factory = {
    createInstance: function(aOuter, aIID) {
        if (aOuter != null)
            throw Cr.NS_ERROR_NO_AGGREGATION;
        return new MockAlertsService().QueryInterface(aIID);
    }
};

function replaceAlertsService() {
  Components.manager.QueryInterface(Ci.nsIComponentRegistrar)
            .registerFactory(FAKE_CID, "",
                             ALERTS_SERVICE_CONTRACT_ID,
                             factory)
}

function restoreAlertsService() {
  Components.manager.QueryInterface(Ci.nsIComponentRegistrar)
            .registerFactory(ALERTS_SERVICE_CID, "",
                             ALERTS_SERVICE_CONTRACT_ID,
                             null);
}
// end of alerts service mock.
