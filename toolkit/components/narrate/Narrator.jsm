/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "LanguageDetector",
  "resource:///modules/translation/LanguageDetector.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");

this.EXPORTED_SYMBOLS = [ "Narrator" ];

// Maximum time into paragraph when pressing "skip previous" will go
// to previous paragraph and not the start of current one.
const PREV_THRESHOLD = 2000;

function Narrator(win) {
  this._winRef = Cu.getWeakReference(win);
  this._inTest = Services.prefs.getBoolPref("narrate.test");
  this._speechOptions = {};
  this._startTime = 0;
  this._stopped = false;
}

Narrator.prototype = {
  get _doc() {
    return this._winRef.get().document;
  },

  get _win() {
    return this._winRef.get();
  },

  get _voiceMap() {
    if (!this._voiceMapInner) {
      this._voiceMapInner = new Map();
      for (let voice of this._win.speechSynthesis.getVoices()) {
        this._voiceMapInner.set(voice.voiceURI, voice);
      }
    }

    return this._voiceMapInner;
  },

  get _paragraphs() {
    if (!this._paragraphsInner) {
      let wu = this._win.QueryInterface(
        Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
      let queryString = "#reader-header > *:not(style):not(:empty), " +
        "#moz-reader-content > .page > * > *:not(style):not(:empty)";
      // filter out zero sized paragraphs.
      let paragraphs = Array.from(this._doc.querySelectorAll(queryString));
      paragraphs = paragraphs.filter(p => {
        let bb = wu.getBoundsWithoutFlushing(p);
        return bb.width && bb.height;
      });

      this._paragraphsInner = paragraphs.map(Cu.getWeakReference);
    }

    return this._paragraphsInner;
  },

  get _timeIntoParagraph() {
    let rv = Date.now() - this._startTime;
    return rv;
  },

  get speaking() {
    return this._win.speechSynthesis.speaking ||
      this._win.speechSynthesis.pending;
  },

  _getParagraphAt: function(index) {
    let paragraph = this._paragraphsInner[index];
    return paragraph ? paragraph.get() : null;
  },

  _isParagraphInView: function(paragraphRef) {
    let paragraph = paragraphRef && paragraphRef.get && paragraphRef.get();
    if (!paragraph) {
      return false;
    }

    let bb = paragraph.getBoundingClientRect();
    return bb.top >= 0 && bb.top < this._win.innerHeight;
  },

  _detectLanguage: function() {
    if (this._speechOptions.lang || this._speechOptions.voice) {
      return Promise.resolve();
    }

    let sampleText = this._doc.getElementById(
      "moz-reader-content").textContent.substring(0, 60 * 1024);
    return LanguageDetector.detectLanguage(sampleText).then(result => {
      if (result.confident) {
        this._speechOptions.lang = result.language;
      }
    });
  },

  _sendTestEvent: function(eventType, detail) {
    let win = this._win;
    win.dispatchEvent(new win.CustomEvent(eventType,
      { detail: Cu.cloneInto(detail, win.document) }));
  },

  _speakInner: function() {
    this._win.speechSynthesis.cancel();
    let paragraph = this._getParagraphAt(this._index);
    let utterance = new this._win.SpeechSynthesisUtterance(
      paragraph.textContent);
    utterance.rate = this._speechOptions.rate;
    if (this._speechOptions.voice) {
      utterance.voice = this._speechOptions.voice;
    } else {
      utterance.lang = this._speechOptions.lang;
    }

    this._startTime = Date.now();

    return new Promise(resolve => {
      utterance.addEventListener("start", () => {
        paragraph.classList.add("narrating");
        let bb = paragraph.getBoundingClientRect();
        if (bb.top < 0 || bb.bottom > this._win.innerHeight) {
          paragraph.scrollIntoView({ behavior: "smooth", block: "start"});
        }

        if (this._inTest) {
          this._sendTestEvent("paragraphstart", {
            voice: utterance.chosenVoiceURI,
            rate: utterance.rate,
            paragraph: this._index
          });
        }
      });

      utterance.addEventListener("end", () => {
        if (!this._win) {
          // page got unloaded, don't do anything.
          return;
        }

        paragraph.classList.remove("narrating");
        this._startTime = 0;
        if (this._inTest) {
          this._sendTestEvent("paragraphend", {});
        }

        if (this._index + 1 >= this._paragraphs.length || this._stopped) {
          // We reached the end of the document, or the user pressed stopped.
          resolve();
        } else {
          this._index++;
          this._speakInner().then(resolve);
        }
      });

      this._win.speechSynthesis.speak(utterance);
    });
  },

  getVoiceOptions: function() {
    return Array.from(this._voiceMap.values());
  },

  start: function(speechOptions) {
    this._speechOptions = {
      rate: speechOptions.rate,
      voice: this._voiceMap.get(speechOptions.voice)
    };

    this._stopped = false;
    return this._detectLanguage().then(() => {
      if (!this._isParagraphInView(this._paragraphs[this._index])) {
        this._index = this._paragraphs.findIndex(
          this._isParagraphInView.bind(this));
      }

      return this._speakInner();
    });
  },

  stop: function() {
    this._stopped = true;
    this._win.speechSynthesis.cancel();
  },

  skipNext: function() {
    this._win.speechSynthesis.cancel();
  },

  skipPrevious: function() {
    this._index -=
      this._index > 0 && this._timeIntoParagraph < PREV_THRESHOLD ? 2 : 1;
    this._win.speechSynthesis.cancel();
  },

  setRate: function(rate) {
    this._speechOptions.rate = rate;
    /* repeat current paragraph */
    this._index--;
    this._win.speechSynthesis.cancel();
  },

  setVoice: function(voice) {
    this._speechOptions.voice = this._voiceMap.get(voice);
    /* repeat current paragraph */
    this._index--;
    this._win.speechSynthesis.cancel();
  }
};
