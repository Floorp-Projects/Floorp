/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { Troubleshoot } = ChromeUtils.import(
  "resource://gre/modules/Troubleshoot.jsm"
);
const { ResetProfile } = ChromeUtils.import(
  "resource://gre/modules/ResetProfile.jsm"
);
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "PluralForm",
  "resource://gre/modules/PluralForm.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PlacesDBUtils",
  "resource://gre/modules/PlacesDBUtils.jsm"
);

window.addEventListener("load", function onload(event) {
  try {
    window.removeEventListener("load", onload);
    Troubleshoot.snapshot(async function(snapshot) {
      for (let prop in snapshotFormatters) {
        await snapshotFormatters[prop](snapshot[prop]);
      }
    });
    populateActionBox();
    setupEventListeners();
  } catch (e) {
    Cu.reportError(
      "stack of load error for about:support: " + e + ": " + e.stack
    );
  }
});

// Fluent uses lisp-case IDs so this converts
// the SentenceCase info IDs to lisp-case.
const FLUENT_IDENT_REGEX = /^[a-zA-Z][a-zA-Z0-9_-]*$/;
function toFluentID(str) {
  if (!FLUENT_IDENT_REGEX.test(str)) {
    return null;
  }
  return str
    .toString()
    .replace(/([a-z0-9])([A-Z])/g, "$1-$2")
    .toLowerCase();
}

// Each property in this object corresponds to a property in Troubleshoot.jsm's
// snapshot data.  Each function is passed its property's corresponding data,
// and it's the function's job to update the page with it.
var snapshotFormatters = {
  async application(data) {
    $("application-box").textContent = data.name;
    $("useragent-box").textContent = data.userAgent;
    $("os-box").textContent = data.osVersion;
    $("binary-box").textContent = Services.dirsvc.get(
      "XREExeF",
      Ci.nsIFile
    ).path;
    $("supportLink").href = data.supportURL;
    let version = AppConstants.MOZ_APP_VERSION_DISPLAY;
    if (data.vendor) {
      version += " (" + data.vendor + ")";
    }
    $("version-box").textContent = version;
    $("buildid-box").textContent = data.buildID;
    if (data.updateChannel) {
      $("updatechannel-box").textContent = data.updateChannel;
    }
    if (AppConstants.MOZ_UPDATER) {
      $("update-dir-box").textContent = Services.dirsvc.get(
        "UpdRootD",
        Ci.nsIFile
      ).path;
    }
    $("profile-dir-box").textContent = Services.dirsvc.get(
      "ProfD",
      Ci.nsIFile
    ).path;

    try {
      let launcherStatusTextId = "launcher-process-status-unknown";
      switch (data.launcherProcessState) {
        case 0:
        case 1:
        case 2:
          launcherStatusTextId =
            "launcher-process-status-" + data.launcherProcessState;
          break;
      }

      document.l10n.setAttributes(
        $("launcher-process-box"),
        launcherStatusTextId
      );
    } catch (e) {}

    let statusTextId = "multi-process-status-unknown";

    // Whitelist of known values with string descriptions:
    switch (data.autoStartStatus) {
      case 0:
      case 1:
      case 2:
      case 4:
      case 6:
      case 7:
      case 8:
        statusTextId = "multi-process-status-" + data.autoStartStatus;
        break;
    }

    document.l10n.setAttributes(
      $("multiprocess-box-process-count"),
      "multi-process-windows",
      {
        remoteWindows: data.numRemoteWindows,
        totalWindows: data.numTotalWindows,
      }
    );
    document.l10n.setAttributes($("multiprocess-box-status"), statusTextId);

    if (Services.policies) {
      let policiesStrId = "";
      let aboutPolicies = "about:policies";
      switch (data.policiesStatus) {
        case Services.policies.INACTIVE:
          policiesStrId = "policies-inactive";
          break;

        case Services.policies.ACTIVE:
          policiesStrId = "policies-active";
          aboutPolicies += "#active";
          break;

        default:
          policiesStrId = "policies-error";
          aboutPolicies += "#errors";
          break;
      }

      if (data.policiesStatus != Services.policies.INACTIVE) {
        let activePolicies = $.new("a", null, null, {
          href: aboutPolicies,
        });
        document.l10n.setAttributes(activePolicies, policiesStrId);
        $("policies-status").appendChild(activePolicies);
      } else {
        document.l10n.setAttributes($("policies-status"), policiesStrId);
      }
    } else {
      $("policies-status-row").hidden = true;
    }

    let keyLocationServiceGoogleFound = data.keyLocationServiceGoogleFound
      ? "found"
      : "missing";
    document.l10n.setAttributes(
      $("key-location-service-google-box"),
      keyLocationServiceGoogleFound
    );

    let keySafebrowsingGoogleFound = data.keySafebrowsingGoogleFound
      ? "found"
      : "missing";
    document.l10n.setAttributes(
      $("key-safebrowsing-google-box"),
      keySafebrowsingGoogleFound
    );

    let keyMozillaFound = data.keyMozillaFound ? "found" : "missing";
    document.l10n.setAttributes($("key-mozilla-box"), keyMozillaFound);

    $("safemode-box").textContent = data.safeMode;
  },

  crashes(data) {
    if (!AppConstants.MOZ_CRASHREPORTER) {
      return;
    }

    let daysRange = Troubleshoot.kMaxCrashAge / (24 * 60 * 60 * 1000);
    document.l10n.setAttributes($("crashes-title"), "report-crash-for-days", {
      days: daysRange,
    });
    let reportURL;
    try {
      reportURL = Services.prefs.getCharPref("breakpad.reportURL");
      // Ignore any non http/https urls
      if (!/^https?:/i.test(reportURL)) {
        reportURL = null;
      }
    } catch (e) {}
    if (!reportURL) {
      $("crashes-noConfig").style.display = "block";
      $("crashes-noConfig").classList.remove("no-copy");
      return;
    }
    $("crashes-allReports").style.display = "block";

    if (data.pending > 0) {
      document.l10n.setAttributes(
        $("crashes-allReportsWithPending"),
        "pending-reports",
        { reports: data.pending }
      );
    }

    let dateNow = new Date();
    $.append(
      $("crashes-tbody"),
      data.submitted.map(function(crash) {
        let date = new Date(crash.date);
        let timePassed = dateNow - date;
        let formattedDateStrId;
        let formattedDateStrArgs;
        if (timePassed >= 24 * 60 * 60 * 1000) {
          let daysPassed = Math.round(timePassed / (24 * 60 * 60 * 1000));
          formattedDateStrId = "crashes-time-days";
          formattedDateStrArgs = { days: daysPassed };
        } else if (timePassed >= 60 * 60 * 1000) {
          let hoursPassed = Math.round(timePassed / (60 * 60 * 1000));
          formattedDateStrId = "crashes-time-hours";
          formattedDateStrArgs = { hours: hoursPassed };
        } else {
          let minutesPassed = Math.max(Math.round(timePassed / (60 * 1000)), 1);
          formattedDateStrId = "crashes-time-minutes";
          formattedDateStrArgs = { minutes: minutesPassed };
        }
        return $.new("tr", [
          $.new("td", [
            $.new("a", crash.id, null, { href: reportURL + crash.id }),
          ]),
          $.new("td", null, null, {
            "data-l10n-id": formattedDateStrId,
            "data-l10n-args": formattedDateStrArgs,
          }),
        ]);
      })
    );
  },

  extensions(data) {
    $.append(
      $("extensions-tbody"),
      data.map(function(extension) {
        return $.new("tr", [
          $.new("td", extension.name),
          $.new("td", extension.version),
          $.new("td", extension.isActive),
          $.new("td", extension.id),
        ]);
      })
    );
  },

  securitySoftware(data) {
    if (!AppConstants.isPlatformAndVersionAtLeast("win", "6.2")) {
      $("security-software-title").hidden = true;
      $("security-software-table").hidden = true;
      return;
    }

    $("security-software-antivirus").textContent = data.registeredAntiVirus;
    $("security-software-antispyware").textContent = data.registeredAntiSpyware;
    $("security-software-firewall").textContent = data.registeredFirewall;
  },

  features(data) {
    $.append(
      $("features-tbody"),
      data.map(function(feature) {
        return $.new("tr", [
          $.new("td", feature.name),
          $.new("td", feature.version),
          $.new("td", feature.id),
        ]);
      })
    );
  },

  async processes(data) {
    async function buildEntry(name, value) {
      let entryName =
        (await document.l10n.formatValue(
          `process-type-${name.toLowerCase()}`
        )) || name;

      $("processes-tbody").appendChild(
        $.new("tr", [$.new("td", entryName), $.new("td", value)])
      );
    }

    let remoteProcessesCount = Object.values(data.remoteTypes).reduce(
      (a, b) => a + b,
      0
    );
    document.querySelector(
      "#remoteprocesses-row a"
    ).textContent = remoteProcessesCount;

    // Display the regular "web" process type first in the list,
    // and with special formatting.
    if (data.remoteTypes.web) {
      await buildEntry(
        "web",
        `${data.remoteTypes.web} / ${data.maxWebContentProcesses}`
      );
      delete data.remoteTypes.web;
    }

    for (let remoteProcessType in data.remoteTypes) {
      await buildEntry(remoteProcessType, data.remoteTypes[remoteProcessType]);
    }
  },

  modifiedPreferences(data) {
    $.append(
      $("prefs-tbody"),
      sortedArrayFromObject(data).map(function([name, value]) {
        return $.new("tr", [
          $.new("td", name, "pref-name"),
          // Very long preference values can cause users problems when they
          // copy and paste them into some text editors.  Long values generally
          // aren't useful anyway, so truncate them to a reasonable length.
          $.new("td", String(value).substr(0, 120), "pref-value"),
        ]);
      })
    );
  },

  lockedPreferences(data) {
    $.append(
      $("locked-prefs-tbody"),
      sortedArrayFromObject(data).map(function([name, value]) {
        return $.new("tr", [
          $.new("td", name, "pref-name"),
          $.new("td", String(value).substr(0, 120), "pref-value"),
        ]);
      })
    );
  },

  async graphics(data) {
    function localizedMsg(msg) {
      if (typeof msg == "object" && msg.key) {
        return document.l10n.formatValue(msg.key, msg.args);
      }
      let msgId = toFluentID(msg);
      if (msgId) {
        return document.l10n.formatValue(msgId);
      }
      return "";
    }

    // Read APZ info out of data.info, stripping it out in the process.
    let apzInfo = [];
    let formatApzInfo = function(info) {
      let out = [];
      for (let type of ["Wheel", "Touch", "Drag", "Keyboard", "Autoscroll"]) {
        let key = "Apz" + type + "Input";

        if (!(key in info)) {
          continue;
        }

        delete info[key];

        out.push(toFluentID(type.toLowerCase() + "Enabled"));
      }

      return out;
    };

    // Create a <tr> element with key and value columns.
    //
    // @key      Text in the key column. Localized automatically, unless starts with "#".
    // @value    Fluent ID for text in the value column, or array of children.
    function buildRow(key, value) {
      let title = key[0] == "#" ? key.substr(1) : key;
      let keyStrId = toFluentID(key);
      let valueStrId = Array.isArray(value) ? null : toFluentID(value);
      let td = $.new("td", value);
      td.style["white-space"] = "pre-wrap";
      if (valueStrId) {
        document.l10n.setAttributes(td, valueStrId);
      }

      let th = $.new("th", title, "column");
      if (!key.startsWith("#")) {
        document.l10n.setAttributes(th, keyStrId);
      }
      return $.new("tr", [th, td]);
    }

    // @where    The name in "graphics-<name>-tbody", of the element to append to.
    // @trs      Array of row elements.
    function addRows(where, trs) {
      $.append($("graphics-" + where + "-tbody"), trs);
    }

    // Build and append a row.
    //
    // @where    The name in "graphics-<name>-tbody", of the element to append to.
    function addRow(where, key, value) {
      addRows(where, [buildRow(key, value)]);
    }
    if ("info" in data) {
      apzInfo = formatApzInfo(data.info);

      let trs = sortedArrayFromObject(data.info).map(function([prop, val]) {
        let td = $.new("td", String(val));
        td.style["word-break"] = "break-all";
        return $.new("tr", [$.new("th", prop, "column"), td]);
      });
      addRows("diagnostics", trs);

      delete data.info;
    }

    let windowUtils = window.windowUtils;
    let gpuProcessPid = windowUtils.gpuProcessPid;

    if (gpuProcessPid != -1) {
      let gpuProcessKillButton = null;
      if (AppConstants.NIGHTLY_BUILD || AppConstants.MOZ_DEV_EDITION) {
        gpuProcessKillButton = $.new("button");

        gpuProcessKillButton.addEventListener("click", function() {
          windowUtils.terminateGPUProcess();
        });

        document.l10n.setAttributes(
          gpuProcessKillButton,
          "gpu-process-kill-button"
        );
      }

      addRow("diagnostics", "gpu-process-pid", [new Text(gpuProcessPid)]);
      if (gpuProcessKillButton) {
        addRow("diagnostics", "gpu-process", [gpuProcessKillButton]);
      }
    }

    if (
      (AppConstants.NIGHTLY_BUILD || AppConstants.MOZ_DEV_EDITION) &&
      AppConstants.platform != "macosx"
    ) {
      let gpuDeviceResetButton = $.new("button");

      gpuDeviceResetButton.addEventListener("click", function() {
        windowUtils.triggerDeviceReset();
      });

      document.l10n.setAttributes(
        gpuDeviceResetButton,
        "gpu-device-reset-button"
      );
      addRow("diagnostics", "gpu-device-reset", [gpuDeviceResetButton]);
    }

    // graphics-failures-tbody tbody
    if ("failures" in data) {
      // If indices is there, it should be the same length as failures,
      // (see Troubleshoot.jsm) but we check anyway:
      if ("indices" in data && data.failures.length == data.indices.length) {
        let combined = [];
        for (let i = 0; i < data.failures.length; i++) {
          let assembled = assembleFromGraphicsFailure(i, data);
          combined.push(assembled);
        }
        combined.sort(function(a, b) {
          if (a.index < b.index) {
            return -1;
          }
          if (a.index > b.index) {
            return 1;
          }
          return 0;
        });
        $.append(
          $("graphics-failures-tbody"),
          combined.map(function(val) {
            return $.new("tr", [
              $.new("th", val.header, "column"),
              $.new("td", val.message),
            ]);
          })
        );
        delete data.indices;
      } else {
        $.append($("graphics-failures-tbody"), [
          $.new("tr", [
            $.new("th", "LogFailure", "column"),
            $.new(
              "td",
              data.failures.map(function(val) {
                return $.new("p", val);
              })
            ),
          ]),
        ]);
      }
      delete data.failures;
    } else {
      $("graphics-failures-tbody").style.display = "none";
    }

    // Add a new row to the table, and take the key (or keys) out of data.
    //
    // @where        Table section to add to.
    // @key          Data key to use.
    // @colKey       The localization key to use, if different from key.
    async function addRowFromKey(where, key, colKey) {
      if (!(key in data)) {
        return;
      }
      colKey = colKey || key;

      let value;
      let messageKey = key + "Message";
      if (messageKey in data) {
        value = await localizedMsg(data[messageKey]);
        delete data[messageKey];
      } else {
        value = data[key];
      }
      delete data[key];

      if (value) {
        addRow(where, colKey, [new Text(value)]);
      }
    }

    // graphics-features-tbody
    let compositor = "";
    if (data.windowLayerManagerRemote) {
      compositor = data.windowLayerManagerType;
      if (data.windowUsingAdvancedLayers) {
        compositor += " (Advanced Layers)";
      }
    } else {
      let noOMTCString = await document.l10n.formatValue("main-thread-no-omtc");
      compositor = "BasicLayers (" + noOMTCString + ")";
    }
    addRow("features", "compositing", [new Text(compositor)]);
    delete data.windowLayerManagerRemote;
    delete data.windowLayerManagerType;
    delete data.numTotalWindows;
    delete data.numAcceleratedWindows;
    delete data.numAcceleratedWindowsMessage;
    delete data.windowUsingAdvancedLayers;

    addRow(
      "features",
      "asyncPanZoom",
      apzInfo.length
        ? [
            new Text(
              (await document.l10n.formatValues(
                apzInfo.map(id => {
                  return { id };
                })
              )).join("; ")
            ),
          ]
        : "apz-none"
    );
    let featureKeys = [
      "webgl1WSIInfo",
      "webgl1Renderer",
      "webgl1Version",
      "webgl1DriverExtensions",
      "webgl1Extensions",
      "webgl2WSIInfo",
      "webgl2Renderer",
      "webgl2Version",
      "webgl2DriverExtensions",
      "webgl2Extensions",
      ["supportsHardwareH264", "hardware-h264"],
      ["direct2DEnabled", "#Direct2D"],
      ["windowProtocol", "graphics-window-protocol"],
      "usesTiling",
      "contentUsesTiling",
      "offMainThreadPaintEnabled",
      "offMainThreadPaintWorkerCount",
      "targetFrameRate",
    ];
    for (let feature of featureKeys) {
      if (Array.isArray(feature)) {
        await addRowFromKey("features", feature[0], feature[1]);
        continue;
      }
      await addRowFromKey("features", feature);
    }

    if ("directWriteEnabled" in data) {
      let message = data.directWriteEnabled;
      if ("directWriteVersion" in data) {
        message += " (" + data.directWriteVersion + ")";
      }
      await addRow("features", "#DirectWrite", [new Text(message)]);
      delete data.directWriteEnabled;
      delete data.directWriteVersion;
    }

    // Adapter tbodies.
    let adapterKeys = [
      ["adapterDescription", "gpu-description"],
      ["adapterVendorID", "gpu-vendor-id"],
      ["adapterDeviceID", "gpu-device-id"],
      ["driverVendor", "gpu-driver-vendor"],
      ["driverVersion", "gpu-driver-version"],
      ["driverDate", "gpu-driver-date"],
      ["adapterDrivers", "gpu-drivers"],
      ["adapterSubsysID", "gpu-subsys-id"],
      ["adapterRAM", "gpu-ram"],
    ];

    function showGpu(id, suffix) {
      function get(prop) {
        return data[prop + suffix];
      }

      let trs = [];
      for (let [prop, key] of adapterKeys) {
        let value = get(prop);
        if (value === undefined || value === "") {
          continue;
        }
        trs.push(buildRow(key, value));
      }

      if (trs.length == 0) {
        $("graphics-" + id + "-tbody").style.display = "none";
        return;
      }

      let active = "yes";
      if ("isGPU2Active" in data && (suffix == "2") != data.isGPU2Active) {
        active = "no";
      }

      addRow(id, "gpu-active", active);
      addRows(id, trs);
    }
    showGpu("gpu-1", "");
    showGpu("gpu-2", "2");

    // Remove adapter keys.
    for (let [prop /* key */] of adapterKeys) {
      delete data[prop];
      delete data[prop + "2"];
    }
    delete data.isGPU2Active;

    let featureLog = data.featureLog;
    delete data.featureLog;

    let features = [];
    for (let feature of featureLog.features) {
      // Only add interesting decisions - ones that were not automatic based on
      // all.js/StaticPrefs defaults.
      if (feature.log.length > 1 || feature.log[0].status != "available") {
        features.push(feature);
      }
    }

    if (features.length) {
      for (let feature of features) {
        let trs = [];
        for (let entry of feature.log) {
          if (entry.type == "default" && entry.status == "available") {
            continue;
          }

          let contents;
          if (entry.message.length > 0 && entry.message[0] == "#") {
            // This is a failure ID. See nsIGfxInfo.idl.
            let m = /#BLOCKLIST_FEATURE_FAILURE_BUG_(\d+)/.exec(entry.message);
            if (m) {
              let bugSpan = $.new("span");
              document.l10n.setAttributes(bugSpan, "blocklisted-bug");

              let bugHref = $.new("a");
              bugHref.href =
                "https://bugzilla.mozilla.org/show_bug.cgi?id=" + m[1];
              document.l10n.setAttributes(bugHref, "bug-link", {
                bugNumber: m[1],
              });

              contents = [bugSpan, bugHref];
            } else {
              let unknownFailure = $.new("span");
              document.l10n.setAttributes(unknownFailure, "unknown-failure", {
                failureCode: entry.message.substr(1),
              });
              contents = [unknownFailure];
            }
          } else {
            contents =
              entry.status + " by " + entry.type + ": " + entry.message;
          }

          trs.push($.new("tr", [$.new("td", contents)]));
        }
        addRow("decisions", "#" + feature.name, [$.new("table", trs)]);
      }
    } else {
      $("graphics-decisions-tbody").style.display = "none";
    }

    if (featureLog.fallbacks.length) {
      for (let fallback of featureLog.fallbacks) {
        addRow("workarounds", "#" + fallback.name, [
          new Text(fallback.message),
        ]);
      }
    } else {
      $("graphics-workarounds-tbody").style.display = "none";
    }

    let crashGuards = data.crashGuards;
    delete data.crashGuards;

    if (crashGuards.length) {
      for (let guard of crashGuards) {
        let resetButton = $.new("button");
        let onClickReset = function() {
          Services.prefs.setIntPref(guard.prefName, 0);
          resetButton.removeEventListener("click", onClickReset);
          resetButton.disabled = true;
        };

        document.l10n.setAttributes(resetButton, "reset-on-next-restart");
        resetButton.addEventListener("click", onClickReset);

        addRow("crashguards", guard.type + "CrashGuard", [resetButton]);
      }
    } else {
      $("graphics-crashguards-tbody").style.display = "none";
    }

    // Now that we're done, grab any remaining keys in data and drop them into
    // the diagnostics section.
    for (let key in data) {
      let value = data[key];
      addRow("diagnostics", key, [new Text(value)]);
    }
  },

  media(data) {
    function insertBasicInfo(key, value) {
      function createRow(key, value) {
        let th = $.new("th", null, "column");
        document.l10n.setAttributes(th, key);
        let td = $.new("td", value);
        td.style["white-space"] = "pre-wrap";
        td.colSpan = 8;
        return $.new("tr", [th, td]);
      }
      $.append($("media-info-tbody"), [createRow(key, value)]);
    }

    function createDeviceInfoRow(device) {
      let deviceInfo = Ci.nsIAudioDeviceInfo;

      let states = {};
      states[deviceInfo.STATE_DISABLED] = "Disabled";
      states[deviceInfo.STATE_UNPLUGGED] = "Unplugged";
      states[deviceInfo.STATE_ENABLED] = "Enabled";

      let preferreds = {};
      preferreds[deviceInfo.PREF_NONE] = "None";
      preferreds[deviceInfo.PREF_MULTIMEDIA] = "Multimedia";
      preferreds[deviceInfo.PREF_VOICE] = "Voice";
      preferreds[deviceInfo.PREF_NOTIFICATION] = "Notification";
      preferreds[deviceInfo.PREF_ALL] = "All";

      let formats = {};
      formats[deviceInfo.FMT_S16LE] = "S16LE";
      formats[deviceInfo.FMT_S16BE] = "S16BE";
      formats[deviceInfo.FMT_F32LE] = "F32LE";
      formats[deviceInfo.FMT_F32BE] = "F32BE";

      function toPreferredString(preferred) {
        if (preferred == deviceInfo.PREF_NONE) {
          return preferreds[deviceInfo.PREF_NONE];
        } else if (preferred & deviceInfo.PREF_ALL) {
          return preferreds[deviceInfo.PREF_ALL];
        }
        let str = "";
        for (let pref of [
          deviceInfo.PREF_MULTIMEDIA,
          deviceInfo.PREF_VOICE,
          deviceInfo.PREF_NOTIFICATION,
        ]) {
          if (preferred & pref) {
            str += " " + preferreds[pref];
          }
        }
        return str;
      }

      function toFromatString(dev) {
        let str = "default: " + formats[dev.defaultFormat] + ", support:";
        for (let fmt of [
          deviceInfo.FMT_S16LE,
          deviceInfo.FMT_S16BE,
          deviceInfo.FMT_F32LE,
          deviceInfo.FMT_F32BE,
        ]) {
          if (dev.supportedFormat & fmt) {
            str += " " + formats[fmt];
          }
        }
        return str;
      }

      function toRateString(dev) {
        return (
          "default: " +
          dev.defaultRate +
          ", support: " +
          dev.minRate +
          " - " +
          dev.maxRate
        );
      }

      function toLatencyString(dev) {
        return dev.minLatency + " - " + dev.maxLatency;
      }

      return $.new("tr", [
        $.new("td", device.name),
        $.new("td", device.groupId),
        $.new("td", device.vendor),
        $.new("td", states[device.state]),
        $.new("td", toPreferredString(device.preferred)),
        $.new("td", toFromatString(device)),
        $.new("td", device.maxChannels),
        $.new("td", toRateString(device)),
        $.new("td", toLatencyString(device)),
      ]);
    }

    function insertDeviceInfo(side, devices) {
      let rows = [];
      for (let dev of devices) {
        rows.push(createDeviceInfoRow(dev));
      }
      $.append($("media-" + side + "-devices-tbody"), rows);
    }

    function insertEnumerateDatabase() {
      let button = $("enumerate-database-button");
      if (button) {
        button.addEventListener("click", function(event) {
          let { KeyValueService } = ChromeUtils.import(
            "resource://gre/modules/kvstore.jsm"
          );
          let currProfDir = Services.dirsvc.get("ProfD", Ci.nsIFile);
          currProfDir.append("mediacapabilities");
          let path = currProfDir.path;

          function enumerateDatabase(name) {
            KeyValueService.getOrCreate(path, name)
              .then(database => {
                return database.enumerate();
              })
              .then(enumerator => {
                var logs = [];
                logs.push(`${name}:`);
                while (enumerator.hasMoreElements()) {
                  const { key, value } = enumerator.getNext();
                  logs.push(`${key}: ${value}`);
                }
                $("enumerate-database-result").textContent +=
                  logs.join("\n") + "\n";
              })
              .catch(err => {
                $("enumerate-database-result").textContent += `${name}:\n`;
              });
          }

          $("enumerate-database-result").style.display = "block";
          $("enumerate-database-result").classList.remove("no-copy");
          $("enumerate-database-result").textContent = "";

          enumerateDatabase("video/av1");
          enumerateDatabase("video/vp8");
          enumerateDatabase("video/vp9");
          enumerateDatabase("video/avc");
          enumerateDatabase("video/theora");
        });
      }
    }

    // Basic information
    insertBasicInfo("audio-backend", data.currentAudioBackend);
    insertBasicInfo("max-audio-channels", data.currentMaxAudioChannels);
    insertBasicInfo("sample-rate", data.currentPreferredSampleRate);

    // Output devices information
    insertDeviceInfo("output", data.audioOutputDevices);

    // Input devices information
    insertDeviceInfo("input", data.audioInputDevices);

    // Media Capabilitites
    insertEnumerateDatabase();
  },

  javaScript(data) {
    $("javascript-incremental-gc").textContent = data.incrementalGCEnabled;
  },

  accessibility(data) {
    $("a11y-activated").textContent = data.isActive;
    $("a11y-force-disabled").textContent = data.forceDisabled || 0;

    let a11yHandlerUsed = $("a11y-handler-used");
    if (a11yHandlerUsed) {
      a11yHandlerUsed.textContent = data.handlerUsed;
    }

    let a11yInstantiator = $("a11y-instantiator");
    if (a11yInstantiator) {
      a11yInstantiator.textContent = data.instantiator;
    }
  },

  libraryVersions(data) {
    let trs = [
      $.new("tr", [
        $.new("th", ""),
        $.new("th", null, null, { "data-l10n-id": "min-lib-versions" }),
        $.new("th", null, null, { "data-l10n-id": "loaded-lib-versions" }),
      ]),
    ];
    sortedArrayFromObject(data).forEach(function([name, val]) {
      trs.push(
        $.new("tr", [
          $.new("td", name),
          $.new("td", val.minVersion),
          $.new("td", val.version),
        ])
      );
    });
    $.append($("libversions-tbody"), trs);
  },

  userJS(data) {
    if (!data.exists) {
      return;
    }
    let userJSFile = Services.dirsvc.get("PrefD", Ci.nsIFile);
    userJSFile.append("user.js");
    $("prefs-user-js-link").href = Services.io.newFileURI(userJSFile).spec;
    $("prefs-user-js-section").style.display = "";
    // Clear the no-copy class
    $("prefs-user-js-section").className = "";
  },

  sandbox(data) {
    if (!AppConstants.MOZ_SANDBOX) {
      return;
    }

    let tbody = $("sandbox-tbody");
    for (let key in data) {
      // Simplify the display a little in the common case.
      if (
        key === "hasPrivilegedUserNamespaces" &&
        data[key] === data.hasUserNamespaces
      ) {
        continue;
      }
      if (key === "syscallLog") {
        // Not in this table.
        continue;
      }
      let keyStrId = toFluentID(key);
      let th = $.new("th", null, "column");
      document.l10n.setAttributes(th, keyStrId);
      tbody.appendChild($.new("tr", [th, $.new("td", data[key])]));
    }

    if ("syscallLog" in data) {
      let syscallBody = $("sandbox-syscalls-tbody");
      let argsHead = $("sandbox-syscalls-argshead");
      for (let syscall of data.syscallLog) {
        if (argsHead.colSpan < syscall.args.length) {
          argsHead.colSpan = syscall.args.length;
        }
        let procTypeStrId = toFluentID(syscall.procType);
        let cells = [
          $.new("td", syscall.index, "integer"),
          $.new("td", syscall.msecAgo / 1000),
          $.new("td", syscall.pid, "integer"),
          $.new("td", syscall.tid, "integer"),
          $.new("td", null, null, {
            "data-l10n-id": "sandbox-proc-type-" + procTypeStrId,
          }),
          $.new("td", syscall.syscall, "integer"),
        ];
        for (let arg of syscall.args) {
          cells.push($.new("td", arg, "integer"));
        }
        syscallBody.appendChild($.new("tr", cells));
      }
    }
  },

  intl(data) {
    $("intl-locale-requested").textContent = JSON.stringify(
      data.localeService.requested
    );
    $("intl-locale-available").textContent = JSON.stringify(
      data.localeService.available
    );
    $("intl-locale-supported").textContent = JSON.stringify(
      data.localeService.supported
    );
    $("intl-locale-regionalprefs").textContent = JSON.stringify(
      data.localeService.regionalPrefs
    );
    $("intl-locale-default").textContent = JSON.stringify(
      data.localeService.defaultLocale
    );

    $("intl-osprefs-systemlocales").textContent = JSON.stringify(
      data.osPrefs.systemLocales
    );
    $("intl-osprefs-regionalprefs").textContent = JSON.stringify(
      data.osPrefs.regionalPrefsLocales
    );
  },
};

var $ = document.getElementById.bind(document);

$.new = function $_new(tag, textContentOrChildren, className, attributes) {
  let elt = document.createElement(tag);
  if (className) {
    elt.className = className;
  }
  if (attributes) {
    if (attributes["data-l10n-id"]) {
      let args = attributes.hasOwnProperty("data-l10n-args")
        ? attributes["data-l10n-args"]
        : undefined;
      document.l10n.setAttributes(elt, attributes["data-l10n-id"], args);
      delete attributes["data-l10n-id"];
      if (args) {
        delete attributes["data-l10n-args"];
      }
    }

    for (let attrName in attributes) {
      elt.setAttribute(attrName, attributes[attrName]);
    }
  }
  if (Array.isArray(textContentOrChildren)) {
    this.append(elt, textContentOrChildren);
  } else if (!attributes || !attributes["data-l10n-id"]) {
    elt.textContent = String(textContentOrChildren);
  }
  return elt;
};

$.append = function $_append(parent, children) {
  children.forEach(c => parent.appendChild(c));
};

function assembleFromGraphicsFailure(i, data) {
  // Only cover the cases we have today; for example, we do not have
  // log failures that assert and we assume the log level is 1/error.
  let message = data.failures[i];
  let index = data.indices[i];
  let what = "";
  if (message.search(/\[GFX1-\]: \(LF\)/) == 0) {
    // Non-asserting log failure - the message is substring(14)
    what = "LogFailure";
    message = message.substring(14);
  } else if (message.search(/\[GFX1-\]: /) == 0) {
    // Non-asserting - the message is substring(9)
    what = "Error";
    message = message.substring(9);
  } else if (message.search(/\[GFX1\]: /) == 0) {
    // Asserting - the message is substring(8)
    what = "Assert";
    message = message.substring(8);
  }
  let assembled = {
    index,
    header: "(#" + index + ") " + what,
    message,
  };
  return assembled;
}

function sortedArrayFromObject(obj) {
  let tuples = [];
  for (let prop in obj) {
    tuples.push([prop, obj[prop]]);
  }
  tuples.sort(([prop1, v1], [prop2, v2]) => prop1.localeCompare(prop2));
  return tuples;
}

function copyRawDataToClipboard(button) {
  if (button) {
    button.disabled = true;
  }
  try {
    Troubleshoot.snapshot(async function(snapshot) {
      if (button) {
        button.disabled = false;
      }
      let str = Cc["@mozilla.org/supports-string;1"].createInstance(
        Ci.nsISupportsString
      );
      str.data = JSON.stringify(snapshot, undefined, 2);
      let transferable = Cc[
        "@mozilla.org/widget/transferable;1"
      ].createInstance(Ci.nsITransferable);
      transferable.init(getLoadContext());
      transferable.addDataFlavor("text/unicode");
      transferable.setTransferData("text/unicode", str, str.data.length * 2);
      Services.clipboard.setData(
        transferable,
        null,
        Ci.nsIClipboard.kGlobalClipboard
      );
      if (AppConstants.platform == "android") {
        // Present a snackbar notification.
        var { Snackbars } = ChromeUtils.import(
          "resource://gre/modules/Snackbars.jsm"
        );
        let rawDataCopiedString = await document.l10n.formatValue(
          "raw-data-copied"
        );
        Snackbars.show(rawDataCopiedString, Snackbars.LENGTH_SHORT);
      }
    });
  } catch (err) {
    if (button) {
      button.disabled = false;
    }
    throw err;
  }
}

function getLoadContext() {
  return window.docShell.QueryInterface(Ci.nsILoadContext);
}

async function copyContentsToClipboard() {
  // Get the HTML and text representations for the important part of the page.
  let contentsDiv = $("contents").cloneNode(true);
  // Remove the items we don't want to copy from the clone:
  contentsDiv.querySelectorAll(".no-copy, [hidden]").forEach(n => n.remove());
  let dataHtml = contentsDiv.innerHTML;
  let dataText = createTextForElement(contentsDiv);

  // We can't use plain strings, we have to use nsSupportsString.
  let supportsStringClass = Cc["@mozilla.org/supports-string;1"];
  let ssHtml = supportsStringClass.createInstance(Ci.nsISupportsString);
  let ssText = supportsStringClass.createInstance(Ci.nsISupportsString);

  let transferable = Cc["@mozilla.org/widget/transferable;1"].createInstance(
    Ci.nsITransferable
  );
  transferable.init(getLoadContext());

  // Add the HTML flavor.
  transferable.addDataFlavor("text/html");
  ssHtml.data = dataHtml;
  transferable.setTransferData("text/html", ssHtml, dataHtml.length * 2);

  // Add the plain text flavor.
  transferable.addDataFlavor("text/unicode");
  ssText.data = dataText;
  transferable.setTransferData("text/unicode", ssText, dataText.length * 2);

  // Store the data into the clipboard.
  Services.clipboard.setData(
    transferable,
    null,
    Services.clipboard.kGlobalClipboard
  );

  if (AppConstants.platform == "android") {
    // Present a snackbar notification.
    var { Snackbars } = ChromeUtils.import(
      "resource://gre/modules/Snackbars.jsm"
    );
    let textCopiedString = await document.l10n.formatValue("text-copied");
    Snackbars.show(textCopiedString, Snackbars.LENGTH_SHORT);
  }
}

// Return the plain text representation of an element.  Do a little bit
// of pretty-printing to make it human-readable.
function createTextForElement(elem) {
  let serializer = new Serializer();
  let text = serializer.serialize(elem);

  // Actual CR/LF pairs are needed for some Windows text editors.
  if (AppConstants.platform == "win") {
    text = text.replace(/\n/g, "\r\n");
  }

  return text;
}

function Serializer() {}

Serializer.prototype = {
  serialize(rootElem) {
    this._lines = [];
    this._startNewLine();
    this._serializeElement(rootElem);
    this._startNewLine();
    return this._lines.join("\n").trim() + "\n";
  },

  // The current line is always the line that writing will start at next.  When
  // an element is serialized, the current line is updated to be the line at
  // which the next element should be written.
  get _currentLine() {
    return this._lines.length ? this._lines[this._lines.length - 1] : null;
  },

  set _currentLine(val) {
    return (this._lines[this._lines.length - 1] = val);
  },

  _serializeElement(elem) {
    // table
    if (elem.localName == "table") {
      this._serializeTable(elem);
      return;
    }

    // all other elements

    let hasText = false;
    for (let child of elem.childNodes) {
      if (child.nodeType == Node.TEXT_NODE) {
        let text = this._nodeText(child);
        this._appendText(text);
        hasText = hasText || !!text.trim();
      } else if (child.nodeType == Node.ELEMENT_NODE) {
        this._serializeElement(child);
      }
    }

    // For headings, draw a "line" underneath them so they stand out.
    let isHeader = /^h[0-9]+$/.test(elem.localName);
    if (isHeader) {
      let headerText = (this._currentLine || "").trim();
      if (headerText) {
        this._startNewLine();
        this._appendText("-".repeat(headerText.length));
      }
    }

    // Add a blank line underneath elements but only if they contain text.
    if (hasText && (isHeader || "p" == elem.localName)) {
      this._startNewLine();
      this._startNewLine();
    }
  },

  _startNewLine(lines) {
    let currLine = this._currentLine;
    if (currLine) {
      // The current line is not empty.  Trim it.
      this._currentLine = currLine.trim();
      if (!this._currentLine) {
        // The current line became empty.  Discard it.
        this._lines.pop();
      }
    }
    this._lines.push("");
  },

  _appendText(text, lines) {
    this._currentLine += text;
  },

  _isHiddenSubHeading(th) {
    return th.parentNode.parentNode.style.display == "none";
  },

  _serializeTable(table) {
    // Collect the table's column headings if in fact there are any.  First
    // check thead.  If there's no thead, check the first tr.
    let colHeadings = {};
    let tableHeadingElem = table.querySelector("thead");
    if (!tableHeadingElem) {
      tableHeadingElem = table.querySelector("tr");
    }
    if (tableHeadingElem) {
      let tableHeadingCols = tableHeadingElem.querySelectorAll("th,td");
      // If there's a contiguous run of th's in the children starting from the
      // rightmost child, then consider them to be column headings.
      for (let i = tableHeadingCols.length - 1; i >= 0; i--) {
        let col = tableHeadingCols[i];
        if (col.localName != "th" || col.classList.contains("title-column")) {
          break;
        }
        colHeadings[i] = this._nodeText(col).trim();
      }
    }
    let hasColHeadings = Object.keys(colHeadings).length > 0;
    if (!hasColHeadings) {
      tableHeadingElem = null;
    }

    let trs = table.querySelectorAll("table > tr, tbody > tr");
    let startRow =
      tableHeadingElem && tableHeadingElem.localName == "tr" ? 1 : 0;

    if (startRow >= trs.length) {
      // The table's empty.
      return;
    }

    if (hasColHeadings) {
      // Use column headings.  Print each tr as a multi-line chunk like:
      //   Heading 1: Column 1 value
      //   Heading 2: Column 2 value
      for (let i = startRow; i < trs.length; i++) {
        let children = trs[i].querySelectorAll("td");
        for (let j = 0; j < children.length; j++) {
          let text = "";
          if (colHeadings[j]) {
            text += colHeadings[j] + ": ";
          }
          text += this._nodeText(children[j]).trim();
          this._appendText(text);
          this._startNewLine();
        }
        this._startNewLine();
      }
      return;
    }

    // Don't use column headings.  Assume the table has only two columns and
    // print each tr in a single line like:
    //   Column 1 value: Column 2 value
    for (let i = startRow; i < trs.length; i++) {
      let children = trs[i].querySelectorAll("th,td");
      let rowHeading = this._nodeText(children[0]).trim();
      if (children[0].classList.contains("title-column")) {
        if (!this._isHiddenSubHeading(children[0])) {
          this._appendText(rowHeading);
        }
      } else if (children.length == 1) {
        // This is a single-cell row.
        this._appendText(rowHeading);
      } else {
        let childTables = trs[i].querySelectorAll("table");
        if (childTables.length) {
          // If we have child tables, don't use nodeText - its trs are already
          // queued up from querySelectorAll earlier.
          this._appendText(rowHeading + ": ");
        } else {
          this._appendText(
            rowHeading + ": " + this._nodeText(children[1]).trim()
          );
        }
      }
      this._startNewLine();
    }
    this._startNewLine();
  },

  _nodeText(node) {
    return node.textContent.replace(/\s+/g, " ");
  },
};

function openProfileDirectory() {
  // Get the profile directory.
  let currProfD = Services.dirsvc.get("ProfD", Ci.nsIFile);
  let profileDir = currProfD.path;

  // Show the profile directory.
  let nsLocalFile = Components.Constructor(
    "@mozilla.org/file/local;1",
    "nsIFile",
    "initWithPath"
  );
  new nsLocalFile(profileDir).reveal();
}

/**
 * Profile reset is only supported for the default profile if the appropriate migrator exists.
 */
function populateActionBox() {
  if (ResetProfile.resetSupported()) {
    $("reset-box").style.display = "block";
    $("action-box").style.display = "block";
  }
  if (!Services.appinfo.inSafeMode && AppConstants.platform !== "android") {
    $("safe-mode-box").style.display = "block";
    $("action-box").style.display = "block";

    if (Services.policies && !Services.policies.isAllowed("safeMode")) {
      $("restart-in-safe-mode-button").setAttribute("disabled", "true");
    }
  }
}

// Prompt user to restart the browser in safe mode
function safeModeRestart() {
  let cancelQuit = Cc["@mozilla.org/supports-PRBool;1"].createInstance(
    Ci.nsISupportsPRBool
  );
  Services.obs.notifyObservers(
    cancelQuit,
    "quit-application-requested",
    "restart"
  );

  if (!cancelQuit.data) {
    Services.startup.restartInSafeMode(Ci.nsIAppStartup.eAttemptQuit);
  }
}
/**
 * Set up event listeners for buttons.
 */
function setupEventListeners() {
  let button = $("reset-box-button");
  if (button) {
    button.addEventListener("click", function(event) {
      ResetProfile.openConfirmationDialog(window);
    });
  }
  button = $("restart-in-safe-mode-button");
  if (button) {
    button.addEventListener("click", function(event) {
      if (
        Services.obs
          .enumerateObservers("restart-in-safe-mode")
          .hasMoreElements()
      ) {
        Services.obs.notifyObservers(null, "restart-in-safe-mode");
      } else {
        safeModeRestart();
      }
    });
  }
  if (AppConstants.MOZ_UPDATER) {
    button = $("update-dir-button");
    if (button) {
      button.addEventListener("click", function(event) {
        // Get the update directory.
        let updateDir = Services.dirsvc.get("UpdRootD", Ci.nsIFile);
        if (!updateDir.exists()) {
          updateDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
        }
        let updateDirPath = updateDir.path;
        // Show the update directory.
        let nsLocalFile = Components.Constructor(
          "@mozilla.org/file/local;1",
          "nsIFile",
          "initWithPath"
        );
        new nsLocalFile(updateDirPath).reveal();
      });
    }
    button = $("show-update-history-button");
    if (button) {
      button.addEventListener("click", function(event) {
        let uri = "chrome://mozapps/content/update/history.xul";
        let features =
          "chrome,centerscreen,resizable=no,titlebar,toolbar=no," +
          "dialog=yes,modal";
        Services.ww.openWindow(window, uri, "Update:History", features, null);
      });
    }
  }
  button = $("verify-place-integrity-button");
  if (button) {
    button.addEventListener("click", function(event) {
      PlacesDBUtils.checkAndFixDatabase().then(tasksStatusMap => {
        let logs = [];
        for (let [key, value] of tasksStatusMap) {
          logs.push(`> Task: ${key}`);
          let prefix = value.succeeded ? "+ " : "- ";
          logs = logs.concat(value.logs.map(m => `${prefix}${m}`));
        }
        $("verify-place-result").style.display = "block";
        $("verify-place-result").classList.remove("no-copy");
        $("verify-place-result").textContent = logs.join("\n");
      });
    });
  }

  $("copy-raw-data-to-clipboard").addEventListener("click", function(event) {
    copyRawDataToClipboard(this);
  });
  $("copy-to-clipboard").addEventListener("click", function(event) {
    copyContentsToClipboard();
  });
  $("profile-dir-button").addEventListener("click", function(event) {
    openProfileDirectory();
  });
}
