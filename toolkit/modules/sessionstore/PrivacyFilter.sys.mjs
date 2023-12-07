/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  PrivacyLevel: "resource://gre/modules/sessionstore/PrivacyLevel.sys.mjs",
});

/**
 * A module that provides methods to filter various kinds of data collected
 * from a tab by the current privacy level as set by the user.
 */
export var PrivacyFilter = Object.freeze({
  /**
   * Filters the given (serialized) session storage |data| according to the
   * current privacy level and returns a new object containing only data that
   * we're allowed to store.
   *
   * @param data The session storage data as collected from a tab.
   * @return object
   */
  filterSessionStorageData(data) {
    let retval = {};

    if (lazy.PrivacyLevel.shouldSaveEverything()) {
      return data;
    }

    if (!lazy.PrivacyLevel.canSaveAnything()) {
      return null;
    }

    for (let host of Object.keys(data)) {
      if (lazy.PrivacyLevel.check(host)) {
        retval[host] = data[host];
      }
    }

    return Object.keys(retval).length ? retval : null;
  },

  /**
   * Filters the given (serialized) form |data| according to the current
   * privacy level and returns a new object containing only data that we're
   * allowed to store.
   *
   * @param data The form data as collected from a tab.
   * @return object
   */
  filterFormData(data) {
    if (lazy.PrivacyLevel.shouldSaveEverything()) {
      return Object.keys(data).length ? data : null;
    }

    if (!lazy.PrivacyLevel.canSaveAnything()) {
      return null;
    }

    // If the given form data object has an associated URL that we are not
    // allowed to store data for, bail out. We explicitly discard data for any
    // children as well even if storing data for those frames would be allowed.
    if (!data || (data.url && !lazy.PrivacyLevel.check(data.url))) {
      return null;
    }

    let retval = {};

    for (let key of Object.keys(data)) {
      if (key === "children") {
        let recurse = child => this.filterFormData(child);
        let children = data.children.map(recurse).filter(child => child);

        if (children.length) {
          retval.children = children;
        }
        // Only copy keys other than "children" if we have a valid URL in
        // data.url and we thus passed the privacy level check.
      } else if (data.url) {
        retval[key] = data[key];
      }
    }

    return Object.keys(retval).length ? retval : null;
  },

  /**
   * Removes any private windows and tabs from a given browser state object.
   *
   * @param browserState (object)
   *        The browser state for which we remove any private windows and tabs.
   *        The given object will be modified.
   */
  filterPrivateWindowsAndTabs(browserState) {
    // Remove private opened windows.
    for (let i = browserState.windows.length - 1; i >= 0; i--) {
      let win = browserState.windows[i];

      if (win.isPrivate) {
        browserState.windows.splice(i, 1);

        if (browserState.selectedWindow >= i) {
          browserState.selectedWindow--;
        }
      } else {
        // Remove private tabs from all open non-private windows.
        this.filterPrivateTabs(win);
      }
    }

    // Remove private closed windows.
    browserState._closedWindows = browserState._closedWindows.filter(
      win => !win.isPrivate
    );

    // Remove private tabs from all remaining closed windows.
    browserState._closedWindows.forEach(win => this.filterPrivateTabs(win));
  },

  /**
   * Removes open private tabs from a given window state object.
   *
   * @param winState (object)
   *        The window state for which we remove any private tabs.
   *        The given object will be modified.
   */
  filterPrivateTabs(winState) {
    // Remove open private tabs.
    for (let i = winState.tabs.length - 1; i >= 0; i--) {
      let tab = winState.tabs[i];

      // Bug 1740261 - We end up with `null` entries in winState.tabs, which if
      // we don't check for we end up throwing here. This does not fix the issue of
      // how null tabs are getting into the state.
      if (!tab || tab.isPrivate) {
        winState.tabs.splice(i, 1);

        if (winState.selected >= i) {
          winState.selected--;
        }
      }
    }

    // Note that closed private tabs are only stored for private windows.
    // There is no need to call this function for private windows as the
    // whole window state should just be discarded so we explicitly don't
    // try to remove closed private tabs as an optimization.
  },
});
