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

  get _treeWalker() {
    if (!this._treeWalkerRef) {
      let wu = this._win.QueryInterface(
        Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
      let nf = this._win.NodeFilter;

      let filter = {
        _matches: new Set(),

        // We want high-level elements that have non-empty text nodes.
        // For example, paragraphs. But nested anchors and other elements
        // are not interesting since their text already appears in their
        // parent's textContent.
        acceptNode: function(node) {
          if (this._matches.has(node.parentNode)) {
            // Reject sub-trees of accepted nodes.
            return nf.FILTER_REJECT;
          }

          if (!/\S/.test(node.textContent)) {
            // Reject nodes with no text.
            return nf.FILTER_REJECT;
          }

          let bb = wu.getBoundsWithoutFlushing(node);
          if (!bb.width || !bb.height) {
            // Skip non-rendered nodes. We don't reject because a zero-sized
            // container can still have visible, "overflowed", content.
            return nf.FILTER_SKIP;
          }

          for (let c = node.firstChild; c; c = c.nextSibling) {
            if (c.nodeType == c.TEXT_NODE && /\S/.test(c.textContent)) {
              // If node has a non-empty text child accept it.
              this._matches.add(node);
              return nf.FILTER_ACCEPT;
            }
          }

          return nf.FILTER_SKIP;
        }
      };

      this._treeWalkerRef = new WeakMap();

      // We can't hold a weak reference on the treewalker, because there
      // are no other strong references, and it will be GC'ed. Instead,
      // we rely on the window's lifetime and use it as a weak reference.
      this._treeWalkerRef.set(this._win,
        this._doc.createTreeWalker(this._doc.getElementById("container"),
          nf.SHOW_ELEMENT, filter, false));
    }

    return this._treeWalkerRef.get(this._win);
  },

  get _timeIntoParagraph() {
    let rv = Date.now() - this._startTime;
    return rv;
  },

  get speaking() {
    return this._win.speechSynthesis.speaking ||
      this._win.speechSynthesis.pending;
  },

  _getVoice: function(voiceURI) {
    if (!this._voiceMap || !this._voiceMap.has(voiceURI)) {
      this._voiceMap = new Map(
        this._win.speechSynthesis.getVoices().map(v => [v.voiceURI, v]));
    }

    return this._voiceMap.get(voiceURI);
  },

  _isParagraphInView: function(paragraph) {
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
    let tw = this._treeWalker;
    let paragraph = tw.currentNode;
    if (paragraph == tw.root) {
      this._sendTestEvent("paragraphsdone", {});
      return Promise.resolve();
    }

    let utterance = new this._win.SpeechSynthesisUtterance(
      paragraph.textContent);
    utterance.rate = this._speechOptions.rate;
    if (this._speechOptions.voice) {
      utterance.voice = this._speechOptions.voice;
    } else {
      utterance.lang = this._speechOptions.lang;
    }

    this._startTime = Date.now();

    return new Promise((resolve, reject) => {
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
            paragraph: paragraph.textContent
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

        if (this._stopped) {
          // User pressed stopped.
          resolve();
        } else {
          tw.currentNode = tw.nextNode() || tw.root;
          this._speakInner().then(resolve, reject);
        }
      });

      utterance.addEventListener("error", () => {
        reject("speech synthesis failed");
      });

      this._win.speechSynthesis.speak(utterance);
    });
  },

  start: function(speechOptions) {
    this._speechOptions = {
      rate: speechOptions.rate,
      voice: this._getVoice(speechOptions.voice)
    };

    this._stopped = false;
    return this._detectLanguage().then(() => {
      let tw = this._treeWalker;
      if (!this._isParagraphInView(tw.currentNode)) {
        tw.currentNode = tw.root;
        while (tw.nextNode()) {
          if (this._isParagraphInView(tw.currentNode)) {
            break;
          }
        }
      }
      if (tw.currentNode == tw.root) {
        tw.nextNode();
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
    this._goBackParagraphs(this._timeIntoParagraph < PREV_THRESHOLD ? 2 : 1);
  },

  setRate: function(rate) {
    this._speechOptions.rate = rate;
    /* repeat current paragraph */
    this._goBackParagraphs(1);
  },

  setVoice: function(voice) {
    this._speechOptions.voice = this._getVoice(voice);
    /* repeat current paragraph */
    this._goBackParagraphs(1);
  },

  _goBackParagraphs: function(count) {
    let tw = this._treeWalker;
    for (let i = 0; i < count; i++) {
      if (!tw.previousNode()) {
        tw.currentNode = tw.root;
      }
    }
    this._win.speechSynthesis.cancel();
  }
};
