/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-*/
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AddonManager } = ChromeUtils.import(
  "resource://gre/modules/AddonManager.jsm"
);
const { ExtensionParent } = ChromeUtils.import(
  "resource://gre/modules/ExtensionParent.jsm"
);

const { WebExtensionPolicy } = Cu.getGlobalForObject(Services);

// Time in ms before we start changing the sort order again after receiving a
// mousemove event.
const TIME_BEFORE_SORTING_AGAIN = 5000;

// How often we should add a sample to our buffer.
const BUFFER_SAMPLING_RATE_MS = 1000;

// The age of the oldest sample to keep.
const BUFFER_DURATION_MS = 10000;

// How often we should update
const UPDATE_INTERVAL_MS = 2000;

// The name of the application
const BRAND_BUNDLE = Services.strings.createBundle(
  "chrome://branding/locale/brand.properties"
);
const BRAND_NAME = BRAND_BUNDLE.GetStringFromName("brandShortName");

function extensionCountersEnabled() {
  return Services.prefs.getBoolPref(
    "extensions.webextensions.enablePerformanceCounters",
    false
  );
}

// The ids of system add-ons, so that we can hide them when the
// toolkit.aboutPerformance.showInternals pref is false.
// The API to access addons is async, so we cache the list during init.
// The list is unlikely to change while the about:performance
// tab is open, so not updating seems fine.
var gSystemAddonIds = new Set();

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

  getAny(ids) {
    for (let id of ids) {
      let result = this.get(id);
      if (result) {
        return result;
      }
    }
    return null;
  },
};

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
    let addons = WebExtensionPolicy.getActiveExtensions();
    let addonHosts = new Map();
    for (let addon of addons) {
      addonHosts.set(addon.mozExtensionHostname, addon.id);
    }

    let counters = await ChromeUtils.requestPerformanceMetrics();
    let tabs = {};
    for (let counter of counters) {
      let {
        items,
        host,
        pid,
        counterId,
        windowId,
        duration,
        isWorker,
        memoryInfo,
        isTopLevel,
      } = counter;
      // If a worker has a windowId of 0 or max uint64, attach it to the
      // browser UI (doc group with id 1).
      if (isWorker && (windowId == 18446744073709552000 || !windowId)) {
        windowId = 1;
      }
      let dispatchCount = 0;
      for (let { count } of items) {
        dispatchCount += count;
      }

      let memory = 0;
      for (let field in memoryInfo) {
        if (field == "media") {
          for (let mediaField of ["audioSize", "videoSize", "resourcesSize"]) {
            memory += memoryInfo.media[mediaField];
          }
          continue;
        }
        memory += memoryInfo[field];
      }

      let tab;
      let id = windowId;
      if (addonHosts.has(host)) {
        id = addonHosts.get(host);
      }
      if (id in tabs) {
        tab = tabs[id];
      } else {
        tab = {
          windowId,
          host,
          dispatchCount: 0,
          duration: 0,
          memory: 0,
          children: [],
        };
        tabs[id] = tab;
      }
      tab.dispatchCount += dispatchCount;
      tab.duration += duration;
      tab.memory += memory;
      if (!isTopLevel || isWorker) {
        tab.children.push({
          host,
          isWorker,
          dispatchCount,
          duration,
          memory,
          counterId: pid + ":" + counterId,
        });
      }
    }

    if (extensionCountersEnabled()) {
      let extCounters = await ExtensionParent.ParentAPIManager.retrievePerformanceCounters();
      for (let [id, apiMap] of extCounters) {
        let dispatchCount = 0,
          duration = 0;
        for (let [, counter] of apiMap) {
          dispatchCount += counter.calls;
          duration += counter.duration;
        }

        let tab;
        if (id in tabs) {
          tab = tabs[id];
        } else {
          tab = {
            windowId: 0,
            host: id,
            dispatchCount: 0,
            duration: 0,
            memory: 0,
            children: [],
          };
          tabs[id] = tab;
        }
        tab.dispatchCount += dispatchCount;
        tab.duration += duration;
      }
    }

    return { tabs, date: Cu.now() };
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

  // We can only know asynchronously if an origin is matched by the tracking
  // protection list, so we cache the result for faster future lookups.
  _trackingState: new Map(),
  isTracker(host) {
    if (!this._trackingState.has(host)) {
      // Temporarily set to false to avoid doing several lookups if a site has
      // several subframes on the same domain.
      this._trackingState.set(host, false);
      if (host.startsWith("about:") || host.startsWith("moz-nullprincipal")) {
        return false;
      }

      let uri = Services.io.newURI("http://" + host);
      let classifier = Cc["@mozilla.org/url-classifier/dbservice;1"].getService(
        Ci.nsIURIClassifier
      );
      let feature = classifier.getFeatureByName("tracking-protection");
      if (!feature) {
        return false;
      }

      classifier.asyncClassifyLocalWithFeatures(
        uri,
        [feature],
        Ci.nsIUrlClassifierFeature.blocklist,
        list => {
          if (list.length) {
            this._trackingState.set(host, true);
          }
        }
      );
    }
    return this._trackingState.get(host);
  },

  getCounters() {
    tabFinder.update();
    // We rebuild the maps during each iteration to make sure that
    // we do not maintain references to groups that has been removed
    // (e.g. pages that have been closed).

    let previous = this._buffer[Math.max(this._buffer.length - 2, 0)].tabs;
    let current = this._latest.tabs;
    let counters = [];
    for (let id of Object.keys(current)) {
      let tab = current[id];
      let oldest;
      for (let index = 0; index <= this._buffer.length - 2; ++index) {
        if (id in this._buffer[index].tabs) {
          oldest = this._buffer[index].tabs[id];
          break;
        }
      }
      let prev = previous[id];
      let host = tab.host;

      let type = "other";
      let name = `${host} (${id})`;
      let image = "chrome://global/skin/icons/defaultFavicon.svg";
      let found = tabFinder.get(parseInt(id));
      if (found) {
        if (found.tabbrowser) {
          name = found.tab.getAttribute("label");
          image = found.tab.getAttribute("image");
          type = "tab";
        } else {
          name = {
            id: "preloaded-tab",
            title: found.tab.linkedBrowser.contentTitle,
          };
        }
      } else if (id == 1) {
        name = BRAND_NAME;
        image = "chrome://branding/content/icon32.png";
        type = "browser";
      } else if (/^[a-f0-9]{8}(-[a-f0-9]{4}){3}-[a-f0-9]{12}$/.test(host)) {
        let addon = WebExtensionPolicy.getByHostname(host);
        if (!addon) {
          continue;
        }
        name = `${addon.name} (${addon.id})`;
        image = "chrome://mozapps/skin/extensions/extension.svg";
        type = gSystemAddonIds.has(addon.id) ? "system-addon" : "addon";
      } else if (id == 0 && !tab.isWorker) {
        name = { id: "ghost-windows" };
      }

      if (
        type != "tab" &&
        type != "addon" &&
        !Services.prefs.getBoolPref(
          "toolkit.aboutPerformance.showInternals",
          false
        )
      ) {
        continue;
      }

      // Create a map of all the child items from the previous time we read the
      // counters, indexed by counterId so that we can quickly find the previous
      // value for any subitem.
      let prevChildren = new Map();
      if (prev) {
        for (let child of prev.children) {
          prevChildren.set(child.counterId, child);
        }
      }
      // For each subitem, create a new object including the deltas since the previous time.
      let children = tab.children.map(child => {
        let {
          host,
          dispatchCount,
          duration,
          memory,
          isWorker,
          counterId,
        } = child;
        let dispatchesSincePrevious = dispatchCount;
        let durationSincePrevious = duration;
        if (prevChildren.has(counterId)) {
          let prevCounter = prevChildren.get(counterId);
          dispatchesSincePrevious -= prevCounter.dispatchCount;
          durationSincePrevious -= prevCounter.duration;
          prevChildren.delete(counterId);
        }

        return {
          host,
          dispatchCount,
          duration,
          isWorker,
          memory,
          dispatchesSincePrevious,
          durationSincePrevious,
        };
      });

      // Any item that remains in prevChildren is a subitem that no longer
      // exists in the current sample; remember the values of its counters
      // so that the values don't go down for the parent item.
      tab.dispatchesFromFormerChildren =
        (prev && prev.dispatchesFromFormerChildren) || 0;
      tab.durationFromFormerChildren =
        (prev && prev.durationFromFormerChildren) || 0;
      for (let [, counter] of prevChildren) {
        tab.dispatchesFromFormerChildren += counter.dispatchCount;
        tab.durationFromFormerChildren += counter.duration;
      }

      // Create the object representing the counters of the parent item including
      // the deltas from the previous times.
      let dispatches = tab.dispatchCount + tab.dispatchesFromFormerChildren;
      let duration = tab.duration + tab.durationFromFormerChildren;
      let durationSincePrevious = NaN;
      let dispatchesSincePrevious = NaN;
      let dispatchesSinceStartOfBuffer = NaN;
      let durationSinceStartOfBuffer = NaN;
      if (prev) {
        durationSincePrevious =
          duration - prev.duration - (prev.durationFromFormerChildren || 0);
        dispatchesSincePrevious =
          dispatches -
          prev.dispatchCount -
          (prev.dispatchesFromFormerChildren || 0);
      }
      if (oldest) {
        dispatchesSinceStartOfBuffer =
          dispatches -
          oldest.dispatchCount -
          (oldest.dispatchesFromFormerChildren || 0);
        durationSinceStartOfBuffer =
          duration - oldest.duration - (oldest.durationFromFormerChildren || 0);
      }
      counters.push({
        id,
        name,
        image,
        type,
        memory: tab.memory,
        totalDispatches: dispatches,
        totalDuration: duration,
        durationSincePrevious,
        dispatchesSincePrevious,
        durationSinceStartOfBuffer,
        dispatchesSinceStartOfBuffer,
        children,
      });
    }
    return counters;
  },

  getMaxEnergyImpact(counters) {
    return Math.max(
      ...counters.map(c => {
        return Control._computeEnergyImpact(
          c.dispatchesSincePrevious,
          c.durationSincePrevious
        );
      })
    );
  },
};

var View = {
  _fragment: document.createDocumentFragment(),
  async commit() {
    let tbody = document.getElementById("dispatch-tbody");

    // Force translation to happen before we insert the new content in the DOM
    // to avoid flicker when resizing.
    await document.l10n.translateFragment(this._fragment);

    // Pause the DOMLocalization mutation observer, or the already translated
    // content will be translated a second time at the next tick.
    document.l10n.pauseObserving();
    while (tbody.firstChild) {
      tbody.firstChild.remove();
    }
    tbody.appendChild(this._fragment);
    document.l10n.resumeObserving();

    this._fragment = document.createDocumentFragment();
  },
  insertAfterRow(row) {
    row.parentNode.insertBefore(this._fragment, row.nextSibling);
    this._fragment = document.createDocumentFragment();
  },
  displayEnergyImpact(elt, energyImpact, maxEnergyImpact) {
    if (!energyImpact) {
      elt.textContent = "–";
      elt.style.setProperty("--bar-width", 0);
    } else {
      let impact;
      let barWidth;
      const mediumEnergyImpact = 25;
      if (energyImpact < 1) {
        impact = "low";
        // Width 0-10%.
        barWidth = 10 * energyImpact;
      } else if (energyImpact < mediumEnergyImpact) {
        impact = "medium";
        // Width 10-50%.
        barWidth = (10 + 2 * energyImpact) * (5 / 6);
      } else {
        impact = "high";
        // Width 50-100%.
        let energyImpactFromZero = energyImpact - mediumEnergyImpact;
        if (maxEnergyImpact > 100) {
          barWidth =
            50 +
            (energyImpactFromZero / (maxEnergyImpact - mediumEnergyImpact)) *
              50;
        } else {
          barWidth = 50 + energyImpactFromZero * (2 / 3);
        }
      }
      document.l10n.setAttributes(elt, "energy-impact-" + impact, {
        value: energyImpact,
      });
      if (maxEnergyImpact != -1) {
        elt.style.setProperty("--bar-width", barWidth);
      }
    }
  },
  appendRow(
    name,
    energyImpact,
    memory,
    tooltip,
    type,
    maxEnergyImpact = -1,
    image = ""
  ) {
    let row = document.createElement("tr");

    let elt = document.createElement("td");
    if (typeof name == "string") {
      elt.textContent = name;
    } else if (name.title) {
      document.l10n.setAttributes(elt, name.id, { title: name.title });
    } else {
      document.l10n.setAttributes(elt, name.id);
    }
    if (image) {
      elt.style.backgroundImage = `url('${image}')`;
    }

    if (["subframe", "tracker", "worker"].includes(type)) {
      elt.classList.add("indent");
    } else {
      elt.classList.add("root");
    }
    if (["tracker", "worker"].includes(type)) {
      elt.classList.add(type);
    }
    row.appendChild(elt);

    elt = document.createElement("td");
    let typeLabelType = type == "system-addon" ? "addon" : type;
    document.l10n.setAttributes(elt, "type-" + typeLabelType);
    row.appendChild(elt);

    elt = document.createElement("td");
    elt.classList.add("energy-impact");
    this.displayEnergyImpact(elt, energyImpact, maxEnergyImpact);
    row.appendChild(elt);

    elt = document.createElement("td");
    if (!memory) {
      elt.textContent = "–";
    } else {
      let unit = "KB";
      memory = Math.ceil(memory / 1024);
      if (memory > 1024) {
        memory = Math.ceil((memory / 1024) * 10) / 10;
        unit = "MB";
        if (memory > 1024) {
          memory = Math.ceil((memory / 1024) * 100) / 100;
          unit = "GB";
        }
      }
      document.l10n.setAttributes(elt, "size-" + unit, { value: memory });
    }
    row.appendChild(elt);

    if (tooltip) {
      for (let key of ["dispatchesSincePrevious", "durationSincePrevious"]) {
        if (Number.isNaN(tooltip[key]) || tooltip[key] < 0) {
          tooltip[key] = "–";
        }
      }
      document.l10n.setAttributes(row, "item", tooltip);
    }

    elt = document.createElement("td");
    if (type == "tab") {
      let img = document.createElement("span");
      img.className = "action-icon close-icon";
      document.l10n.setAttributes(img, "close-tab");
      elt.appendChild(img);
    } else if (type == "addon") {
      let img = document.createElement("span");
      img.className = "action-icon addon-icon";
      document.l10n.setAttributes(img, "show-addon");
      elt.appendChild(img);
    }
    row.appendChild(elt);

    this._fragment.appendChild(row);
    return row;
  },
};

var Control = {
  _openItems: new Set(),
  _sortOrder: "",
  _removeSubtree(row) {
    while (
      row.nextSibling &&
      row.nextSibling.firstChild.classList.contains("indent")
    ) {
      row.nextSibling.remove();
    }
  },
  init() {
    let tbody = document.getElementById("dispatch-tbody");
    tbody.addEventListener("click", event => {
      this._updateLastMouseEvent();
      this._handleActivate(event.target);
    });
    tbody.addEventListener("keydown", event => {
      if (event.key === "Enter" || event.key === " ") {
        this._handleActivate(event.target);
      }
    });

    // Select the tab of double clicked items.
    tbody.addEventListener("dblclick", event => {
      let id = parseInt(event.target.parentNode.windowId);
      if (isNaN(id)) {
        return;
      }
      let found = tabFinder.get(id);
      if (!found || !found.tabbrowser) {
        return;
      }
      let { tabbrowser, tab } = found;
      tabbrowser.selectedTab = tab;
      tabbrowser.ownerGlobal.focus();
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
      .getElementById("dispatch-thead")
      .addEventListener("click", async event => {
        if (!event.target.classList.contains("clickable")) {
          return;
        }

        if (this._sortOrder) {
          let [column, direction] = this._sortOrder.split("_");
          const td = document.getElementById(`column-${column}`);
          td.classList.remove(direction);
        }

        const columnId = event.target.id;
        if (columnId == "column-type") {
          this._sortOrder =
            this._sortOrder == "type_asc" ? "type_desc" : "type_asc";
        } else if (columnId == "column-energy-impact") {
          this._sortOrder =
            this._sortOrder == "energy-impact_desc"
              ? "energy-impact_asc"
              : "energy-impact_desc";
        } else if (columnId == "column-memory") {
          this._sortOrder =
            this._sortOrder == "memory_desc" ? "memory_asc" : "memory_desc";
        } else if (columnId == "column-name") {
          this._sortOrder =
            this._sortOrder == "name_asc" ? "name_desc" : "name_asc";
        }

        let direction = this._sortOrder.split("_")[1];
        event.target.classList.add(direction);

        await this._updateDisplay(true);
      });
  },
  _lastMouseEvent: 0,
  _updateLastMouseEvent() {
    this._lastMouseEvent = Date.now();
  },
  _handleActivate(target) {
    // Handle showing or hiding subitems of a row.
    if (target.classList.contains("twisty")) {
      let row = target.parentNode.parentNode;
      let id = row.windowId;
      if (target.classList.toggle("open")) {
        target.setAttribute("aria-expanded", "true");
        this._openItems.add(id);
        this._showChildren(row);
        View.insertAfterRow(row);
      } else {
        target.setAttribute("aria-expanded", "false");
        this._openItems.delete(id);
        this._removeSubtree(row);
      }
      return;
    }
    // Handle closing a tab.
    if (target.classList.contains("close-icon")) {
      let row = target.parentNode.parentNode;
      let id = parseInt(row.windowId);
      let found = tabFinder.get(id);
      if (!found || !found.tabbrowser) {
        return;
      }
      let { tabbrowser, tab } = found;
      tabbrowser.removeTab(tab);
      this._removeSubtree(row);
      row.remove();
      return;
    }
    // Handle opening addon details.
    if (target.classList.contains("addon-icon")) {
      let row = target.parentNode.parentNode;
      let id = row.windowId;
      let parentWin =
        window.docShell.browsingContext.embedderElement.ownerGlobal;
      parentWin.BrowserOpenAddonsMgr(
        "addons://detail/" + encodeURIComponent(id)
      );
      return;
    }
    // Handle selection changes.
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
  // moved recently or when an element like a Twisty button is focused.
  async _updateDisplay(force = false) {
    let counters = State.getCounters();
    let maxEnergyImpact = State.getMaxEnergyImpact(counters);
    let focusedEl;

    // If the mouse has been moved recently, update the data displayed
    // without moving any item to avoid the risk of users clicking an action
    // button for the wrong item.
    // Memory use is unlikely to change dramatically within a few seconds, so
    // it's probably fine to not update the Memory column in this case.
    if (
      !force &&
      Date.now() - this._lastMouseEvent < TIME_BEFORE_SORTING_AGAIN
    ) {
      let energyImpactPerId = new Map();
      for (let {
        id,
        dispatchesSincePrevious,
        durationSincePrevious,
      } of counters) {
        let energyImpact = this._computeEnergyImpact(
          dispatchesSincePrevious,
          durationSincePrevious
        );
        energyImpactPerId.set(id, energyImpact);
      }

      let row = document.getElementById("dispatch-tbody").firstChild;
      while (row) {
        if (row.windowId && energyImpactPerId.has(row.windowId)) {
          // We update the value in the Energy Impact column, but don't
          // update the children, as if the child count changes there's a
          // risk of making other rows move up or down.
          const kEnergyImpactColumn = 2;
          let elt = row.childNodes[kEnergyImpactColumn];
          View.displayEnergyImpact(
            elt,
            energyImpactPerId.get(row.windowId),
            maxEnergyImpact
          );
        }
        row = row.nextSibling;
      }
      return;
    }

    let selectedId = -1;
    // Reset the selectedRow field and the _openItems set each time we redraw
    // to avoid keeping forever references to closed window ids.
    if (this.selectedRow) {
      selectedId = this.selectedRow.windowId;
      this.selectedRow = null;
    }
    let openItems = this._openItems;
    this._openItems = new Set();

    // Preserving a focus target if it is located within the page content
    let focusedId;
    const isFocusable = document.activeElement.classList.contains("twisty");
    if (document.hasFocus() && isFocusable) {
      focusedId = document.activeElement.parentNode.parentNode.windowId;
    }

    counters = this._sortCounters(counters);
    for (let {
      id,
      name,
      image,
      type,
      totalDispatches,
      dispatchesSincePrevious,
      memory,
      totalDuration,
      durationSincePrevious,
      children,
    } of counters) {
      let row = View.appendRow(
        name,
        this._computeEnergyImpact(
          dispatchesSincePrevious,
          durationSincePrevious
        ),
        memory,
        {
          totalDispatches,
          totalDuration: Math.ceil(totalDuration / 1000),
          dispatchesSincePrevious,
          durationSincePrevious: Math.ceil(durationSincePrevious / 1000),
        },
        type,
        maxEnergyImpact,
        image
      );
      row.windowId = id;
      if (id == selectedId) {
        row.setAttribute("selected", "true");
        this.selectedRow = row;
      }

      if (!children.length) {
        continue;
      }

      // Show the twisty image as a disclosure button.
      let elt = row.firstChild;
      let img = document.createElement("span");
      img.className = "twisty";
      img.setAttribute("role", "button");
      img.setAttribute("tabindex", "0");
      img.setAttribute("aria-label", name);

      let open = openItems.has(id);
      if (open) {
        img.classList.add("open");
        img.setAttribute("aria-expanded", "true");

        this._openItems.add(id);
      } else {
        img.setAttribute("aria-expanded", "false");
      }
      if (id === focusedId) {
        focusedEl = img;
      }

      // If there's an l10n id on our <td> node, any image we add will be
      // removed during localization, so move the l10n id to a <span>
      let l10nAttrs = document.l10n.getAttributes(elt);
      if (l10nAttrs.id) {
        let span = document.createElement("span");
        document.l10n.setAttributes(span, l10nAttrs.id, l10nAttrs.args);
        elt.removeAttribute("data-l10n-id");
        elt.removeAttribute("data-l10n-args");
        elt.insertBefore(span, elt.firstChild);
      }

      elt.insertBefore(img, elt.firstChild);

      row._children = children;
      if (open) {
        this._showChildren(row);
      }
    }

    await View.commit();

    if (focusedEl) {
      focusedEl.focus();
    }
  },
  _showChildren(row) {
    let children = row._children;
    children.sort(
      (a, b) => b.dispatchesSincePrevious - a.dispatchesSincePrevious
    );
    for (let row of children) {
      let host = row.host.replace(/^blob:https?:\/\//, "");
      let type = "subframe";
      if (State.isTracker(host)) {
        type = "tracker";
      }
      if (row.isWorker) {
        type = "worker";
      }
      View.appendRow(
        row.host,
        this._computeEnergyImpact(
          row.dispatchesSincePrevious,
          row.durationSincePrevious
        ),
        row.memory,
        {
          totalDispatches: row.dispatchCount,
          totalDuration: Math.ceil(row.duration / 1000),
          dispatchesSincePrevious: row.dispatchesSincePrevious,
          durationSincePrevious: Math.ceil(row.durationSincePrevious / 1000),
        },
        type
      );
    }
  },
  _computeEnergyImpact(dispatches, duration) {
    // 'Dispatches' doesn't make sense to users, and it's difficult to present
    // two numbers in a meaningful way, so we need to somehow aggregate the
    // dispatches and duration values we have.
    // The current formula to aggregate the numbers assumes that the cost of
    // a dispatch is equivalent to 1ms of CPU time.
    // Dividing the result by the sampling interval and by 10 gives a number that
    // looks like a familiar percentage to users, as fullying using one core will
    // result in a number close to 100.
    let energyImpact =
      Math.max(duration || 0, dispatches * 1000) / UPDATE_INTERVAL_MS / 10;
    // Keep only 2 digits after the decimal point.
    return Math.ceil(energyImpact * 100) / 100;
  },
  _getTypeWeight(type) {
    let weights = {
      tab: 3,
      addon: 2,
      "system-addon": 1,
    };
    return weights[type] || 0;
  },
  _sortCounters(counters) {
    return counters.sort((a, b) => {
      // Force 'Recently Closed Tabs' to be always at the bottom, because it'll
      // never be actionable.
      if (a.name.id && a.name.id == "ghost-windows") {
        return 1;
      }

      if (this._sortOrder) {
        let res;
        let [column, order] = this._sortOrder.split("_");
        switch (column) {
          case "memory":
            res = a.memory - b.memory;
            break;
          case "type":
            if (a.type != b.type) {
              res = this._getTypeWeight(b.type) - this._getTypeWeight(a.type);
            } else {
              res = String.prototype.localeCompare.call(a.name, b.name);
            }
            break;
          case "name":
            res = String.prototype.localeCompare.call(a.name, b.name);
            break;
          case "energy-impact":
            res =
              this._computeEnergyImpact(
                a.dispatchesSincePrevious,
                a.durationSincePrevious
              ) -
              this._computeEnergyImpact(
                b.dispatchesSincePrevious,
                b.durationSincePrevious
              );
            break;
          default:
            res = String.prototype.localeCompare.call(a.name, b.name);
        }
        if (order == "desc") {
          res = -1 * res;
        }
        return res;
      }

      // Note: _computeEnergyImpact uses UPDATE_INTERVAL_MS which doesn't match
      // the time between the most recent sample and the start of the buffer,
      // BUFFER_DURATION_MS would be better, but the values is never displayed
      // so this is OK.
      let aEI = this._computeEnergyImpact(
        a.dispatchesSinceStartOfBuffer,
        a.durationSinceStartOfBuffer
      );
      let bEI = this._computeEnergyImpact(
        b.dispatchesSinceStartOfBuffer,
        b.durationSinceStartOfBuffer
      );
      if (aEI != bEI) {
        return bEI - aEI;
      }

      // a.name is sometimes an object, so we can't use a.name.localeCompare.
      return String.prototype.localeCompare.call(a.name, b.name);
    });
  },
};

window.onload = async function() {
  Control.init();

  let addons = await AddonManager.getAddonsByTypes(["extension"]);
  for (let addon of addons) {
    if (addon.isSystem) {
      gSystemAddonIds.add(addon.id);
    }
  }

  await Control.update();
  window.setInterval(() => Control.update(), UPDATE_INTERVAL_MS);
};
