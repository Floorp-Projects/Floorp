/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests for the FxA push service.

/* eslint-disable no-shadow */
/* eslint-disable mozilla/use-chromeutils-generateqi */

ChromeUtils.import("resource://gre/modules/FxAccountsCommon.js");
ChromeUtils.import("resource://gre/modules/Log.jsm");

let importScope = {};
Services.scriptloader.loadSubScript("resource://gre/components/FxAccountsPush.js", importScope);
const FxAccountsPushService = importScope.FxAccountsPushService;

XPCOMUtils.defineLazyServiceGetter(this, "pushService",
  "@mozilla.org/push/Service;1", "nsIPushService");

initTestLogging("Trace");
log.level = Log.Level.Trace;

const MOCK_ENDPOINT = "http://mochi.test:8888";

// tests do not allow external connections, mock the PushService
let mockPushService = {
  pushTopic: this.pushService.pushTopic,
  subscriptionChangeTopic: this.pushService.subscriptionChangeTopic,
  subscribe(scope, principal, cb) {
    cb(Cr.NS_OK, {
      endpoint: MOCK_ENDPOINT
    });
  },
  unsubscribe(scope, principal, cb) {
    cb(Cr.NS_OK, true);
  }
};

let mockFxAccounts = {
  checkVerificationStatus() {},
  updateDeviceRegistration() {}
};

let mockLog = {
  trace() {},
  debug() {},
  warn() {},
  error() {}
};


add_task(async function initialize() {
  let pushService = new FxAccountsPushService();
  equal(pushService.initialize(), false);
});

add_task(async function registerPushEndpointSuccess() {
  let pushService = new FxAccountsPushService({
    pushService: mockPushService,
    fxAccounts: mockFxAccounts,
  });

  let subscription = await pushService.registerPushEndpoint();
  equal(subscription.endpoint, MOCK_ENDPOINT);
});

add_task(async function registerPushEndpointFailure() {
  let failPushService = Object.assign(mockPushService, {
    subscribe(scope, principal, cb) {
      cb(Cr.NS_ERROR_ABORT);
    }
  });

  let pushService = new FxAccountsPushService({
    pushService: failPushService,
    fxAccounts: mockFxAccounts,
  });

  let subscription = await pushService.registerPushEndpoint();
  equal(subscription, null);
});

add_task(async function unsubscribeSuccess() {
  let pushService = new FxAccountsPushService({
    pushService: mockPushService,
    fxAccounts: mockFxAccounts,
  });

  let result = await pushService.unsubscribe();
  equal(result, true);
});

add_task(async function unsubscribeFailure() {
  let failPushService = Object.assign(mockPushService, {
    unsubscribe(scope, principal, cb) {
      cb(Cr.NS_ERROR_ABORT);
    }
  });

  let pushService = new FxAccountsPushService({
    pushService: failPushService,
    fxAccounts: mockFxAccounts,
  });

  let result = await pushService.unsubscribe();
  equal(result, null);
});

add_test(function observeLogout() {
  let customLog = Object.assign(mockLog, {
    trace(msg) {
      if (msg === "FxAccountsPushService unsubscribe") {
        // logout means we unsubscribe
        run_next_test();
      }
    }
  });

  let pushService = new FxAccountsPushService({
    pushService: mockPushService,
    log: customLog
  });

  pushService.observe(null, ONLOGOUT_NOTIFICATION);
});

add_test(function observePushTopicVerify() {
  let emptyMsg = {
    QueryInterface() {
      return this;
    }
  };
  let customAccounts = Object.assign(mockFxAccounts, {
    checkVerificationStatus() {
      // checking verification status on push messages without data
      run_next_test();
    }
  });

  let pushService = new FxAccountsPushService({
    pushService: mockPushService,
    fxAccounts: customAccounts,
  });

  pushService.observe(emptyMsg, mockPushService.pushTopic, FXA_PUSH_SCOPE_ACCOUNT_UPDATE);
});

add_test(function observePushTopicDeviceConnected() {
  let msg = {
    data: {
      json: () => ({
        command: ON_DEVICE_CONNECTED_NOTIFICATION,
        data: {
          deviceName: "My phone"
        }
      })
    },
    QueryInterface() {
      return this;
    }
  };
  let obs = (subject, topic, data) => {
    Services.obs.removeObserver(obs, topic);
    run_next_test();
  };
  Services.obs.addObserver(obs, ON_DEVICE_CONNECTED_NOTIFICATION);

  let pushService = new FxAccountsPushService({
    pushService: mockPushService,
    fxAccounts: mockFxAccounts,
  });

  pushService.observe(msg, mockPushService.pushTopic, FXA_PUSH_SCOPE_ACCOUNT_UPDATE);
});

add_task(async function observePushTopicDeviceDisconnected_current_device() {
  const deviceId = "bogusid";
  let msg = {
    data: {
      json: () => ({
        command: ON_DEVICE_DISCONNECTED_NOTIFICATION,
        data: {
          id: deviceId
        }
      })
    },
    QueryInterface() {
      return this;
    }
  };

  let signoutCalled = false;
  let { FxAccounts } = ChromeUtils.import("resource://gre/modules/FxAccounts.jsm", {});
  const fxAccountsMock = new FxAccounts({
    newAccountState() {
      return {
        async getUserAccountData() {
          return {device: {id: deviceId}};
        }
      };
    },
    signOut() {
      signoutCalled = true;
    }
  });

  const deviceDisconnectedNotificationObserved = new Promise(resolve => {
    Services.obs.addObserver(function obs(subject, topic, data) {
      Services.obs.removeObserver(obs, topic);
      equal(data, JSON.stringify({ isLocalDevice: true }));
      resolve();
    }, ON_DEVICE_DISCONNECTED_NOTIFICATION);
  });

  let pushService = new FxAccountsPushService({
    pushService: mockPushService,
    fxAccounts: fxAccountsMock,
  });

  pushService.observe(msg, mockPushService.pushTopic, FXA_PUSH_SCOPE_ACCOUNT_UPDATE);

  await deviceDisconnectedNotificationObserved;
  ok(signoutCalled);
});

add_task(async function observePushTopicDeviceDisconnected_another_device() {
  const deviceId = "bogusid";
  let msg = {
    data: {
      json: () => ({
        command: ON_DEVICE_DISCONNECTED_NOTIFICATION,
        data: {
          id: deviceId
        }
      })
    },
    QueryInterface() {
      return this;
    }
  };

  let signoutCalled = false;
  let { FxAccounts } = ChromeUtils.import("resource://gre/modules/FxAccounts.jsm", {});
  const fxAccountsMock = new FxAccounts({
    newAccountState() {
      return {
        async getUserAccountData() {
          return {device: {id: "thelocaldevice"}};
        }
      };
    },
    signOut() {
      signoutCalled = true;
    }
  });

  const deviceDisconnectedNotificationObserved = new Promise(resolve => {
    Services.obs.addObserver(function obs(subject, topic, data) {
      Services.obs.removeObserver(obs, topic);
      equal(data, JSON.stringify({ isLocalDevice: false }));
      resolve();
    }, ON_DEVICE_DISCONNECTED_NOTIFICATION);
  });

  let pushService = new FxAccountsPushService({
    pushService: mockPushService,
    fxAccounts: fxAccountsMock,
  });

  pushService.observe(msg, mockPushService.pushTopic, FXA_PUSH_SCOPE_ACCOUNT_UPDATE);

  await deviceDisconnectedNotificationObserved;
  ok(!signoutCalled);
});

add_test(function observePushTopicAccountDestroyed() {
  const uid = "bogusuid";
  let msg = {
    data: {
      json: () => ({
        command: ON_ACCOUNT_DESTROYED_NOTIFICATION,
        data: {
          uid
        }
      })
    },
    QueryInterface() {
      return this;
    }
  };
  let customAccounts = Object.assign(mockFxAccounts, {
    handleAccountDestroyed() {
      // checking verification status on push messages without data
      run_next_test();
    }
  });

  let pushService = new FxAccountsPushService({
    pushService: mockPushService,
    fxAccounts: customAccounts,
  });

  pushService.observe(msg, mockPushService.pushTopic, FXA_PUSH_SCOPE_ACCOUNT_UPDATE);
});

add_test(function observePushTopicVerifyLogin() {
  let url = "http://localhost/newLogin";
  let title = "bogustitle";
  let body = "bogusbody";
  let msg = {
    data: {
      json: () => ({
        command: ON_VERIFY_LOGIN_NOTIFICATION,
        data: {
          body,
          title,
          url
        }
      })
    },
    QueryInterface() {
      return this;
    }
  };
  let obs = (subject, topic, data) => {
    Services.obs.removeObserver(obs, topic);
    Assert.equal(data, JSON.stringify(msg.data.json().data));
    run_next_test();
  };
  Services.obs.addObserver(obs, ON_VERIFY_LOGIN_NOTIFICATION);

  let pushService = new FxAccountsPushService({
    pushService: mockPushService,
    fxAccounts: mockFxAccounts,
  });

  pushService.observe(msg, mockPushService.pushTopic, FXA_PUSH_SCOPE_ACCOUNT_UPDATE);
});

add_test(function observePushTopicProfileUpdated() {
  let msg = {
    data: {
      json: () => ({
        command: ON_PROFILE_UPDATED_NOTIFICATION
      })
    },
    QueryInterface() {
      return this;
    }
  };
  let obs = (subject, topic, data) => {
    Services.obs.removeObserver(obs, topic);
    run_next_test();
  };
  Services.obs.addObserver(obs, ON_PROFILE_CHANGE_NOTIFICATION);

  let pushService = new FxAccountsPushService({
    pushService: mockPushService,
    fxAccounts: mockFxAccounts,
  });

  pushService.observe(msg, mockPushService.pushTopic, FXA_PUSH_SCOPE_ACCOUNT_UPDATE);
});

add_test(function observePushTopicPasswordChanged() {
  let msg = {
    data: {
      json: () => ({
        command: ON_PASSWORD_CHANGED_NOTIFICATION
      })
    },
    QueryInterface() {
      return this;
    }
  };

  let pushService = new FxAccountsPushService({
    pushService: mockPushService,
  });

  pushService._onPasswordChanged = function() {
    run_next_test();
  };

  pushService.observe(msg, mockPushService.pushTopic, FXA_PUSH_SCOPE_ACCOUNT_UPDATE);
});

add_test(function observePushTopicPasswordReset() {
  let msg = {
    data: {
      json: () => ({
        command: ON_PASSWORD_RESET_NOTIFICATION
      })
    },
    QueryInterface() {
      return this;
    }
  };

  let pushService = new FxAccountsPushService({
    pushService: mockPushService
  });

  pushService._onPasswordChanged = function() {
    run_next_test();
  };

  pushService.observe(msg, mockPushService.pushTopic, FXA_PUSH_SCOPE_ACCOUNT_UPDATE);
});

add_task(async function commandReceived() {
  let msg = {
    data: {
      json: () => ({
        command: "fxaccounts:command_received",
        data: {
          url: "https://api.accounts.firefox.com/auth/v1/account/device/commands?index=42&limit=1"
        }
      })
    },
    QueryInterface() {
      return this;
    }
  };

  let fxAccountsMock = {};
  const promiseConsumeRemoteMessagesCalled = new Promise(res => {
    fxAccountsMock.commands = {
      consumeRemoteCommand() {
        res();
      }
    };
  });

  let pushService = new FxAccountsPushService({
    pushService: mockPushService,
    fxAccounts: fxAccountsMock,
  });

  pushService.observe(msg, mockPushService.pushTopic, FXA_PUSH_SCOPE_ACCOUNT_UPDATE);
  await promiseConsumeRemoteMessagesCalled;
});

add_test(function observeSubscriptionChangeTopic() {
  let customAccounts = Object.assign(mockFxAccounts, {
    updateDeviceRegistration() {
      // subscription change means updating the device registration
      run_next_test();
    }
  });

  let pushService = new FxAccountsPushService({
    pushService: mockPushService,
    fxAccounts: customAccounts,
  });

  pushService.observe(null, mockPushService.subscriptionChangeTopic, FXA_PUSH_SCOPE_ACCOUNT_UPDATE);
});
