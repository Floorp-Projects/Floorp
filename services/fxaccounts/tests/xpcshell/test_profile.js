/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/FxAccountsCommon.js");
Cu.import("resource://gre/modules/FxAccountsProfileClient.jsm");
Cu.import("resource://gre/modules/FxAccountsProfile.jsm");

const URL_STRING = "https://example.com";
Services.prefs.setCharPref("identity.fxaccounts.settings.uri", "https://example.com/settings");

const PROFILE_CLIENT_OPTIONS = {
  token: "123ABC",
  serverURL: "http://127.0.0.1:1111/v1",
  profileServerUrl: "http://127.0.0.1:1111/v1"
};

const STATUS_SUCCESS = 200;

/**
 * Mock request responder
 * @param {String} response
 *        Mocked raw response from the server
 * @returns {Function}
 */
let mockResponse = function (response) {
  let Request = function (requestUri) {
    // Store the request uri so tests can inspect it
    Request._requestUri = requestUri;
    return {
      setHeader: function () {},
      head: function () {
        this.response = response;
        this.onComplete();
      }
    };
  };

  return Request;
};

/**
 * Mock request error responder
 * @param {Error} error
 *        Error object
 * @returns {Function}
 */
let mockResponseError = function (error) {
  return function () {
    return {
      setHeader: function () {},
      head: function () {
        this.onComplete(error);
      }
    };
  };
};

let mockClient = function () {
  let client = new FxAccountsProfileClient(PROFILE_CLIENT_OPTIONS);
  return client;
};

const ACCOUNT_DATA = {
  uid: "abc123"
};

function AccountData () {
}
AccountData.prototype = {
  getUserAccountData: function () {
    return Promise.resolve(ACCOUNT_DATA);
  }
};

let mockAccountData = function () {
  return new AccountData();
};

add_test(function getCachedProfile() {
  let accountData = mockAccountData();
  accountData.getUserAccountData = function () {
    return Promise.resolve({
      profile: { avatar: "myurl" }
    });
  };
  let profile = new FxAccountsProfile(accountData, PROFILE_CLIENT_OPTIONS);

  return profile._getCachedProfile()
    .then(function (cached) {
      do_check_eq(cached.avatar, "myurl");
      run_next_test();
    });
});

add_test(function cacheProfile_change() {
  let accountData = mockAccountData();
  let setUserAccountDataCalled = false;
  accountData.setUserAccountData = function (data) {
    setUserAccountDataCalled = true;
    do_check_eq(data.profile.avatar, "myurl");
    return Promise.resolve();
  };
  let profile = new FxAccountsProfile(accountData, PROFILE_CLIENT_OPTIONS);

  makeObserver(ON_PROFILE_CHANGE_NOTIFICATION, function (subject, topic, data) {
    do_check_eq(data, ACCOUNT_DATA.uid);
    do_check_true(setUserAccountDataCalled);
    run_next_test();
  });

  return profile._cacheProfile({ avatar: "myurl" });
});

add_test(function cacheProfile_no_change() {
  let accountData = mockAccountData();
  accountData.getUserAccountData = function () {
    return Promise.resolve({
      profile: { avatar: "myurl" }
    });
  };
  accountData.setUserAccountData = function (data) {
    throw new Error("should not update account data");
  };
  let profile = new FxAccountsProfile(accountData, PROFILE_CLIENT_OPTIONS);

  return profile._cacheProfile({ avatar: "myurl" })
    .then((result) => {
      do_check_false(!!result);
      run_next_test();
    });
});

add_test(function fetchAndCacheProfile_ok() {
  let client = mockClient();
  client.fetchProfile = function () {
    return Promise.resolve({ avatar: "myimg"});
  };
  let profile = new FxAccountsProfile(mockAccountData(), {
    profileClient: client
  });

  profile._cacheProfile = function (toCache) {
    do_check_eq(toCache.avatar, "myimg");
    return Promise.resolve();
  };

  return profile._fetchAndCacheProfile()
    .then(result => {
      do_check_eq(result.avatar, "myimg");
      run_next_test();
    });
});

add_test(function tearDown_ok() {
  let profile = new FxAccountsProfile(mockAccountData(), PROFILE_CLIENT_OPTIONS);

  do_check_true(!!profile.client);
  do_check_true(!!profile.currentAccountState);

  profile.tearDown();
  do_check_null(profile.currentAccountState);
  do_check_null(profile.client);

  run_next_test();
});

add_test(function getProfile_ok() {
  let cachedUrl = "myurl";
  let accountData = mockAccountData();
  let didFetch = false;

  let profile = new FxAccountsProfile(accountData, PROFILE_CLIENT_OPTIONS);
  profile._getCachedProfile = function () {
    return Promise.resolve({ avatar: cachedUrl });
  };

  profile._fetchAndCacheProfile = function () {
    didFetch = true;
  };

  return profile.getProfile()
    .then(result => {
      do_check_eq(result.avatar, cachedUrl);
      do_check_true(didFetch);
      run_next_test();
    });
});

add_test(function getProfile_no_cache() {
  let fetchedUrl = "newUrl";
  let accountData = mockAccountData();

  let profile = new FxAccountsProfile(accountData, PROFILE_CLIENT_OPTIONS);
  profile._getCachedProfile = function () {
    return Promise.resolve();
  };

  profile._fetchAndCacheProfile = function () {
    return Promise.resolve({ avatar: fetchedUrl });
  };

  return profile.getProfile()
    .then(result => {
      do_check_eq(result.avatar, fetchedUrl);
      run_next_test();
    });
});

add_test(function getProfile_has_cached_fetch_deleted() {
  let cachedUrl = "myurl";

  let client = mockClient();
  client.fetchProfile = function () {
    return Promise.resolve({ avatar: null });
  };

  let accountData = mockAccountData();
  accountData.getUserAccountData = function () {
    return Promise.resolve({ profile: { avatar: cachedUrl } });
  };
  accountData.setUserAccountData = function (data) {
    do_check_null(data.profile.avatar);
    run_next_test();
    return Promise.resolve();
  };

  let profile = new FxAccountsProfile(accountData, {
    profileClient: client
  });

  return profile.getProfile()
    .then(result => {
      do_check_eq(result.avatar, "myurl");
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
