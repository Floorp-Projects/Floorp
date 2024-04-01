/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { BinarySearch } from "resource://gre/modules/BinarySearch.sys.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "ClipboardHelper",
  "@mozilla.org/widget/clipboardhelper;1",
  "nsIClipboardHelper"
);

/**
 * Create a function to format messages.
 *
 * @param  {...any} ftlFiles to be used for formatting messages
 * @returns {Function} a function that can be used to format messsages
 */
function createFormatMessages(...ftlFiles) {
  const strings = new Localization(ftlFiles);

  return async (...ids) => {
    for (const i in ids) {
      if (typeof ids[i] == "string") {
        ids[i] = { id: ids[i] };
      }
    }

    const messages = await strings.formatMessages(ids);
    return messages.map(message => {
      if (message.attributes) {
        return message.attributes.reduce(
          (result, { name, value }) => ({ ...result, [name]: value }),
          {}
        );
      }
      return message.value;
    });
  };
}

/**
 * Base datasource class
 */
export class DataSourceBase {
  #aggregatorApi;

  constructor(aggregatorApi) {
    this.#aggregatorApi = aggregatorApi;
  }

  // proxy consumer api functions to datasource interface

  refreshSingleLineOnScreen(line) {
    this.#aggregatorApi.refreshSingleLineOnScreen(line);
  }

  refreshAllLinesOnScreen() {
    this.#aggregatorApi.refreshAllLinesOnScreen();
  }

  formatMessages = createFormatMessages("preview/megalist.ftl");

  /**
   * Prototype for the each line.
   * See this link for details:
   * https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/defineProperties#props
   */
  #linePrototype = {
    /**
     * Reference to the Data Source that owns this line.
     */
    source: this,

    /**
     * Each line has a reference to the actual data record.
     */
    record: { writable: true },

    /**
     * Is line ready to be displayed?
     * Used by the View Model.
     *
     * @returns {boolean} true if line can be sent to the view.
     *          false if line is not ready to be displayed. In this case
     *          data source will start pulling value from the underlying
     *          storage and will push data to screen when it's ready.
     */
    lineIsReady() {
      return true;
    },

    copyToClipboard(text) {
      lazy.ClipboardHelper.copyString(text, lazy.ClipboardHelper.Sensitive);
    },

    openLinkInTab(url) {
      const { BrowserWindowTracker } = ChromeUtils.importESModule(
        "resource:///modules/BrowserWindowTracker.sys.mjs"
      );
      const browser = BrowserWindowTracker.getTopWindow().gBrowser;
      browser.addWebTab(url, { inBackground: false });
    },

    /**
     * Simple version of Copy command. Line still needs to add "Copy" command.
     * Override if copied value != displayed value.
     */
    executeCopy() {
      this.copyToClipboard(this.value);
    },

    executeOpen() {
      this.openLinkInTab(this.href);
    },

    executeEditInProgress(value) {
      this.editingValue = value;
      this.refreshOnScreen();
    },

    executeCancel() {
      delete this.editingValue;
      this.refreshOnScreen();
    },

    get template() {
      return "editingValue" in this ? "editingLineTemplate" : undefined;
    },

    refreshOnScreen() {
      this.source.refreshSingleLineOnScreen(this);
    },
  };

  /**
   * Creates collapsible section header line.
   *
   * @param {string} label for the section
   * @returns {object} section header line
   */
  createHeaderLine(label) {
    const toggleCommand = { id: "Toggle", label: "" };
    const result = {
      label,
      value: "",
      collapsed: false,
      start: true,
      end: true,
      source: this,

      /**
       * Use different templates depending on the collapsed state.
       */
      get template() {
        return this.collapsed
          ? "collapsedSectionTemplate"
          : "expandedSectionTemplate";
      },

      lineIsReady: () => true,

      commands: [toggleCommand],

      executeToggle() {
        this.collapsed = !this.collapsed;
        this.source.refreshAllLinesOnScreen();
      },
    };

    this.formatMessages("command-toggle").then(([toggleLabel]) => {
      toggleCommand.label = toggleLabel;
    });

    return result;
  }

  /**
   * Create a prototype to be used for data lines,
   * provides common set of features like Copy command.
   *
   * @param {object} properties to customize data line
   * @returns {object} data line prototype
   */
  prototypeDataLine(properties) {
    return Object.create(this.#linePrototype, properties);
  }

  lines = [];
  #collator = new Intl.Collator();
  #linesToForget;

  /**
   * Code to run before reloading data source.
   * It will start tracking which lines are no longer at the source so
   * afterReloadingDataSource() can remove them.
   */
  beforeReloadingDataSource() {
    this.#linesToForget = new Set(this.lines);
  }

  /**
   * Code to run after reloading data source.
   * It will forget lines that are no longer at the source and refresh screen.
   */
  afterReloadingDataSource() {
    if (this.#linesToForget.size) {
      for (let i = this.lines.length; i >= 0; i--) {
        if (this.#linesToForget.has(this.lines[i])) {
          this.lines.splice(i, 1);
        }
      }
    }

    this.#linesToForget = null;
    this.refreshAllLinesOnScreen();
  }

  /**
   * Add or update line associated with the record.
   *
   * @param {object} record with which line is associated
   * @param {*} id sortable line id
   * @param {*} fieldPrototype to be used when creating a line.
   */
  addOrUpdateLine(record, id, fieldPrototype) {
    let [found, index] = BinarySearch.search(
      (target, value) => this.#collator.compare(target, value.id),
      this.lines,
      id
    );

    if (found) {
      this.#linesToForget.delete(this.lines[index]);
    } else {
      const line = Object.create(fieldPrototype, { id: { value: id } });
      this.lines.splice(index, 0, line);
    }
    this.lines[index].record = record;
    return this.lines[index];
  }

  *enumerateLinesForMatchingRecords(searchText, stats, match) {
    stats.total = 0;
    stats.count = 0;

    if (searchText) {
      let i = 0;
      while (i < this.lines.length) {
        const currentRecord = this.lines[i].record;
        stats.total += 1;

        if (match(currentRecord)) {
          // Record matches, yield all it's lines
          while (
            i < this.lines.length &&
            currentRecord == this.lines[i].record
          ) {
            yield this.lines[i];
            i += 1;
          }
          stats.count += 1;
        } else {
          // Record does not match, skip until the next one
          while (
            i < this.lines.length &&
            currentRecord == this.lines[i].record
          ) {
            i += 1;
          }
        }
      }
    } else {
      // No search text is provided - send all lines out, count records
      let currentRecord;
      for (const line of this.lines) {
        yield line;

        if (line.record != currentRecord) {
          stats.total += 1;
          currentRecord = line.record;
        }
      }
      stats.count = stats.total;
    }
  }
}
