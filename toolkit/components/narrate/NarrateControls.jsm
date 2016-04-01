/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cu = Components.utils;

Cu.import("resource://gre/modules/narrate/VoiceSelect.jsm");
Cu.import("resource://gre/modules/narrate/Narrator.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/AsyncPrefs.jsm");

this.EXPORTED_SYMBOLS = ["NarrateControls"];

var gStrings = Services.strings.createBundle("chrome://global/locale/narrate.properties");

function NarrateControls(mm, win) {
  this._mm = mm;
  this._winRef = Cu.getWeakReference(win);

  // Append content style sheet in document head
  let style = win.document.createElement("link");
  style.rel = "stylesheet";
  style.href = "chrome://global/skin/narrate.css";
  win.document.head.appendChild(style);

  function localize(pieces, ...substitutions) {
    let result = pieces[0];
    for (let i = 0; i < substitutions.length; ++i) {
      result += gStrings.GetStringFromName(substitutions[i]) + pieces[i + 1];
    }
    return result;
  }

  let dropdown = win.document.createElement("ul");
  dropdown.className = "dropdown";
  dropdown.id = "narrate-dropdown";
  dropdown.innerHTML =
    localize`<style scoped>
      @import url("chrome://global/skin/narrateControls.css");
    </style>
    <li>
       <button class="dropdown-toggle button"
               id="narrate-toggle" title="${"narrate"}"></button>
    </li>
    <li class="dropdown-popup">
      <div id="narrate-control" class="narrate-row">
        <button disabled id="narrate-skip-previous"
                title="${"back"}"></button>
        <button id="narrate-start-stop" title="${"start"}"></button>
        <button disabled id="narrate-skip-next"
                title="${"forward"}"></button>
      </div>
      <div id="narrate-rate" class="narrate-row">
        <input id="narrate-rate-input" value="0" title="${"speed"}"
               step="25" max="100" min="-100" type="range">
      </div>
      <div id="narrate-voices" class="narrate-row"></div>
      <div class="dropdown-arrow"></div>
    </li>`;

  this.narrator = new Narrator(win);

  let branch = Services.prefs.getBranch("narrate.");
  let selectLabel = gStrings.GetStringFromName("selectvoicelabel");
  this.voiceSelect = new VoiceSelect(win, selectLabel);
  this.voiceSelect.addOptions(this._getVoiceOptions(),
    branch.getCharPref("voice"));
  this.voiceSelect.element.addEventListener("change", this);
  this.voiceSelect.element.id = "voice-select";
  win.speechSynthesis.addEventListener("voiceschanged", this);
  dropdown.querySelector("#narrate-voices").appendChild(
    this.voiceSelect.element);

  dropdown.addEventListener("click", this, true);

  let rateRange = dropdown.querySelector("#narrate-rate > input");
  rateRange.addEventListener("input", this);
  rateRange.addEventListener("mousedown", this);
  rateRange.addEventListener("mouseup", this);

  // The rate is stored as an integer.
  rateRange.value = branch.getIntPref("rate");

  let tb = win.document.getElementById("reader-toolbar");
  tb.appendChild(dropdown);
}

NarrateControls.prototype = {
  handleEvent: function(evt) {
    switch (evt.type) {
      case "mousedown":
        this._rateMousedown = true;
        break;
      case "mouseup":
        this._rateMousedown = false;
        break;
      case "input":
        this._onRateInput(evt);
        break;
      case "change":
        this._onVoiceChange();
        break;
      case "click":
        this._onButtonClick(evt);
        break;
      case "voiceschanged":
        this.voiceSelect.clear();
        this.voiceSelect.addOptions(this._getVoiceOptions(),
          Services.prefs.getCharPref("narrate.voice"));
        break;
    }
  },

  _getVoiceOptions: function() {
    let win = this._win;
    let comparer = win.Intl ?
      (new Intl.Collator()).compare : (a, b) => a.localeCompare(b);
    let options = win.speechSynthesis.getVoices().map(v => {
      return {
        label: this._createVoiceLabel(v),
        value: v.voiceURI
      };
    }).sort((a, b) => comparer(a.label, b.label));
    options.unshift({
      label: gStrings.GetStringFromName("defaultvoice"),
      value: "automatic"
    });

    return options;
  },

  _onRateInput: function(evt) {
    if (!this._rateMousedown) {
      AsyncPrefs.set("narrate.rate", parseInt(evt.target.value, 10));
      this.narrator.setRate(this._convertRate(evt.target.value));
    }
  },

  _onVoiceChange: function() {
    let voice = this.voice;
    AsyncPrefs.set("narrate.voice", voice);
    this.narrator.setVoice(voice);
  },

  _onButtonClick: function(evt) {
    switch (evt.target.id) {
      case "narrate-skip-previous":
        this.narrator.skipPrevious();
        break;
      case "narrate-skip-next":
        this.narrator.skipNext();
        break;
      case "narrate-start-stop":
        if (this.narrator.speaking) {
          this.narrator.stop();
        } else {
          this._updateSpeechControls(true);
          let options = { rate: this.rate, voice: this.voice };
          this.narrator.start(options).then(() => {
            this._updateSpeechControls(false);
          }, err => {
            Cu.reportError(`Narrate failed: ${err}.`);
            this._updateSpeechControls(false);
          });
        }
        break;
      case "narrate-toggle":
        let dropdown = this._doc.getElementById("narrate-dropdown");
        if (dropdown.classList.contains("open")) {
          if (this.narrator.speaking) {
            this.narrator.stop();
          }

          // We need to remove "keep-open" class here so that AboutReader
          // closes this dropdown properly. This class is eventually removed in
          // _updateSpeechControls which gets called after narration stops,
          // but that happend asynchronously and is too late.
          dropdown.classList.remove("keep-open");
        }
        break;
    }
  },

  _updateSpeechControls: function(speaking) {
    let dropdown = this._doc.getElementById("narrate-dropdown");
    dropdown.classList.toggle("keep-open", speaking);

    let startStopButton = this._doc.getElementById("narrate-start-stop");
    startStopButton.classList.toggle("speaking", speaking);
    startStopButton.title =
      gStrings.GetStringFromName(speaking ? "start" : "stop");

    this._doc.getElementById("narrate-skip-previous").disabled = !speaking;
    this._doc.getElementById("narrate-skip-next").disabled = !speaking;
  },

  _createVoiceLabel: function(voice) {
    // This is a highly imperfect method of making human-readable labels
    // for system voices. Because each platform has a different naming scheme
    // for voices, we use a different method for each platform.
    switch (Services.appinfo.OS) {
      case "WINNT":
        // On windows the language is included in the name, so just use the name
        return voice.name;
      case "Linux":
        // On Linux, the name is usually the unlocalized language name.
        // Use a localized language name, and have the language tag in
        // parenthisis. This is to avoid six languages called "English".
        return gStrings.formatStringFromName("voiceLabel",
          [this._getLanguageName(voice.lang) || voice.name, voice.lang], 2);
      default:
        // On Mac the language is not included in the name, find a localized
        // language name or show the tag if none exists.
        // This is the ideal naming scheme so it is also the "default".
        return gStrings.formatStringFromName("voiceLabel",
          [voice.name, this._getLanguageName(voice.lang) || voice.lang], 2);
    }
  },

  _getLanguageName: function(lang) {
    if (!this._langStrings) {
      this._langStrings = Services.strings.createBundle(
        "chrome://global/locale/languageNames.properties ");
    }

    try {
      // language tags will be lower case ascii between 2 and 3 characters long.
      return this._langStrings.GetStringFromName(lang.match(/^[a-z]{2,3}/)[0]);
    } catch (e) {
      return "";
    }
  },

  _convertRate: function(rate) {
    // We need to convert a relative percentage value to a fraction rate value.
    // eg. -100 is half the speed, 100 is twice the speed in percentage,
    // 0.5 is half the speed and 2 is twice the speed in fractions.
    return Math.pow(Math.abs(rate / 100) + 1, rate < 0 ? -1 : 1);
  },

  get _win() {
    return this._winRef.get();
  },

  get _doc() {
    return this._win.document;
  },

  get rate() {
    return this._convertRate(
      this._doc.getElementById("narrate-rate-input").value);
  },

  get voice() {
    return this.voiceSelect.value;
  }
};
