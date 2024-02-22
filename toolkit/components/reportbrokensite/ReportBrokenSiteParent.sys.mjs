/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Troubleshoot } from "resource://gre/modules/Troubleshoot.sys.mjs";

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

export class ReportBrokenSiteParent extends JSWindowActorParent {
  #getAntitrackingBlockList() {
    // If content-track-digest256 is in the tracking table,
    // the user has enabled the strict list.
    const trackingTable = Services.prefs.getCharPref(
      "urlclassifier.trackingTable"
    );
    return trackingTable.includes("content") ? "strict" : "basic";
  }

  #getAntitrackingInfo(browsingContext) {
    return {
      blockList: this.#getAntitrackingBlockList(),
      isPrivateBrowsing: browsingContext.usePrivateBrowsing,
      hasTrackingContentBlocked: !!(
        browsingContext.currentWindowGlobal.contentBlockingEvents &
        Ci.nsIWebProgressListener.STATE_BLOCKED_TRACKING_CONTENT
      ),
      hasMixedActiveContentBlocked: !!(
        browsingContext.secureBrowserUI.state &
        Ci.nsIWebProgressListener.STATE_BLOCKED_MIXED_ACTIVE_CONTENT
      ),
      hasMixedDisplayContentBlocked: !!(
        browsingContext.secureBrowserUI.state &
        Ci.nsIWebProgressListener.STATE_BLOCKED_MIXED_DISPLAY_CONTENT
      ),
    };
  }

  #parseGfxInfo(info) {
    const get = name => {
      try {
        return info[name];
      } catch (e) {}
      return undefined;
    };

    const clean = rawObj => {
      const obj = JSON.parse(JSON.stringify(rawObj));
      if (!Object.keys(obj).length) {
        return undefined;
      }
      return obj;
    };

    const cleanDevice = (vendorID, deviceID, subsysID) => {
      return clean({ vendorID, deviceID, subsysID });
    };

    const d1 = cleanDevice(
      get("adapterVendorID"),
      get("adapterDeviceID"),
      get("adapterSubsysID")
    );
    const d2 = cleanDevice(
      get("adapterVendorID2"),
      get("adapterDeviceID2"),
      get("adapterSubsysID2")
    );
    const devices = (get("isGPU2Active") ? [d2, d1] : [d1, d2]).filter(
      v => v !== undefined
    );

    return clean({
      direct2DEnabled: get("direct2DEnabled"),
      directWriteEnabled: get("directWriteEnabled"),
      directWriteVersion: get("directWriteVersion"),
      hasTouchScreen: info.ApzTouchInput == 1,
      clearTypeParameters: get("clearTypeParameters"),
      targetFrameRate: get("targetFrameRate"),
      devices,
    });
  }

  #parseCodecSupportInfo(codecSupportInfo) {
    if (!codecSupportInfo) {
      return undefined;
    }

    const codecs = {};
    for (const item of codecSupportInfo.split("\n")) {
      const [codec, ...types] = item.split(" ");
      if (!codecs[codec]) {
        codecs[codec] = { hardware: false, software: false };
      }
      codecs[codec].software ||= types.includes("SW");
      codecs[codec].hardware ||= types.includes("HW");
    }
    return codecs;
  }

  #parseFeatureLog(featureLog = {}) {
    const { features } = featureLog;
    if (!features) {
      return undefined;
    }

    const parsedFeatures = {};
    for (let { name, log, status } of features) {
      for (const item of log.reverse()) {
        if (!item.failureId || item.status != status) {
          continue;
        }
        status = `${status} (${item.message || item.failureId})`;
      }
      parsedFeatures[name] = status;
    }
    return parsedFeatures;
  }

  #getGraphicsInfo(troubleshoot) {
    const { graphics, media } = troubleshoot;
    const { featureLog } = graphics;
    const data = this.#parseGfxInfo(graphics);
    data.drivers = [
      {
        renderer: graphics.webgl1Renderer,
        version: graphics.webgl1Version,
      },
      {
        renderer: graphics.webgl2Renderer,
        version: graphics.webgl2Version,
      },
    ].filter(({ version }) => version && version != "-");

    data.codecSupport = this.#parseCodecSupportInfo(media.codecSupportInfo);
    data.features = this.#parseFeatureLog(featureLog);

    const gfxInfo = Cc["@mozilla.org/gfx/info;1"].getService(Ci.nsIGfxInfo);
    data.monitors = gfxInfo.getMonitors();

    return data;
  }

  #getAppInfo(troubleshootingInfo) {
    const { application } = troubleshootingInfo;
    return {
      applicationName: application.name,
      buildId: application.buildID,
      defaultUserAgent: application.userAgent,
      updateChannel: application.updateChannel,
      version: application.version,
    };
  }

  #getSysinfoProperty(propertyName, defaultValue) {
    try {
      return Services.sysinfo.getProperty(propertyName);
    } catch (e) {}
    return defaultValue;
  }

  #getPrefs() {
    const prefs = {};
    for (const name of [
      "layers.acceleration.force-enabled",
      "gfx.webrender.software",
      "browser.opaqueResponseBlocking",
      "extensions.InstallTrigger.enabled",
      "privacy.resistFingerprinting",
      "privacy.globalprivacycontrol.enabled",
    ]) {
      prefs[name] = Services.prefs.getBoolPref(name, undefined);
    }
    const cookieBehavior = "network.cookie.cookieBehavior";
    prefs[cookieBehavior] = Services.prefs.getIntPref(cookieBehavior, -1);
    return prefs;
  }

  async #getPlatformInfo(troubleshootingInfo) {
    const { application } = troubleshootingInfo;
    const { memorySizeBytes, fissionAutoStart } = application;

    let memoryMB = memorySizeBytes;
    if (memoryMB) {
      memoryMB = Math.round(memoryMB / 1024 / 1024);
    }

    const info = {
      fissionEnabled: fissionAutoStart,
      memoryMB,
      osArchitecture: this.#getSysinfoProperty("arch", null),
      osName: this.#getSysinfoProperty("name", null),
      osVersion: this.#getSysinfoProperty("version", null),
      name: AppConstants.platform,
    };
    if (info.os === "android") {
      info.device = this.#getSysinfoProperty("device", null);
      info.isTablet = this.#getSysinfoProperty("tablet", false);
    }
    if (
      info.osName == "Windows_NT" &&
      (await Services.sysinfo.processInfo).isWindowsSMode
    ) {
      info.osVersion += " S";
    }
    return info;
  }

  #getSecurityInfo(troubleshootingInfo) {
    const result = {};
    for (const [k, v] of Object.entries(troubleshootingInfo.securitySoftware)) {
      result[k.replace("registered", "").toLowerCase()] = v
        ? v.split(";")
        : null;
    }

    // Right now, security data is only available for Windows builds, and
    // we might as well not return anything at all if no data is available.
    if (!Object.values(result).filter(e => e).length) {
      return undefined;
    }

    return result;
  }

  async #getBrowserInfo() {
    const troubleshootingInfo = await Troubleshoot.snapshot();
    return {
      app: this.#getAppInfo(troubleshootingInfo),
      graphics: this.#getGraphicsInfo(troubleshootingInfo),
      locales: troubleshootingInfo.intl.localeService.available,
      prefs: this.#getPrefs(),
      platform: await this.#getPlatformInfo(troubleshootingInfo),
      security: this.#getSecurityInfo(troubleshootingInfo),
    };
  }

  async #getScreenshot(browsingContext, format, quality) {
    const zoom = browsingContext.fullZoom;
    const scale = browsingContext.topChromeWindow?.devicePixelRatio || 1;
    const wgp = browsingContext.currentWindowGlobal;

    const image = await wgp.drawSnapshot(
      undefined, // rect
      scale * zoom,
      "white",
      undefined // resetScrollPosition
    );

    const doc = Services.appShell.hiddenDOMWindow.document;
    const canvas = doc.createElement("canvas");
    canvas.width = image.width;
    canvas.height = image.height;

    const ctx = canvas.getContext("2d", { alpha: false });
    ctx.drawImage(image, 0, 0);
    image.close();

    return canvas.toDataURL(`image/${format}`, quality / 100);
  }

  async receiveMessage(msg) {
    switch (msg.name) {
      case "GetWebcompatInfoFromParentProcess": {
        const { format, quality } = msg.data;
        const screenshot = await this.#getScreenshot(
          msg.target.browsingContext,
          format,
          quality
        ).catch(e => {
          console.error("Report Broken Site: getting a screenshot failed", e);
          return Promise.resolve(undefined);
        });

        return {
          antitracking: this.#getAntitrackingInfo(msg.target.browsingContext),
          browser: await this.#getBrowserInfo(),
          screenshot,
        };
      }
    }
    return null;
  }
}
