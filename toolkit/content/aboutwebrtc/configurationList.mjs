/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { CopyButton } from "chrome://global/content/aboutwebrtc/copyButton.mjs";

function getPref(path) {
  switch (Services.prefs.getPrefType(path)) {
    case Services.prefs.PREF_BOOL:
      return Services.prefs.getBoolPref(path);
    case Services.prefs.PREF_INT:
      return Services.prefs.getIntPref(path);
    case Services.prefs.PREF_STRING:
      return Services.prefs.getStringPref(path);
  }
  return "";
}

/*
 * This provides a visual list of configuration settings given an array of
 * configuration paths. To change the list one can call setPrefPaths.
 */
class ConfigurationList {
  constructor(aPreferencePaths) {
    this.list = document.createElement("list");
    this.list.classList.add("prefList");
    this.setPrefPaths(aPreferencePaths);
  }

  /** @return {Element} */
  view() {
    return this.list;
  }

  /**
   * @return {Element[]}
   */
  getPrefListItems() {
    return [...this.list.children].flatMap(e =>
      e.dataset.prefPath !== undefined ? [e] : []
    );
  }

  /**
   * @return {string[]}
   */
  getPrefPaths() {
    return [...this.getPrefListItems()].map(e => e.dataset.prefPath);
  }

  //     setPrefPaths adds and removes list items from the list and updates
  //     existing elements

  setPrefPaths(aPreferencePaths) {
    const currentPaths = this.getPrefPaths();
    // Take the difference of the two arrays of preferences. There are three
    // groups of paths: those removed from the current list, those to remain
    // in the current list, and those to be added.
    const { kept: keptPaths, removed: removedPaths } = currentPaths.reduce(
      (acc, p) => {
        if (aPreferencePaths.includes(p)) {
          acc.kept.push(p);
        } else {
          acc.removed.push(p);
        }
        return acc;
      },
      { removed: [], kept: [] }
    );

    const addedPaths = aPreferencePaths.filter(p => !keptPaths.includes(p));

    // Remove items
    this.getPrefListItems()
      .filter(e => removedPaths.includes(e.dataset.prefPath))
      .forEach(e => e.remove() /* Remove from DOM*/);

    const addItemForPath = path => {
      const item = document.createElement("li");
      item.dataset.prefPath = path;

      item.appendChild(new CopyButton(() => path).element);

      const pathSpan = document.createElement("span");
      pathSpan.textContent = path;
      pathSpan.classList.add(["pathDisplay"]);
      item.appendChild(pathSpan);

      const valueSpan = document.createElement("span");
      valueSpan.classList.add(["valueDisplay"]);
      item.appendChild(valueSpan);

      this.list.appendChild(item);
    };

    // Add items
    addedPaths.forEach(addItemForPath);

    // Update all pref values
    this.updatePrefValues();
  }

  updatePrefValues() {
    for (const e of this.getPrefListItems()) {
      const value = getPref(e.dataset.prefPath);
      const valueSpan = e.getElementsByClassName("valueDisplay").item(0);
      if ("prefPath" in e.dataset) {
        valueSpan.textContent = value;
      }
    }
  }

  update() {
    this.updatePrefValues();
  }
}

export { ConfigurationList };
