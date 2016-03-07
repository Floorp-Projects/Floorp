/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Components.utils.import("resource://gre/modules/Services.jsm");

this.EXPORTED_SYMBOLS = [ "NarrateTestUtils" ];

this.NarrateTestUtils = {
  TOGGLE: "#narrate-toggle",
  POPUP: "#narrate-dropdown .dropdown-popup",
  VOICE_SELECT: "#narrate-voices .select-toggle",
  VOICE_OPTIONS: "#narrate-voices .options",
  VOICE_SELECTED: "#narrate-voices .options .option.selected",
  VOICE_SELECT_LABEL: "#narrate-voices .select-toggle .current-voice",
  RATE: "#narrate-rate-input",
  START: "#narrate-start-stop:not(.speaking)",
  STOP: "#narrate-start-stop.speaking",
  BACK: "#narrate-skip-previous",
  FORWARD: "#narrate-skip-next",

  isVisible: function(element) {
    let style = element.ownerDocument.defaultView.getComputedStyle(element, "");
    if (style.display == "none") {
      return false;
    } else if (style.visibility != "visible") {
      return false;
    } else if (style.display == "-moz-popup" && element.state != "open") {
      return false;
    }

    // Hiding a parent element will hide all its children
    if (element.parentNode != element.ownerDocument) {
      return this.isVisible(element.parentNode);
    }

    return true;
  },

  isStoppedState: function(window, ok) {
    let $ = window.document.querySelector.bind(window.document);
    ok($(this.BACK).disabled, "back button is disabled");
    ok($(this.FORWARD).disabled, "forward button is disabled");
    ok(!!$(this.START), "start button is showing");
    ok(!$(this.STOP), "stop button is hidden");
  },

  isStartedState: function(window, ok) {
    let $ = window.document.querySelector.bind(window.document);
    ok(!$(this.BACK).disabled, "back button is enabled");
    ok(!$(this.FORWARD).disabled, "forward button is enabled");
    ok(!$(this.START), "start button is hidden");
    ok(!!$(this.STOP), "stop button is showing");
  },

  selectVoice: function(window, voiceUri) {
    if (!this.isVisible(window.document.querySelector(this.VOICE_OPTIONS))) {
      window.document.querySelector(this.VOICE_SELECT).click();
    }

    let voiceOption = window.document.querySelector(
      `#narrate-voices .option[data-value="${voiceUri}"]`);

    voiceOption.focus();
    voiceOption.click();

    return voiceOption.classList.contains("selected");
  },

  getEventUtils: function(window) {
    let eventUtils = {
      "_EU_Ci": Components.interfaces,
      "_EU_Cc": Components.classes,
      window: window,
      parent: window,
      navigator: window.navigator,
      KeyboardEvent: window.KeyboardEvent,
      KeyEvent: window.KeyEvent
    };
    Services.scriptloader.loadSubScript(
      "chrome://mochikit/content/tests/SimpleTest/EventUtils.js", eventUtils);
    return eventUtils;
  },

  getReaderReadyPromise: function(window) {
    return new Promise(resolve => {
      function observeReady(subject, topic) {
        if (subject == window) {
          Services.obs.removeObserver(observeReady, topic);
          resolve();
        }
      }

      if (window.document.body.classList.contains("loaded")) {
        resolve();
      } else {
        Services.obs.addObserver(observeReady, "AboutReader:Ready", false);
      }
    });
  },

  waitForPrefChange: function(pref) {
    return new Promise(resolve => {
      function observeChange() {
        Services.prefs.removeObserver(pref, observeChange);
        resolve();
      }

      Services.prefs.addObserver(pref, observeChange, false);
    });
  }
};
