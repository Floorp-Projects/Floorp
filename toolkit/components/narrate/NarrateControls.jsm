/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cu = Components.utils;

Cu.import("resource://gre/modules/narrate/VoiceSelect.jsm");
Cu.import("resource://gre/modules/narrate/Narrator.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/AsyncPrefs.jsm");
Cu.import("resource://gre/modules/TelemetryStopwatch.jsm");

this.EXPORTED_SYMBOLS = ["NarrateControls"];

var gStrings = Services.strings.createBundle("chrome://global/locale/narrate.properties");

function NarrateControls(mm, win, languagePromise) {
  this._mm = mm;
  this._winRef = Cu.getWeakReference(win);
  this._languagePromise = languagePromise;

  win.addEventListener("unload", this);

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
  // We need inline svg here for the animation to work (bug 908634 & 1190881).
  // The style animation can't be scoped (bug 830056).
  dropdown.innerHTML =
    localize`<style scoped>
      @import url("chrome://global/skin/narrateControls.css");
    </style>
    <li>
       <button class="dropdown-toggle button" id="narrate-toggle"
               title="${"narrate"}" hidden>
         <svg xmlns="http://www.w3.org/2000/svg"
              xmlns:xlink="http://www.w3.org/1999/xlink"
              width="24" height="24" viewBox="0 0 24 24">
          <style>
            @keyframes grow {
              0%   { transform: scaleY(1);   }
              15%  { transform: scaleY(1.5); }
              15%  { transform: scaleY(1.5); }
              30%  { transform: scaleY(1);   }
              100% { transform: scaleY(1);   }
            }

            #waveform > rect {
              fill: #808080;
            }

            .speaking #waveform > rect {
              fill: #58bf43;
              transform-box: fill-box;
              transform-origin: 50% 50%;
              animation-name: grow;
              animation-duration: 1750ms;
              animation-iteration-count: infinite;
              animation-timing-function: linear;
            }

            #waveform > rect:nth-child(2) { animation-delay: 250ms; }
            #waveform > rect:nth-child(3) { animation-delay: 500ms; }
            #waveform > rect:nth-child(4) { animation-delay: 750ms; }
            #waveform > rect:nth-child(5) { animation-delay: 1000ms; }
            #waveform > rect:nth-child(6) { animation-delay: 1250ms; }
            #waveform > rect:nth-child(7) { animation-delay: 1500ms; }

          </style>
          <g id="waveform">
            <rect x="1"  y="8" width="2" height="8"  rx=".5" ry=".5" />
            <rect x="4"  y="5" width="2" height="14" rx=".5" ry=".5" />
            <rect x="7"  y="8" width="2" height="8"  rx=".5" ry=".5" />
            <rect x="10" y="4" width="2" height="16" rx=".5" ry=".5" />
            <rect x="13" y="2" width="2" height="20" rx=".5" ry=".5" />
            <rect x="16" y="4" width="2" height="16" rx=".5" ry=".5" />
            <rect x="19" y="7" width="2" height="10" rx=".5" ry=".5" />
          </g>
         </svg>
        </button>
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
               step="5" max="100" min="-100" type="range">
      </div>
      <div id="narrate-voices" class="narrate-row"></div>
      <div class="dropdown-arrow"></div>
    </li>`;

  this.narrator = new Narrator(win, languagePromise);

  let branch = Services.prefs.getBranch("narrate.");
  let selectLabel = gStrings.GetStringFromName("selectvoicelabel");
  this.voiceSelect = new VoiceSelect(win, selectLabel);
  this.voiceSelect.element.addEventListener("change", this);
  this.voiceSelect.element.id = "voice-select";
  win.speechSynthesis.addEventListener("voiceschanged", this);
  dropdown.querySelector("#narrate-voices").appendChild(
    this.voiceSelect.element);

  dropdown.addEventListener("click", this, true);

  let rateRange = dropdown.querySelector("#narrate-rate > input");
  rateRange.addEventListener("change", this);

  // The rate is stored as an integer.
  rateRange.value = branch.getIntPref("rate");

  this._setupVoices();

  let tb = win.document.getElementById("reader-toolbar");
  tb.appendChild(dropdown);
}

NarrateControls.prototype = {
  handleEvent: function(evt) {
    switch (evt.type) {
      case "change":
        if (evt.target.id == "narrate-rate-input") {
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
        if (this.narrator.speaking) {
          TelemetryStopwatch.finish("NARRATE_CONTENT_SPEAKTIME_MS", this);
        }
        break;
    }
  },

  /**
   * Returns true if synth voices are available.
   */
  _setupVoices: function() {
    return this._languagePromise.then(language => {
      this.voiceSelect.clear();
      let win = this._win;
      let voicePrefs = this._getVoicePref();
      let selectedVoice = voicePrefs[language || "default"];
      let comparer = win.Intl ?
        (new Intl.Collator()).compare : (a, b) => a.localeCompare(b);
      let filter = !Services.prefs.getBoolPref("narrate.filter-voices");
      let options = win.speechSynthesis.getVoices().filter(v => {
        return filter || !language || v.lang.split("-")[0] == language;
      }).map(v => {
        return {
          label: this._createVoiceLabel(v),
          value: v.voiceURI,
          selected: selectedVoice == v.voiceURI
        };
      }).sort((a, b) => comparer(a.label, b.label));

      if (options.length) {
        options.unshift({
          label: gStrings.GetStringFromName("defaultvoice"),
          value: "automatic",
          selected: selectedVoice == "automatic"
        });
        this.voiceSelect.addOptions(options);
      }

      let narrateToggle = win.document.getElementById("narrate-toggle");
      let histogram =
        Services.telemetry.getKeyedHistogramById("NARRATE_CONTENT_BY_LANGUAGE");
      let initial = !!this._voicesInitialized;
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

  _getVoicePref: function() {
    let voicePref = Services.prefs.getCharPref("narrate.voice");
    try {
      return JSON.parse(voicePref);
    } catch (e) {
      return { default: voicePref };
    }
  },

  _onRateInput: function(evt) {
    AsyncPrefs.set("narrate.rate", parseInt(evt.target.value, 10));
    this.narrator.setRate(this._convertRate(evt.target.value));
  },

  _onVoiceChange: function() {
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
    }
  },

  _updateSpeechControls: function(speaking) {
    let dropdown = this._doc.getElementById("narrate-dropdown");
    dropdown.classList.toggle("keep-open", speaking);
    dropdown.classList.toggle("speaking", speaking);

    let startStopButton = this._doc.getElementById("narrate-start-stop");
    startStopButton.title =
      gStrings.GetStringFromName(speaking ? "stop" : "start");

    this._doc.getElementById("narrate-skip-previous").disabled = !speaking;
    this._doc.getElementById("narrate-skip-next").disabled = !speaking;

    if (speaking) {
      TelemetryStopwatch.start("NARRATE_CONTENT_SPEAKTIME_MS", this);
    } else {
      TelemetryStopwatch.finish("NARRATE_CONTENT_SPEAKTIME_MS", this);
    }
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
