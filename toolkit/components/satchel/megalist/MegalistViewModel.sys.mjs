/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { DefaultAggregator } from "resource://gre/modules/megalist/aggregator/DefaultAggregator.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  OSKeyStore: "resource://gre/modules/OSKeyStore.sys.mjs",
});

/**
 * View Model for Megalist.
 *
 * Responsible for filtering, grouping, moving selection, editing.
 * Refers to the same MegalistAggregator in the parent process to access data.
 * Paired to exactly one MegalistView in the child process to present to the user.
 * Receives user commands from MegalistView.
 *
 * There can be multiple snapshots of the same line displayed in different contexts.
 *
 * snapshotId - an id for a snapshot of a line used between View Model and View.
 */
export class MegalistViewModel {
  /**
   *
   * View Model prepares snapshots in the parent process to be displayed
   * by the View in the child process. View gets the firstSnapshotId + length of the
   * list. Making it a very short message for each time we filter or refresh data.
   *
   * View requests line data by providing snapshotId = firstSnapshotId + index.
   *
   */
  #firstSnapshotId = 0;
  #snapshots = [];
  #selectedIndex = 0;
  #searchText = "";
  #messageToView;
  static #aggregator = new DefaultAggregator();

  constructor(messageToView) {
    this.#messageToView = messageToView;
    MegalistViewModel.#aggregator.attachViewModel(this);
  }

  willDestroy() {
    MegalistViewModel.#aggregator.detachViewModel(this);
  }

  refreshAllLinesOnScreen() {
    this.#rebuildSnapshots();
  }

  refreshSingleLineOnScreen(line) {
    if (this.#searchText) {
      // Data is filtered, which may require rebuilding the whole list
      //@sg check if current filter would affected by this line
      //@sg throttle refresh operation
      this.#rebuildSnapshots();
    } else {
      const snapshotIndex = this.#snapshots.indexOf(line);
      if (snapshotIndex >= 0) {
        const snapshotId = snapshotIndex + this.#firstSnapshotId;
        this.#sendSnapshotToView(snapshotId, line);
      }
    }
  }

  #commandsArray(snapshot) {
    if (Array.isArray(snapshot.commands)) {
      return snapshot.commands;
    }
    return Array.from(snapshot.commands());
  }

  /**
   *
   * Send snapshot of necessary line data across parent-child boundary.
   *
   * @param {number} snapshotId
   * @param {object} snapshotData
   */
  async #sendSnapshotToView(snapshotId, snapshotData) {
    if (!snapshotData) {
      return;
    }

    // Only usable set of fields is sent over to the View.
    // Line object may contain other data used by the Data Source.
    const snapshot = {
      label: snapshotData.label,
      value: await snapshotData.value,
    };
    if ("template" in snapshotData) {
      snapshot.template = snapshotData.template;
    }
    if ("start" in snapshotData) {
      snapshot.start = snapshotData.start;
    }
    if ("end" in snapshotData) {
      snapshot.end = snapshotData.end;
    }
    if ("commands" in snapshotData) {
      snapshot.commands = this.#commandsArray(snapshotData);
    }
    if ("valueIcon" in snapshotData) {
      snapshot.valueIcon = snapshotData.valueIcon;
    }
    if ("href" in snapshotData) {
      snapshot.href = snapshotData.href;
    }
    if (snapshotData.stickers) {
      for (const sticker of snapshotData.stickers()) {
        snapshot.stickers ??= [];
        snapshot.stickers.push(sticker);
      }
    }

    this.#messageToView("Snapshot", { snapshotId, snapshot });
  }

  receiveRequestSnapshot({ snapshotId }) {
    const snapshotIndex = snapshotId - this.#firstSnapshotId;
    const snapshot = this.#snapshots[snapshotIndex];
    if (!snapshot) {
      // Ignore request for unknown line index or outdated list
      return;
    }

    if (snapshot.lineIsReady()) {
      this.#sendSnapshotToView(snapshotId, snapshot);
    }
  }

  handleViewMessage({ name, data }) {
    const handlerName = `receive${name}`;
    if (!(handlerName in this)) {
      throw new Error(`Received unknown message "${name}"`);
    }
    return this[handlerName](data);
  }

  receiveRefresh() {
    this.#rebuildSnapshots();
  }

  #rebuildSnapshots() {
    // Remember current selection to attempt to restore it later
    const prevSelected = this.#snapshots[this.#selectedIndex];

    // Rebuild snapshots
    this.#firstSnapshotId += this.#snapshots.length;
    this.#snapshots = Array.from(
      MegalistViewModel.#aggregator.enumerateLines(this.#searchText)
    );

    // Update snapshots on screen
    this.#messageToView("ShowSnapshots", {
      firstSnapshotId: this.#firstSnapshotId,
      count: this.#snapshots.length,
    });

    // Restore selection
    const usedToBeSelectedNewIndex = this.#snapshots.findIndex(
      snapshot => snapshot == prevSelected
    );
    if (usedToBeSelectedNewIndex >= 0) {
      this.#selectSnapshotByIndex(usedToBeSelectedNewIndex);
    } else {
      // Make sure selection is within visible lines
      this.#selectSnapshotByIndex(
        Math.min(this.#selectedIndex, this.#snapshots.length - 1)
      );
    }
  }

  receiveUpdateFilter({ searchText } = { searchText: "" }) {
    if (this.#searchText != searchText) {
      this.#searchText = searchText;
      this.#messageToView("MegalistUpdateFilter", { searchText });
      this.#rebuildSnapshots();
    }
  }

  async receiveCommand({ commandId, snapshotId, value } = {}) {
    const dotIndex = commandId?.indexOf(".");
    const index = snapshotId
      ? snapshotId - this.#firstSnapshotId
      : this.#selectedIndex;
    const snapshot = this.#snapshots[index];

    if (dotIndex >= 0) {
      const dataSourceName = commandId.substring(0, dotIndex);
      const functionName = commandId.substring(dotIndex + 1);
      MegalistViewModel.#aggregator.callFunction(
        dataSourceName,
        functionName,
        snapshot?.record
      );
      return;
    }

    if (snapshot) {
      const commands = this.#commandsArray(snapshot);
      commandId = commandId ?? commands[0]?.id;
      const mustVerify = commands.find(c => c.id == commandId)?.verify;
      if (!mustVerify || (await this.#verifyUser())) {
        // TODO:Enter the prompt message and pref for #verifyUser()
        await snapshot[`execute${commandId}`]?.(value);
      }
    }
  }

  receiveSelectSnapshot({ snapshotId }) {
    const index = snapshotId - this.#firstSnapshotId;
    if (index >= 0) {
      this.#selectSnapshotByIndex(index);
    }
  }

  receiveSelectNextSnapshot() {
    this.#selectSnapshotByIndex(this.#selectedIndex + 1);
  }

  receiveSelectPreviousSnapshot() {
    this.#selectSnapshotByIndex(this.#selectedIndex - 1);
  }

  receiveSelectNextGroup() {
    let i = this.#selectedIndex + 1;
    while (i < this.#snapshots.length - 1 && !this.#snapshots[i].start) {
      i += 1;
    }
    this.#selectSnapshotByIndex(i);
  }

  receiveSelectPreviousGroup() {
    let i = this.#selectedIndex - 1;
    while (i >= 0 && !this.#snapshots[i].start) {
      i -= 1;
    }
    this.#selectSnapshotByIndex(i);
  }

  #selectSnapshotByIndex(index) {
    if (index >= 0 && index < this.#snapshots.length) {
      this.#selectedIndex = index;
      const selectedIndex = this.#selectedIndex;
      this.#messageToView("UpdateSelection", { selectedIndex });
    }
  }

  setLayout(layout) {
    this.#messageToView("SetLayout", { layout });
  }

  async #verifyUser(promptMessage, prefName) {
    if (!this.getOSAuthEnabled(prefName)) {
      promptMessage = false;
    }
    let result = await lazy.OSKeyStore.ensureLoggedIn(promptMessage);
    return result.authenticated;
  }

  /**
   * Get the decrypted value for a string pref.
   *
   * @param {string} prefName -> The pref whose value is needed.
   * @param {string} safeDefaultValue -> Value to be returned incase the pref is not yet set.
   * @returns {string}
   */
  #getSecurePref(prefName, safeDefaultValue) {
    try {
      let encryptedValue = Services.prefs.getStringPref(prefName, "");
      return this._crypto.decrypt(encryptedValue);
    } catch {
      return safeDefaultValue;
    }
  }

  /**
   * Set the pref to the encrypted form of the value.
   *
   * @param {string} prefName -> The pref whose value is to be set.
   * @param {string} value -> The value to be set in its encryoted form.
   */
  #setSecurePref(prefName, value) {
    let encryptedValue = this._crypto.encrypt(value);
    Services.prefs.setStringPref(prefName, encryptedValue);
  }

  /**
   * Get whether the OSAuth is enabled or not.
   *
   * @param {string} prefName -> The name of the pref (creditcards or addresses)
   * @returns {boolean}
   */
  getOSAuthEnabled(prefName) {
    return this.#getSecurePref(prefName, "") !== "opt out";
  }

  /**
   * Set whether the OSAuth is enabled or not.
   *
   * @param {string} prefName -> The pref to encrypt.
   * @param {boolean} enable -> Whether the pref is to be enabled.
   */
  setOSAuthEnabled(prefName, enable) {
    if (enable) {
      Services.prefs.clearUserPref(prefName);
    } else {
      this.#setSecurePref(prefName, "opt out");
    }
  }
}
