"use strict";

const {HttpServer} = ChromeUtils.import("resource://testing-common/httpd.js");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

const ReferrerInfo = Components.Constructor("@mozilla.org/referrer-info;1",
                                            "nsIReferrerInfo",
                                            "init");

let observer = {
  QueryInterface: function eventsink_qi(iid) {
    if (iid.equals(Ci.nsISupports) ||
        iid.equals(Ci.nsIObserver))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  observe: function(subject, topic, data) {
    subject = subject.QueryInterface(Ci.nsIRequest);
    subject.cancel(Cr.NS_BINDING_ABORTED);

    // ENSURE_CALLED_BEFORE_CONNECT: setting values should still work
    try {
      subject.QueryInterface(Ci.nsIHttpChannel);
      let currentReferrer = subject.getRequestHeader("Referer");
      Assert.equal(currentReferrer, "http://site1.com/");
      let uri = Services.io.newURI("http://site2.com");
      subject.referrerInfo = new ReferrerInfo(Ci.nsIHttpChannel.REFERRER_POLICY_UNSET, true, uri);
    } catch (ex) {
      do_throw("Exception: " + ex);
    }

    let obs = Cc["@mozilla.org/observer-service;1"].getService();
    obs = obs.QueryInterface(Ci.nsIObserverService);
    obs.removeObserver(observer, "http-on-modify-request");
  }
};

function run_test() {

  let obs = Cc["@mozilla.org/observer-service;1"].getService();
  obs = obs.QueryInterface(Ci.nsIObserverService);
  obs.addObserver(observer, "http-on-modify-request");

  run_test_in_child("../unit/test_httpcancel.js");
}
