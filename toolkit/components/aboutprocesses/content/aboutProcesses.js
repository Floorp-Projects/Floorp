/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-*/
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Time in ms before we start changing the sort order again after receiving a
// mousemove event.
const TIME_BEFORE_SORTING_AGAIN = 5000;

// How often we should add a sample to our buffer.
const BUFFER_SAMPLING_RATE_MS = 1000;

// The age of the oldest sample to keep.
const BUFFER_DURATION_MS = 10000;

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
const { WebExtensionPolicy } = Cu.getGlobalForObject(Services);

const SHOW_THREADS = Services.prefs.getBoolPref(
  "toolkit.aboutProcesses.showThreads"
);
const SHOW_ALL_SUBFRAMES = Services.prefs.getBoolPref(
  "toolkit.aboutProcesses.showAllSubframes"
);

/**
 * Returns a Promise that's resolved after the next turn of the event loop.
 *
 * Just returning a resolved Promise would mean that any `then` callbacks
 * would be called right after the end of the current turn, so `setTimeout`
 * is used to delay Promise resolution until the next turn.
 *
 * In mochi tests, it's possible for this to be called after the
 * about:performance window has been torn down, which causes `setTimeout` to
 * throw an NS_ERROR_NOT_INITIALIZED exception. In that case, returning
 * `undefined` is fine.
 */
function wait(ms = 0) {
  try {
    let resolve;
    let p = new Promise(resolve_ => {
      resolve = resolve_;
    });
    setTimeout(resolve, ms);
    return p;
  } catch (e) {
    dump(
      "WARNING: wait aborted because of an invalid Window state in aboutPerformance.js.\n"
    );
    return undefined;
  }
}

/**
 * For the time being, Fluent doesn't support duration or memory formats, so we need
 * to fetch units from Fluent. To avoid re-fetching at each update, we prefetch these
 * units during initialization, asynchronously.
 *
 * @type Promise<{
 *   duration: { ns: String, us: String, ms: String, s: String, m: String, h: String, d: String },
 *   memory: { B: String, KB: String, MB: String, GB: String, TB: String, PB: String, EB: String }
 * }.
 */
let gPromisePrefetchedUnits;

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
  /**
   * Indexed by the number of minutes since the snapshot was taken.
   *
   * @type {Array<ApplicationSnapshot>}
   */
  _buffer: [],
  /**
   * The latest snapshot.
   *
   * @type ApplicationSnapshot
   */
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
    // If the buffer is empty, add one value for bootstraping purposes.
    if (!this._buffer.length) {
      this._latest = await this._promiseSnapshot();
      this._buffer.push(this._latest);
      await wait(BUFFER_SAMPLING_RATE_MS * 1.1);
    }

    let now = Cu.now();

    // If we haven't sampled in a while, add a sample to the buffer.
    let latestInBuffer = this._buffer[this._buffer.length - 1];
    let deltaT = now - latestInBuffer.date;
    if (force || deltaT > BUFFER_SAMPLING_RATE_MS) {
      this._latest = await this._promiseSnapshot();
      this._buffer.push(this._latest);
    }

    // If we have too many samples, remove the oldest sample.
    let oldestInBuffer = this._buffer[0];
    if (oldestInBuffer.date + BUFFER_DURATION_MS < this._latest.date) {
      this._buffer.shift();
    }
  },

  _getThreadDelta(cur, prev, deltaT) {
    let name = cur.name || "???";
    let result = {
      tid: cur.tid,
      name,
      // Total amount of CPU used, in ns (user).
      totalCpuUser: cur.cpuUser,
      slopeCpuUser: null,
      // Total amount of CPU used, in ns (kernel).
      totalCpuKernel: cur.cpuKernel,
      slopeCpuKernel: null,
      // Total amount of CPU used, in ns (user + kernel).
      totalCpu: cur.cpuUser + cur.cpuKernel,
      slopeCpu: null,
    };
    if (!prev) {
      return result;
    }
    if (prev.tid != cur.tid) {
      throw new Error("Assertion failed: A thread cannot change tid.");
    }
    result.slopeCpuUser = (cur.cpuUser - prev.cpuUser) / deltaT;
    result.slopeCpuKernel = (cur.cpuKernel - prev.cpuKernel) / deltaT;
    result.slopeCpu = result.slopeCpuKernel + result.slopeCpuUser;
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
    // Resident set size is the total memory used by the process, including shared memory.
    // Resident unique size is the memory used by the process, without shared memory.
    // Since all processes share memory with the parent process, we count the shared memory
    // as part of the parent process (`"browser"`) rather than as part of the individual
    // processes.
    let totalRamSize =
      cur.type == "browser" ? cur.residentSetSize : cur.residentUniqueSize;
    let result = {
      pid: cur.pid,
      childID: cur.childID,
      filename: cur.filename,
      totalRamSize,
      deltaRamSize: null,
      totalCpuUser: cur.cpuUser,
      slopeCpuUser: null,
      totalCpuKernel: cur.cpuKernel,
      slopeCpuKernel: null,
      totalCpu: cur.cpuUser + cur.cpuKernel,
      slopeCpu: null,
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
        result.threads = cur.threads.map(data =>
          this._getThreadDelta(data, null, null)
        );
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
      threads = cur.threads.map(curThread => {
        let prevThread = prevThreads.get(curThread.tid);
        if (!prevThread) {
          return this._getThreadDelta(curThread);
        }
        return this._getThreadDelta(curThread, prevThread, deltaT);
      });
    }
    result.deltaRamSize =
      cur.type == "browser"
        ? cur.residentSetSize - prev.residentSetSize
        : cur.residentUniqueSize - prev.residentUniqueSize;
    result.slopeCpuUser = (cur.cpuUser - prev.cpuUser) / deltaT;
    result.slopeCpuKernel = (cur.cpuKernel - prev.cpuKernel) / deltaT;
    result.slopeCpu = result.slopeCpuUser + result.slopeCpuKernel;
    result.threads = threads;
    return result;
  },

  getCounters() {
    tabFinder.update();

    // We rebuild the maps during each iteration to make sure that
    // we do not maintain references to processes that have been
    // shutdown.

    let current = this._latest;
    let counters = [];

    for (let cur of current.processes.values()) {
      // Look for the oldest point of comparison
      let oldest = null;
      let delta;
      for (let index = 0; index <= this._buffer.length - 2; ++index) {
        oldest = this._buffer[index].processes.get(cur.pid);
        if (oldest) {
          // Found it!
          break;
        }
      }
      if (oldest) {
        // Existing process. Let's display slopes info.
        delta = this._getProcessDelta(cur, oldest);
      } else {
        // New process. Let's display basic info.
        delta = this._getProcessDelta(cur, null);
      }
      counters.push(delta);
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
    while ((nextRow = this._orderedRows.shift())) {
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

  /**
   * Display a row showing a single process (without its threads).
   *
   * @param {ProcessDelta} data The data to display.
   * @return {DOMElement} The row displaying the process.
   */
  displayProcessRow(data, units) {
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
      let fluentName;
      let classNames = [];
      switch (data.type) {
        case "web":
          fluentName = "about-processes-web-process-name";
          break;
        case "webIsolated":
          fluentName = "about-processes-web-isolated-process-name";
          break;
        case "webLargeAllocation":
          fluentName = "about-processes-web-large-allocation-process-name";
          break;
        case "file":
          fluentName = "about-processes-file-process-name";
          break;
        case "extension":
          fluentName = "about-processes-extension-process-name";
          classNames = ["extensions"];
          break;
        case "privilegedabout":
          fluentName = "about-processes-privilegedabout-process-name";
          break;
        case "withCoopCoep":
          fluentName = "about-processes-with-coop-coep-process-name";
          break;
        case "browser":
          fluentName = "about-processes-browser-process-name";
          break;
        case "plugin":
          fluentName = "about-processes-plugin-process-name";
          break;
        case "gmpPlugin":
          fluentName = "about-processes-gmp-plugin-process-name";
          break;
        case "gpu":
          fluentName = "about-processes-gpu-process-name";
          break;
        case "vr":
          fluentName = "about-processes-vr-process-name";
          break;
        case "rdd":
          fluentName = "about-processes-rdd-process-name";
          break;
        case "socket":
          fluentName = "about-processes-socket-process-name";
          break;
        case "remoteSandboxBroker":
          fluentName = "about-processes-remote-sandbox-broker-process-name";
          break;
        case "forkServer":
          fluentName = "about-processes-fork-server-process-name";
          break;
        case "preallocated":
          fluentName = "about-processes-preallocated-process-name";
          break;
        // The following are probably not going to show up for users
        // but let's handle the case anyway to avoid heisenoranges
        // during tests in case of a leftover process from a previous
        // test.
        default:
          fluentName = "about-processes-unknown-process-name";
          break;
      }
      this._fillCell(nameCell, {
        fluentName,
        fluentArgs: {
          pid: "" + data.pid, // Make sure that this number is not localized
          origin: data.origin,
          type: data.type,
        },
        classes: ["type", "favicon", ...classNames],
      });

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
            image = "chrome://browser/skin/link.svg";
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
          fluentName: "about-processes-total-memory-size",
          fluentArgs: {
            total: formattedTotal.amount,
            totalUnit: units.memory[formattedTotal.unit],
            delta: Math.abs(formattedDelta.amount),
            deltaUnit: units.memory[formattedDelta.unit],
            deltaSign: data.deltaRamSize > 0 ? "+" : "-",
          },
          classes: ["memory"],
        });
      } else {
        this._fillCell(memoryCell, {
          fluentName: "about-processes-total-memory-size-no-change",
          fluentArgs: {
            total: formattedTotal.amount,
            totalUnit: units.memory[formattedTotal.unit],
          },
          classes: ["memory"],
        });
      }
    }

    // Column: CPU: User and Kernel
    let cpuCell = memoryCell.nextSibling;
    if (data.slopeCpu == null) {
      this._fillCell(cpuCell, {
        fluentName: "about-processes-cpu-user-and-kernel-not-ready",
        classes: ["cpu"],
      });
    } else {
      let { duration, unit } = this._getDuration(data.totalCpu);
      let localizedUnit = units.duration[unit];
      if (data.slopeCpu == 0) {
        this._fillCell(cpuCell, {
          fluentName: "about-processes-cpu-user-and-kernel-idle",
          fluentArgs: {
            total: duration,
            unit: localizedUnit,
          },
          classes: ["cpu"],
        });
      } else {
        this._fillCell(cpuCell, {
          fluentName: "about-processes-cpu-user-and-kernel",
          fluentArgs: {
            percent: data.slopeCpu,
            total: duration,
            unit: localizedUnit,
          },
          classes: ["cpu"],
        });
      }
    }

    // Column: Kill button – but not for all processes.
    let killButton = cpuCell.nextSibling;
    killButton.className = "action-icon";

    if (["web", "webIsolated", "webLargeAllocation"].includes(data.type)) {
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
    let fluentName = "about-processes-thread-summary";
    let fluentArgs = { number: data.threads.length };
    if (!nameCell.firstChild) {
      // Create the nodes
      this._fillCell(nameCell, {
        fluentName,
        fluentArgs,
        classes: ["name", "indent"],
      });
      let img = document.createElement("span");
      img.classList.add("twisty");
      nameCell.insertBefore(img, nameCell.firstChild);
    } else {
      // The only thing that can change is the thread count.
      let img = nameCell.firstChild;
      isOpen = img.classList.contains("open");
      let span = img.nextSibling;
      document.l10n.setAttributes(span, fluentName, fluentArgs);
    }

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

    // Column: filename
    let nameCell = row.firstChild;
    let tab = tabFinder.get(data.outerWindowId);
    let fluentName;
    let name;
    let className;
    if (parent.type == "extension") {
      fluentName = "about-processes-extension-name";
      if (data.addon) {
        name = data.addon.name;
      } else if (data.documentURI.scheme == "about") {
        // about: URLs don't have an host.
        name = data.documentURI.spec;
      } else {
        name = data.documentURI.host;
      }
    } else if (tab && tab.tabbrowser) {
      fluentName = "about-processes-tab-name";
      name = data.documentTitle;
      className = "tab";
    } else if (tab) {
      fluentName = "about-processes-preloaded-tab";
      name = null;
      className = "preloaded-tab";
    } else if (data.count == 1) {
      fluentName = "about-processes-frame-name-one";
      name = data.prePath;
      className = "frame-one";
    } else {
      fluentName = "about-processes-frame-name-many";
      name = data.prePath;
      className = "frame-many";
    }
    this._fillCell(nameCell, {
      fluentName,
      fluentArgs: {
        name,
        url: data.documentURI.spec,
        number: data.count,
        shortUrl:
          data.documentURI.scheme == "about"
            ? data.documentURI.spec
            : data.documentURI.prePath,
      },
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
  displayThreadRow(data, units) {
    const cellCount = 3;
    let rowId = "t:" + data.tid;
    let row = this._getOrCreateRow(rowId, cellCount);
    row.thread = data;
    row.className = "thread";

    // Column: filename
    let nameCell = row.firstChild;
    this._fillCell(nameCell, {
      fluentName: "about-processes-thread-name",
      fluentArgs: {
        name: data.name,
        tid: "" + data.tid /* Make sure that this number is not localized */,
      },
      classes: ["name", "double_indent"],
    });

    // Column: CPU: User and Kernel
    let cpuCell = nameCell.nextSibling;
    if (data.slopeCpu == null) {
      this._fillCell(cpuCell, {
        fluentName: "about-processes-cpu-user-and-kernel-not-ready",
        classes: ["cpu"],
      });
    } else {
      let { duration, unit } = this._getDuration(data.totalCpu);
      let localizedUnit = units.duration[unit];
      if (data.slopeCpu == 0) {
        this._fillCell(cpuCell, {
          fluentName: "about-processes-cpu-user-and-kernel-idle",
          fluentArgs: {
            total: duration,
            unit: localizedUnit,
          },
          classes: ["cpu"],
        });
      } else {
        this._fillCell(cpuCell, {
          fluentName: "about-processes-cpu-user-and-kernel",
          fluentArgs: {
            percent: data.slopeCpu,
            total: duration,
            unit: localizedUnit,
          },
          classes: ["cpu"],
        });
      }
    }

    // Third column (Buttons) is empty, nothing to do.
  },

  _orderedRows: [],
  _fillCell(elt, { classes, fluentName, fluentArgs }) {
    let span = elt.firstChild;
    if (!span) {
      span = document.createElement("span");
      elt.appendChild(span);
    }
    document.l10n.setAttributes(span, fluentName, fluentArgs);
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
    gPromisePrefetchedUnits = (async function() {
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

    await wait(0);

    await this._updateDisplay(force);
  },

  // The force parameter can force a full update even when the mouse has been
  // moved recently.
  async _updateDisplay(force = false) {
    let counters = State.getCounters();
    let units = await gPromisePrefetchedUnits;

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

      let processRow = View.displayProcessRow(process, units);

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
          this._showThreads(processRow, units);
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
  },
  _showThreads(row, units) {
    let process = row.process;
    this._sortThreads(process.threads);
    for (let thread of process.threads) {
      View.displayThreadRow(thread, units);
    }
  },
  _sortThreads(threads) {
    return threads.sort((a, b) => {
      let order;
      switch (this._sortColumn) {
        case "column-name":
          order = a.name.localeCompare(b.name) || a.pid - b.pid;
          break;
        case "column-cpu-total":
          order = b.slopeCpu - a.slopeCpu;
          break;

        case "column-memory-resident":
        case "column-pid":
        case null:
          order = b.tid - a.tid;
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
        case "column-pid":
          order = b.pid - a.pid;
          break;
        case "column-name":
          order =
            String(a.origin).localeCompare(b.origin) ||
            String(a.type).localeCompare(b.type) ||
            a.pid - b.pid;
          break;
        case "column-cpu-total":
          order = b.slopeCpu - a.slopeCpu;
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
  async _handleTwisty(target) {
    // We await immediately, to ensure that all DOM changes are made in the same tick.
    // Otherwise, it's both wasteful and harder to test.
    let units = await gPromisePrefetchedUnits;
    let row = target.parentNode.parentNode;
    if (target.classList.toggle("open")) {
      this._showThreads(row, units);
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
  await Control.update();
  window.setInterval(() => Control.update(), UPDATE_INTERVAL_MS);
};
