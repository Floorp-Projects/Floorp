/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["GeckoViewActorManager"];

const { GeckoViewUtils } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewUtils.jsm"
);

const actors = new Set();

var GeckoViewActorManager = {
  addJSWindowActors(actors) {
    for (const [actorName, actor] of Object.entries(actors)) {
      this._register(actorName, actor);
    }
  },

  _register(actorName, actor) {
    if (actors.has(actorName)) {
      // Actor already registered, nothing to do
      return;
    }

    ChromeUtils.registerWindowActor(actorName, actor);
    actors.add(actorName);
  },
};

const { debug, warn } = GeckoViewUtils.initLogging("GeckoViewActorManager");
