/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals browser */

const Config = {
  newIssueEndpoint: "https://webcompat.com/issues/new",
  newIssueEndpointPref: "newIssueEndpoint",
  screenshotFormat: {
    format: "jpeg",
    quality: 75,
  },
};

const FRAMEWORK_KEYS = ["hasFastClick", "hasMobify", "hasMarfeel"];

// If parental controls are on, we don't activate (that is, we don't show our
// menu item or prompt the user when they use "request desktop site".
browser.browserInfo.getParentalControlsEnabled().then(enabled => {
  if (enabled) {
    return;
  }

  browser.aboutConfigPrefs.onEndpointPrefChange.addListener(checkEndpointPref);
  checkEndpointPref();

  activateMenuItem();
  activateDesktopViewPrompts();
});

function activateDesktopViewPrompts() {
  Promise.all([
    browser.l10n.getMessage("webcompat.reportDesktopMode.message"),
    browser.l10n.getMessage("webcompat.reportDesktopModeYes.label"),
  ]).then(([message, button]) => {
    browser.tabExtras.onDesktopSiteRequested.addListener(async tabId => {
      browser.tabs.get(tabId).then(tab => {
        browser.snackbars.show(message, button).then(() => {
          reportForTab(tab);
        }).catch(() => {});
      }).catch(() => {});
    });
  }).catch(() => {});
}

function activateMenuItem() {
  browser.nativeMenu.show();

  browser.l10n.getMessage("webcompat.menu.name").then(label => {
    // We use Fennec NativeMenus because its BrowserAction implementation
    // lacks support for enabling/disabling its items.
    browser.nativeMenu.setLabel(label);
  }).catch(() => {});

  browser.nativeMenu.onClicked.addListener(async () => {
    const tabs = await browser.tabs.query({active: true});
    reportForTab(tabs[0]);
  });

  async function updateMenuItem(url) {
    if (isReportableUrl(url)) {
      await browser.nativeMenu.enable();
    } else {
      await browser.nativeMenu.disable();
    }
  }

  browser.tabs.onUpdated.addListener((tabId, changeInfo, tab) => {
    if ("url" in changeInfo && tab.active) {
      updateMenuItem(tab.url);
    }
  });

  browser.tabs.onActivated.addListener(({tabId}) => {
    browser.tabs.get(tabId).then(({url}) => {
      updateMenuItem(url);
    }).catch(() => {
      updateMenuItem("about"); // So the action is disabled
    });
  });

  browser.tabs.query({active: true}).then(tabs => {
    updateMenuItem(tabs[0].url);
  }).catch(() => {});
}

function isReportableUrl(url) {
  return url && !(url.startsWith("about") ||
                  url.startsWith("chrome") ||
                  url.startsWith("file") ||
                  url.startsWith("resource"));
}

async function reportForTab(tab) {
  return getWebCompatInfoForTab(tab).then(async info => {
    return openWebCompatTab(info, tab.incognito);
  }).catch(err => {
    console.error("Report Site Issue: unexpected error", err);
  });
}

async function checkEndpointPref() {
  const value = await browser.aboutConfigPrefs.getEndpointPref();
  if (value === undefined) {
    browser.aboutConfigPrefs.setEndpointPref(Config.newIssueEndpoint);
  } else {
    Config.newIssueEndpoint = value;
  }
}

function hasFastClickPageScript() {
  const win = window.wrappedJSObject;

  if (win.FastClick) {
    return true;
  }

  for (const property in win) {
    try {
      const proto = win[property].prototype;
      if (proto && proto.needsClick) {
        return true;
      }
    } catch (_) {
    }
  }

  return false;
}

function hasMobifyPageScript() {
  const win = window.wrappedJSObject;
  return !!(win.Mobify && win.Mobify.Tag);
}

function hasMarfeelPageScript() {
  const win = window.wrappedJSObject;
  return !!win.marfeel;
}

function checkForFrameworks(tabId) {
  return browser.tabs.executeScript(tabId, {
    code: `
      (function() {
        ${hasFastClickPageScript};
        ${hasMobifyPageScript};
        ${hasMarfeelPageScript};
        
        const result = {
          hasFastClick: hasFastClickPageScript(),
          hasMobify: hasMobifyPageScript(),
          hasMarfeel: hasMarfeelPageScript(),
        }

        return result;
      })();
    `,
  }).then(([results]) => results).catch(() => false);
}

function getWebCompatInfoForTab(tab) {
  const {id, windiwId, url} = tab;
  return Promise.all([
    browser.browserInfo.getBlockList(),
    browser.browserInfo.getBuildID(),
    browser.browserInfo.getGraphicsPrefs(),
    browser.browserInfo.getUpdateChannel(),
    browser.browserInfo.hasTouchScreen(),
    browser.tabExtras.getWebcompatInfo(id),
    checkForFrameworks(id),
    browser.tabs.captureVisibleTab(windiwId, Config.screenshotFormat).catch(e => {
      console.error("Report Site Issue: getting a screenshot failed", e);
      return Promise.resolve(undefined);
    }),
  ]).then(([blockList, buildID, graphicsPrefs, channel, hasTouchScreen,
            frameInfo, frameworks, screenshot]) => {
    if (channel !== "linux") {
      delete graphicsPrefs["layers.acceleration.force-enabled"];
    }

    const consoleLog = frameInfo.log;
    delete frameInfo.log;

    return Object.assign(frameInfo, {
      tabId: id,
      blockList,
      details: Object.assign(graphicsPrefs, {
        buildID,
        channel,
        consoleLog,
        frameworks,
        hasTouchScreen,
        "mixed active content blocked": frameInfo.hasMixedActiveContentBlocked,
        "mixed passive content blocked": frameInfo.hasMixedDisplayContentBlocked,
        "tracking content blocked": frameInfo.hasTrackingContentBlocked ?
                                    `true (${blockList})` : "false",
      }),
      screenshot,
      url,
    });
  });
}

function stripNonASCIIChars(str) {
  // eslint-disable-next-line no-control-regex
  return str.replace(/[^\x00-\x7F]/g, "");
}

async function openWebCompatTab(compatInfo, usePrivateTab) {
  const url = new URL(Config.newIssueEndpoint);
  const {details} = compatInfo;
  const params = {
    url: `${compatInfo.url}`,
    utm_source: "mobile-reporter",
    utm_campaign: "report-site-issue-button",
    src: "mobile-reporter",
    details,
    extra_labels: [],
  };

  for (let framework of FRAMEWORK_KEYS) {
    if (details.frameworks[framework]) {
      params.details[framework] = true;
      params.extra_labels.push(framework.replace(/^has/, "type-").toLowerCase());
    }
  }
  delete details.frameworks;

  if (details["gfx.webrender.all"] || details["gfx.webrender.enabled"]) {
    params.extra_labels.push("type-webrender-enabled");
  }
  if (compatInfo.hasTrackingContentBlocked) {
    params.extra_labels.push(`type-tracking-protection-${compatInfo.blockList}`);
  }

  // Need custom API for private tabs until https://bugzil.la/1372178 is fixed
  const tab = usePrivateTab ? await browser.tabExtras.createPrivateTab() :
                              await browser.tabs.create({url: "about:blank"});
  const json = stripNonASCIIChars(JSON.stringify(params));
  await browser.tabExtras.loadURIWithPostData(tab.id, url.href, json,
                                              "application/json");
  await browser.tabs.executeScript(tab.id, {
    runAt: "document_end",
    code: `(function() {
      async function sendScreenshot(dataURI) {
        const res = await fetch(dataURI);
        const blob = await res.blob();
        postMessage(blob, "${url.origin}");
      }
      sendScreenshot("${compatInfo.screenshot}");
    })()`,
  });
}
