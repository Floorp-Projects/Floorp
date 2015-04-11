/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/FxAccountsCommon.js");
const { FxAccountsWebChannel, FxAccountsWebChannelHelpers } =
    Cu.import("resource://gre/modules/FxAccountsWebChannel.jsm");

const URL_STRING = "https://example.com";

const mockSendingContext = {
  browser: {},
  principal: {},
  eventTarget: {}
};

add_test(function () {
  validationHelper(undefined,
  "Error: Missing configuration options");

  validationHelper({
    channel_id: WEBCHANNEL_ID
  },
  "Error: Missing 'content_uri' option");

  validationHelper({
    content_uri: 'bad uri',
    channel_id: WEBCHANNEL_ID
  },
  /NS_ERROR_MALFORMED_URI/);

  validationHelper({
    content_uri: URL_STRING
  },
  'Error: Missing \'channel_id\' option');

  run_next_test();
});

add_test(function test_profile_image_change_message() {
  var mockMessage = {
    command: "profile:change",
    data: { uid: "foo" }
  };

  makeObserver(ON_PROFILE_CHANGE_NOTIFICATION, function (subject, topic, data) {
    do_check_eq(data, "foo");
    run_next_test();
  });

  var channel = new FxAccountsWebChannel({
    channel_id: WEBCHANNEL_ID,
    content_uri: URL_STRING
  });

  channel._channelCallback(WEBCHANNEL_ID, mockMessage, mockSendingContext);
});

add_test(function test_login_message() {
  let mockMessage = {
    command: 'fxaccounts:login',
    data: { email: 'testuser@testuser.com' }
  };

  let channel = new FxAccountsWebChannel({
    channel_id: WEBCHANNEL_ID,
    content_uri: URL_STRING,
    helpers: {
      login: function (accountData) {
        do_check_eq(accountData.email, 'testuser@testuser.com');
        run_next_test();
      }
    }
  });

  channel._channelCallback(WEBCHANNEL_ID, mockMessage, mockSendingContext);
});

add_test(function test_can_link_account_message() {
  let mockMessage = {
    command: 'fxaccounts:can_link_account',
    data: { email: 'testuser@testuser.com' }
  };

  let channel = new FxAccountsWebChannel({
    channel_id: WEBCHANNEL_ID,
    content_uri: URL_STRING,
    helpers: {
      shouldAllowRelink: function (email) {
        do_check_eq(email, 'testuser@testuser.com');
        run_next_test();
      }
    }
  });

  channel._channelCallback(WEBCHANNEL_ID, mockMessage, mockSendingContext);
});

add_test(function test_unrecognized_message() {
  let mockMessage = {
    command: 'fxaccounts:unrecognized',
    data: {}
  };

  let channel = new FxAccountsWebChannel({
    channel_id: WEBCHANNEL_ID,
    content_uri: URL_STRING
  });

  // no error is expected.
  channel._channelCallback(WEBCHANNEL_ID, mockMessage, mockSendingContext);
  run_next_test();
});


add_test(function test_helpers_should_allow_relink_same_email() {
  let helpers = new FxAccountsWebChannelHelpers();

  helpers.setPreviousAccountNameHashPref('testuser@testuser.com');
  do_check_true(helpers.shouldAllowRelink('testuser@testuser.com'));

  run_next_test();
});

add_test(function test_helpers_should_allow_relink_different_email() {
  let helpers = new FxAccountsWebChannelHelpers();

  helpers.setPreviousAccountNameHashPref('testuser@testuser.com');

  helpers._promptForRelink = (acctName) => {
    return acctName === 'allowed_to_relink@testuser.com';
  };

  do_check_true(helpers.shouldAllowRelink('allowed_to_relink@testuser.com'));
  do_check_false(helpers.shouldAllowRelink('not_allowed_to_relink@testuser.com'));

  run_next_test();
});

add_test(function test_helpers_login_without_customize_sync() {
  let helpers = new FxAccountsWebChannelHelpers({
    fxAccounts: {
      setSignedInUser: function(accountData) {
        // ensure fxAccounts is informed of the new user being signed in.
        do_check_eq(accountData.email, 'testuser@testuser.com');

        // verifiedCanLinkAccount should be stripped in the data.
        do_check_false('verifiedCanLinkAccount' in accountData);

        // the customizeSync pref should not update
        do_check_false(helpers.getShowCustomizeSyncPref());

        // previously signed in user preference is updated.
        do_check_eq(helpers.getPreviousAccountNameHashPref(), helpers.sha256('testuser@testuser.com'));

        run_next_test();
      }
    }
  });

  // the show customize sync pref should stay the same
  helpers.setShowCustomizeSyncPref(false);

  // ensure the previous account pref is overwritten.
  helpers.setPreviousAccountNameHashPref('lastuser@testuser.com');

  helpers.login({
    email: 'testuser@testuser.com',
    verifiedCanLinkAccount: true,
    customizeSync: false
  });
});

add_test(function test_helpers_login_with_customize_sync() {
  let helpers = new FxAccountsWebChannelHelpers({
    fxAccounts: {
      setSignedInUser: function(accountData) {
        // ensure fxAccounts is informed of the new user being signed in.
        do_check_eq(accountData.email, 'testuser@testuser.com');

        // customizeSync should be stripped in the data.
        do_check_false('customizeSync' in accountData);

        // the customizeSync pref should not update
        do_check_true(helpers.getShowCustomizeSyncPref());

        run_next_test();
      }
    }
  });

  // the customize sync pref should be overwritten
  helpers.setShowCustomizeSyncPref(false);

  helpers.login({
    email: 'testuser@testuser.com',
    verifiedCanLinkAccount: true,
    customizeSync: true
  });
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
    new FxAccountsWebChannel(params);
  } catch (e) {
    if (typeof expected === 'string') {
      return do_check_eq(e.toString(), expected);
    } else {
      return do_check_true(e.toString().match(expected));
    }
  }
  throw new Error("Validation helper error");
}
