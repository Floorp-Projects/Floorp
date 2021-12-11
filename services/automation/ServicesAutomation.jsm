/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/*
 * This module is used in automation to connect the browser to
 * a specific FxA account and trigger FX Sync.
 *
 * To use it, you can call this sequence:
 *
 *    initConfig("https://accounts.stage.mozaws.net");
 *    await Authentication.signIn(username, password);
 *    await Sync.triggerSync();
 *    await Authentication.signOut();
 *
 *
 * Where username is your FxA e-mail. it will connect your browser
 * to that account and trigger a Sync (on stage servers.)
 *
 * You can also use the convenience function that does everything:
 *
 *    await triggerSync(username, password, "https://accounts.stage.mozaws.net");
 *
 */
var EXPORTED_SYMBOLS = ["Sync", "Authentication", "initConfig", "triggerSync"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",
  Log: "resource://gre/modules/Log.jsm",
  Weave: "resource://services-sync/main.js",
  Svc: "resource://services-sync/util.js",
  fxAccounts: "resource://gre/modules/FxAccounts.jsm",
  FxAccountsClient: "resource://gre/modules/FxAccountsClient.jsm",
  FxAccountsConfig: "resource://gre/modules/FxAccountsConfig.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  setTimeout: "resource://gre/modules/Timer.jsm",
  clearTimeout: "resource://gre/modules/Timer.jsm",
});

XPCOMUtils.defineLazyGlobalGetters(this, ["fetch"]);

const AUTOCONFIG_PREF = "identity.fxaccounts.autoconfig.uri";

/*
 * Log helpers.
 */
var _LOG = [];

function LOG(msg, error) {
  console.debug(msg);
  _LOG.push(msg);
  if (error) {
    console.debug(JSON.stringify(error));
    _LOG.push(JSON.stringify(error));
  }
}

function dumpLogs() {
  let res = _LOG.join("\n");
  _LOG = [];
  return res;
}

function promiseObserver(aEventName) {
  LOG("wait for " + aEventName);
  return new Promise(resolve => {
    let handler = () => {
      Svc.Obs.remove(aEventName, handler);
      resolve();
    };
    let handlerTimeout = () => {
      Svc.Obs.remove(aEventName, handler);
      LOG("handler timed out " + aEventName);
      resolve();
    };
    Svc.Obs.add(aEventName, handler);
    setTimeout(handlerTimeout, 3000);
  });
}

/*
 *  Authentication
 *
 *  Used to sign in an FxA account, takes care of
 *  the e-mail verification flow.
 *
 *  Usage:
 *
 *    await Authentication.signIn(username, password);
 */
var Authentication = {
  async isLoggedIn() {
    return !!(await this.getSignedInUser());
  },

  async isReady() {
    let user = await this.getSignedInUser();
    if (user) {
      LOG("current user " + JSON.stringify(user));
    }
    return user && user.verified;
  },

  async getSignedInUser() {
    try {
      return await fxAccounts.getSignedInUser();
    } catch (error) {
      LOG("getSignedInUser() failed", error);
      throw error;
    }
  },

  async shortWaitForVerification(ms) {
    LOG("shortWaitForVerification");
    let userData = await this.getSignedInUser();
    let timeoutID;
    LOG("set a timeout");
    let timeoutPromise = new Promise(resolve => {
      timeoutID = setTimeout(() => {
        LOG(`Warning: no verification after ${ms}ms.`);
        resolve();
      }, ms);
    });
    LOG("set a fxAccounts.whenVerified");
    await Promise.race([
      fxAccounts.whenVerified(userData).finally(() => clearTimeout(timeoutID)),
      timeoutPromise,
    ]);
    LOG("done");
    return this.isReady();
  },

  async _confirmUser(uri) {
    LOG("Open new tab and load verification page");
    let mainWindow = Services.wm.getMostRecentWindow("navigator:browser");
    let newtab = mainWindow.gBrowser.addWebTab(uri);
    let win = mainWindow.gBrowser.getBrowserForTab(newtab);
    win.addEventListener("load", function(e) {
      LOG("load");
    });

    win.addEventListener("loadstart", function(e) {
      LOG("loadstart");
    });

    win.addEventListener("error", function(msg, url, lineNo, columnNo, error) {
      var string = msg.toLowerCase();
      var substring = "script error";
      if (string.indexOf(substring) > -1) {
        LOG("Script Error: See Browser Console for Detail");
      } else {
        var message = [
          "Message: " + msg,
          "URL: " + url,
          "Line: " + lineNo,
          "Column: " + columnNo,
          "Error object: " + JSON.stringify(error),
        ].join(" - ");

        LOG(message);
      }
    });

    LOG("wait for page to load");
    await new Promise(resolve => {
      let handlerTimeout = () => {
        LOG("timed out ");
        resolve();
      };
      var timer = setTimeout(handlerTimeout, 10000);
      win.addEventListener("loadend", function() {
        resolve();
        clearTimeout(timer);
      });
    });
    LOG("Page Loaded");
    let didVerify = await this.shortWaitForVerification(10000);
    LOG("remove tab");
    mainWindow.gBrowser.removeTab(newtab);
    return didVerify;
  },

  /*
   * This whole verification process may be bypassed if the
   * account is whitelisted.
   */
  async _completeVerification(username) {
    LOG("Fetching mail (from restmail) for user " + username);
    let restmailURI = `https://www.restmail.net/mail/${encodeURIComponent(
      username
    )}`;
    let triedAlready = new Set();
    const tries = 10;
    const normalWait = 4000;
    for (let i = 0; i < tries; ++i) {
      let resp = await fetch(restmailURI);
      let messages = await resp.json();
      // Sort so that the most recent emails are first.
      messages.sort((a, b) => new Date(b.receivedAt) - new Date(a.receivedAt));
      for (let m of messages) {
        // We look for a link that has a x-link that we haven't yet tried.
        if (!m.headers["x-link"] || triedAlready.has(m.headers["x-link"])) {
          continue;
        }
        if (!m.headers["x-verify-code"]) {
          continue;
        }
        let confirmLink = m.headers["x-link"];
        triedAlready.add(confirmLink);
        LOG("Trying confirmation link " + confirmLink);
        try {
          if (await this._confirmUser(confirmLink)) {
            LOG("confirmation done");
            return true;
          }
          LOG("confirmation failed");
        } catch (e) {
          LOG(
            "Warning: Failed to follow confirmation link: " +
              Log.exceptionStr(e)
          );
        }
      }
      if (i === 0) {
        // first time through after failing we'll do this.
        LOG("resendVerificationEmail");
        await fxAccounts.resendVerificationEmail();
      }
      if (await this.shortWaitForVerification(normalWait)) {
        return true;
      }
    }
    // One last try.
    return this.shortWaitForVerification(normalWait);
  },

  async signIn(username, password) {
    LOG("Login user: " + username);
    try {
      // Required here since we don't go through the real login page
      LOG("Calling FxAccountsConfig.ensureConfigured");
      await FxAccountsConfig.ensureConfigured();
      let client = new FxAccountsClient();
      LOG("Signing in");
      let credentials = await client.signIn(username, password, true);
      LOG("Signed in, setting up the signed user in fxAccounts");
      await fxAccounts._internal.setSignedInUser(credentials);

      // If the account is not whitelisted for tests, we need to verify it
      if (!credentials.verified) {
        LOG("We need to verify the account");
        await this._completeVerification(username);
      } else {
        LOG("Credentials already verified");
      }
      return true;
    } catch (error) {
      LOG("signIn() failed", error);
      throw error;
    }
  },

  async signOut() {
    if (await Authentication.isLoggedIn()) {
      // Note: This will clean up the device ID.
      await fxAccounts.signOut();
    }
  },
};

/*
 * Sync
 *
 * Used to trigger sync.
 *
 * usage:
 *
 *   await Sync.triggerSync();
 */
var Sync = {
  getSyncLogsDirectory() {
    return OS.Path.join(OS.Constants.Path.profileDir, ...["weave", "logs"]);
  },

  async init() {
    Svc.Obs.add("weave:service:sync:error", this);
    Svc.Obs.add("weave:service:setup-complete", this);
    Svc.Obs.add("weave:service:tracking-started", this);
    // Delay the automatic sync operations, so we can trigger it manually
    Weave.Svc.Prefs.set("scheduler.immediateInterval", 7200);
    Weave.Svc.Prefs.set("scheduler.idleInterval", 7200);
    Weave.Svc.Prefs.set("scheduler.activeInterval", 7200);
    Weave.Svc.Prefs.set("syncThreshold", 10000000);
    // Wipe all the logs
    await this.wipeLogs();
  },

  observe(subject, topic, data) {
    LOG("Event received " + topic);
  },

  async configureSync() {
    // todo, enable all sync engines here
    // the addon engine requires kinto creds...
    LOG("configuring sync");
    console.assert(await Authentication.isReady(), "You are not connected");
    await Weave.Service.configure();
    if (!Weave.Status.ready) {
      await promiseObserver("weave:service:ready");
    }
    if (Weave.Service.locked) {
      await promiseObserver("weave:service:resyncs-finished");
    }
  },

  /*
   * triggerSync() runs the whole process of Syncing.
   *
   * returns 1 on success, 0 on failure.
   */
  async triggerSync() {
    if (!(await Authentication.isLoggedIn())) {
      LOG("Not connected");
      return 1;
    }
    await this.init();
    let result = 1;
    try {
      await this.configureSync();
      LOG("Triggering a sync");
      await Weave.Service.sync();

      // wait a second for things to settle...
      await new Promise(resolve => setTimeout(resolve, 1000));

      LOG("Sync done");
      result = 0;
    } catch (error) {
      LOG("triggerSync() failed", error);
    }

    return result;
  },

  async wipeLogs() {
    let outputDirectory = this.getSyncLogsDirectory();
    if (!(await OS.File.exists(outputDirectory))) {
      return;
    }
    LOG("Wiping existing Sync logs");
    try {
      let iterator = new OS.File.DirectoryIterator(outputDirectory);
      await iterator.forEach(async entry => {
        try {
          await OS.File.remove(entry.path);
        } catch (error) {
          LOG("wipeLogs() could not remove " + entry.path, error);
        }
      });
    } catch (error) {
      LOG("wipeLogs() failed", error);
    }
  },

  async getLogs() {
    let outputDirectory = this.getSyncLogsDirectory();
    let entries = [];

    if (await OS.File.exists(outputDirectory)) {
      // Iterate through the directory
      let iterator = new OS.File.DirectoryIterator(outputDirectory);

      await iterator.forEach(async entry => {
        let info = await OS.File.stat(entry.path);
        entries.push({
          path: entry.path,
          name: entry.name,
          lastModificationDate: info.lastModificationDate,
        });
      });
      entries.sort(function(a, b) {
        return b.lastModificationDate - a.lastModificationDate;
      });
    }

    const promises = entries.map(async entry => {
      let content = await OS.File.read(entry.path, {
        encoding: "utf-8",
      });
      return {
        name: entry.name,
        content,
      };
    });
    return Promise.all(promises);
  },
};

function initConfig(autoconfig) {
  Services.prefs.setCharPref(AUTOCONFIG_PREF, autoconfig);
}

async function triggerSync(username, password, autoconfig) {
  initConfig(autoconfig);
  await Authentication.signIn(username, password);
  var result = await Sync.triggerSync();
  await Authentication.signOut();
  var logs = {
    sync: await Sync.getLogs(),
    condprof: [
      {
        name: "console.txt",
        content: dumpLogs(),
      },
    ],
  };
  return {
    result,
    logs,
  };
}
