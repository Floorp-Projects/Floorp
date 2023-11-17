/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

const SCREENSHOT_FORMAT = { format: "jpeg", quality: 75 };

function RunScriptInFrame(win, script) {
  const contentPrincipal = win.document.nodePrincipal;
  const sandbox = Cu.Sandbox([contentPrincipal], {
    sandboxName: "Report Broken Site webcompat.com helper",
    sandboxPrototype: win,
    sameZoneAs: win,
    originAttributes: contentPrincipal.originAttributes,
  });
  return Cu.evalInSandbox(script, sandbox, null, "sandbox eval code", 1);
}

class ConsoleLogHelper {
  static PREVIEW_MAX_ITEMS = 10;
  static LOG_LEVELS = ["debug", "info", "warn", "error"];

  #windowId = undefined;

  constructor(windowId) {
    this.#windowId = windowId;
  }

  getLoggedMessages(alsoIncludePrivate = true) {
    return this.getConsoleAPIMessages().concat(
      this.getScriptErrors(alsoIncludePrivate)
    );
  }

  getConsoleAPIMessages() {
    const ConsoleAPIStorage = Cc[
      "@mozilla.org/consoleAPI-storage;1"
    ].getService(Ci.nsIConsoleAPIStorage);
    let messages = ConsoleAPIStorage.getEvents(this.#windowId);
    return messages.map(evt => {
      const { columnNumber, filename, level, lineNumber, timeStamp } = evt;

      const args = [];
      for (const arg of evt.arguments) {
        args.push(this.#getArgs(arg));
      }

      const message = {
        level,
        log: args,
        uri: filename,
        pos: `${lineNumber}:${columnNumber}`,
      };

      return { timeStamp, message };
    });
  }

  getScriptErrors(alsoIncludePrivate) {
    const messages = Services.console.getMessageArray();
    return messages
      .filter(message => {
        if (message instanceof Ci.nsIScriptError) {
          if (!alsoIncludePrivate && message.isFromPrivateWindow) {
            return false;
          }
          if (this.#windowId && this.#windowId !== message.innerWindowID) {
            return false;
          }
          return true;
        }

        // If this is not an nsIScriptError and we need to do window-based
        // filtering we skip this message.
        return false;
      })
      .map(error => {
        const {
          timeStamp,
          errorMessage,
          sourceName,
          lineNumber,
          columnNumber,
          logLevel,
        } = error;
        const message = {
          level: ConsoleLogHelper.LOG_LEVELS[logLevel],
          log: [errorMessage],
          uri: sourceName,
          pos: `${lineNumber}:${columnNumber}`,
        };
        return { timeStamp, message };
      });
  }

  #getPreview(value) {
    switch (typeof value) {
      case "symbol":
        return value.toString();

      case "function":
        return "function ()";

      case "object":
        if (value === null) {
          return null;
        }
        if (Array.isArray(value)) {
          return `(${value.length})[...]`;
        }
        return "{...}";

      case "undefined":
        return "undefined";

      default:
        try {
          structuredClone(value);
        } catch (_) {
          return `${value}` || "?";
        }
        return value;
    }
  }

  #getArrayPreview(arr) {
    const preview = [];
    let count = 0;
    for (const value of arr) {
      if (++count > ConsoleLogHelper.PREVIEW_MAX_ITEMS) {
        break;
      }
      preview.push(this.#getPreview(value));
    }

    return preview;
  }

  #getObjectPreview(obj) {
    const preview = {};
    let count = 0;
    for (const key of Object.keys(obj)) {
      if (++count > ConsoleLogHelper.PREVIEW_MAX_ITEMS) {
        break;
      }
      preview[key] = this.#getPreview(obj[key]);
    }

    return preview;
  }

  #getArgs(value) {
    if (typeof value === "object" && value !== null) {
      if (Array.isArray(value)) {
        return this.#getArrayPreview(value);
      }
      return this.#getObjectPreview(value);
    }

    return this.#getPreview(value);
  }
}

const FrameworkDetector = {
  hasFastClickPageScript(window) {
    if (window.FastClick) {
      return true;
    }

    for (const property in window) {
      try {
        const proto = window[property].prototype;
        if (proto && proto.needsClick) {
          return true;
        }
      } catch (_) {}
    }

    return false;
  },

  hasMobifyPageScript(window) {
    return !!window.Mobify?.Tag;
  },

  hasMarfeelPageScript(window) {
    return !!window.marfeel;
  },

  checkWindow(window) {
    const script = `
      (function() {
        function ${FrameworkDetector.hasFastClickPageScript};
        function ${FrameworkDetector.hasMobifyPageScript};
        function ${FrameworkDetector.hasMarfeelPageScript};
        const win = window.wrappedJSObject || window;
        return {
          fastclick: hasFastClickPageScript(win),
          mobify: hasMobifyPageScript(win),
          marfeel: hasMarfeelPageScript(win),
        }
      })();
    `;
    return RunScriptInFrame(window, script);
  },
};

function getSysinfoProperty(propertyName, defaultValue) {
  try {
    return Services.sysinfo.getProperty(propertyName);
  } catch (e) {}
  return defaultValue;
}

function limitStringToLength(str, maxLength) {
  if (typeof str !== "string") {
    return null;
  }
  return str.substring(0, maxLength);
}

const BrowserInfo = {
  getAppInfo() {
    const { userAgent } = Cc[
      "@mozilla.org/network/protocol;1?name=http"
    ].getService(Ci.nsIHttpProtocolHandler);
    return {
      applicationName: Services.appinfo.name,
      buildId: Services.appinfo.appBuildID,
      defaultUserAgent: userAgent,
      updateChannel: AppConstants.MOZ_UPDATE_CHANNEL,
      version: Services.appinfo.version,
    };
  },

  getPrefs() {
    const prefs = {};
    for (const [name, dflt] of Object.entries({
      "layers.acceleration.force-enabled": undefined,
      "gfx.webrender.software": undefined,
      "browser.opaqueResponseBlocking": undefined,
      "extensions.InstallTrigger.enabled": undefined,
      "privacy.resistFingerprinting": undefined,
      "privacy.globalprivacycontrol.enabled": undefined,
    })) {
      prefs[name] = Services.prefs.getBoolPref(name, dflt);
    }
    const cookieBehavior = "network.cookie.cookieBehavior";
    prefs[cookieBehavior] = Services.prefs.getIntPref(cookieBehavior);
    return prefs;
  },

  getPlatformInfo() {
    let memoryMB = getSysinfoProperty("memsize", null);
    if (memoryMB) {
      memoryMB = Math.round(memoryMB / 1024 / 1024);
    }

    const info = {
      fissionEnabled: Services.appinfo.fissionAutostart,
      memoryMB,
      osArchitecture: getSysinfoProperty("arch", null),
      osName: getSysinfoProperty("name", null),
      osVersion: getSysinfoProperty("version", null),
      os: AppConstants.platform,
    };
    if (AppConstants.platform === "android") {
      info.device = getSysinfoProperty("device", null);
      info.isTablet = getSysinfoProperty("tablet", false);
    }
    return info;
  },

  getSecurityInfo() {
    if (AppConstants.platform != "win") {
      return undefined;
    }

    const maxStringLength = 256;

    const keys = [
      ["registeredAntiVirus", "antivirus"],
      ["registeredAntiSpyware", "antispyware"],
      ["registeredFirewall", "firewall"],
    ];

    let result = {};

    for (let [inKey, outKey] of keys) {
      let prop = getSysinfoProperty(inKey, null);
      if (prop) {
        prop = limitStringToLength(prop, maxStringLength).split(";");
      }

      result[outKey] = prop;
    }

    return result;
  },

  getAllData() {
    return {
      app: BrowserInfo.getAppInfo(),
      prefs: BrowserInfo.getPrefs(),
      platform: BrowserInfo.getPlatformInfo(),
      security: BrowserInfo.getSecurityInfo(),
    };
  },
};

export class ReportBrokenSiteChild extends JSWindowActorChild {
  #getWebCompatInfo(docShell) {
    return Promise.all([
      this.#getConsoleLogs(docShell),
      this.sendQuery(
        "GetWebcompatInfoOnlyAvailableInParentProcess",
        SCREENSHOT_FORMAT
      ),
    ]).then(([consoleLog, infoFromParent]) => {
      const { antitracking, graphics, locales, screenshot } = infoFromParent;

      const browser = BrowserInfo.getAllData();
      browser.graphics = graphics;
      browser.locales = locales;

      const win = docShell.domWindow;
      const frameworks = FrameworkDetector.checkWindow(win);

      if (browser.platform.os !== "linux") {
        delete browser.prefs["layers.acceleration.force-enabled"];
      }

      return {
        antitracking,
        browser,
        consoleLog,
        devicePixelRatio: win.devicePixelRatio,
        frameworks,
        languages: win.navigator.languages,
        screenshot,
        url: win.location.href,
        userAgent: win.navigator.userAgent,
      };
    });
  }

  async #getConsoleLogs(docShell) {
    return this.#getLoggedMessages()
      .flat()
      .sort((a, b) => a.timeStamp - b.timeStamp)
      .map(m => m.message);
  }

  #getLoggedMessages(alsoIncludePrivate = false) {
    const windowId = this.contentWindow.windowGlobalChild.innerWindowId;
    const helper = new ConsoleLogHelper(windowId, alsoIncludePrivate);
    return helper.getLoggedMessages();
  }

  #formatReportDataForWebcompatCom({
    reason,
    description,
    reportUrl,
    reporterConfig,
    webcompatInfo,
  }) {
    const extra_labels = [];

    const message = Object.assign({}, reporterConfig, {
      url: reportUrl,
      category: reason,
      description,
      details: {},
      extra_labels,
    });

    const payload = {
      message,
    };

    if (webcompatInfo) {
      const {
        antitracking,
        browser,
        devicePixelRatio,
        consoleLog,
        frameworks,
        languages,
        screenshot,
        url,
        userAgent,
      } = webcompatInfo;

      const {
        blockList,
        isPrivateBrowsing,
        hasMixedActiveContentBlocked,
        hasMixedDisplayContentBlocked,
        hasTrackingContentBlocked,
      } = antitracking;

      message.blockList = blockList;

      const { app, graphics, prefs, platform, security } = browser;

      const { applicationName, version, updateChannel, defaultUserAgent } = app;

      const {
        fissionEnabled,
        memoryMb,
        osArchitecture,
        osName,
        osVersion,
        device,
        isTablet,
      } = platform;

      const additionalData = {
        applicationName,
        blockList,
        devicePixelRatio,
        finalUserAgent: userAgent,
        fissionEnabled,
        gfxData: graphics,
        hasMixedActiveContentBlocked,
        hasMixedDisplayContentBlocked,
        hasTrackingContentBlocked,
        isPB: isPrivateBrowsing,
        languages,
        memoryMb,
        osArchitecture,
        osName,
        osVersion,
        prefs,
        updateChannel,
        userAgent: defaultUserAgent,
        version,
      };
      if (security !== undefined) {
        additionalData.sec = security;
      }
      if (device !== undefined) {
        additionalData.device = device;
      }
      if (isTablet !== undefined) {
        additionalData.isTablet = isTablet;
      }

      const specialPrefs = {};
      for (const pref of [
        "layers.acceleration.force-enabled",
        "gfx.webrender.software",
      ]) {
        specialPrefs[pref] = prefs[pref];
      }

      const details = Object.assign(message.details, specialPrefs, {
        additionalData,
        buildId: browser.buildId,
        blockList,
        channel: browser.updateChannel,
        hasTouchScreen: browser.graphics.hasTouchScreen,
      });

      // If the user enters a URL unrelated to the current tab,
      // don't bother sending a screnshot or logs/etc
      let sendRecordedPageSpecificDetails = false;
      try {
        const givenUri = new URL(reportUrl);
        const recordedUri = new URL(url);
        sendRecordedPageSpecificDetails =
          givenUri.origin == recordedUri.origin &&
          givenUri.pathname == recordedUri.pathname;
      } catch (_) {}

      if (sendRecordedPageSpecificDetails) {
        payload.screenshot = screenshot;

        details.consoleLog = consoleLog;
        details.frameworks = frameworks;
        details["mixed active content blocked"] =
          antitracking.hasMixedActiveContentBlocked;
        details["mixed passive content blocked"] =
          antitracking.hasMixedDisplayContentBlocked;
        details["tracking content blocked"] =
          antitracking.hasTrackingContentBlocked
            ? `true (${antitracking.blockList})`
            : "false";

        if (antitracking.hasTrackingContentBlocked) {
          extra_labels.push(
            `type-tracking-protection-${antitracking.blockList}`
          );
        }

        for (const [framework, active] of Object.entries(frameworks)) {
          if (!active) {
            continue;
          }
          details[framework] = true;
          extra_labels.push(`type-${framework}`);
        }
      }
    }

    return payload;
  }

  #stripNonASCIIChars(str) {
    // eslint-disable-next-line no-control-regex
    return str.replace(/[^\x00-\x7F]/g, "");
  }

  async receiveMessage(msg) {
    const { docShell } = this;
    switch (msg.name) {
      case "SendDataToWebcompatCom": {
        const win = docShell.domWindow;
        const expectedEndpoint = msg.data.endpointUrl;
        if (win.location.href == expectedEndpoint) {
          // Ensure that the tab has fully loaded and is waiting for messages
          const onLoad = () => {
            const payload = this.#formatReportDataForWebcompatCom(msg.data);
            const json = this.#stripNonASCIIChars(JSON.stringify(payload));
            const expectedOrigin = JSON.stringify(
              new URL(expectedEndpoint).origin
            );
            // webcompat.com checks that the message comes from its own origin
            const script = `
            const wrtReady = window.wrappedJSObject?.wrtReady;
            if (wrtReady) {
              console.info("Report Broken Site is waiting");
            }
            Promise.resolve(wrtReady).then(() => {
              console.debug(${json});
              postMessage(${json}, ${expectedOrigin})
            });`;
            RunScriptInFrame(win, script);
          };
          if (win.document.readyState == "complete") {
            onLoad();
          } else {
            win.addEventListener("load", onLoad, { once: true });
          }
        }
        return null;
      }
      case "GetWebCompatInfo": {
        return this.#getWebCompatInfo(docShell);
      }
      case "GetConsoleLog": {
        return this.#getLoggedMessages();
      }
    }
    return null;
  }
}
