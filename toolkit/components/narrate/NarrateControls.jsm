/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { VoiceSelect } = ChromeUtils.import(
  "resource://gre/modules/narrate/VoiceSelect.jsm"
);
const { Narrator } = ChromeUtils.import(
  "resource://gre/modules/narrate/Narrator.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { AsyncPrefs } = ChromeUtils.import(
  "resource://gre/modules/AsyncPrefs.jsm"
);

var EXPORTED_SYMBOLS = ["NarrateControls"];

var gStrings = Services.strings.createBundle(
  "chrome://global/locale/narrate.properties"
);

function NarrateControls(win, languagePromise) {
  this._winRef = Cu.getWeakReference(win);
  this._languagePromise = languagePromise;

  win.addEventListener("unload", this);

  // Append content style sheet in document head
  let style = win.document.createElement("link");
  style.rel = "stylesheet";
  style.href = "chrome://global/skin/narrate.css";
  win.document.head.appendChild(style);

  let elemL10nMap = {
    ".narrate-skip-previous": "back",
    ".narrate-start-stop": "start",
    ".narrate-skip-next": "forward",
    ".narrate-rate-input": "speed",
  };

  let dropdown = win.document.createElement("ul");
  dropdown.className = "dropdown narrate-dropdown";

  let toggle = win.document.createElement("li");
  let toggleButton = win.document.createElement("button");
  toggleButton.className = "dropdown-toggle button narrate-toggle";
  let tip = win.document.createElement("span");
  let labelText = gStrings.GetStringFromName("listen");
  tip.textContent = labelText;
  tip.className = "hover-label";
  toggleButton.append(tip);
  toggleButton.setAttribute("aria-label", labelText);
  toggleButton.hidden = true;
  dropdown.appendChild(toggle);
  toggle.appendChild(toggleButton);

  let dropdownList = win.document.createElement("li");
  dropdownList.className = "dropdown-popup";
  dropdown.appendChild(dropdownList);

  let narrateControl = win.document.createElement("div");
  narrateControl.className = "narrate-row narrate-control";
  dropdownList.appendChild(narrateControl);

  let narrateRate = win.document.createElement("div");
  narrateRate.className = "narrate-row narrate-rate";
  dropdownList.appendChild(narrateRate);

  let narrateVoices = win.document.createElement("div");
  narrateVoices.className = "narrate-row narrate-voices";
  dropdownList.appendChild(narrateVoices);

  let dropdownArrow = win.document.createElement("div");
  dropdownArrow.className = "dropdown-arrow";
  dropdownList.appendChild(dropdownArrow);

  let narrateSkipPrevious = win.document.createElement("button");
  narrateSkipPrevious.className = "narrate-skip-previous";
  narrateSkipPrevious.disabled = true;
  narrateControl.appendChild(narrateSkipPrevious);

  let narrateStartStop = win.document.createElement("button");
  narrateStartStop.className = "narrate-start-stop";
  narrateControl.appendChild(narrateStartStop);

  let narrateSkipNext = win.document.createElement("button");
  narrateSkipNext.className = "narrate-skip-next";
  narrateSkipNext.disabled = true;
  narrateControl.appendChild(narrateSkipNext);

  let narrateRateInput = win.document.createElement("input");
  narrateRateInput.className = "narrate-rate-input";
  narrateRateInput.setAttribute("value", "0");
  narrateRateInput.setAttribute("step", "5");
  narrateRateInput.setAttribute("max", "100");
  narrateRateInput.setAttribute("min", "-100");
  narrateRateInput.setAttribute("type", "range");
  narrateRate.appendChild(narrateRateInput);

  for (let [selector, stringID] of Object.entries(elemL10nMap)) {
    dropdown
      .querySelector(selector)
      .setAttribute("title", gStrings.GetStringFromName(stringID));
  }

  this.narrator = new Narrator(win, languagePromise);

  let branch = Services.prefs.getBranch("narrate.");
  let selectLabel = gStrings.GetStringFromName("selectvoicelabel");
  this.voiceSelect = new VoiceSelect(win, selectLabel);
  this.voiceSelect.element.addEventListener("change", this);
  this.voiceSelect.element.classList.add("voice-select");
  win.speechSynthesis.addEventListener("voiceschanged", this);
  dropdown
    .querySelector(".narrate-voices")
    .appendChild(this.voiceSelect.element);

  dropdown.addEventListener("click", this, true);

  let rateRange = dropdown.querySelector(".narrate-rate > input");
  rateRange.addEventListener("change", this);

  // The rate is stored as an integer.
  rateRange.value = branch.getIntPref("rate");

  this._setupVoices();

  let tb = win.document.querySelector(".reader-controls");
  tb.appendChild(dropdown);
}

NarrateControls.prototype = {
  handleEvent(evt) {
    switch (evt.type) {
      case "change":
        if (evt.target.classList.contains("narrate-rate-input")) {
          this._onRateInput(evt);
        } else {
          this._onVoiceChange();
        }
        break;
      case "click":
        this._onButtonClick(evt);
        break;
      case "voiceschanged":
        this._setupVoices();
        break;
      case "unload":
        this.narrator.stop();
        break;
    }
  },

  /**
   * Returns true if synth voices are available.
   */
  _setupVoices() {
    return this._languagePromise.then(language => {
      this.voiceSelect.clear();
      let win = this._win;
      let voicePrefs = this._getVoicePref();
      let selectedVoice = voicePrefs[language || "default"];
      let comparer = new Services.intl.Collator().compare;
      let filter = !Services.prefs.getBoolPref("narrate.filter-voices");
      let options = win.speechSynthesis
        .getVoices()
        .filter(v => {
          return filter || !language || v.lang.split("-")[0] == language;
        })
        .map(v => {
          return {
            label: this._createVoiceLabel(v),
            value: v.voiceURI,
            selected: selectedVoice == v.voiceURI,
          };
        })
        .sort((a, b) => comparer(a.label, b.label));

      if (options.length) {
        options.unshift({
          label: gStrings.GetStringFromName("defaultvoice"),
          value: "automatic",
          selected: selectedVoice == "automatic",
        });
        this.voiceSelect.addOptions(options);
      }

      let narrateToggle = win.document.querySelector(".narrate-toggle");
      let histogram = Services.telemetry.getKeyedHistogramById(
        "NARRATE_CONTENT_BY_LANGUAGE_2"
      );
      let initial = !this._voicesInitialized;
      this._voicesInitialized = true;

      if (initial) {
        histogram.add(language, 0);
      }

      if (options.length && narrateToggle.hidden) {
        // About to show for the first time..
        histogram.add(language, 1);
      }

      // We disable this entire feature if there are no available voices.
      narrateToggle.hidden = !options.length;
    });
  },

  _getVoicePref() {
    let voicePref = Services.prefs.getCharPref("narrate.voice");
    try {
      return JSON.parse(voicePref);
    } catch (e) {
      return { default: voicePref };
    }
  },

  _onRateInput(evt) {
    AsyncPrefs.set("narrate.rate", parseInt(evt.target.value, 10));
    this.narrator.setRate(this._convertRate(evt.target.value));
  },

  _onVoiceChange() {
    let voice = this.voice;
    this.narrator.setVoice(voice);
    this._languagePromise.then(language => {
      if (language) {
        let voicePref = this._getVoicePref();
        voicePref[language || "default"] = voice;
        AsyncPrefs.set("narrate.voice", JSON.stringify(voicePref));
      }
    });
  },

  _onButtonClick(evt) {
    let classList = evt.target.classList;
    if (classList.contains("narrate-skip-previous")) {
      this.narrator.skipPrevious();
    } else if (classList.contains("narrate-skip-next")) {
      this.narrator.skipNext();
    } else if (classList.contains("narrate-start-stop")) {
      if (this.narrator.speaking) {
        this.narrator.stop();
      } else {
        this._updateSpeechControls(true);
        TelemetryStopwatch.start("NARRATE_CONTENT_SPEAKTIME_MS", this);
        let options = { rate: this.rate, voice: this.voice };
        this.narrator
          .start(options)
          .catch(err => {
            Cu.reportError(`Narrate failed: ${err}.`);
          })
          .then(() => {
            this._updateSpeechControls(false);
            TelemetryStopwatch.finish("NARRATE_CONTENT_SPEAKTIME_MS", this);
          });
      }
    }
  },

  _updateSpeechControls(speaking) {
    let dropdown = this._doc.querySelector(".narrate-dropdown");
    if (!dropdown) {
      // Elements got destroyed, but window lingers on for a bit.
      return;
    }

    dropdown.classList.toggle("keep-open", speaking);
    dropdown.classList.toggle("speaking", speaking);

    let startStopButton = this._doc.querySelector(".narrate-start-stop");
    startStopButton.title = gStrings.GetStringFromName(
      speaking ? "stop" : "start"
    );

    this._doc.querySelector(".narrate-skip-previous").disabled = !speaking;
    this._doc.querySelector(".narrate-skip-next").disabled = !speaking;
  },

  _createVoiceLabel(voice) {
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
        return gStrings.formatStringFromName("voiceLabel", [
          this._getLanguageName(voice.lang) || voice.name,
          voice.lang,
        ]);
      default:
        // On Mac the language is not included in the name, find a localized
        // language name or show the tag if none exists.
        // This is the ideal naming scheme so it is also the "default".
        return gStrings.formatStringFromName("voiceLabel", [
          voice.name,
          this._getLanguageName(voice.lang) || voice.lang,
        ]);
    }
  },

  _getLanguageName(lang) {
    try {
      // This may throw if the lang doesn't match.
      // XXX: Replace with Intl.Locale once bug 1433303 lands.
      let langCode = lang.match(/^[a-z]{2,3}/)[0];

      return Services.intl.getLanguageDisplayNames(undefined, [langCode]);
    } catch (e) {
      return "";
    }
  },

  _convertRate(rate) {
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
      this._doc.querySelector(".narrate-rate-input").value
    );
  },

  get voice() {
    return this.voiceSelect.value;
  },
};
