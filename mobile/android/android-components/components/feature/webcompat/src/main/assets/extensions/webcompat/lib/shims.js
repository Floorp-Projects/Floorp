/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals browser, module, onMessageFromTab */

const releaseBranchPromise = browser.appConstants.getReleaseBranch();

const platformPromise = browser.runtime.getPlatformInfo().then(info => {
  return info.os === "android" ? "android" : "desktop";
});

let debug = async function() {
  if ((await releaseBranchPromise) !== "beta_or_release") {
    console.debug.apply(this, arguments);
  }
};
let error = async function() {
  if ((await releaseBranchPromise) !== "beta_or_release") {
    console.error.apply(this, arguments);
  }
};
let warn = async function() {
  if ((await releaseBranchPromise) !== "beta_or_release") {
    console.warn.apply(this, arguments);
  }
};

class Shim {
  constructor(opts) {
    const { matches, unblocksOnOptIn } = opts;

    this.branches = opts.branches;
    this.bug = opts.bug;
    this.file = opts.file;
    this.hosts = opts.hosts;
    this.id = opts.id;
    this.matches = matches;
    this.name = opts.name;
    this.notHosts = opts.notHosts;
    this.onlyIfBlockedByETP = opts.onlyIfBlockedByETP;
    this._options = opts.options || {};
    this.needsShimHelpers = opts.needsShimHelpers;
    this.platform = opts.platform || "all";
    this.unblocksOnOptIn = unblocksOnOptIn;

    this._hostOptIns = new Set();

    this._disabledByConfig = opts.disabled;
    this._disabledGlobally = false;
    this._disabledByPlatform = false;
    this._disabledByReleaseBranch = false;

    const pref = `disabled_shims.${this.id}`;

    browser.aboutConfigPrefs.onPrefChange.addListener(async () => {
      const value = await browser.aboutConfigPrefs.getPref(pref);
      this._disabledPrefValue = value;
      this._onEnabledStateChanged();
    }, pref);

    this.ready = Promise.all([
      browser.aboutConfigPrefs.getPref(pref),
      platformPromise,
      releaseBranchPromise,
    ]).then(([disabledPrefValue, platform, branch]) => {
      this._disabledPrefValue = disabledPrefValue;

      this._disabledByPlatform =
        this.platform !== "all" && this.platform !== platform;

      this._disabledByReleaseBranch = false;
      for (const supportedBranchAndPlatform of this.branches || []) {
        const [
          supportedBranch,
          supportedPlatform,
        ] = supportedBranchAndPlatform.split(":");
        if (
          (!supportedPlatform || supportedPlatform == platform) &&
          supportedBranch != branch
        ) {
          this._disabledByReleaseBranch = true;
        }
      }

      this._preprocessOptions(platform, branch);
      this._onEnabledStateChanged();
    });
  }

  _preprocessOptions(platform, branch) {
    // options may be any value, but can optionally be gated for specified
    // platform/branches, if in the format `{value, branches, platform}`
    this.options = {};
    for (const [k, v] of Object.entries(this._options)) {
      if (v?.value) {
        if (
          (!v.platform || v.platform === platform) &&
          (!v.branches || v.branches.includes(branch))
        ) {
          this.options[k] = v.value;
        }
      } else {
        this.options[k] = v;
      }
    }
  }

  get enabled() {
    if (this._disabledGlobally) {
      return false;
    }

    if (this._disabledPrefValue !== undefined) {
      return !this._disabledPrefValue;
    }

    return (
      !this._disabledByConfig &&
      !this._disabledByPlatform &&
      !this._disabledByReleaseBranch
    );
  }

  enable() {
    this._disabledGlobally = false;
    this._onEnabledStateChanged();
  }

  disable() {
    this._disabledGlobally = true;
    this._onEnabledStateChanged();
  }

  _onEnabledStateChanged() {
    if (!this.enabled) {
      return this._revokeRequestsInETP();
    }
    return this._allowRequestsInETP();
  }

  _allowRequestsInETP() {
    return browser.trackingProtection.allow(this.id, this.matches, {
      hosts: this.hosts,
      notHosts: this.notHosts,
      hostOptIns: Array.from(this._hostOptIns),
    });
  }

  _revokeRequestsInETP() {
    return browser.trackingProtection.revoke(this.id);
  }

  meantForHost(host) {
    const { hosts, notHosts } = this;
    if (hosts || notHosts) {
      if (
        (notHosts && notHosts.includes(host)) ||
        (hosts && !hosts.includes(host))
      ) {
        return false;
      }
    }
    return true;
  }

  isTriggeredByURL(url) {
    if (!this.matches) {
      return false;
    }

    if (!this._matcher) {
      this._matcher = browser.matchPatterns.getMatcher(this.matches);
    }

    return this._matcher.matches(url);
  }

  async onUserOptIn(host) {
    const { matches, unblocksOnOptIn } = this;
    if (unblocksOnOptIn) {
      this.userHasOptedIn = true;
      const toUnblock = matches.concat(unblocksOnOptIn);
      await browser.trackingProtection.allow(this.id, toUnblock, {
        hosts: this.hosts,
        hostOptIns: [host],
      });
    }

    this._hostOptIns.add(host);
  }

  hasUserOptedInAlready(host) {
    return this._hostOptIns.has(host);
  }
}

class Shims {
  constructor(availableShims) {
    if (!browser.trackingProtection) {
      console.error("Required experimental add-on APIs for shims unavailable");
      return;
    }

    this._registerShims(availableShims);

    onMessageFromTab(this._onMessageFromShim.bind(this));

    this.ENABLED_PREF = "enable_shims";
    browser.aboutConfigPrefs.onPrefChange.addListener(() => {
      this._checkEnabledPref();
    }, this.ENABLED_PREF);
    this._haveCheckedEnabledPref = this._checkEnabledPref();
  }

  _registerShims(shims) {
    if (this.shims) {
      throw new Error("_registerShims has already been called");
    }

    this.shims = new Map();
    for (const shimOpts of shims) {
      const { id } = shimOpts;
      if (!this.shims.has(id)) {
        this.shims.set(shimOpts.id, new Shim(shimOpts));
      }
    }

    const allShimPatterns = new Set();
    for (const { matches } of this.shims.values()) {
      for (const matchPattern of matches) {
        allShimPatterns.add(matchPattern);
      }
    }

    if (!allShimPatterns.size) {
      debug("Skipping shims; none enabled");
      return;
    }

    const urls = Array.from(allShimPatterns);
    debug("Shimming these match patterns", urls);

    browser.webRequest.onBeforeRequest.addListener(
      this._ensureShimForRequestOnTab.bind(this),
      { urls, types: ["script"] },
      ["blocking"]
    );
  }

  async _checkEnabledPref() {
    await browser.aboutConfigPrefs.getPref(this.ENABLED_PREF).then(value => {
      if (value === undefined) {
        browser.aboutConfigPrefs.setPref(this.ENABLED_PREF, true);
      } else if (value === false) {
        this.enabled = false;
      } else {
        this.enabled = true;
      }
    });
  }

  get enabled() {
    return this._enabled;
  }

  set enabled(enabled) {
    if (enabled === this._enabled) {
      return;
    }

    this._enabled = enabled;

    for (const shim of this.shims.values()) {
      if (enabled) {
        shim.enable();
      } else {
        shim.disable();
      }
    }
  }

  async _onMessageFromShim(payload, sender, sendResponse) {
    const { tab, frameId } = sender;
    const { id, url } = tab;
    const { shimId, message } = payload;

    // Ignore unknown messages (for instance, from about:compat).
    if (message !== "getOptions" && message !== "optIn") {
      return undefined;
    }

    if (sender.id !== browser.runtime.id || id === -1) {
      throw new Error("not allowed");
    }

    // Important! It is entirely possible for sites to spoof
    // these messages, due to shims allowing web pages to
    // communicate with the extension.

    const shim = this.shims.get(shimId);
    if (!shim?.needsShimHelpers?.includes(message)) {
      throw new Error("not allowed");
    }

    if (message === "getOptions") {
      return shim.options;
    } else if (message === "optIn") {
      try {
        await shim.onUserOptIn(new URL(url).hostname);
        const { name, bug } = shim;
        const origin = new URL(tab.url).origin;
        warn(
          "** User opted in for",
          name,
          "shim on",
          origin,
          "on tab",
          id,
          "frame",
          frameId
        );
        const warning = `${name} is now being allowed on ${origin} for this browsing session. See https://bugzilla.mozilla.org/show_bug.cgi?id=${bug} for details.`;
        await browser.tabs.executeScript(id, {
          code: `console.warn(${JSON.stringify(warning)})`,
          frameId,
          runAt: "document_start",
        });
      } catch (err) {
        console.error(err);
        throw new Error("error");
      }
    }

    return undefined;
  }

  async _ensureShimForRequestOnTab(details) {
    await this._haveCheckedEnabledPref;

    if (!this.enabled) {
      return undefined;
    }

    // We only ever reach this point if a request is for a URL which ought to
    // be shimmed. We never get here if a request is blocked, and we only
    // unblock requests if at least one shim matches it.

    const { frameId, originUrl, requestId, tabId, url } = details;

    // Ignore requests unrelated to tabs
    if (tabId < 0) {
      return undefined;
    }

    // We need to base our checks not on the frame's host, but the tab's.
    const topHost = new URL((await browser.tabs.get(tabId)).url).hostname;
    const unblocked = await browser.trackingProtection.wasRequestUnblocked(
      requestId
    );

    let shimToApply;
    for (const shim of this.shims.values()) {
      await shim.ready;

      if (!shim.enabled) {
        continue;
      }

      // Do not apply the shim if it is only meant to apply when strict mode ETP
      // (content blocking) was going to block the request.
      if (!unblocked && shim.onlyIfBlockedByETP) {
        continue;
      }

      if (!shim.meantForHost(topHost)) {
        continue;
      }

      // If the user has already opted in for this shim, all requests it covers
      // should be allowed; no need for a shim anymore.
      if (shim.hasUserOptedInAlready(topHost)) {
        return undefined;
      }

      // If this URL isn't meant for this shim, don't apply it.
      if (!shim.isTriggeredByURL(url)) {
        continue;
      }

      shimToApply = shim;
      break;
    }

    if (shimToApply) {
      // Note that sites may request the same shim twice, but because the requests
      // may differ enough for some to fail (CSP/CORS/etc), we always re-run the
      // shim JS just in case. Shims should gracefully handle this as well.
      const { bug, file, id, name, needsShimHelpers } = shimToApply;
      warn("Shimming", name, "on tabId", tabId, "frameId", frameId);

      const warning = `${name} is being shimmed by Firefox. See https://bugzilla.mozilla.org/show_bug.cgi?id=${bug} for details.`;

      try {
        if (needsShimHelpers?.length) {
          await browser.tabs.executeScript(tabId, {
            file: "/lib/shim_messaging_helper.js",
            frameId,
            runAt: "document_start",
          });
          const origin = new URL(originUrl).origin;
          await browser.tabs.sendMessage(
            tabId,
            { origin, shimId: id, needsShimHelpers, warning },
            { frameId }
          );
        } else {
          await browser.tabs.executeScript(tabId, {
            code: `console.warn(${JSON.stringify(warning)})`,
            frameId,
            runAt: "document_start",
          });
        }
      } catch (_) {}

      // If any shims matched the script to replace it, then let the original
      // request complete without ever hitting the network, with a blank script.
      return { redirectUrl: browser.runtime.getURL(`shims/${file}`) };
    }

    // Sanity check: if no shims are over-riding a given URL and it was meant to
    // be blocked by ETP, then block it.
    if (unblocked) {
      error("unexpected:", url, "was not shimmed, and had to be re-blocked");
      return { cancel: true };
    }

    debug("ignoring", url);
    return undefined;
  }
}

module.exports = Shims;
