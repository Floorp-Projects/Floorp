/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/FxAccountsCommon.js");
Cu.import("resource://gre/modules/FxAccountsProfileClient.jsm");
Cu.import("resource://gre/modules/FxAccountsProfile.jsm");
Cu.import("resource://gre/modules/PromiseUtils.jsm");

const URL_STRING = "https://example.com";
Services.prefs.setCharPref("identity.fxaccounts.settings.uri", "https://example.com/settings");

const STATUS_SUCCESS = 200;

/**
 * Mock request responder
 * @param {String} response
 *        Mocked raw response from the server
 * @returns {Function}
 */
let mockResponse = function(response) {
  let Request = function(requestUri) {
    // Store the request uri so tests can inspect it
    Request._requestUri = requestUri;
    return {
      setHeader() {},
      head() {
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
let mockResponseError = function(error) {
  return function() {
    return {
      setHeader() {},
      head() {
        this.onComplete(error);
      }
    };
  };
};

let mockClient = function(fxa) {
  let options = {
    serverURL: "http://127.0.0.1:1111/v1",
    fxa,
  };
  return new FxAccountsProfileClient(options);
};

const ACCOUNT_UID = "abc123";
const ACCOUNT_EMAIL = "foo@bar.com";
const ACCOUNT_DATA = {
  uid: ACCOUNT_UID,
  email: ACCOUNT_EMAIL
};

function FxaMock() {
}
FxaMock.prototype = {
  currentAccountState: {
    profile: null,
    get isCurrent() {
      return true;
    }
  },

  getSignedInUser() {
    return Promise.resolve(ACCOUNT_DATA);
  },

  getProfileCache() {
    return Promise.resolve(this.profileCache);
  },

  setProfileCache(profileCache) {
    this.profileCache = profileCache;
    return Promise.resolve();
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
    fxa,
    profileServerUrl: "http://127.0.0.1:1111/v1"
  };
  if (client) {
    options.profileClient = client;
  }
  return new FxAccountsProfile(options);
}

add_test(function cacheProfile_change() {
  let setProfileCacheCalled = false;
  let fxa = mockFxa();
  fxa.setProfileCache = (data) => {
    setProfileCacheCalled = true;
    do_check_eq(data.profile.avatar, "myurl");
    do_check_eq(data.etag, "bogusetag");
    return Promise.resolve();
  };
  let profile = CreateFxAccountsProfile(fxa);

  makeObserver(ON_PROFILE_CHANGE_NOTIFICATION, function(subject, topic, data) {
    do_check_eq(data, ACCOUNT_DATA.uid);
    do_check_true(setProfileCacheCalled);
    run_next_test();
  });

  return profile._cacheProfile({ body: { uid: ACCOUNT_UID, email: ACCOUNT_EMAIL, avatar: "myurl" }, etag: "bogusetag" });
});

add_test(function fetchAndCacheProfile_ok() {
  let client = mockClient(mockFxa());
  client.fetchProfile = function() {
    return Promise.resolve({ body: { uid: ACCOUNT_UID, avatar: "myimg"} });
  };
  let profile = CreateFxAccountsProfile(null, client);
  profile._cachedAt = 12345;

  profile._cacheProfile = function(toCache) {
    do_check_eq(toCache.body.avatar, "myimg");
    return Promise.resolve(toCache.body);
  };

  return profile._fetchAndCacheProfile()
    .then(result => {
      do_check_eq(result.avatar, "myimg");
      do_check_neq(profile._cachedAt, 12345, "cachedAt has been bumped");
      run_next_test();
    });
});

add_test(function fetchAndCacheProfile_always_bumps_cachedAt() {
  let client = mockClient(mockFxa());
  client.fetchProfile = function() {
    return Promise.reject(new Error("oops"));
  };
  let profile = CreateFxAccountsProfile(null, client);
  profile._cachedAt = 12345;

  return profile._fetchAndCacheProfile()
    .then(result => {
      do_throw("Should not succeed");
    }, err => {
      do_check_neq(profile._cachedAt, 12345, "cachedAt has been bumped");
      run_next_test();
    });
});

add_test(function fetchAndCacheProfile_sendsETag() {
  let fxa = mockFxa();
  fxa.profileCache = { profile: {}, etag: "bogusETag" };
  let client = mockClient(fxa);
  client.fetchProfile = function(etag) {
    do_check_eq(etag, "bogusETag");
    return Promise.resolve({ body: { uid: ACCOUNT_UID, email: ACCOUNT_EMAIL, avatar: "myimg"} });
  };
  let profile = CreateFxAccountsProfile(fxa, client);

  return profile._fetchAndCacheProfile()
    .then(result => {
      run_next_test();
    });
});

// Check that a second profile request when one is already in-flight reuses
// the in-flight one.
add_task(async function fetchAndCacheProfileOnce() {
  // A promise that remains unresolved while we fire off 2 requests for
  // a profile.
  let resolveProfile;
  let promiseProfile = new Promise(resolve => {
    resolveProfile = resolve;
  });
  let numFetches = 0;
  let client = mockClient(mockFxa());
  client.fetchProfile = function() {
    numFetches += 1;
    return promiseProfile;
  };
  let fxa = mockFxa();
  let profile = CreateFxAccountsProfile(fxa, client);

  let request1 = profile._fetchAndCacheProfile();
  profile._fetchAndCacheProfile();
  await new Promise(res => setTimeout(res, 0)); // Yield so fetchProfile() is called (promise)

  // should be one request made to fetch the profile (but the promise returned
  // by it remains unresolved)
  do_check_eq(numFetches, 1);

  // resolve the promise.
  resolveProfile({ body: { uid: ACCOUNT_UID, email: ACCOUNT_EMAIL, avatar: "myimg"} });

  // both requests should complete with the same data.
  let got1 = await request1;
  do_check_eq(got1.avatar, "myimg");
  let got2 = await request1;
  do_check_eq(got2.avatar, "myimg");

  // and still only 1 request was made.
  do_check_eq(numFetches, 1);
});

// Check that sharing a single fetch promise works correctly when the promise
// is rejected.
add_task(async function fetchAndCacheProfileOnce() {
  // A promise that remains unresolved while we fire off 2 requests for
  // a profile.
  let rejectProfile;
  let promiseProfile = new Promise((resolve, reject) => {
    rejectProfile = reject;
  });
  let numFetches = 0;
  let client = mockClient(mockFxa());
  client.fetchProfile = function() {
    numFetches += 1;
    return promiseProfile;
  };
  let fxa = mockFxa();
  let profile = CreateFxAccountsProfile(fxa, client);

  let request1 = profile._fetchAndCacheProfile();
  let request2 = profile._fetchAndCacheProfile();
  await new Promise(res => setTimeout(res, 0)); // Yield so fetchProfile() is called (promise)

  // should be one request made to fetch the profile (but the promise returned
  // by it remains unresolved)
  do_check_eq(numFetches, 1);

  // reject the promise.
  rejectProfile("oh noes");

  // both requests should reject.
  try {
    await request1;
    throw new Error("should have rejected");
  } catch (ex) {
    if (ex != "oh noes") {
      throw ex;
    }
  }
  try {
    await request2;
    throw new Error("should have rejected");
  } catch (ex) {
    if (ex != "oh noes") {
      throw ex;
    }
  }

  // but a new request should works.
  client.fetchProfile = function() {
    return Promise.resolve({body: { uid: ACCOUNT_UID, email: ACCOUNT_EMAIL, avatar: "myimg"}});
  };

  let got = await profile._fetchAndCacheProfile();
  do_check_eq(got.avatar, "myimg");
});

add_test(function fetchAndCacheProfile_alreadyCached() {
  let cachedUrl = "cachedurl";
  let fxa = mockFxa();
  fxa.profileCache = { profile: { uid: ACCOUNT_UID, avatar: cachedUrl }, etag: "bogusETag" };
  let client = mockClient(fxa);
  client.fetchProfile = function(etag) {
    do_check_eq(etag, "bogusETag");
    return Promise.resolve(null);
  };

  let profile = CreateFxAccountsProfile(fxa, client);
  profile._cacheProfile = function(toCache) {
    do_throw("This method should not be called.");
  };

  return profile._fetchAndCacheProfile()
    .then(result => {
      do_check_eq(result, null);
      do_check_eq(fxa.profileCache.profile.avatar, cachedUrl);
      run_next_test();
    });
});

// Check that a new profile request within PROFILE_FRESHNESS_THRESHOLD of the
// last one doesn't kick off a new request to check the cached copy is fresh.
add_task(async function fetchAndCacheProfileAfterThreshold() {
  let numFetches = 0;
  let client = mockClient(mockFxa());
  client.fetchProfile = async function() {
    numFetches += 1;
    return {body: {uid: ACCOUNT_UID, email: ACCOUNT_EMAIL, avatar: "myimg"}};
  };
  let profile = CreateFxAccountsProfile(null, client);
  profile.PROFILE_FRESHNESS_THRESHOLD = 1000;

  await profile.getProfile();
  do_check_eq(numFetches, 1);

  await profile.getProfile();
  do_check_eq(numFetches, 1);

  await new Promise(resolve => {
    do_timeout(1000, resolve);
  });

  let origFetchAndCatch = profile._fetchAndCacheProfile;
  let backgroundFetchDone = PromiseUtils.defer();
  profile._fetchAndCacheProfile = async () => {
    await origFetchAndCatch.call(profile);
    backgroundFetchDone.resolve();
  };
  await profile.getProfile();
  await backgroundFetchDone.promise;
  do_check_eq(numFetches, 2);
});

// Check that a new profile request within PROFILE_FRESHNESS_THRESHOLD of the
// last one *does* kick off a new request if ON_PROFILE_CHANGE_NOTIFICATION
// is sent.
add_task(async function fetchAndCacheProfileBeforeThresholdOnNotification() {
  let numFetches = 0;
  let client = mockClient(mockFxa());
  client.fetchProfile = async function() {
    numFetches += 1;
    return {body: {uid: ACCOUNT_UID, email: ACCOUNT_EMAIL, avatar: "myimg"}};
  };
  let profile = CreateFxAccountsProfile(null, client);
  profile.PROFILE_FRESHNESS_THRESHOLD = 1000;

  await profile.getProfile();
  do_check_eq(numFetches, 1);

  Services.obs.notifyObservers(null, ON_PROFILE_CHANGE_NOTIFICATION);

  let origFetchAndCatch = profile._fetchAndCacheProfile;
  let backgroundFetchDone = PromiseUtils.defer();
  profile._fetchAndCacheProfile = async () => {
    await origFetchAndCatch.call(profile);
    backgroundFetchDone.resolve();
  };
  await profile.getProfile();
  await backgroundFetchDone.promise;
  do_check_eq(numFetches, 2);
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

  let fxa = mockFxa();
  fxa.profileCache = { profile: { uid: ACCOUNT_UID, avatar: cachedUrl } };
  let profile = CreateFxAccountsProfile(fxa);

  profile._fetchAndCacheProfile = function() {
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
  let fxa = mockFxa();
  fxa.profileCache = null;
  let profile = CreateFxAccountsProfile(fxa);

  profile._fetchAndCacheProfile = function() {
    return Promise.resolve({ uid: ACCOUNT_UID, avatar: fetchedUrl });
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
  client.fetchProfile = function() {
    return Promise.resolve({ body: { uid: ACCOUNT_UID, email: ACCOUNT_EMAIL, avatar: null } });
  };

  let profile = CreateFxAccountsProfile(fxa, client);
  fxa.profileCache = { profile: { uid: ACCOUNT_UID, email: ACCOUNT_EMAIL, avatar: cachedUrl } };

// instead of checking this in a mocked "save" function, just check after the
// observer
  makeObserver(ON_PROFILE_CHANGE_NOTIFICATION, function(subject, topic, data) {
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

add_test(function getProfile_fetchAndCacheProfile_throws() {
  let fxa = mockFxa();
  fxa.profileCache = { profile: { uid: ACCOUNT_UID, email: ACCOUNT_EMAIL, avatar: "myimg" } };
  let profile = CreateFxAccountsProfile(fxa);

  profile._fetchAndCacheProfile = () => Promise.reject(new Error());

  return profile.getProfile()
    .then(result => {
      do_check_eq(result.avatar, "myimg");
      run_next_test();
    });
});

add_test(function getProfile_email_changed() {
  let fxa = mockFxa();
  let client = mockClient(fxa);
  client.fetchProfile = function() {
    return Promise.resolve({ body: { uid: ACCOUNT_UID, email: "newemail@bar.com" } });
  };
  fxa.handleEmailUpdated = email => {
    do_check_eq(email, "newemail@bar.com");
    run_next_test();
  };

  let profile = CreateFxAccountsProfile(fxa, client);
  return profile._fetchAndCacheProfile();
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
