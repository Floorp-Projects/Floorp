/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {ON_PROFILE_CHANGE_NOTIFICATION, WEBCHANNEL_ID, log} = ChromeUtils.import("resource://gre/modules/FxAccountsCommon.js");
const {CryptoUtils} = ChromeUtils.import("resource://services-crypto/utils.js");
const { FxAccountsWebChannel, FxAccountsWebChannelHelpers } =
    ChromeUtils.import("resource://gre/modules/FxAccountsWebChannel.jsm", null);

const URL_STRING = "https://example.com";

const mockSendingContext = {
  browser: {},
  principal: {},
  eventTarget: {},
};

add_test(function() {
  validationHelper(undefined,
  "Error: Missing configuration options");

  validationHelper({
    channel_id: WEBCHANNEL_ID,
  },
  "Error: Missing 'content_uri' option");

  validationHelper({
    content_uri: "bad uri",
    channel_id: WEBCHANNEL_ID,
  },
  /NS_ERROR_MALFORMED_URI/);

  validationHelper({
    content_uri: URL_STRING,
  },
  "Error: Missing 'channel_id' option");

  run_next_test();
});

add_task(async function test_rejection_reporting() {
  let mockMessage = {
    command: "fxaccounts:login",
    messageId: "1234",
    data: { email: "testuser@testuser.com" },
  };

  let channel = new FxAccountsWebChannel({
    channel_id: WEBCHANNEL_ID,
    content_uri: URL_STRING,
    helpers: {
      login(accountData) {
        equal(accountData.email, "testuser@testuser.com",
          "Should forward incoming message data to the helper");
        return Promise.reject(new Error("oops"));
      },
    },
  });

  let promiseSend = new Promise(resolve => {
    channel._channel.send = (message, context) => {
      resolve({ message, context });
    };
  });

  channel._channelCallback(WEBCHANNEL_ID, mockMessage, mockSendingContext);

  let { message, context } = await promiseSend;

  equal(context, mockSendingContext, "Should forward the original context");
  equal(message.command, "fxaccounts:login",
    "Should include the incoming command");
  equal(message.messageId, "1234", "Should include the message ID");
  equal(message.data.error.message, "Error: oops",
    "Should convert the error message to a string");
  notStrictEqual(message.data.error.stack, null,
    "Should include the stack for JS error rejections");
});

add_test(function test_exception_reporting() {
  let mockMessage = {
    command: "fxaccounts:sync_preferences",
    messageId: "5678",
    data: { entryPoint: "fxa:verification_complete" },
  };

  let channel = new FxAccountsWebChannel({
    channel_id: WEBCHANNEL_ID,
    content_uri: URL_STRING,
    helpers: {
      openSyncPreferences(browser, entryPoint) {
        equal(entryPoint, "fxa:verification_complete",
          "Should forward incoming message data to the helper");
        throw new TypeError("splines not reticulated");
      },
    },
  });

  channel._channel.send = (message, context) => {
    equal(context, mockSendingContext, "Should forward the original context");
    equal(message.command, "fxaccounts:sync_preferences",
      "Should include the incoming command");
    equal(message.messageId, "5678", "Should include the message ID");
    equal(message.data.error.message, "TypeError: splines not reticulated",
      "Should convert the exception to a string");
    notStrictEqual(message.data.error.stack, null,
      "Should include the stack for JS exceptions");

    run_next_test();
  };

  channel._channelCallback(WEBCHANNEL_ID, mockMessage, mockSendingContext);
});

add_test(function test_profile_image_change_message() {
  var mockMessage = {
    command: "profile:change",
    data: { uid: "foo" },
  };

  makeObserver(ON_PROFILE_CHANGE_NOTIFICATION, function(subject, topic, data) {
    Assert.equal(data, "foo");
    run_next_test();
  });

  var channel = new FxAccountsWebChannel({
    channel_id: WEBCHANNEL_ID,
    content_uri: URL_STRING,
  });

  channel._channelCallback(WEBCHANNEL_ID, mockMessage, mockSendingContext);
});

add_test(function test_login_message() {
  let mockMessage = {
    command: "fxaccounts:login",
    data: { email: "testuser@testuser.com" },
  };

  let channel = new FxAccountsWebChannel({
    channel_id: WEBCHANNEL_ID,
    content_uri: URL_STRING,
    helpers: {
      login(accountData) {
        Assert.equal(accountData.email, "testuser@testuser.com");
        run_next_test();
        return Promise.resolve();
      },
    },
  });

  channel._channelCallback(WEBCHANNEL_ID, mockMessage, mockSendingContext);
});

add_test(function test_logout_message() {
  let mockMessage = {
    command: "fxaccounts:logout",
    data: { uid: "foo" },
  };

  let channel = new FxAccountsWebChannel({
    channel_id: WEBCHANNEL_ID,
    content_uri: URL_STRING,
    helpers: {
      logout(uid) {
        Assert.equal(uid, "foo");
        run_next_test();
        return Promise.resolve();
      },
    },
  });

  channel._channelCallback(WEBCHANNEL_ID, mockMessage, mockSendingContext);
});

add_test(function test_delete_message() {
  let mockMessage = {
    command: "fxaccounts:delete",
    data: { uid: "foo" },
  };

  let channel = new FxAccountsWebChannel({
    channel_id: WEBCHANNEL_ID,
    content_uri: URL_STRING,
    helpers: {
      logout(uid) {
        Assert.equal(uid, "foo");
        run_next_test();
        return Promise.resolve();
      },
    },
  });

  channel._channelCallback(WEBCHANNEL_ID, mockMessage, mockSendingContext);
});

add_test(function test_can_link_account_message() {
  let mockMessage = {
    command: "fxaccounts:can_link_account",
    data: { email: "testuser@testuser.com" },
  };

  let channel = new FxAccountsWebChannel({
    channel_id: WEBCHANNEL_ID,
    content_uri: URL_STRING,
    helpers: {
      shouldAllowRelink(email) {
        Assert.equal(email, "testuser@testuser.com");
        run_next_test();
      },
    },
  });

  channel._channelCallback(WEBCHANNEL_ID, mockMessage, mockSendingContext);
});

add_test(function test_sync_preferences_message() {
  let mockMessage = {
    command: "fxaccounts:sync_preferences",
    data: { entryPoint: "fxa:verification_complete" },
  };

  let channel = new FxAccountsWebChannel({
    channel_id: WEBCHANNEL_ID,
    content_uri: URL_STRING,
    helpers: {
      openSyncPreferences(browser, entryPoint) {
        Assert.equal(entryPoint, "fxa:verification_complete");
        Assert.equal(browser, mockSendingContext.browser);
        run_next_test();
      },
    },
  });

  channel._channelCallback(WEBCHANNEL_ID, mockMessage, mockSendingContext);
});

add_test(function test_fxa_status_message() {
  let mockMessage = {
    command: "fxaccounts:fxa_status",
    messageId: 123,
    data: {
      service: "sync",
    },
  };

  let channel = new FxAccountsWebChannel({
    channel_id: WEBCHANNEL_ID,
    content_uri: URL_STRING,
    helpers: {
      async getFxaStatus() {
        return {
          signedInUser: {
            email: "testuser@testuser.com",
            sessionToken: "session-token",
            uid: "uid",
            verified: true,
          },
          capabilities: {
            engines: ["creditcards", "addresses"],
          },
        };
      },
    },
  });

  channel._channel = {
    send(response, sendingContext) {
      Assert.equal(response.command, "fxaccounts:fxa_status");
      Assert.equal(response.messageId, 123);

      let signedInUser = response.data.signedInUser;
      Assert.ok(!!signedInUser);
      Assert.equal(signedInUser.email, "testuser@testuser.com");
      Assert.equal(signedInUser.sessionToken, "session-token");
      Assert.equal(signedInUser.uid, "uid");
      Assert.equal(signedInUser.verified, true);

      deepEqual(response.data.capabilities.engines, ["creditcards", "addresses"]);

      run_next_test();
    },
  };

  channel._channelCallback(WEBCHANNEL_ID, mockMessage, mockSendingContext);
});

add_test(function test_unrecognized_message() {
  let mockMessage = {
    command: "fxaccounts:unrecognized",
    data: {},
  };

  let channel = new FxAccountsWebChannel({
    channel_id: WEBCHANNEL_ID,
    content_uri: URL_STRING,
  });

  // no error is expected.
  channel._channelCallback(WEBCHANNEL_ID, mockMessage, mockSendingContext);
  run_next_test();
});


add_test(function test_helpers_should_allow_relink_same_email() {
  let helpers = new FxAccountsWebChannelHelpers();

  helpers.setPreviousAccountNameHashPref("testuser@testuser.com");
  Assert.ok(helpers.shouldAllowRelink("testuser@testuser.com"));

  run_next_test();
});

add_test(function test_helpers_should_allow_relink_different_email() {
  let helpers = new FxAccountsWebChannelHelpers();

  helpers.setPreviousAccountNameHashPref("testuser@testuser.com");

  helpers._promptForRelink = (acctName) => {
    return acctName === "allowed_to_relink@testuser.com";
  };

  Assert.ok(helpers.shouldAllowRelink("allowed_to_relink@testuser.com"));
  Assert.ok(!helpers.shouldAllowRelink("not_allowed_to_relink@testuser.com"));

  run_next_test();
});

add_task(async function test_helpers_login_without_customize_sync() {
  let helpers = new FxAccountsWebChannelHelpers({
    fxAccounts: {
      setSignedInUser(accountData) {
        return new Promise(resolve => {
          // ensure fxAccounts is informed of the new user being signed in.
          Assert.equal(accountData.email, "testuser@testuser.com");

          // verifiedCanLinkAccount should be stripped in the data.
          Assert.equal(false, "verifiedCanLinkAccount" in accountData);

          // previously signed in user preference is updated.
          Assert.equal(helpers.getPreviousAccountNameHashPref(),
                       CryptoUtils.sha256Base64("testuser@testuser.com"));

          resolve();
        });
      },
    },
  });

  // ensure the previous account pref is overwritten.
  helpers.setPreviousAccountNameHashPref("lastuser@testuser.com");

  await helpers.login({
    email: "testuser@testuser.com",
    verifiedCanLinkAccount: true,
    customizeSync: false,
  });
});

add_task(async function test_helpers_login_with_customize_sync() {
  let helpers = new FxAccountsWebChannelHelpers({
    fxAccounts: {
      setSignedInUser(accountData) {
        return new Promise(resolve => {
          // ensure fxAccounts is informed of the new user being signed in.
          Assert.equal(accountData.email, "testuser@testuser.com");

          // customizeSync should be stripped in the data.
          Assert.equal(false, "customizeSync" in accountData);

          resolve();
        });
      },
    },
  });

  await helpers.login({
    email: "testuser@testuser.com",
    verifiedCanLinkAccount: true,
    customizeSync: true,
  });
});

add_task(async function test_helpers_login_with_customize_sync_and_declined_engines() {
  let helpers = new FxAccountsWebChannelHelpers({
    fxAccounts: {
      setSignedInUser(accountData) {
        return new Promise(resolve => {
          // ensure fxAccounts is informed of the new user being signed in.
          Assert.equal(accountData.email, "testuser@testuser.com");

          // customizeSync should be stripped in the data.
          Assert.equal(false, "customizeSync" in accountData);
          Assert.equal(false, "declinedSyncEngines" in accountData);
          Assert.equal(Services.prefs.getBoolPref("services.sync.engine.addons"), false);
          Assert.equal(Services.prefs.getBoolPref("services.sync.engine.bookmarks"), true);
          Assert.equal(Services.prefs.getBoolPref("services.sync.engine.history"), true);
          Assert.equal(Services.prefs.getBoolPref("services.sync.engine.passwords"), true);
          Assert.equal(Services.prefs.getBoolPref("services.sync.engine.prefs"), false);
          Assert.equal(Services.prefs.getBoolPref("services.sync.engine.tabs"), true);

          resolve();
        });
      },
    },
  });

  Assert.equal(Services.prefs.getBoolPref("services.sync.engine.addons"), true);
  Assert.equal(Services.prefs.getBoolPref("services.sync.engine.bookmarks"), true);
  Assert.equal(Services.prefs.getBoolPref("services.sync.engine.history"), true);
  Assert.equal(Services.prefs.getBoolPref("services.sync.engine.passwords"), true);
  Assert.equal(Services.prefs.getBoolPref("services.sync.engine.prefs"), true);
  Assert.equal(Services.prefs.getBoolPref("services.sync.engine.tabs"), true);
  await helpers.login({
    email: "testuser@testuser.com",
    verifiedCanLinkAccount: true,
    customizeSync: true,
    declinedSyncEngines: ["addons", "prefs"],
  });
});

add_task(async function test_helpers_login_with_offered_sync_engines() {
  let helpers;
  const setSignedInUserCalled = new Promise(resolve => {
    helpers = new FxAccountsWebChannelHelpers({
      fxAccounts: {
        async setSignedInUser(accountData) {
          resolve(accountData);
        },
      },
    });
  });

  Services.prefs.setBoolPref("services.sync.engine.creditcards", false);
  Services.prefs.setBoolPref("services.sync.engine.addresses", false);

  await helpers.login({
    email: "testuser@testuser.com",
    verifiedCanLinkAccount: true,
    customizeSync: true,
    declinedSyncEngines: ["addresses"],
    offeredSyncEngines: ["creditcards", "addresses"],
  });

  const accountData = await setSignedInUserCalled;

  // ensure fxAccounts is informed of the new user being signed in.
  equal(accountData.email, "testuser@testuser.com");

  // offeredSyncEngines should be stripped in the data.
  ok(!("offeredSyncEngines" in accountData));
  // credit cards was offered but not declined.
  equal(Services.prefs.getBoolPref("services.sync.engine.creditcards"), true);
  // addresses was offered and explicitely declined.
  equal(Services.prefs.getBoolPref("services.sync.engine.addresses"), false);
});

add_test(function test_helpers_open_sync_preferences() {
  let helpers = new FxAccountsWebChannelHelpers({
    fxAccounts: {
    },
  });

  let mockBrowser = {
    loadURI(uri) {
      Assert.equal(uri, "about:preferences?entrypoint=fxa%3Averification_complete#sync");
      run_next_test();
    },
  };

  helpers.openSyncPreferences(mockBrowser, "fxa:verification_complete");
});

add_task(async function test_helpers_getFxAStatus_extra_engines() {
  let helpers = new FxAccountsWebChannelHelpers({
    fxAccounts: {
      getSignedInUser() {
        return Promise.resolve({
          email: "testuser@testuser.com",
          kSync: "kSync",
          kXCS: "kXCS",
          kExtSync: "kExtSync",
          kExtKbHash: "kExtKbHash",
          sessionToken: "sessionToken",
          uid: "uid",
          verified: true,
        });
      },
    },
    privateBrowsingUtils: {
      isBrowserPrivate: () => true,
    },
  });

  Services.prefs.setBoolPref("services.sync.engine.creditcards.available", true);
  // Not defining "services.sync.engine.addresses.available" on purpose.

  let fxaStatus = await helpers.getFxaStatus("sync", mockSendingContext);
  ok(!!fxaStatus);
  ok(!!fxaStatus.signedInUser);
  deepEqual(fxaStatus.capabilities.engines, ["creditcards"]);
});


add_task(async function test_helpers_getFxaStatus_allowed_signedInUser() {
  let wasCalled = {
    getSignedInUser: false,
    shouldAllowFxaStatus: false,
  };

  let helpers = new FxAccountsWebChannelHelpers({
    fxAccounts: {
      getSignedInUser() {
        wasCalled.getSignedInUser = true;
        return Promise.resolve({
          email: "testuser@testuser.com",
          kSync: "kSync",
          kXCS: "kXCS",
          kExtSync: "kExtSync",
          kExtKbHash: "kExtKbHash",
          sessionToken: "sessionToken",
          uid: "uid",
          verified: true,
        });
      },
    },
  });

  helpers.shouldAllowFxaStatus = (service, sendingContext) => {
    wasCalled.shouldAllowFxaStatus = true;
    Assert.equal(service, "sync");
    Assert.equal(sendingContext, mockSendingContext);

    return true;
  };

  return helpers.getFxaStatus("sync", mockSendingContext)
    .then(fxaStatus => {
      Assert.ok(!!fxaStatus);
      Assert.ok(wasCalled.getSignedInUser);
      Assert.ok(wasCalled.shouldAllowFxaStatus);

      Assert.ok(!!fxaStatus.signedInUser);
      let {signedInUser} = fxaStatus;

      Assert.equal(signedInUser.email, "testuser@testuser.com");
      Assert.equal(signedInUser.sessionToken, "sessionToken");
      Assert.equal(signedInUser.uid, "uid");
      Assert.ok(signedInUser.verified);

      // These properties are filtered and should not
      // be returned to the requester.
      Assert.equal(false, "kSync" in signedInUser);
      Assert.equal(false, "kXCS" in signedInUser);
      Assert.equal(false, "kExtSync" in signedInUser);
      Assert.equal(false, "kExtKbHash" in signedInUser);
    });
});

add_task(async function test_helpers_getFxaStatus_allowed_no_signedInUser() {
  let wasCalled = {
    getSignedInUser: false,
    shouldAllowFxaStatus: false,
  };

  let helpers = new FxAccountsWebChannelHelpers({
    fxAccounts: {
      getSignedInUser() {
        wasCalled.getSignedInUser = true;
        return Promise.resolve(null);
      },
    },
  });

  helpers.shouldAllowFxaStatus = (service, sendingContext) => {
    wasCalled.shouldAllowFxaStatus = true;
    Assert.equal(service, "sync");
    Assert.equal(sendingContext, mockSendingContext);

    return true;
  };

  return helpers.getFxaStatus("sync", mockSendingContext)
    .then(fxaStatus => {
      Assert.ok(!!fxaStatus);
      Assert.ok(wasCalled.getSignedInUser);
      Assert.ok(wasCalled.shouldAllowFxaStatus);

      Assert.equal(null, fxaStatus.signedInUser);
    });
});

add_task(async function test_helpers_getFxaStatus_not_allowed() {
  let wasCalled = {
    getSignedInUser: false,
    shouldAllowFxaStatus: false,
  };

  let helpers = new FxAccountsWebChannelHelpers({
    fxAccounts: {
      getSignedInUser() {
        wasCalled.getSignedInUser = true;
        return Promise.resolve(null);
      },
    },
  });

  helpers.shouldAllowFxaStatus = (service, sendingContext) => {
    wasCalled.shouldAllowFxaStatus = true;
    Assert.equal(service, "sync");
    Assert.equal(sendingContext, mockSendingContext);

    return false;
  };

  return helpers.getFxaStatus("sync", mockSendingContext)
    .then(fxaStatus => {
      Assert.ok(!!fxaStatus);
      Assert.ok(!wasCalled.getSignedInUser);
      Assert.ok(wasCalled.shouldAllowFxaStatus);

      Assert.equal(null, fxaStatus.signedInUser);
    });
});

add_task(async function test_helpers_shouldAllowFxaStatus_sync_service_not_private_browsing() {
  let wasCalled = {
    isPrivateBrowsingMode: false,
  };
  let helpers = new FxAccountsWebChannelHelpers({});

  helpers.isPrivateBrowsingMode = (sendingContext) => {
    wasCalled.isPrivateBrowsingMode = true;
    Assert.equal(sendingContext, mockSendingContext);
    return false;
  };

  let shouldAllowFxaStatus = helpers.shouldAllowFxaStatus("sync", mockSendingContext, false);
  Assert.ok(shouldAllowFxaStatus);
  Assert.ok(wasCalled.isPrivateBrowsingMode);
});

add_task(async function test_helpers_shouldAllowFxaStatus_oauth_service_not_private_browsing() {
  let wasCalled = {
    isPrivateBrowsingMode: false,
  };
  let helpers = new FxAccountsWebChannelHelpers({});

  helpers.isPrivateBrowsingMode = (sendingContext) => {
    wasCalled.isPrivateBrowsingMode = true;
    Assert.equal(sendingContext, mockSendingContext);
    return false;
  };

  let shouldAllowFxaStatus = helpers.shouldAllowFxaStatus("dcdb5ae7add825d2", mockSendingContext, false);
  Assert.ok(shouldAllowFxaStatus);
  Assert.ok(wasCalled.isPrivateBrowsingMode);
});

add_task(async function test_helpers_shouldAllowFxaStatus_no_service_not_private_browsing() {
  let wasCalled = {
    isPrivateBrowsingMode: false,
  };
  let helpers = new FxAccountsWebChannelHelpers({});

  helpers.isPrivateBrowsingMode = (sendingContext) => {
    wasCalled.isPrivateBrowsingMode = true;
    Assert.equal(sendingContext, mockSendingContext);
    return false;
  };

  let shouldAllowFxaStatus = helpers.shouldAllowFxaStatus("", mockSendingContext, false);
  Assert.ok(shouldAllowFxaStatus);
  Assert.ok(wasCalled.isPrivateBrowsingMode);
});

add_task(async function test_helpers_shouldAllowFxaStatus_sync_service_private_browsing() {
  let wasCalled = {
    isPrivateBrowsingMode: false,
  };
  let helpers = new FxAccountsWebChannelHelpers({});

  helpers.isPrivateBrowsingMode = (sendingContext) => {
    wasCalled.isPrivateBrowsingMode = true;
    Assert.equal(sendingContext, mockSendingContext);
    return true;
  };

  let shouldAllowFxaStatus = helpers.shouldAllowFxaStatus("sync", mockSendingContext, false);
  Assert.ok(shouldAllowFxaStatus);
  Assert.ok(wasCalled.isPrivateBrowsingMode);
});

add_task(async function test_helpers_shouldAllowFxaStatus_oauth_service_private_browsing() {
  let wasCalled = {
    isPrivateBrowsingMode: false,
  };
  let helpers = new FxAccountsWebChannelHelpers({});

  helpers.isPrivateBrowsingMode = (sendingContext) => {
    wasCalled.isPrivateBrowsingMode = true;
    Assert.equal(sendingContext, mockSendingContext);
    return true;
  };

  let shouldAllowFxaStatus = helpers.shouldAllowFxaStatus("dcdb5ae7add825d2", mockSendingContext, false);
  Assert.ok(!shouldAllowFxaStatus);
  Assert.ok(wasCalled.isPrivateBrowsingMode);
});

add_task(async function test_helpers_shouldAllowFxaStatus_oauth_service_pairing_private_browsing() {
  let wasCalled = {
    isPrivateBrowsingMode: false,
  };
  let helpers = new FxAccountsWebChannelHelpers({});

  helpers.isPrivateBrowsingMode = (sendingContext) => {
    wasCalled.isPrivateBrowsingMode = true;
    Assert.equal(sendingContext, mockSendingContext);
    return true;
  };

  let shouldAllowFxaStatus = helpers.shouldAllowFxaStatus("dcdb5ae7add825d2", mockSendingContext, true);
  Assert.ok(shouldAllowFxaStatus);
  Assert.ok(wasCalled.isPrivateBrowsingMode);
});

add_task(async function test_helpers_shouldAllowFxaStatus_no_service_private_browsing() {
  let wasCalled = {
    isPrivateBrowsingMode: false,
  };
  let helpers = new FxAccountsWebChannelHelpers({});

  helpers.isPrivateBrowsingMode = (sendingContext) => {
    wasCalled.isPrivateBrowsingMode = true;
    Assert.equal(sendingContext, mockSendingContext);
    return true;
  };

  let shouldAllowFxaStatus = helpers.shouldAllowFxaStatus("", mockSendingContext, false);
  Assert.ok(!shouldAllowFxaStatus);
  Assert.ok(wasCalled.isPrivateBrowsingMode);
});

add_task(async function test_helpers_isPrivateBrowsingMode_private_browsing() {
  let wasCalled = {
    isBrowserPrivate: false,
  };
  let helpers = new FxAccountsWebChannelHelpers({
    privateBrowsingUtils: {
      isBrowserPrivate(browser) {
        wasCalled.isBrowserPrivate = true;
        Assert.equal(browser, mockSendingContext.browser);
        return true;
      },
    },
  });

  let isPrivateBrowsingMode = helpers.isPrivateBrowsingMode(mockSendingContext);
  Assert.ok(isPrivateBrowsingMode);
  Assert.ok(wasCalled.isBrowserPrivate);
});

add_task(async function test_helpers_isPrivateBrowsingMode_private_browsing() {
  let wasCalled = {
    isBrowserPrivate: false,
  };
  let helpers = new FxAccountsWebChannelHelpers({
    privateBrowsingUtils: {
      isBrowserPrivate(browser) {
        wasCalled.isBrowserPrivate = true;
        Assert.equal(browser, mockSendingContext.browser);
        return false;
      },
    },
  });

  let isPrivateBrowsingMode = helpers.isPrivateBrowsingMode(mockSendingContext);
  Assert.ok(!isPrivateBrowsingMode);
  Assert.ok(wasCalled.isBrowserPrivate);
});

add_task(async function test_helpers_change_password() {
  let wasCalled = {
    updateUserAccountData: false,
    updateDeviceRegistration: false,
  };
  let helpers = new FxAccountsWebChannelHelpers({
    fxAccounts: {
      updateUserAccountData(credentials) {
        return new Promise(resolve => {
          Assert.ok(credentials.hasOwnProperty("email"));
          Assert.ok(credentials.hasOwnProperty("uid"));
          Assert.ok(credentials.hasOwnProperty("unwrapBKey"));
          Assert.ok(credentials.hasOwnProperty("device"));
          Assert.equal(null, credentials.device);
          // "foo" isn't a field known by storage, so should be dropped.
          Assert.ok(!credentials.hasOwnProperty("foo"));
          wasCalled.updateUserAccountData = true;

          resolve();
        });
      },

      updateDeviceRegistration() {
        Assert.equal(arguments.length, 0);
        wasCalled.updateDeviceRegistration = true;
        return Promise.resolve();
      },
    },
  });
  await helpers.changePassword({ email: "email", uid: "uid", unwrapBKey: "unwrapBKey", foo: "foo" });
  Assert.ok(wasCalled.updateUserAccountData);
  Assert.ok(wasCalled.updateDeviceRegistration);
});

add_task(async function test_helpers_change_password_with_error() {
  let wasCalled = {
    updateUserAccountData: false,
    updateDeviceRegistration: false,
  };
  let helpers = new FxAccountsWebChannelHelpers({
    fxAccounts: {
      updateUserAccountData() {
        wasCalled.updateUserAccountData = true;
        return Promise.reject();
      },

      updateDeviceRegistration() {
        wasCalled.updateDeviceRegistration = true;
        return Promise.resolve();
      },
    },
  });
  try {
    await helpers.changePassword({});
    Assert.equal(false, "changePassword should have rejected");
  } catch (_) {
    Assert.ok(wasCalled.updateUserAccountData);
    Assert.ok(!wasCalled.updateDeviceRegistration);
  }
});

function makeObserver(aObserveTopic, aObserveFunc) {
  let callback = function(aSubject, aTopic, aData) {
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

  Services.obs.addObserver(callback, aObserveTopic);
  return removeMe;
}

function validationHelper(params, expected) {
  try {
    new FxAccountsWebChannel(params);
  } catch (e) {
    if (typeof expected === "string") {
      return Assert.equal(e.toString(), expected);
    }
    return Assert.ok(e.toString().match(expected));
  }
  throw new Error("Validation helper error");
}
