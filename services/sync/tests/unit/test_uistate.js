/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ================================================
// Load mocking/stubbing library, sinon
// docs: http://sinonjs.org/releases/v2.3.2/
Cu.import("resource://gre/modules/Timer.jsm");
const {Loader} = Cu.import("resource://gre/modules/commonjs/toolkit/loader.js", {});
const loader = new Loader.Loader({
  paths: {
    "": "resource://testing-common/",
  },
  globals: {
    setTimeout,
    setInterval,
    clearTimeout,
    clearInterval,
  },
});
const require = Loader.Require(loader, {id: ""});
const sinon = require("sinon-2.3.2");
// ================================================

Cu.import("resource://services-sync/UIState.jsm");

const UIStateInternal = UIState._internal;

add_task(async function test_isReady() {
  UIState.reset();

  let refreshState = sinon.spy(UIStateInternal, "refreshState");

  // On the first call, returns false and triggers a refresh of the state
  ok(!UIState.isReady());
  ok(refreshState.calledOnce);
  refreshState.reset();

  // On subsequent calls, only return true
  ok(UIState.isReady());
  ok(!refreshState.called);

  refreshState.restore();
});

add_task(async function test_refreshState_signedin() {
  UIState.reset();
  const fxAccountsOrig = UIStateInternal.fxAccounts;

  const now = new Date().toString();
  Services.prefs.setCharPref("services.sync.lastSync", now);
  UIStateInternal.syncing = false;

  UIStateInternal.fxAccounts = {
    getSignedInUser: () => Promise.resolve({ verified: true, email: "foo@bar.com" }),
    getSignedInUserProfile: () => Promise.resolve({ displayName: "Foo Bar", avatar: "https://foo/bar" })
  }

  let state = await UIState.refresh();

  equal(state.status, UIState.STATUS_SIGNED_IN);
  equal(state.email, "foo@bar.com");
  equal(state.displayName, "Foo Bar");
  equal(state.avatarURL, "https://foo/bar");
  equal(state.lastSync, now);
  equal(state.syncing, false);

  UIStateInternal.fxAccounts = fxAccountsOrig;
});

add_task(async function test_refreshState_signedin_profile_unavailable() {
  UIState.reset();
  const fxAccountsOrig = UIStateInternal.fxAccounts;

  const now = new Date().toString();
  Services.prefs.setCharPref("services.sync.lastSync", now);
  UIStateInternal.syncing = false;

  UIStateInternal.fxAccounts = {
    getSignedInUser: () => Promise.resolve({ verified: true, email: "foo@bar.com" }),
    getSignedInUserProfile: () => Promise.reject(new Error("Profile unavailable"))
  }

  let state = await UIState.refresh();

  equal(state.status, UIState.STATUS_SIGNED_IN);
  equal(state.email, "foo@bar.com");
  equal(state.displayName, undefined);
  equal(state.avatarURL, undefined);
  equal(state.lastSync, now);
  equal(state.syncing, false);

  UIStateInternal.fxAccounts = fxAccountsOrig;
});

add_task(async function test_refreshState_unconfigured() {
  UIState.reset();
  const fxAccountsOrig = UIStateInternal.fxAccounts;

  let getSignedInUserProfile = sinon.spy();
  UIStateInternal.fxAccounts = {
    getSignedInUser: () => Promise.resolve(null),
    getSignedInUserProfile
  }

  let state = await UIState.refresh();

  equal(state.status, UIState.STATUS_NOT_CONFIGURED);
  equal(state.email, undefined);
  equal(state.displayName, undefined);
  equal(state.avatarURL, undefined);
  equal(state.lastSync, undefined);

  ok(!getSignedInUserProfile.called);

  UIStateInternal.fxAccounts = fxAccountsOrig;
});

add_task(async function test_refreshState_unverified() {
  UIState.reset();
  const fxAccountsOrig = UIStateInternal.fxAccounts;

  let getSignedInUserProfile = sinon.spy();
  UIStateInternal.fxAccounts = {
    getSignedInUser: () => Promise.resolve({ verified: false, email: "foo@bar.com" }),
    getSignedInUserProfile
  }

  let state = await UIState.refresh();

  equal(state.status, UIState.STATUS_NOT_VERIFIED);
  equal(state.email, "foo@bar.com");
  equal(state.displayName, undefined);
  equal(state.avatarURL, undefined);
  equal(state.lastSync, undefined);

  ok(!getSignedInUserProfile.called);

  UIStateInternal.fxAccounts = fxAccountsOrig;
});

add_task(async function test_refreshState_loginFailed() {
  UIState.reset();
  const fxAccountsOrig = UIStateInternal.fxAccounts;

  let loginFailed = sinon.stub(UIStateInternal, "_loginFailed");
  loginFailed.returns(true);

  let getSignedInUserProfile = sinon.spy();
  UIStateInternal.fxAccounts = {
    getSignedInUser: () => Promise.resolve({ verified: true, email: "foo@bar.com" }),
    getSignedInUserProfile
  }

  let state = await UIState.refresh();

  equal(state.status, UIState.STATUS_LOGIN_FAILED);
  equal(state.email, "foo@bar.com");
  equal(state.displayName, undefined);
  equal(state.avatarURL, undefined);
  equal(state.lastSync, undefined);

  ok(!getSignedInUserProfile.called);

  loginFailed.restore();
  UIStateInternal.fxAccounts = fxAccountsOrig;
});

add_task(async function test_observer_refreshState() {
  let refreshState = sinon.spy(UIStateInternal, "refreshState");

  let shouldRefresh = ["weave:service:login:change", "weave:service:login:error",
                       "weave:service:ready", "fxaccounts:onverified",
                       "fxaccounts:onlogin", "fxaccounts:onlogout",
                       "fxaccounts:profilechange"];

  for (let topic of shouldRefresh) {
    let uiUpdateObserved = observeUIUpdate();
    Services.obs.notifyObservers(null, topic);
    await uiUpdateObserved;
    ok(refreshState.calledOnce);
    refreshState.reset();
  }

  refreshState.restore();
});

// Drive the UIState in a configured state.
async function configureUIState(syncing, lastSync = new Date()) {
  UIState.reset();
  const fxAccountsOrig = UIStateInternal.fxAccounts;

  UIStateInternal._syncing = syncing;
  Services.prefs.setCharPref("services.sync.lastSync", lastSync.toString());

  UIStateInternal.fxAccounts = {
    getSignedInUser: () => Promise.resolve({ verified: true, email: "foo@bar.com" }),
    getSignedInUserProfile: () => Promise.resolve({ displayName: "Foo Bar", avatar: "https://foo/bar" })
  }
  await UIState.refresh();
  UIStateInternal.fxAccounts = fxAccountsOrig;
}

add_task(async function test_syncStarted() {
  await configureUIState(false);

  const oldState = Object.assign({}, UIState.get());
  ok(!oldState.syncing);

  let uiUpdateObserved = observeUIUpdate();
  Services.obs.notifyObservers(null, "weave:service:sync:start");
  await uiUpdateObserved;

  const newState = Object.assign({}, UIState.get());
  ok(newState.syncing);
});

add_task(async function test_syncFinished() {
  let yesterday = new Date();
  yesterday.setDate(yesterday.getDate() - 1);
  await configureUIState(true, yesterday);

  const oldState = Object.assign({}, UIState.get());
  ok(oldState.syncing);

  let uiUpdateObserved = observeUIUpdate();
  Services.prefs.setCharPref("services.sync.lastSync", new Date().toString());
  Services.obs.notifyObservers(null, "weave:service:sync:finish");
  await uiUpdateObserved;

  const newState = Object.assign({}, UIState.get());
  ok(!newState.syncing);
  ok(new Date(newState.lastSync) > new Date(oldState.lastSync));
});

add_task(async function test_syncError() {
  let yesterday = new Date();
  yesterday.setDate(yesterday.getDate() - 1);
  await configureUIState(true, yesterday);

  const oldState = Object.assign({}, UIState.get());
  ok(oldState.syncing);

  let uiUpdateObserved = observeUIUpdate();
  Services.obs.notifyObservers(null, "weave:service:sync:error");
  await uiUpdateObserved;

  const newState = Object.assign({}, UIState.get());
  ok(!newState.syncing);
  deepEqual(newState.lastSync, oldState.lastSync);
});

function observeUIUpdate() {
  return new Promise(resolve => {
    let obs = (aSubject, aTopic, aData) => {
      Services.obs.removeObserver(obs, aTopic);
      const state = UIState.get();
      resolve(state);
    }
    Services.obs.addObserver(obs, UIState.ON_UPDATE);
  });
}
