/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/FxAccountsCommon.js");
Cu.import("resource://gre/modules/FxAccountsProfileClient.jsm");
Cu.import("resource://gre/modules/FxAccountsProfile.jsm");

const URL_STRING = "https://example.com";
Services.prefs.setCharPref("identity.fxaccounts.settings.uri", "https://example.com/settings");

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

let mockClient = function (fxa) {
  let options = {
    serverURL: "http://127.0.0.1:1111/v1",
    fxa: fxa,
  }
  return new FxAccountsProfileClient(options);
};

const ACCOUNT_DATA = {
  uid: "abc123"
};

function FxaMock() {
}
FxaMock.prototype = {
  currentAccountState: {
    profile: null,
    get isCurrent() true,
  },

  getSignedInUser: function () {
    return Promise.resolve(ACCOUNT_DATA);
  }
};

let mockFxa = function() {
  return new FxaMock();
};

function CreateFxAccountsProfile(fxa = null, client = null) {
  if (!fxa) {
    fxa = mockFxa();
  }
  let options = {
    fxa: fxa,
    profileServerUrl: "http://127.0.0.1:1111/v1"
  }
  if (client) {
    options.profileClient = client;
  }
  return new FxAccountsProfile(options);
}

add_test(function getCachedProfile() {
  let profile = CreateFxAccountsProfile();
  // a little pointless until bug 1157529 is fixed...
  profile._cachedProfile = { avatar: "myurl" };

  return profile._getCachedProfile()
    .then(function (cached) {
      do_check_eq(cached.avatar, "myurl");
      run_next_test();
    });
});

add_test(function cacheProfile_change() {
  let fxa = mockFxa();
/* Saving profile data disabled - bug 1157529
  let setUserAccountDataCalled = false;
  fxa.setUserAccountData = function (data) {
    setUserAccountDataCalled = true;
    do_check_eq(data.profile.avatar, "myurl");
    return Promise.resolve();
  };
*/
  let profile = CreateFxAccountsProfile(fxa);

  makeObserver(ON_PROFILE_CHANGE_NOTIFICATION, function (subject, topic, data) {
    do_check_eq(data, ACCOUNT_DATA.uid);
//    do_check_true(setUserAccountDataCalled); - bug 1157529
    run_next_test();
  });

  return profile._cacheProfile({ avatar: "myurl" });
});

add_test(function cacheProfile_no_change() {
  let fxa = mockFxa();
  let profile = CreateFxAccountsProfile(fxa)
  profile._cachedProfile = { avatar: "myurl" };
// XXX - saving is disabled (but we can leave that in for now as we are
// just checking it is *not* called)
  fxa.setSignedInUser = function (data) {
    throw new Error("should not update account data");
  };

  return profile._cacheProfile({ avatar: "myurl" })
    .then((result) => {
      do_check_false(!!result);
      run_next_test();
    });
});

add_test(function fetchAndCacheProfile_ok() {
  let client = mockClient(mockFxa());
  client.fetchProfile = function () {
    return Promise.resolve({ avatar: "myimg"});
  };
  let profile = CreateFxAccountsProfile(null, client);

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
  let profile = CreateFxAccountsProfile();

  do_check_true(!!profile.client);
  do_check_true(!!profile.fxa);

  profile.tearDown();
  do_check_null(profile.fxa);
  do_check_null(profile.client);

  run_next_test();
});

add_test(function getProfile_ok() {
  let cachedUrl = "myurl";
  let didFetch = false;

  let profile = CreateFxAccountsProfile();
  profile._getCachedProfile = function () {
    return Promise.resolve({ avatar: cachedUrl });
  };

  profile._fetchAndCacheProfile = function () {
    didFetch = true;
    return Promise.resolve();
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
  let profile = CreateFxAccountsProfile();
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

  let fxa = mockFxa();
  let client = mockClient(fxa);
  client.fetchProfile = function () {
    return Promise.resolve({ avatar: null });
  };

  let profile = CreateFxAccountsProfile(fxa, client);
  profile._cachedProfile = { avatar: cachedUrl };

// instead of checking this in a mocked "save" function, just check after the
// observer
  makeObserver(ON_PROFILE_CHANGE_NOTIFICATION, function (subject, topic, data) {
    profile.getProfile()
      .then(profileData => {
        do_check_null(profileData.avatar);
        run_next_test();
      });
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
