/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/FxAccountsCommon.js");
Cu.import("resource://gre/modules/FxAccountsProfileChannel.jsm");

const URL_STRING = "https://example.com";

add_test(function () {
  validationHelper(undefined,
  "Error: Missing configuration options");

  validationHelper({},
  "Error: Missing 'content_uri' option");

  validationHelper({ content_uri: 'bad uri' },
  /NS_ERROR_MALFORMED_URI/);

  run_next_test();
});

add_test(function () {
  var mockMessage = {
    command: "profile:change",
    data: { uid: "foo" }
  };

  makeObserver(ON_PROFILE_CHANGE_NOTIFICATION, function (subject, topic, data) {
    do_check_eq(data, "foo");
    run_next_test();
  });

  var channel = new FxAccountsProfileChannel({
    content_uri: URL_STRING
  });

  channel._channelCallback(PROFILE_WEBCHANNEL_ID, mockMessage);
});

function run_test() {
  run_next_test();
}

function makeObserver(aObserveTopic, aObserveFunc) {
  let callback = function (aSubject, aTopic, aData) {
    log.debug("observed " + aTopic + " " + aData);
    if (aTopic == aObserveTopic) {
      removeMe();
      aObserveFunc(aSubject, aTopic, aData);
    }
  };

  function removeMe() {
    log.debug("removing observer for " + aObserveTopic);
    Services.obs.removeObserver(callback, aObserveTopic);
  }

  Services.obs.addObserver(callback, aObserveTopic, false);
  return removeMe;
}

function validationHelper(params, expected) {
  try {
    new FxAccountsProfileChannel(params);
  } catch (e) {
    if (typeof expected === 'string') {
      return do_check_eq(e.toString(), expected);
    } else {
      return do_check_true(e.toString().match(expected));
    }
  }
  throw new Error("Validation helper error");
}
