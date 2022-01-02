/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-*/
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Time in ms before we start changing the sort order again after receiving a
// mousemove event.
const TIME_BEFORE_SORTING_AGAIN = 5000;

// How long we should wait between samples.
const MINIMUM_INTERVAL_BETWEEN_SAMPLES_MS = 1000;

// How often we should update
const UPDATE_INTERVAL_MS = 2000;

const NS_PER_US = 1000;
const NS_PER_MS = 1000 * 1000;
const NS_PER_S = 1000 * 1000 * 1000;
const NS_PER_MIN = NS_PER_S * 60;
const NS_PER_HOUR = NS_PER_MIN * 60;
const NS_PER_DAY = NS_PER_HOUR * 24;

const ONE_GIGA = 1024 * 1024 * 1024;
const ONE_MEGA = 1024 * 1024;
const ONE_KILO = 1024;

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  ContextualIdentityService:
    "resource://gre/modules/ContextualIdentityService.jsm",
});

XPCOMUtils.defineLazyGetter(this, "ProfilerPopupBackground", function() {
  return ChromeUtils.import(
    "resource://devtools/client/performance-new/popup/background.jsm.js"
  );
});

const { WebExtensionPolicy } = Cu.getGlobalForObject(Services);

const SHOW_THREADS = Services.prefs.getBoolPref(
  "toolkit.aboutProcesses.showThreads"
);
const SHOW_ALL_SUBFRAMES = Services.prefs.getBoolPref(
  "toolkit.aboutProcesses.showAllSubframes"
);
const SHOW_PROFILER_ICONS = Services.prefs.getBoolPref(
  "toolkit.aboutProcesses.showProfilerIcons"
);
const PROFILE_DURATION = Math.max(
  1,
  Services.prefs.getIntPref("toolkit.aboutProcesses.profileDuration")
);

/**
 * For the time being, Fluent doesn't support duration or memory formats, so we need
 * to fetch units from Fluent. To avoid re-fetching at each update, we prefetch these
 * units during initialization, asynchronously, and keep them.
 *
 * @type {
 *   duration: { ns: String, us: String, ms: String, s: String, m: String, h: String, d: String },
 *   memory: { B: String, KB: String, MB: String, GB: String, TB: String, PB: String, EB: String }
 * }.
 */
let gLocalizedUnits;

let tabFinder = {
  update() {
    this._map = new Map();
    for (let win of Services.wm.getEnumerator("navigator:browser")) {
      let tabbrowser = win.gBrowser;
      for (let browser of tabbrowser.browsers) {
        let id = browser.outerWindowID; // May be `null` if the browser isn't loaded yet
        if (id != null) {
          this._map.set(id, browser);
        }
      }
      if (tabbrowser.preloadedBrowser) {
        let browser = tabbrowser.preloadedBrowser;
        if (browser.outerWindowID) {
          this._map.set(browser.outerWindowID, browser);
        }
      }
    }
  },

  /**
   * Find the <xul:tab> for a window id.
   *
   * This is useful e.g. for reloading or closing tabs.
   *
   * @return null If the xul:tab could not be found, e.g. if the
   * windowId is that of a chrome window.
   * @return {{tabbrowser: <xul:tabbrowser>, tab: <xul.tab>}} The
   * tabbrowser and tab if the latter could be found.
   */
  get(id) {
    let browser = this._map.get(id);
    if (!browser) {
      return null;
    }
    let tabbrowser = browser.getTabBrowser();
    if (!tabbrowser) {
      return {
        tabbrowser: null,
        tab: {
          getAttribute() {
            return "";
          },
          linkedBrowser: browser,
        },
      };
    }
    return { tabbrowser, tab: tabbrowser.getTabForBrowser(browser) };
  },
};

/**
 * Utilities for dealing with state
 */
var State = {
  // Store the previous and current samples so they can be compared.
  _previous: null,
  _latest: null,

  async _promiseSnapshot() {
    let date = Cu.now();
    let main = await ChromeUtils.requestProcInfo();
    main.date = date;

    let processes = new Map();
    processes.set(main.pid, main);
    for (let child of main.children) {
      child.date = date;
      processes.set(child.pid, child);
    }

    return { processes, date };
  },

  /**
   * Update the internal state.
   *
   * @return {Promise}
   */
  async update(force = false) {
    if (
      force ||
      !this._latest ||
      Cu.now() - this._latest.date > MINIMUM_INTERVAL_BETWEEN_SAMPLES_MS
    ) {
      // Replacing this._previous before we are done awaiting
      // this._promiseSnapshot can cause this._previous and this._latest to be
      // equal for a short amount of time, which can cause test failures when
      // a forced update of the display is triggered in the meantime.
      let newSnapshot = await this._promiseSnapshot();
      this._previous = this._latest;
      this._latest = newSnapshot;
    }
  },

  _getThreadDelta(cur, prev, deltaT) {
    let result = {
      tid: cur.tid,
      name: cur.name || `(${cur.tid})`,
      // Total amount of CPU used, in ns.
      totalCpu: cur.cpuTime,
      slopeCpu: null,
      active: null,
    };
    if (!deltaT) {
      return result;
    }
    result.slopeCpu = (result.totalCpu - (prev ? prev.cpuTime : 0)) / deltaT;
    result.active =
      !!result.slopeCpu || cur.cpuCycleCount > (prev ? prev.cpuCycleCount : 0);
    return result;
  },

  _getDOMWindows(process) {
    if (!process.windows) {
      return [];
    }
    if (!process.type == "extensions") {
      return [];
    }
    let windows = process.windows.map(win => {
      let tab = tabFinder.get(win.outerWindowId);
      let addon =
        process.type == "extension"
          ? WebExtensionPolicy.getByURI(win.documentURI)
          : null;
      let displayRank;
      if (tab) {
        displayRank = 1;
      } else if (win.isProcessRoot) {
        displayRank = 2;
      } else if (win.documentTitle) {
        displayRank = 3;
      } else {
        displayRank = 4;
      }
      return {
        outerWindowId: win.outerWindowId,
        documentURI: win.documentURI,
        documentTitle: win.documentTitle,
        isProcessRoot: win.isProcessRoot,
        isInProcess: win.isInProcess,
        tab,
        addon,
        // The number of instances we have collapsed.
        count: 1,
        // A rank used to quickly sort windows.
        displayRank,
      };
    });

    // We keep all tabs and addons but we collapse subframes that have the same host.

    // A map from host -> subframe.
    let collapsible = new Map();
    let result = [];
    for (let win of windows) {
      if (win.tab || win.addon) {
        result.push(win);
        continue;
      }
      let prev = collapsible.get(win.documentURI.prePath);
      if (prev) {
        prev.count += 1;
      } else {
        collapsible.set(win.documentURI.prePath, win);
        result.push(win);
      }
    }
    return result;
  },

  /**
   * Compute the delta between two process snapshots.
   *
   * @param {ProcessSnapshot} cur
   * @param {ProcessSnapshot?} prev
   */
  _getProcessDelta(cur, prev) {
    let windows = this._getDOMWindows(cur);
    let result = {
      pid: cur.pid,
      childID: cur.childID,
      totalRamSize: cur.memory,
      deltaRamSize: null,
      totalCpu: cur.cpuTime,
      slopeCpu: null,
      active: null,
      type: cur.type,
      origin: cur.origin || "",
      threads: null,
      displayRank: Control._getDisplayGroupRank(cur, windows),
      windows,
      // If this process has an unambiguous title, store it here.
      title: null,
    };
    // Attempt to determine a title for this process.
    let titles = [
      ...new Set(
        result.windows
          .filter(win => win.documentTitle)
          .map(win => win.documentTitle)
      ),
    ];
    if (titles.length == 1) {
      result.title = titles[0];
    }
    if (!prev) {
      if (SHOW_THREADS) {
        result.threads = cur.threads.map(data => this._getThreadDelta(data));
      }
      return result;
    }
    if (prev.pid != cur.pid) {
      throw new Error("Assertion failed: A process cannot change pid.");
    }
    let deltaT = (cur.date - prev.date) * NS_PER_MS;
    let threads = null;
    if (SHOW_THREADS) {
      let prevThreads = new Map();
      for (let thread of prev.threads) {
        prevThreads.set(thread.tid, thread);
      }
      threads = cur.threads.map(curThread =>
        this._getThreadDelta(curThread, prevThreads.get(curThread.tid), deltaT)
      );
    }
    result.deltaRamSize = cur.memory - prev.memory;
    result.slopeCpu = (cur.cpuTime - prev.cpuTime) / deltaT;
    result.active = !!result.slopeCpu || cur.cpuCycleCount > prev.cpuCycleCount;
    result.threads = threads;
    return result;
  },

  getCounters() {
    tabFinder.update();

    let counters = [];

    for (let cur of this._latest.processes.values()) {
      let prev = this._previous?.processes.get(cur.pid);
      counters.push(this._getProcessDelta(cur, prev));
    }

    return counters;
  },
};

var View = {
  // Processes, tabs and subframes that we killed during the previous iteration.
  // Array<{pid:Number} | {windowId:Number}>
  _killedRecently: [],
  commit() {
    this._killedRecently.length = 0;
    let tbody = document.getElementById("process-tbody");

    let insertPoint = tbody.firstChild;
    let nextRow;
    while ((nextRow = this._orderedRows.shift())) {
      if (insertPoint && insertPoint === nextRow) {
        insertPoint = insertPoint.nextSibling;
      } else {
        tbody.insertBefore(nextRow, insertPoint);
      }
    }

    if (insertPoint) {
      while ((nextRow = insertPoint.nextSibling)) {
        this._removeRow(nextRow);
      }
      this._removeRow(insertPoint);
    }
  },
  // If we are not going to display the updated list of rows, drop references
  // to rows that haven't been inserted in the DOM tree.
  discardUpdate() {
    for (let row of this._orderedRows) {
      if (!row.parentNode) {
        this._rowsById.delete(row.rowId);
      }
    }
    this._orderedRows = [];
  },
  insertAfterRow(row) {
    let tbody = row.parentNode;
    let nextRow;
    while ((nextRow = this._orderedRows.pop())) {
      tbody.insertBefore(nextRow, row.nextSibling);
    }
  },

  _rowsById: new Map(),
  _removeRow(row) {
    this._rowsById.delete(row.rowId);

    row.remove();
  },
  _getOrCreateRow(rowId, cellCount) {
    let row = this._rowsById.get(rowId);
    if (!row) {
      row = document.createElement("tr");
      while (cellCount--) {
        row.appendChild(document.createElement("td"));
      }
      row.rowId = rowId;
      this._rowsById.set(rowId, row);
    }
    this._orderedRows.push(row);
    return row;
  },

  displayCpu(data, cpuCell) {
    if (data.slopeCpu == null) {
      this._fillCell(cpuCell, {
        fluentName: "about-processes-cpu-user-and-kernel-not-ready",
        classes: ["cpu"],
      });
    } else {
      let { duration, unit } = this._getDuration(data.totalCpu);
      if (data.totalCpu == 0 && AppConstants.platform == "win") {
        // The minimum non zero CPU time we can get on Windows is 16ms
        // so avoid displaying '0ns'.
        unit = "ms";
      }
      let localizedUnit = gLocalizedUnits.duration[unit];
      if (data.slopeCpu == 0) {
        let fluentName = data.active
          ? "about-processes-cpu-almost-idle"
          : "about-processes-cpu-fully-idle";
        this._fillCell(cpuCell, {
          fluentName,
          fluentArgs: {
            total: duration,
            unit: localizedUnit,
          },
          classes: ["cpu"],
        });
      } else {
        this._fillCell(cpuCell, {
          fluentName: "about-processes-cpu",
          fluentArgs: {
            percent: data.slopeCpu,
            total: duration,
            unit: localizedUnit,
          },
          classes: ["cpu"],
        });
      }
    }
  },

  /**
   * Display a row showing a single process (without its threads).
   *
   * @param {ProcessDelta} data The data to display.
   * @return {DOMElement} The row displaying the process.
   */
  displayProcessRow(data) {
    const cellCount = 4;
    let rowId = "p:" + data.pid;
    let row = this._getOrCreateRow(rowId, cellCount);
    row.process = data;
    {
      let classNames = "process";
      if (data.isHung) {
        classNames += " hung";
      }
      row.className = classNames;
    }

    // Column: Name
    let nameCell = row.firstChild;
    {
      let classNames = [];
      let fluentName;
      let fluentArgs = {
        pid: "" + data.pid, // Make sure that this number is not localized
      };
      switch (data.type) {
        case "web":
          fluentName = "about-processes-web-process";
          break;
        case "webIsolated":
          fluentName = "about-processes-web-isolated-process";
          fluentArgs.origin = data.origin;
          break;
        case "webServiceWorker":
          fluentName = "about-processes-web-serviceworker";
          fluentArgs.origin = data.origin;
          break;
        case "webLargeAllocation":
          fluentName = "about-processes-web-large-allocation-process";
          fluentArgs.origin = data.origin;
          break;
        case "file":
          fluentName = "about-processes-file-process";
          break;
        case "extension":
          fluentName = "about-processes-extension-process";
          classNames = ["extensions"];
          break;
        case "privilegedabout":
          fluentName = "about-processes-privilegedabout-process";
          break;
        case "privilegedmozilla":
          fluentName = "about-processes-privilegedmozilla-process";
          break;
        case "withCoopCoep":
          fluentName = "about-processes-with-coop-coep-process";
          fluentArgs.origin = data.origin;
          break;
        case "browser":
          fluentName = "about-processes-browser-process";
          break;
        case "plugin":
          fluentName = "about-processes-plugin-process";
          break;
        case "gmpPlugin":
          fluentName = "about-processes-gmp-plugin-process";
          break;
        case "gpu":
          fluentName = "about-processes-gpu-process";
          break;
        case "vr":
          fluentName = "about-processes-vr-process";
          break;
        case "rdd":
          fluentName = "about-processes-rdd-process";
          break;
        case "socket":
          fluentName = "about-processes-socket-process";
          break;
        case "remoteSandboxBroker":
          fluentName = "about-processes-remote-sandbox-broker-process";
          break;
        case "forkServer":
          fluentName = "about-processes-fork-server-process";
          break;
        case "preallocated":
          fluentName = "about-processes-preallocated-process";
          break;
        // The following are probably not going to show up for users
        // but let's handle the case anyway to avoid heisenoranges
        // during tests in case of a leftover process from a previous
        // test.
        default:
          fluentName = "about-processes-unknown-process";
          fluentArgs.type = data.type;
          break;
      }

      // Show container names instead of raw origin attribute suffixes.
      if (fluentArgs.origin?.includes("^")) {
        let origin = fluentArgs.origin;
        let privateBrowsingId, userContextId;
        try {
          ({
            privateBrowsingId,
            userContextId,
          } = ChromeUtils.createOriginAttributesFromOrigin(origin));
          fluentArgs.origin = origin.slice(0, origin.indexOf("^"));
        } catch (e) {
          // createOriginAttributesFromOrigin can throw NS_ERROR_FAILURE for incorrect origin strings.
        }
        if (userContextId) {
          let identityLabel = ContextualIdentityService.getUserContextLabel(
            userContextId
          );
          if (identityLabel) {
            fluentArgs.origin += ` — ${identityLabel}`;
          }
        }
        if (privateBrowsingId) {
          fluentName += "-private";
        }
      }

      let processNameElement = nameCell;
      if (SHOW_PROFILER_ICONS) {
        if (!nameCell.firstChild) {
          processNameElement = document.createElement("span");
          nameCell.appendChild(processNameElement);

          let profilerIcon = document.createElement("span");
          profilerIcon.className = "profiler-icon";
          document.l10n.setAttributes(
            profilerIcon,
            "about-processes-profile-process",
            { duration: PROFILE_DURATION }
          );
          nameCell.appendChild(profilerIcon);
        } else {
          processNameElement = nameCell.firstChild;
        }
      }
      document.l10n.setAttributes(processNameElement, fluentName, fluentArgs);
      nameCell.className = ["type", "favicon", ...classNames].join(" ");

      let image;
      switch (data.type) {
        case "browser":
        case "privilegedabout":
          image = "chrome://branding/content/icon32.png";
          break;
        case "extension":
          image = "chrome://mozapps/skin/extensions/extension.svg";
          break;
        default:
          // If all favicons match, pick the shared favicon.
          // Otherwise, pick a default icon.
          // If some tabs have no favicon, we ignore them.
          for (let win of data.windows || []) {
            if (!win.tab) {
              continue;
            }
            let favicon = win.tab.tab.getAttribute("image");
            if (!favicon) {
              // No favicon here, let's ignore the tab.
            } else if (!image) {
              // Let's pick a first favicon.
              // We'll remove it later if we find conflicting favicons.
              image = favicon;
            } else if (image == favicon) {
              // So far, no conflict, keep the favicon.
            } else {
              // Conflicting favicons, fallback to default.
              image = null;
              break;
            }
          }
          if (!image) {
            image = "chrome://global/skin/icons/link.svg";
          }
      }
      nameCell.style.backgroundImage = `url('${image}')`;
    }

    // Column: Memory
    let memoryCell = nameCell.nextSibling;
    {
      let formattedTotal = this._formatMemory(data.totalRamSize);
      if (data.deltaRamSize) {
        let formattedDelta = this._formatMemory(data.deltaRamSize);
        this._fillCell(memoryCell, {
          fluentName: "about-processes-total-memory-size-changed",
          fluentArgs: {
            total: formattedTotal.amount,
            totalUnit: gLocalizedUnits.memory[formattedTotal.unit],
            delta: Math.abs(formattedDelta.amount),
            deltaUnit: gLocalizedUnits.memory[formattedDelta.unit],
            deltaSign: data.deltaRamSize > 0 ? "+" : "-",
          },
          classes: ["memory"],
        });
      } else {
        this._fillCell(memoryCell, {
          fluentName: "about-processes-total-memory-size-no-change",
          fluentArgs: {
            total: formattedTotal.amount,
            totalUnit: gLocalizedUnits.memory[formattedTotal.unit],
          },
          classes: ["memory"],
        });
      }
    }

    // Column: CPU
    let cpuCell = memoryCell.nextSibling;
    this.displayCpu(data, cpuCell);

    // Column: Kill button – but not for all processes.
    let killButton = cpuCell.nextSibling;
    killButton.className = "action-icon";

    if (data.type.startsWith("web")) {
      // This type of process can be killed.
      if (this._killedRecently.some(kill => kill.pid && kill.pid == data.pid)) {
        // We're racing between the "kill" action and the visual refresh.
        // In a few cases, we could end up with the visual refresh showing
        // a process as un-killed while we actually just killed it.
        //
        // We still want to display the process in case something actually
        // went bad and the user needs the information to realize this.
        // But we also want to make it visible that the process is being
        // killed.
        row.classList.add("killed");
      } else {
        // Otherwise, let's display the kill button.
        killButton.classList.add("close-icon");
        document.l10n.setAttributes(
          killButton,
          "about-processes-shutdown-process"
        );
      }
    }

    return row;
  },

  /**
   * Display a thread summary row with the thread count and a twisty to
   * open/close the list.
   *
   * @param {ProcessDelta} data The data to display.
   * @return {boolean} Whether the full thread list should be displayed.
   */
  displayThreadSummaryRow(data) {
    const cellCount = 2;
    let rowId = "ts:" + data.pid;
    let row = this._getOrCreateRow(rowId, cellCount);
    row.process = data;
    row.className = "thread-summary";
    let isOpen = false;

    // Column: Name
    let nameCell = row.firstChild;
    let threads = data.threads;
    let activeThreads = new Map();
    let activeThreadCount = 0;
    for (let t of data.threads) {
      if (!t.active) {
        continue;
      }
      ++activeThreadCount;
      let name = t.name.replace(/ ?#[0-9]+$/, "");
      if (!activeThreads.has(name)) {
        activeThreads.set(name, { name, slopeCpu: t.slopeCpu, count: 1 });
      } else {
        let thread = activeThreads.get(name);
        thread.count++;
        thread.slopeCpu += t.slopeCpu;
      }
    }
    let fluentName, fluentArgs;
    if (activeThreadCount) {
      let percentFormatter = new Intl.NumberFormat(undefined, {
        style: "percent",
        minimumSignificantDigits: 1,
      });

      let threadList = Array.from(activeThreads.values());
      threadList.sort((t1, t2) => t2.slopeCpu - t1.slopeCpu);

      fluentName = "about-processes-active-threads";
      fluentArgs = {
        number: threads.length,
        active: activeThreadCount,
        list: new Intl.ListFormat(undefined, { style: "narrow" }).format(
          threadList.map(t => {
            let name = t.count > 1 ? `${t.count} × ${t.name}` : t.name;
            let percent = Math.round(t.slopeCpu * 1000) / 1000;
            if (percent) {
              return `${name} ${percentFormatter.format(percent)}`;
            }
            return name;
          })
        ),
      };
    } else {
      fluentName = "about-processes-inactive-threads";
      fluentArgs = {
        number: threads.length,
      };
    }

    let span;
    if (!nameCell.firstChild) {
      nameCell.className = "name indent";
      // Create the nodes
      let img = document.createElement("span");
      img.className = "twisty";
      nameCell.appendChild(img);

      span = document.createElement("span");
      nameCell.appendChild(span);
    } else {
      // The only thing that can change is the thread count.
      let img = nameCell.firstChild;
      isOpen = img.classList.contains("open");
      span = img.nextSibling;
    }
    document.l10n.setAttributes(span, fluentName, fluentArgs);

    // Column: action
    let actionCell = nameCell.nextSibling;
    actionCell.className = "action-icon";

    return isOpen;
  },

  displayDOMWindowRow(data, parent) {
    const cellCount = 2;
    let rowId = "w:" + data.outerWindowId;
    let row = this._getOrCreateRow(rowId, cellCount);
    row.win = data;
    row.className = "window";

    // Column: name
    let nameCell = row.firstChild;
    let tab = tabFinder.get(data.outerWindowId);
    let fluentName;
    let fluentArgs = {};
    let className;
    if (tab && tab.tabbrowser) {
      fluentName = "about-processes-tab-name";
      fluentArgs.name = tab.tab.label;
      className = "tab";
    } else if (tab) {
      fluentName = "about-processes-preloaded-tab";
      className = "preloaded-tab";
    } else if (data.count == 1) {
      fluentName = "about-processes-frame-name-one";
      fluentArgs.url = data.documentURI.spec;
      className = "frame-one";
    } else {
      fluentName = "about-processes-frame-name-many";
      fluentArgs.number = data.count;
      fluentArgs.shortUrl =
        data.documentURI.scheme == "about"
          ? data.documentURI.spec
          : data.documentURI.prePath;
      className = "frame-many";
    }
    this._fillCell(nameCell, {
      fluentName,
      fluentArgs,
      classes: ["name", "indent", "favicon", className],
    });
    let image = tab?.tab.getAttribute("image");
    if (image) {
      nameCell.style.backgroundImage = `url('${image}')`;
    }

    // Column: action
    let killButton = nameCell.nextSibling;
    killButton.className = "action-icon";

    if (data.tab && data.tab.tabbrowser) {
      // A tab. We want to be able to close it.
      if (
        this._killedRecently.some(
          kill => kill.windowId && kill.windowId == data.outerWindowId
        )
      ) {
        // We're racing between the "kill" action and the visual refresh.
        // In a few cases, we could end up with the visual refresh showing
        // a window as un-killed while we actually just killed it.
        //
        // We still want to display the window in case something actually
        // went bad and the user needs the information to realize this.
        // But we also want to make it visible that the window is being
        // killed.
        row.classList.add("killed");
      } else {
        // Otherwise, let's display the kill button.
        killButton.classList.add("close-icon");
        document.l10n.setAttributes(killButton, "about-processes-shutdown-tab");
      }
    }
  },

  /**
   * Display a row showing a single thread.
   *
   * @param {ThreadDelta} data The data to display.
   */
  displayThreadRow(data) {
    const cellCount = 3;
    let rowId = "t:" + data.tid;
    let row = this._getOrCreateRow(rowId, cellCount);
    row.thread = data;
    row.className = "thread";

    // Column: name
    let nameCell = row.firstChild;
    this._fillCell(nameCell, {
      fluentName: "about-processes-thread-name-and-id",
      fluentArgs: {
        name: data.name,
        tid: "" + data.tid /* Make sure that this number is not localized */,
      },
      classes: ["name", "double_indent"],
    });

    // Column: CPU
    this.displayCpu(data, nameCell.nextSibling);

    // Third column (Buttons) is empty, nothing to do.
  },

  _orderedRows: [],
  _fillCell(elt, { classes, fluentName, fluentArgs }) {
    document.l10n.setAttributes(elt, fluentName, fluentArgs);
    elt.className = classes.join(" ");
  },

  _getDuration(rawDurationNS) {
    if (rawDurationNS <= NS_PER_US) {
      return { duration: rawDurationNS, unit: "ns" };
    }
    if (rawDurationNS <= NS_PER_MS) {
      return { duration: rawDurationNS / NS_PER_US, unit: "us" };
    }
    if (rawDurationNS <= NS_PER_S) {
      return { duration: rawDurationNS / NS_PER_MS, unit: "ms" };
    }
    if (rawDurationNS <= NS_PER_MIN) {
      return { duration: rawDurationNS / NS_PER_S, unit: "s" };
    }
    if (rawDurationNS <= NS_PER_HOUR) {
      return { duration: rawDurationNS / NS_PER_MIN, unit: "m" };
    }
    if (rawDurationNS <= NS_PER_DAY) {
      return { duration: rawDurationNS / NS_PER_HOUR, unit: "h" };
    }
    return { duration: rawDurationNS / NS_PER_DAY, unit: "d" };
  },

  /**
   * Format a value representing an amount of memory.
   *
   * As a special case, we also handle `null`, which represents the case in which we do
   * not have sufficient information to compute an amount of memory.
   *
   * @param {Number?} value The value to format. Must be either `null` or a non-negative number.
   * @return { {unit: "GB" | "MB" | "KB" | B" | "?"}, amount: Number } The formated amount and its
   *  unit, which may be used for e.g. additional CSS formating.
   */
  _formatMemory(value) {
    if (value == null) {
      return { unit: "?", amount: 0 };
    }
    if (typeof value != "number") {
      throw new Error(`Invalid memory value ${value}`);
    }
    let abs = Math.abs(value);
    if (abs >= ONE_GIGA) {
      return {
        unit: "GB",
        amount: value / ONE_GIGA,
      };
    }
    if (abs >= ONE_MEGA) {
      return {
        unit: "MB",
        amount: value / ONE_MEGA,
      };
    }
    if (abs >= ONE_KILO) {
      return {
        unit: "KB",
        amount: value / ONE_KILO,
      };
    }
    return {
      unit: "B",
      amount: value,
    };
  },
};

var Control = {
  // The set of all processes reported as "hung" by the process hang monitor.
  //
  // type: Set<ChildID>
  _hungItems: new Set(),
  _sortColumn: null,
  _sortAscendent: true,
  _removeSubtree(row) {
    let sibling = row.nextSibling;
    while (sibling && !sibling.classList.contains("process")) {
      let next = sibling.nextSibling;
      if (sibling.classList.contains("thread")) {
        View._removeRow(sibling);
      }
      sibling = next;
    }
  },
  init() {
    this._initHangReports();

    // Start prefetching units.
    this._promisePrefetchedUnits = (async function() {
      let [
        ns,
        us,
        ms,
        s,
        m,
        h,
        d,
        B,
        KB,
        MB,
        GB,
        TB,
        PB,
        EB,
      ] = await document.l10n.formatValues([
        { id: "duration-unit-ns" },
        { id: "duration-unit-us" },
        { id: "duration-unit-ms" },
        { id: "duration-unit-s" },
        { id: "duration-unit-m" },
        { id: "duration-unit-h" },
        { id: "duration-unit-d" },
        { id: "memory-unit-B" },
        { id: "memory-unit-KB" },
        { id: "memory-unit-MB" },
        { id: "memory-unit-GB" },
        { id: "memory-unit-TB" },
        { id: "memory-unit-PB" },
        { id: "memory-unit-EB" },
      ]);
      return {
        duration: { ns, us, ms, s, m, h, d },
        memory: { B, KB, MB, GB, TB, PB, EB },
      };
    })();

    let tbody = document.getElementById("process-tbody");

    // Single click:
    // - show or hide the contents of a twisty;
    // - change selection.
    tbody.addEventListener("click", event => {
      this._updateLastMouseEvent();

      // Handle showing or hiding subitems of a row.
      let target = event.target;
      if (target.classList.contains("twisty")) {
        this._handleTwisty(target);
        return;
      }
      if (target.classList.contains("close-icon")) {
        this._handleKill(target);
        return;
      }

      if (target.classList.contains("profiler-icon")) {
        if (Services.profiler.IsActive()) {
          return;
        }
        Services.profiler.StartProfiler(
          10000000,
          1,
          ["default", "ipcmessages"],
          ["pid:" + target.parentNode.parentNode.process.pid]
        );
        target.classList.add("profiler-active");
        setTimeout(() => {
          ProfilerPopupBackground.captureProfile("aboutprofiling");
          target.classList.remove("profiler-active");
        }, PROFILE_DURATION * 1000);
        return;
      }

      // Handle selection changes
      let row = target.closest("tr");
      if (!row) {
        return;
      }
      if (this.selectedRow) {
        this.selectedRow.removeAttribute("selected");
        if (this.selectedRow.rowId == row.rowId) {
          // Clicking the same row again clears the selection.
          this.selectedRow = null;
          return;
        }
      }
      row.setAttribute("selected", "true");
      this.selectedRow = row;
    });

    // Double click:
    // - navigate to tab;
    // - navigate to about:addons.
    tbody.addEventListener("dblclick", event => {
      this._updateLastMouseEvent();
      event.stopPropagation();

      // Bubble up the doubleclick manually.
      for (
        let target = event.target;
        target && target.getAttribute("id") != "process-tbody";
        target = target.parentNode
      ) {
        if (target.classList.contains("tab")) {
          // We've clicked on a tab, navigate.
          let { tab, tabbrowser } = target.parentNode.win.tab;
          tabbrowser.selectedTab = tab;
          tabbrowser.ownerGlobal.focus();
          return;
        }
        if (target.classList.contains("extensions")) {
          // We've clicked on the extensions process, open or reuse window.
          let parentWin =
            window.docShell.browsingContext.embedderElement.ownerGlobal;
          parentWin.BrowserOpenAddonsMgr();
          return;
        }
        // Otherwise, proceed.
      }
    });

    tbody.addEventListener("mousemove", () => {
      this._updateLastMouseEvent();
    });

    // Visibility change:
    // - stop updating while the user isn't looking;
    // - resume updating when the user returns.
    window.addEventListener("visibilitychange", event => {
      if (!document.hidden) {
        this._updateDisplay(true);
      }
    });

    document
      .getElementById("process-thead")
      .addEventListener("click", async event => {
        if (!event.target.classList.contains("clickable")) {
          return;
        }

        if (this._sortColumn) {
          const td = document.getElementById(this._sortColumn);
          td.classList.remove("asc");
          td.classList.remove("desc");
        }

        const columnId = event.target.id;
        if (columnId == this._sortColumn) {
          // Reverse sorting order.
          this._sortAscendent = !this._sortAscendent;
        } else {
          this._sortColumn = columnId;
          this._sortAscendent = true;
        }

        if (this._sortAscendent) {
          event.target.classList.remove("desc");
          event.target.classList.add("asc");
        } else {
          event.target.classList.remove("asc");
          event.target.classList.add("desc");
        }

        await this._updateDisplay(true);
      });
  },
  _lastMouseEvent: 0,
  _updateLastMouseEvent() {
    this._lastMouseEvent = Date.now();
  },
  _initHangReports() {
    const PROCESS_HANG_REPORT_NOTIFICATION = "process-hang-report";

    // Receiving report of a hung child.
    // Let's store if for our next update.
    let hangReporter = report => {
      report.QueryInterface(Ci.nsIHangReport);
      this._hungItems.add(report.childID);
    };
    Services.obs.addObserver(hangReporter, PROCESS_HANG_REPORT_NOTIFICATION);

    // Don't forget to unregister the reporter.
    window.addEventListener(
      "unload",
      () => {
        Services.obs.removeObserver(
          hangReporter,
          PROCESS_HANG_REPORT_NOTIFICATION
        );
      },
      { once: true }
    );
  },
  async update(force = false) {
    await State.update(force);

    if (document.hidden) {
      return;
    }

    await this._updateDisplay(force);
  },

  // The force parameter can force a full update even when the mouse has been
  // moved recently.
  async _updateDisplay(force = false) {
    let counters = State.getCounters();
    if (this._promisePrefetchedUnits) {
      gLocalizedUnits = await this._promisePrefetchedUnits;
      this._promisePrefetchedUnits = null;
    }

    // We reset `_hungItems`, based on the assumption that the process hang
    // monitor will inform us again before the next update. Since the process hang monitor
    // pings its clients about once per second and we update about once per 2 seconds
    // (or more if the mouse moves), we should be ok.
    let hungItems = this._hungItems;
    this._hungItems = new Set();

    counters = this._sortProcesses(counters);
    let previousProcess = null;
    for (let process of counters) {
      this._sortDOMWindows(process.windows);

      process.isHung = process.childID && hungItems.has(process.childID);

      let processRow = View.displayProcessRow(process);

      if (process.type != "extension") {
        // We do not want to display extensions.
        for (let win of process.windows) {
          if (SHOW_ALL_SUBFRAMES || win.tab || win.isProcessRoot) {
            View.displayDOMWindowRow(win, process);
          }
        }
      }

      if (SHOW_THREADS) {
        if (View.displayThreadSummaryRow(process)) {
          this._showThreads(processRow);
        }
      }
      if (
        this._sortColumn == null &&
        previousProcess &&
        previousProcess.displayRank != process.displayRank
      ) {
        // Add a separation between successive categories of processes.
        processRow.classList.add("separate-from-previous-process-group");
      }
      previousProcess = process;
    }

    if (
      !force &&
      Date.now() - this._lastMouseEvent < TIME_BEFORE_SORTING_AGAIN
    ) {
      // If there has been a recent mouse event, we don't want to reorder,
      // add or remove rows so that the table content under the mouse pointer
      // doesn't change when the user might be about to click to close a tab
      // or kill a process.
      // We didn't return earlier because updating CPU and memory values is
      // still valuable.
      View.discardUpdate();
      return;
    }

    View.commit();

    // Reset the selectedRow field if that row is no longer in the DOM
    // to avoid keeping forever references to dead processes.
    if (this.selectedRow && !this.selectedRow.parentNode) {
      this.selectedRow = null;
    }

    // Used by tests to differentiate full updates from l10n updates.
    document.dispatchEvent(new CustomEvent("AboutProcessesUpdated"));
  },
  _compareCpu(a, b) {
    return (
      b.slopeCpu - a.slopeCpu || b.active - a.active || b.totalCpu - a.totalCpu
    );
  },
  _showThreads(row) {
    let process = row.process;
    this._sortThreads(process.threads);
    for (let thread of process.threads) {
      View.displayThreadRow(thread);
    }
  },
  _sortThreads(threads) {
    return threads.sort((a, b) => {
      let order;
      switch (this._sortColumn) {
        case "column-name":
          order = a.name.localeCompare(b.name) || a.tid - b.tid;
          break;
        case "column-cpu-total":
          order = this._compareCpu(a, b);
          break;
        case "column-memory-resident":
        case null:
          order = a.tid - b.tid;
          break;
        default:
          throw new Error("Unsupported order: " + this._sortColumn);
      }
      if (!this._sortAscendent) {
        order = -order;
      }
      return order;
    });
  },
  _sortProcesses(counters) {
    return counters.sort((a, b) => {
      let order;
      switch (this._sortColumn) {
        case "column-name":
          order =
            String(a.origin).localeCompare(b.origin) ||
            String(a.type).localeCompare(b.type) ||
            a.pid - b.pid;
          break;
        case "column-cpu-total":
          order = this._compareCpu(a, b);
          break;
        case "column-memory-resident":
          order = b.totalRamSize - a.totalRamSize;
          break;
        case null:
          // Default order: classify processes by group.
          order =
            a.displayRank - b.displayRank ||
            // Other processes are ordered by origin.
            String(a.origin).localeCompare(b.origin);
          break;
        default:
          throw new Error("Unsupported order: " + this._sortColumn);
      }
      if (!this._sortAscendent) {
        order = -order;
      }
      return order;
    });
  },
  _sortDOMWindows(windows) {
    return windows.sort((a, b) => {
      let order =
        a.displayRank - b.displayRank ||
        a.documentTitle.localeCompare(b.documentTitle) ||
        a.documentURI.spec.localeCompare(b.documentURI.spec);
      if (!this._sortAscendent) {
        order = -order;
      }
      return order;
    });
  },

  // Assign a display rank to a process.
  //
  // The `browser` process comes first (rank 0).
  // Then come web tabs (rank 1).
  // Then come web frames (rank 2).
  // Then come special processes (minus preallocated) (rank 3).
  // Then come preallocated processes (rank 4).
  _getDisplayGroupRank(data, windows) {
    const RANK_BROWSER = 0;
    const RANK_WEB_TABS = 1;
    const RANK_WEB_FRAMES = 2;
    const RANK_UTILITY = 3;
    const RANK_PREALLOCATED = 4;
    let type = data.type;
    switch (type) {
      // Browser comes first.
      case "browser":
        return RANK_BROWSER;
      // Web content comes next.
      case "webIsolated":
      case "webServiceWorker":
      case "webLargeAllocation":
      case "withCoopCoep": {
        if (windows.some(w => w.tab)) {
          return RANK_WEB_TABS;
        }
        return RANK_WEB_FRAMES;
      }
      // Preallocated processes come last.
      case "preallocated":
        return RANK_PREALLOCATED;
      // "web" is special, as it could be one of:
      // - web content currently loading/unloading/...
      // - a preallocated process.
      case "web":
        if (windows.some(w => w.tab)) {
          return RANK_WEB_TABS;
        }
        if (windows.length >= 1) {
          return RANK_WEB_FRAMES;
        }
        // For the time being, we do not display DOM workers
        // (and there's no API to get information on them).
        // Once the blockers for bug 1663737 have landed, we'll be able
        // to find out whether this process has DOM workers. If so, we'll
        // count this process as a content process.
        return RANK_PREALLOCATED;
      // Other special processes before preallocated.
      default:
        return RANK_UTILITY;
    }
  },

  // Open/close list of threads.
  _handleTwisty(target) {
    let row = target.parentNode.parentNode;
    if (target.classList.toggle("open")) {
      this._showThreads(row);
      View.insertAfterRow(row);
    } else {
      this._removeSubtree(row);
    }
  },

  // Kill process/close tab/close subframe
  _handleKill(target) {
    let row = target.parentNode;
    if (row.process) {
      // Kill process immediately.
      let pid = row.process.pid;

      // Make sure that the user can't click twice on the kill button.
      // Otherwise, chaos might ensue. Plus we risk crashing under Windows.
      View._killedRecently.push({ pid });

      // Discard tab contents and show that the process and all its contents are getting killed.
      row.classList.add("killing");
      for (
        let childRow = row.nextSibling;
        childRow && !childRow.classList.contains("process");
        childRow = childRow.nextSibling
      ) {
        childRow.classList.add("killing");
        let win = childRow.win;
        if (win) {
          View._killedRecently.push({ pid: win.outerWindowId });
          if (win.tab && win.tab.tabbrowser) {
            win.tab.tabbrowser.discardBrowser(
              win.tab.tab,
              /* aForceDiscard = */ true
            );
          }
        }
      }

      // Finally, kill the process.
      const ProcessTools = Cc["@mozilla.org/processtools-service;1"].getService(
        Ci.nsIProcessToolsService
      );
      ProcessTools.kill(pid);
    } else if (row.win && row.win.tab && row.win.tab.tabbrowser) {
      // This is a tab, close it.
      row.win.tab.tabbrowser.removeTab(row.win.tab.tab, {
        skipPermitUnload: true,
        animate: true,
      });
      View._killedRecently.push({ outerWindowId: row.win.outerWindowId });
      row.classList.add("killing");

      // If this was the only root window of the process, show that the process is also getting killed.
      if (row.previousSibling.classList.contains("process")) {
        let parentRow = row.previousSibling;
        let roots = 0;
        for (let win of parentRow.process.windows) {
          if (win.isProcessRoot) {
            roots += 1;
          }
        }
        if (roots <= 1) {
          // Yes, we're the only process root, so the process is dying.
          //
          // It might actually become a preloaded process rather than
          // dying. That's an acceptable error. Even if we display incorrectly
          // that the process is dying, this error will last only one refresh.
          View._killedRecently.push({ pid: parentRow.process.pid });
          parentRow.classList.add("killing");
        }
      }
    }
  },
};

window.onload = async function() {
  Control.init();

  // Display immediately the list of processes. CPU values will be missing.
  await Control.update();

  // After the minimum interval between samples, force an update to show
  // valid CPU values asap.
  await new Promise(resolve =>
    setTimeout(resolve, MINIMUM_INTERVAL_BETWEEN_SAMPLES_MS)
  );
  await Control.update(true);

  // Then update at the normal frequency.
  window.setInterval(() => Control.update(), UPDATE_INTERVAL_MS);
};
