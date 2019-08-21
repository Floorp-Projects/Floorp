"use strict";

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const ReferrerInfo = Components.Constructor(
  "@mozilla.org/referrer-info;1",
  "nsIReferrerInfo",
  "init"
);

let observer = null;

function run_test() {
  do_await_remote_message("register-observer").then(() => {
    observer = {
      QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),

      observe(subject, topic, data) {
        subject = subject.QueryInterface(Ci.nsIRequest);
        subject.cancel(Cr.NS_BINDING_ABORTED);

        // ENSURE_CALLED_BEFORE_CONNECT: setting values should still work
        try {
          subject.QueryInterface(Ci.nsIHttpChannel);
          let currentReferrer = subject.getRequestHeader("Referer");
          Assert.equal(currentReferrer, "http://site1.com/");
          let uri = Services.io.newURI("http://site2.com");
          subject.referrerInfo = new ReferrerInfo(
            Ci.nsIReferrerInfo.EMPTY,
            true,
            uri
          );
        } catch (ex) {
          do_throw("Exception: " + ex);
        }
      },
    };

    Services.obs.addObserver(observer, "http-on-modify-request");
    do_send_remote_message("register-observer-done");
  });

  do_await_remote_message("unregister-observer").then(() => {
    Services.obs.removeObserver(observer, "http-on-modify-request");

    do_send_remote_message("unregister-observer-done");
  });

  run_test_in_child("../unit/test_httpcancel.js");
}
