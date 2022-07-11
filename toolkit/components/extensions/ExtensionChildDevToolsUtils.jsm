/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * @fileOverview
 * This module contains utilities for interacting with DevTools
 * from the child process.
 */

var EXPORTED_SYMBOLS = ["ExtensionChildDevToolsUtils"];

const { EventEmitter } = ChromeUtils.import(
  "resource://gre/modules/EventEmitter.jsm"
);

// Create a variable to hold the cached ThemeChangeObserver which does not
// get created until a devtools context has been created.
let themeChangeObserver;

/**
 * An observer that watches for changes to the devtools theme and provides
 * that information to the devtools.panels.themeName API property, as well as
 * emits events for the devtools.panels.onThemeChanged event. It also caches
 * the current value of devtools.themeName.
 */
class ThemeChangeObserver extends EventEmitter {
  constructor(themeName, onDestroyed) {
    super();
    this.themeName = themeName;
    this.onDestroyed = onDestroyed;
    this.contexts = new Set();

    Services.cpmm.addMessageListener("Extension:DevToolsThemeChanged", this);
  }

  addContext(context) {
    if (this.contexts.has(context)) {
      throw new Error(
        "addContext on the ThemeChangeObserver was called more than once" +
          " for the context."
      );
    }

    context.callOnClose({
      close: () => this.onContextClosed(context),
    });

    this.contexts.add(context);
  }

  onContextClosed(context) {
    this.contexts.delete(context);

    if (this.contexts.size === 0) {
      this.destroy();
    }
  }

  onThemeChanged(themeName) {
    // Update the cached themeName and emit an event for the API.
    this.themeName = themeName;
    this.emit("themeChanged", themeName);
  }

  receiveMessage({ name, data }) {
    if (name === "Extension:DevToolsThemeChanged") {
      this.onThemeChanged(data.themeName);
    }
  }

  destroy() {
    Services.cpmm.removeMessageListener("Extension:DevToolsThemeChanged", this);
    this.onDestroyed();
    this.onDestroyed = null;
    this.contexts.clear();
    this.contexts = null;
  }
}

var ExtensionChildDevToolsUtils = {
  /**
   * Creates an cached instance of the ThemeChangeObserver class and
   * initializes it with the current themeName. This cached instance is
   * destroyed when all of the contexts added to it are closed.
   *
   * @param {string} themeName The name of the current devtools theme.
   * @param {DevToolsContextChild} context The newly created devtools page context.
   */
  initThemeChangeObserver(themeName, context) {
    if (!themeChangeObserver) {
      themeChangeObserver = new ThemeChangeObserver(themeName, function() {
        themeChangeObserver = null;
      });
    }
    themeChangeObserver.addContext(context);
  },

  /**
   * Returns the cached instance of ThemeChangeObserver.
   *
   * @returns {ThemeChangeObserver} The cached instance of ThemeChangeObserver.
   */
  getThemeChangeObserver() {
    if (!themeChangeObserver) {
      throw new Error(
        "A ThemeChangeObserver must be created before being retrieved."
      );
    }
    return themeChangeObserver;
  },
};
