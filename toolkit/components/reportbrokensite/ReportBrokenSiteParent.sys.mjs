/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

function getSysinfoProperty(propertyName, defaultValue) {
  try {
    return Services.sysinfo.getProperty(propertyName);
  } catch (e) {}
  return defaultValue;
}

function getSecurityInfo() {
  const keys = [
    ["registeredAntiVirus", "antivirus"],
    ["registeredAntiSpyware", "antispyware"],
    ["registeredFirewall", "firewall"],
  ];

  let result = {};

  for (let [inKey, outKey] of keys) {
    const str = getSysinfoProperty(inKey, null);
    result[outKey] = !str ? null : str.split(";");
  }

  // Right now, security data is only available for Windows builds, and
  // we might as well not return anything at all if no data is available.
  if (!Object.values(result).filter(e => e).length) {
    return undefined;
  }

  return result;
}

class DriverInfo {
  constructor(gl, ext) {
    try {
      this.extensions = ext.getParameter(ext.EXTENSIONS);
    } catch (e) {}

    try {
      this.renderer = ext.getParameter(gl.RENDERER);
    } catch (e) {}

    try {
      this.vendor = ext.getParameter(gl.VENDOR);
    } catch (e) {}

    try {
      this.version = ext.getParameter(gl.VERSION);
    } catch (e) {}

    try {
      this.wsiInfo = ext.getParameter(ext.WSI_INFO);
    } catch (e) {}
  }

  equals(info2) {
    return this.renderer == info2.renderer && this.version == info2.version;
  }

  static getByType(driver) {
    const doc = new DOMParser().parseFromString("<html/>", "text/html");
    const canvas = doc.createElement("canvas");
    canvas.width = 1;
    canvas.height = 1;
    let error;
    canvas.addEventListener("webglcontextcreationerror", function (e) {
      error = true;
    });
    let gl = null;
    try {
      gl = canvas.getContext(driver);
    } catch (e) {
      error = true;
    }
    if (error || !gl?.getExtension) {
      return undefined;
    }

    let ext = null;
    try {
      ext = gl.getExtension("MOZ_debug");
    } catch (e) {}
    if (!ext) {
      return undefined;
    }

    const data = new DriverInfo(gl, ext);

    try {
      gl.getExtension("WEBGL_lose_context").loseContext();
    } catch (e) {}

    return data;
  }

  static getAll() {
    const drivers = [];

    function tryDriver(type) {
      const driver = DriverInfo.getByType(type);
      if (driver) {
        drivers.push(driver);
      }
    }

    tryDriver("webgl");
    tryDriver("webgl2");
    tryDriver("webgpu");

    return drivers;
  }
}

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

  #getBasicGraphicsInfo(gfxInfo) {
    const get = name => {
      try {
        return gfxInfo[name];
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
      hasTouchScreen: gfxInfo.getInfo().ApzTouchInput == 1,
      cleartypeParameters: get("clearTypeParameters"),
      targetFrameRate: get("targetFrameRate"),
      devices,
    });
  }

  #getGraphicsInfo() {
    const gfxInfo = Cc["@mozilla.org/gfx/info;1"].getService(Ci.nsIGfxInfo);

    const data = this.#getBasicGraphicsInfo(gfxInfo);

    data.drivers = DriverInfo.getAll().map(({ renderer, vendor, version }) => {
      return { renderer: `${vendor} -- ${renderer}`, version };
    });

    try {
      const info = gfxInfo.CodecSupportInfo;
      if (info) {
        const codecs = {};
        for (const item of gfxInfo.CodecSupportInfo.split("\n")) {
          const [codec, ...types] = item.split(" ");
          if (!codecs[codec]) {
            codecs[codec] = { software: false, hardware: false };
          }
          if (types.includes("SW")) {
            codecs[codec].software = true;
          }
          if (types.includes("HW")) {
            codecs[codec].hardware = true;
          }
        }
        data.codecSupport = codecs;
      }
    } catch (e) {}

    try {
      const { features } = gfxInfo.getFeatureLog();
      data.features = {};
      for (let { name, log, status } of features) {
        for (const item of log.reverse()) {
          if (!item.failureId || item.status != status) {
            continue;
          }
          status = `${status} (${item.message || item.failureId})`;
        }
        data.features[name] = status;
      }
    } catch (e) {}

    try {
      if (AppConstants.platform !== "android") {
        data.monitors = gfxInfo.getMonitors();
      }
    } catch (e) {}

    return data;
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
      case "GetWebcompatInfoOnlyAvailableInParentProcess": {
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
          graphics: this.#getGraphicsInfo(),
          locales: Services.locale.availableLocales,
          screenshot,
          security: getSecurityInfo(),
        };
      }
    }
    return null;
  }
}
