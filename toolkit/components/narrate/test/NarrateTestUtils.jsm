/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cu = Components.utils;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://testing-common/ContentTaskUtils.jsm");

this.EXPORTED_SYMBOLS = [ "NarrateTestUtils" ];

this.NarrateTestUtils = {
  TOGGLE: "#narrate-toggle",
  POPUP: "#narrate-dropdown .dropdown-popup",
  VOICE_SELECT: "#narrate-voices .select-toggle",
  VOICE_OPTIONS: "#narrate-voices .options",
  VOICE_SELECTED: "#narrate-voices .options .option.selected",
  VOICE_SELECT_LABEL: "#narrate-voices .select-toggle .current-voice",
  RATE: "#narrate-rate-input",
  START: "#narrate-dropdown:not(.speaking) #narrate-start-stop",
  STOP: "#narrate-dropdown.speaking #narrate-start-stop",
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
    // This checks for a localized label. Not the best...
    ok($(this.START).title == "Start", "Button tooltip is correct");
  },

  isStartedState: function(window, ok) {
    let $ = window.document.querySelector.bind(window.document);
    ok(!$(this.BACK).disabled, "back button is enabled");
    ok(!$(this.FORWARD).disabled, "forward button is enabled");
    ok(!$(this.START), "start button is hidden");
    ok(!!$(this.STOP), "stop button is showing");
    // This checks for a localized label. Not the best...
    ok($(this.STOP).title == "Stop", "Button tooltip is correct");
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

  waitForNarrateToggle: function(window) {
    let toggle = window.document.querySelector(this.TOGGLE);
    return ContentTaskUtils.waitForCondition(
      () => !toggle.hidden, "");
  },

  waitForPrefChange: function(pref) {
    return new Promise(resolve => {
      function observeChange() {
        Services.prefs.removeObserver(pref, observeChange);
        resolve(Preferences.get(pref));
      }

      Services.prefs.addObserver(pref, observeChange, false);
    });
  },

  sendBoundaryEvent: function(window, name, charIndex, charLength) {
    let detail = { type: "boundary", args: { name, charIndex, charLength } };
    window.dispatchEvent(new window.CustomEvent("testsynthevent",
      { detail: detail }));
  },

  isWordHighlightGone: function(window, ok) {
    let $ = window.document.querySelector.bind(window.document);
    ok(!$(".narrate-word-highlight"), "No more word highlights exist");
  },

  getWordHighlights: function(window) {
    let $$ = window.document.querySelectorAll.bind(window.document);
    let nodes = Array.from($$(".narrate-word-highlight"));
    return nodes.map(node => {
      return { word: node.dataset.word,
               left: Number(node.style.left.replace(/px$/, "")),
               top: Number(node.style.top.replace(/px$/, ""))};
    });
  }
};
