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
    let main = await ChromeUtils.requestProcInfo();

    let processes = new Map();
    processes.set(main.pid, main);
    for (let child of main.children) {
      processes.set(child.pid, child);
    }

    return { processes, date: Cu.now() };
  },

  /**
   * Update the internal state.
   *
   * @return {Promise}
   */
  async update() {
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
    if (deltaT > BUFFER_SAMPLING_RATE_MS) {
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
    };
    if (!prev) {
      return result;
    }
    if (prev.tid != cur.tid) {
      throw new Error("Assertion failed: A thread cannot change tid.");
    }
    result.slopeCpuUser = (cur.cpuUser - prev.cpuUser) / deltaT;
    result.slopeCpuKernel = (cur.cpuKernel - prev.cpuKernel) / deltaT;
    return result;
  },

  /**
   * Compute the delta between two process snapshots.
   *
   * @param {ProcessSnapshot} cur
   * @param {ProcessSnapshot?} prev
   * @param {Number?} deltaT A number of nanoseconds elapsed between `cur` and `prev`.
   */
  _getProcessDelta(cur, prev, deltaT) {
    let result = {
      pid: cur.pid,
      filename: cur.filename,
      totalVirtualMemorySize: cur.virtualMemorySize,
      deltaVirtualMemorySize: null,
      totalResidentSize: cur.residentSetSize,
      deltaResidentSize: null,
      totalCpuUser: cur.cpuUser,
      slopeCpuUser: null,
      totalCpuKernel: cur.cpuKernel,
      slopeCpuKernel: null,
      type: cur.type,
      origin: cur.origin || "",
      threads: null,
    };
    if (!prev) {
      result.threads = cur.threads.map(data =>
        this._getThreadDelta(data, null, null)
      );
      return result;
    }
    if (prev.pid != cur.pid) {
      throw new Error("Assertion failed: A process cannot change pid.");
    }
    if (prev.type != cur.type) {
      throw new Error("Assertion failed: A process cannot change type.");
    }
    let prevThreads = new Map();
    for (let thread of prev.threads) {
      prevThreads.set(thread.tid, thread);
    }
    let threads = cur.threads.map(curThread => {
      let prevThread = prevThreads.get(curThread.tid);
      if (!prevThread) {
        return this._getThreadDelta(curThread);
      }
      return this._getThreadDelta(curThread, prevThread, deltaT);
    });
    result.deltaVirtualMemorySize =
      cur.virtualMemorySize - prev.virtualMemorySize;
    result.deltaResidentSize = cur.residentSetSize - prev.residentSetSize;
    result.slopeCpuUser = (cur.cpuUser - prev.cpuUser) / deltaT;
    result.slopeCpuKernel = (cur.cpuKernel - prev.cpuKernel) / deltaT;
    result.threads = threads;
    return result;
  },

  getCounters() {
    // We rebuild the maps during each iteration to make sure that
    // we do not maintain references to processes that have been
    // shutdown.

    let previous = this._buffer[Math.max(this._buffer.length - 2, 0)];
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
        delta = this._getProcessDelta(
          cur,
          oldest,
          (current.date - previous.date) * MS_PER_NS
        );
      } else {
        // New process. Let's display basic info.
        delta = this._getProcessDelta(cur, null, null);
      }
      counters.push(delta);
    }

    return counters;
  },
};

var View = {
  _fragment: document.createDocumentFragment(),
  async commit() {
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
   * @param {bool} isOpen `true` if we're also displaying the threads of this process, `false` otherwise.
   * @return {DOMElement} The row displaying the process.
   */
  appendProcessRow(data, isOpen) {
    let row = document.createElement("tr");
    row.classList.add("process");

    // Column: pid / twisty image
    {
      let elt = this._addCell(row, {
        content: data.pid,
        classes: ["pid", "root"],
      });

      if (data.threads.length) {
        let img = document.createElement("span");
        img.classList.add("twisty", "process");
        if (isOpen) {
          img.classList.add("open");
        }
        elt.insertBefore(img, elt.firstChild);
      }
    }

    // Column: type
    {
      let content = data.origin ? `${data.origin} (${data.type})` : data.type;
      this._addCell(row, {
        content,
        classes: ["type"],
      });
    }

    // Column: Resident size
    {
      let { formatedDelta, formatedValue } = this._formatMemoryAndDelta(
        data.totalResidentSize,
        data.deltaResidentSize
      );
      let content = formatedDelta
        ? `${formatedValue}${formatedDelta}`
        : formatedValue;
      this._addCell(row, {
        content,
        classes: ["totalResidentSize"],
      });
    }

    // Column: VM size
    {
      let { formatedDelta, formatedValue } = this._formatMemoryAndDelta(
        data.totalVirtualMemorySize,
        data.deltaVirtualMemorySize
      );
      let content = formatedDelta
        ? `${formatedValue}${formatedDelta}`
        : formatedValue;
      this._addCell(row, {
        content,
        classes: ["totalVirtualMemorySize"],
      });
    }

    // Column: CPU: User
    {
      let slope = this._formatPercentage(data.slopeCpuUser);
      let content = `${slope} (${(
        data.totalCpuUser / MS_PER_NS
      ).toLocaleString(undefined, { maximumFractionDigits: 0 })}ms)`;
      this._addCell(row, {
        content,
        classes: ["cpuUser"],
      });
    }

    // Column: CPU: Kernel
    {
      let slope = this._formatPercentage(data.slopeCpuKernel);
      let content = `${slope} (${(
        data.totalCpuKernel / MS_PER_NS
      ).toLocaleString(undefined, { maximumFractionDigits: 0 })}ms)`;
      this._addCell(row, {
        content,
        classes: ["cpuKernel"],
      });
    }

    // Column: Number of threads
    this._addCell(row, {
      content: data.threads.length,
      classes: ["numberOfThreads"],
    });

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

    // Column: id
    this._addCell(row, {
      content: data.tid,
      classes: ["tid", "indent"],
    });

    // Column: filename
    this._addCell(row, {
      content: data.name,
      classes: ["name"],
    });

    // Column: Resident size (empty)
    this._addCell(row, {
      content: "",
      classes: ["totalResidentSize"],
    });

    // Column: VM size (empty)
    this._addCell(row, {
      content: "",
      classes: ["totalVirtualMemorySize"],
    });

    // Column: CPU: User
    {
      let slope = this._formatPercentage(data.slopeCpuUser);
      let text = `${slope} (${(
        data.totalCpuUser / MS_PER_NS
      ).toLocaleString(undefined, { maximumFractionDigits: 0 })} ms)`;
      this._addCell(row, {
        content: text,
        classes: ["cpuUser"],
      });
    }

    // Column: CPU: Kernel
    {
      let slope = this._formatPercentage(data.slopeCpuKernel);
      let text = `${slope} (${(
        data.totalCpuKernel / MS_PER_NS
      ).toLocaleString(undefined, { maximumFractionDigits: 0 })} ms)`;
      this._addCell(row, {
        content: text,
        classes: ["cpuKernel"],
      });
    }

    // Column: Number of threads (empty)
    this._addCell(row, {
      content: "",
      classes: ["numberOfThreads"],
    });

    this._fragment.appendChild(row);
    return row;
  },

  _addCell(row, { content, classes }) {
    let elt = document.createElement("td");
    this._setTextAndTooltip(elt, content);
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
  _setTextAndTooltip(elt, text, tooltip = text) {
    elt.textContent = text;
    elt.setAttribute("title", tooltip);
  },
};

var Control = {
  _openItems: new Set(),
  _sortColumn: null,
  _sortAscendent: true,
  _removeSubtree(row) {
    while (row.nextSibling && row.nextSibling.classList.contains("thread")) {
      row.nextSibling.remove();
    }
  },
  init() {
    let tbody = document.getElementById("process-tbody");
    tbody.addEventListener("click", event => {
      this._updateLastMouseEvent();

      // Handle showing or hiding subitems of a row.
      let target = event.target;
      if (target.classList.contains("twisty")) {
        let row = target.parentNode.parentNode;
        let id = row.process.pid;
        if (target.classList.toggle("open")) {
          this._openItems.add(id);
          this._showChildren(row);
          View.insertAfterRow(row);
        } else {
          this._openItems.delete(id);
          this._removeSubtree(row);
        }
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

    tbody.addEventListener("mousemove", () => {
      this._updateLastMouseEvent();
    });

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
  async update() {
    await State.update();

    if (document.hidden) {
      return;
    }

    await wait(0);

    await this._updateDisplay();
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

    counters = this._sortProcesses(counters);
    for (let process of counters) {
      let isOpen = openItems.has(process.pid);
      let row = View.appendProcessRow(process, isOpen);
      row.process = process;
      if (isOpen) {
        this._openItems.add(process.pid);
        this._showChildren(row);
      }
    }

    await View.commit();
  },
  _showChildren(row) {
    let process = row.process;
    this._sortThreads(process.threads);
    for (let thread of process.threads) {
      View.appendThreadRow(thread);
    }
  },
  _sortThreads(threads) {
    return threads.sort((a, b) => {
      let order;
      switch (this._sortColumn) {
        case "column-name":
          order = a.name.localeCompare(b.name);
          break;
        case "column-cpu-user":
          order = b.slopeCpuUser - a.slopeCpuUser;
          if (order == 0) {
            order = b.totalCpuUser - a.totalCpuUser;
          }
          break;
        case "column-cpu-kernel":
          order = b.slopeCpuKernel - a.slopeCpuKernel;
          if (order == 0) {
            order = b.totalCpuKernel - a.totalCpuKernel;
          }
          break;
        case "column-cpu-threads":
        case "column-memory-resident":
        case "column-memory-virtual":
        case "column-type":
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
        case "column-type":
          order = String(a.origin).localeCompare(b.origin);
          if (order == 0) {
            order = String(a.type).localeCompare(b.type);
          }
          break;
        case "column-name":
          order = String(a.name).localeCompare(b.name);
          break;
        case "column-cpu-user":
          order = b.slopeCpuUser - a.slopeCpuUser;
          if (order == 0) {
            order = b.totalCpuUser - a.totalCpuUser;
          }
          break;
        case "column-cpu-kernel":
          order = b.slopeCpuKernel - a.slopeCpuKernel;
          if (order == 0) {
            order = b.totalCpuKernel - a.totalCpuKernel;
          }
          break;
        case "column-cpu-threads":
          order = b.threads.length - a.threads.length;
          break;
        case "column-memory-resident":
          order = b.totalResidentSize - a.totalResidentSize;
          break;
        case "column-memory-virtual":
          order = b.totalVirtualMemorySize - a.totalVirtualMemorySize;
          break;
        case null:
          // Default order: browser goes first.
          if (a.type == "browser") {
            order = -1;
          } else if (b.type == "browser") {
            order = 1;
          }
          // Other processes by increasing pid, arbitrarily.
          order = b.pid - a.pid;
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
};

window.onload = async function() {
  Control.init();
  await Control.update();
  window.setInterval(() => Control.update(), UPDATE_INTERVAL_MS);
};
