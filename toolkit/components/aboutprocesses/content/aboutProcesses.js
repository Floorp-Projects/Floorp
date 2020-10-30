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

const MS_PER_NS = 1000000;
const NS_PER_S = 1000000000;

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
    let deltaT = (cur.date - prev.date) * MS_PER_NS;
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
  _fragment: document.createDocumentFragment(),
  // Processes, tabs and subframes that we killed during the previous iteration.
  // Array<{pid:Number} | {windowId:Number}>
  _killedRecently: [],
  async commit() {
    this._killedRecently.length = 0;
    let tbody = document.getElementById("process-tbody");

    // Force translation to happen before we insert the new content in the DOM
    // to avoid flicker when resizing.
    await document.l10n.translateFragment(this._fragment);

    while (tbody.firstChild) {
      tbody.firstChild.remove();
    }
    tbody.appendChild(this._fragment);
    this._fragment = document.createDocumentFragment();
  },
  insertAfterRow(row) {
    row.parentNode.insertBefore(this._fragment, row.nextSibling);
    this._fragment = document.createDocumentFragment();
  },

  /**
   * Append a row showing a single process (without its threads).
   *
   * @param {ProcessDelta} data The data to display.
   * @return {DOMElement} The row displaying the process.
   */
  appendProcessRow(data) {
    let row = document.createElement("tr");
    row.classList.add("process");

    if (data.isHung) {
      row.classList.add("hung");
    }

    // Column: Name
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
      let elt = this._addCell(row, {
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
      elt.style.backgroundImage = `url('${image}')`;
    }

    // Column: Resident size
    {
      let { formatedDelta, formatedValue } = this._formatMemoryAndDelta(
        data.totalRamSize,
        data.deltaRamSize
      );
      let content = formatedDelta
        ? `${formatedValue}${formatedDelta}`
        : formatedValue;
      this._addCell(row, {
        content,
        classes: ["totalRamSize"],
      });
    }

    // Column: CPU: User and Kernel
    {
      let slope = this._formatPercentage(data.slopeCpu);
      let content = `${slope} (${(
        data.totalCpu / MS_PER_NS
      ).toLocaleString(undefined, { maximumFractionDigits: 0 })}ms)`;
      this._addCell(row, {
        content,
        classes: ["cpu"],
      });
    }

    // Column: Kill button – but not for all processes.
    let killButton = this._addCell(row, {
      content: "",
      classes: ["action-icon"],
    });

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

    this._fragment.appendChild(row);
    return row;
  },

  appendThreadSummaryRow(data, isOpen) {
    let row = document.createElement("tr");
    row.classList.add("thread-summary");

    // Column: Name
    let elt = this._addCell(row, {
      fluentName: "about-processes-thread-summary",
      fluentArgs: { number: data.threads.length },
      classes: ["name", "indent"],
    });
    if (data.threads.length) {
      let img = document.createElement("span");
      img.classList.add("twisty");
      if (data.isOpen) {
        img.classList.add("open");
      }
      elt.insertBefore(img, elt.firstChild);
    }

    // Column: Resident size
    this._addCell(row, {
      content: "",
      classes: ["totalRamSize"],
    });

    // Column: CPU: User and Kernel
    this._addCell(row, {
      content: "",
      classes: ["cpu"],
    });

    // Column: action
    this._addCell(row, {
      content: "",
      classes: ["action-icon"],
    });

    this._fragment.appendChild(row);
    return row;
  },

  appendDOMWindowRow(data, parent) {
    let row = document.createElement("tr");
    row.classList.add("window");

    // Column: filename
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
    let elt = this._addCell(row, {
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
      elt.style.backgroundImage = `url('${image}')`;
    }

    // Column: Resident size (empty)
    this._addCell(row, {
      content: "",
      classes: ["totalRamSize"],
    });

    // Column: CPU (empty)
    this._addCell(row, {
      content: "",
      classes: ["cpu"],
    });

    // Column: action
    let killButton = this._addCell(row, {
      content: "",
      classes: ["action-icon"],
    });

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
    this._fragment.appendChild(row);
    return row;
  },

  /**
   * Append a row showing a single thread.
   *
   * @param {ThreadDelta} data The data to display.
   * @return {DOMElement} The row displaying the thread.
   */
  appendThreadRow(data) {
    let row = document.createElement("tr");
    row.classList.add("thread");

    // Column: filename
    this._addCell(row, {
      fluentName: "about-processes-thread-name",
      fluentArgs: {
        name: data.name,
        tid: "" + data.tid /* Make sure that this number is not localized */,
      },
      classes: ["name", "double_indent"],
    });

    // Column: Resident size (empty)
    this._addCell(row, {
      content: "",
      classes: ["totalRamSize"],
    });

    // Column: CPU: User and Kernel
    {
      let slope = this._formatPercentage(data.slopeCpu);
      let text = `${slope} (${(
        data.totalCpu / MS_PER_NS
      ).toLocaleString(undefined, { maximumFractionDigits: 0 })} ms)`;
      this._addCell(row, {
        content: text,
        classes: ["cpu"],
      });
    }

    // Column: Buttons (empty)
    this._addCell(row, {
      content: "",
      classes: [],
    });

    this._fragment.appendChild(row);
    return row;
  },

  _addCell(row, { content, classes, fluentName, fluentArgs }) {
    let elt = document.createElement("td");
    if (fluentName) {
      let span = document.createElement("span");
      document.l10n.setAttributes(span, fluentName, fluentArgs);
      elt.appendChild(span);
    } else {
      elt.textContent = content;
      elt.setAttribute("title", content);
    }
    elt.classList.add(...classes);
    row.appendChild(elt);
    return elt;
  },

  /**
   * Utility method to format an optional percentage.
   *
   * As a special case, we also handle `null`, which represents the case in which we do
   * not have sufficient information to compute a percentage.
   *
   * @param {Number?} value The value to format. Must be either `null` or a non-negative number.
   * A value of 1 means 100%. A value larger than 1 is possible as processes can use several
   * cores.
   * @return {String}
   */
  _formatPercentage(value) {
    if (value == null) {
      return "?";
    }
    if (value < 0 || typeof value != "number") {
      throw new Error(`Invalid percentage value ${value}`);
    }
    if (value == 0) {
      // Let's make sure that we do not confuse idle and "close to 0%",
      // otherwise this results in weird displays.
      return "idle";
    }
    // Now work with actual percentages.
    let percentage = value * 100;
    if (percentage < 0.01) {
      // Tiny percentage, let's display something more useful than "0".
      return "~0%";
    }
    if (percentage < 1) {
      // Still a small percentage, but it should fit within 2 digits.
      return `${percentage.toLocaleString(undefined, {
        maximumFractionDigits: 2,
      })}%`;
    }
    // For other percentages, just return a round number.
    return `${Math.round(percentage)}%`;
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
    if (value < 0 || typeof value != "number") {
      throw new Error(`Invalid memory value ${value}`);
    }
    if (value >= ONE_GIGA) {
      return {
        unit: "GB",
        amount: Math.ceil((value / ONE_GIGA) * 100) / 100,
      };
    }
    if (value >= ONE_MEGA) {
      return {
        unit: "MB",
        amount: Math.ceil((value / ONE_MEGA) * 100) / 100,
      };
    }
    if (value >= ONE_KILO) {
      return {
        unit: "KB",
        amount: Math.ceil((value / ONE_KILO) * 100) / 100,
      };
    }
    return {
      unit: "B",
      amount: Math.round(value),
    };
  },

  /**
   * Format a value representing an amount of memory and a delta.
   *
   * @param {Number?} value The value to format. Must be either `null` or a non-negative number.
   * @param {Number?} value The delta to format. Must be either `null` or a non-negative number.
   * @return {
   *   {unitValue: "GB" | "MB" | "KB" | B" | "?"},
   *    formatedValue: string,
   *   {unitDelta: "GB" | "MB" | "KB" | B" | "?"},
   *    formatedDelta: string
   * }
   */
  _formatMemoryAndDelta(value, delta) {
    let formatedDelta;
    let unitDelta;
    if (delta == null) {
      formatedDelta == "";
      unitDelta = null;
    } else if (delta == 0) {
      formatedDelta = null;
      unitDelta = null;
    } else if (delta >= 0) {
      let { unit, amount } = this._formatMemory(delta);
      formatedDelta = ` (+${amount}${unit})`;
      unitDelta = unit;
    } else {
      let { unit, amount } = this._formatMemory(-delta);
      formatedDelta = ` (-${amount}${unit})`;
      unitDelta = unit;
    }
    let { unit: unitValue, amount } = this._formatMemory(value);
    return {
      unitValue,
      unitDelta,
      formatedDelta,
      formatedValue: `${amount}${unitValue}`,
    };
  },
};

var Control = {
  _openItems: new Set(),
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
        sibling.remove();
      }
      sibling = next;
    }
  },
  init() {
    this._initHangReports();

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
      let row = target.parentNode;
      if (this.selectedRow) {
        this.selectedRow.removeAttribute("selected");
      }
      if (row.windowId) {
        row.setAttribute("selected", "true");
        this.selectedRow = row;
      } else if (this.selectedRow) {
        this.selectedRow = null;
      }
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
    if (
      !force &&
      Date.now() - this._lastMouseEvent < TIME_BEFORE_SORTING_AGAIN
    ) {
      return;
    }

    let counters = State.getCounters();

    // Reset the selectedRow field and the _openItems set each time we redraw
    // to avoid keeping forever references to dead processes.
    let openItems = this._openItems;
    this._openItems = new Set();

    // Similarly, we reset `_hungItems`, based on the assumption that the process hang
    // monitor will inform us again before the next update. Since the process hang monitor
    // pings its clients about once per second and we update about once per 2 seconds
    // (or more if the mouse moves), we should be ok.
    let hungItems = this._hungItems;
    this._hungItems = new Set();

    counters = this._sortProcesses(counters);
    let previousProcess = null;
    for (let process of counters) {
      this._sortDOMWindows(process.windows);

      let isOpen = openItems.has(process.pid);
      process.isOpen = isOpen;

      let isHung = process.childID && hungItems.has(process.childID);
      process.isHung = isHung;

      let processRow = View.appendProcessRow(process, isOpen);
      processRow.process = process;

      if (process.type != "extension") {
        // We do not want to display extensions.
        let winRow;
        for (let win of process.windows) {
          if (SHOW_ALL_SUBFRAMES || win.tab || win.isProcessRoot) {
            winRow = View.appendDOMWindowRow(win, process);
            winRow.win = win;
          }
        }
      }

      if (SHOW_THREADS) {
        let threadSummaryRow = View.appendThreadSummaryRow(process, isOpen);
        threadSummaryRow.process = process;

        if (isOpen) {
          this._openItems.add(process.pid);
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

    await View.commit();
  },
  _showThreads(row) {
    let process = row.process;
    this._sortThreads(process.threads);
    let elt = row;
    for (let thread of process.threads) {
      // Enrich `elt` with a property `thread`, used for testing.
      elt = View.appendThreadRow(thread);
      elt.thread = thread;
    }
    return elt;
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
  _handleTwisty(target) {
    let row = target.parentNode.parentNode;
    let id = row.process.pid;
    if (target.classList.toggle("open")) {
      this._openItems.add(id);
      this._showThreads(row);
      View.insertAfterRow(row);
    } else {
      this._openItems.delete(id);
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
