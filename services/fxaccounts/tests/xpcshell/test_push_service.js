/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests for the FxA push service.

Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/FxAccountsCommon.js");
Cu.import("resource://gre/modules/Log.jsm");

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
    cb(Components.results.NS_OK, {
      endpoint: MOCK_ENDPOINT
    });
  },
  unsubscribe(scope, principal, cb) {
    cb(Components.results.NS_OK, true);
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


add_task(function* initialize() {
  let pushService = new FxAccountsPushService();
  equal(pushService.initialize(), false);
});

add_task(function* registerPushEndpointSuccess() {
  let pushService = new FxAccountsPushService({
    pushService: mockPushService,
    fxAccounts: mockFxAccounts,
  });

  let subscription = yield pushService.registerPushEndpoint();
  equal(subscription.endpoint, MOCK_ENDPOINT);
});

add_task(function* registerPushEndpointFailure() {
  let failPushService = Object.assign(mockPushService, {
    subscribe(scope, principal, cb) {
      cb(Components.results.NS_ERROR_ABORT);
    }
  });

  let pushService = new FxAccountsPushService({
    pushService: failPushService,
    fxAccounts: mockFxAccounts,
  });

  let subscription = yield pushService.registerPushEndpoint();
  equal(subscription, null);
});

add_task(function* unsubscribeSuccess() {
  let pushService = new FxAccountsPushService({
    pushService: mockPushService,
    fxAccounts: mockFxAccounts,
  });

  let result = yield pushService.unsubscribe();
  equal(result, true);
});

add_task(function* unsubscribeFailure() {
  let failPushService = Object.assign(mockPushService, {
    unsubscribe(scope, principal, cb) {
      cb(Components.results.NS_ERROR_ABORT);
    }
  });

  let pushService = new FxAccountsPushService({
    pushService: failPushService,
    fxAccounts: mockFxAccounts,
  });

  let result = yield pushService.unsubscribe();
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
  Services.obs.addObserver(obs, ON_DEVICE_CONNECTED_NOTIFICATION, false);

  let pushService = new FxAccountsPushService({
    pushService: mockPushService,
    fxAccounts: mockFxAccounts,
  });

  pushService.observe(msg, mockPushService.pushTopic, FXA_PUSH_SCOPE_ACCOUNT_UPDATE);
});

add_test(function observePushTopicDeviceDisconnected() {
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
  let customAccounts = Object.assign(mockFxAccounts, {
    handleDeviceDisconnection() {
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
  Services.obs.addObserver(obs, ON_PROFILE_CHANGE_NOTIFICATION, false);

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
  }

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
  }

  pushService.observe(msg, mockPushService.pushTopic, FXA_PUSH_SCOPE_ACCOUNT_UPDATE);
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
