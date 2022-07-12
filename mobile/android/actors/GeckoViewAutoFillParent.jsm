/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewAutoFillParent"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const { GeckoViewActorParent } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewActorParent.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  gAutofillManager: "resource://gre/modules/GeckoViewAutofill.jsm",
});

class GeckoViewAutoFillParent extends GeckoViewActorParent {
  constructor() {
    super();
    this.sessionId = Services.uuid
      .generateUUID()
      .toString()
      .slice(1, -1); // discard the surrounding curly braces
  }

  get rootActor() {
    return this.browsingContext.top.currentWindowGlobal.getActor(
      "GeckoViewAutoFill"
    );
  }

  get autofill() {
    return lazy.gAutofillManager.get(this.sessionId);
  }

  add(node) {
    // We will start a new session if the current one does not exist.
    const autofill = lazy.gAutofillManager.ensure(
      this.sessionId,
      this.eventDispatcher
    );
    return autofill?.add(node);
  }

  focus(node) {
    this.autofill?.focus(node);
  }

  commit(node) {
    this.autofill?.commit(node);
  }

  update(node) {
    this.autofill?.update(node);
  }

  clear() {
    lazy.gAutofillManager.delete(this.sessionId);
  }

  async receiveMessage(aMessage) {
    const { name } = aMessage;
    debug`receiveMessage ${name}`;

    // We need to re-route all messages through the root actor to ensure that we
    // have a consistent sessionId for the entire browsingContext tree.
    switch (name) {
      case "Add": {
        return this.rootActor.add(aMessage.data.node);
      }
      case "Focus": {
        this.rootActor.focus(aMessage.data.node);
        break;
      }
      case "Update": {
        this.rootActor.update(aMessage.data.node);
        break;
      }
      case "Commit": {
        this.rootActor.commit(aMessage.data.node);
        break;
      }
      case "Clear": {
        if (this.browsingContext === this.browsingContext.top) {
          this.clear();
        }
        break;
      }
    }

    return null;
  }
}

const { debug, warn } = GeckoViewAutoFillParent.initLogging(
  "GeckoViewAutoFill"
);
