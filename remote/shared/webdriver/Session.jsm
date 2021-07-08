/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["WebDriverSession"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  accessibility: "chrome://remote/content/marionette/accessibility.js",
  allowAllCerts: "chrome://remote/content/marionette/cert.js",
  Capabilities: "chrome://remote/content/shared/webdriver/Capabilities.jsm",
  clearActionInputState:
    "chrome://remote/content/marionette/actors/MarionetteCommandsChild.jsm",
  error: "chrome://remote/content/shared/webdriver/Errors.jsm",
  Log: "chrome://remote/content/shared/Log.jsm",
});

XPCOMUtils.defineLazyServiceGetter(
  this,
  "uuidGen",
  "@mozilla.org/uuid-generator;1",
  "nsIUUIDGenerator"
);

XPCOMUtils.defineLazyGetter(this, "logger", () => Log.get());

/** Representation of WebDriver session. */
class WebDriverSession {
  constructor(capabilities) {
    try {
      this.capabilities = Capabilities.fromJSON(capabilities);
    } catch (e) {
      throw new error.SessionNotCreatedError(e);
    }

    const uuid = uuidGen.generateUUID().toString();
    this.id = uuid.substring(1, uuid.length - 1);

    if (this.capabilities.get("acceptInsecureCerts")) {
      logger.warn("TLS certificate errors will be ignored for this session");
      allowAllCerts.enable();
    }

    if (this.proxy.init()) {
      logger.info(`Proxy settings initialised: ${JSON.stringify(this.proxy)}`);
    }

    // If we are testing accessibility with marionette, start a11y service in
    // chrome first. This will ensure that we do not have any content-only
    // services hanging around.
    if (this.a11yChecks && accessibility.service) {
      logger.info("Preemptively starting accessibility service in Chrome");
    }
  }

  destroy() {
    allowAllCerts.disable();

    clearActionInputState();
  }

  get a11yChecks() {
    return this.capabilities.get("moz:accessibilityChecks");
  }

  get pageLoadStrategy() {
    return this.capabilities.get("pageLoadStrategy");
  }

  get proxy() {
    return this.capabilities.get("proxy");
  }

  get strictFileInteractability() {
    return this.capabilities.get("strictFileInteractability");
  }

  get timeouts() {
    return this.capabilities.get("timeouts");
  }

  set timeouts(timeouts) {
    this.capabilities.set("timeouts", timeouts);
  }

  get unhandledPromptBehavior() {
    return this.capabilities.get("unhandledPromptBehavior");
  }
}
