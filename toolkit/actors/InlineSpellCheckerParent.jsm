/* -*- mode: js; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["InlineSpellCheckerParent"];

class InlineSpellCheckerParent extends JSWindowActorParent {
  selectDictionary({ localeCode }) {
    this.sendAsyncMessage("InlineSpellChecker:selectDictionary", {
      localeCode,
    });
  }

  replaceMisspelling({ suggestion }) {
    this.sendAsyncMessage("InlineSpellChecker:replaceMisspelling", {
      suggestion,
    });
  }

  toggleEnabled() {
    this.sendAsyncMessage("InlineSpellChecker:toggleEnabled", {});
  }

  recheckSpelling() {
    this.sendAsyncMessage("InlineSpellChecker:recheck", {});
  }

  uninit() {
    // This method gets called by InlineSpellChecker when the context menu
    // goes away and the InlineSpellChecker instance is still alive.
    // Stop referencing it and tidy the child end of us.
    this.sendAsyncMessage("InlineSpellChecker:uninit", {});
  }

  _destructionObservers = new Set();
  registerDestructionObserver(obj) {
    this._destructionObservers.add(obj);
  }

  unregisterDestructionObserver(obj) {
    this._destructionObservers.delete(obj);
  }

  didDestroy() {
    for (let obs of this._destructionObservers) {
      obs.actorDestroyed(this);
    }
    this._destructionObservers = null;
  }
}
