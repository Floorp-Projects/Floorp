// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  HiddenFrame: "resource://gre/modules/HiddenFrame.sys.mjs",
  Preferences: "resource://gre/modules/Preferences.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "console", () => {
  return console.createInstance({
    prefix: "UserCharacteristicsPage",
    maxLogLevelPref: "toolkit.telemetry.user_characteristics_ping.logLevel",
  });
});

ChromeUtils.defineLazyGetter(lazy, "contentPrefs", () => {
  return Cc["@mozilla.org/content-pref/service;1"].getService(
    Ci.nsIContentPrefService2
  );
});

const BACKGROUND_WIDTH = 1024;
const BACKGROUND_HEIGHT = 768;

/**
 * A manager for hidden browsers. Responsible for creating and destroying a
 * hidden frame to hold them.
 * All of this is copied from PageDataService.sys.mjs
 */
class HiddenBrowserManager {
  /**
   * The hidden frame if one has been created.
   *
   * @type {HiddenFrame | null}
   */
  #frame = null;
  /**
   * The number of hidden browser elements currently in use.
   *
   * @type {number}
   */
  #browsers = 0;

  /**
   * Creates and returns a new hidden browser.
   *
   * @returns {Browser}
   */
  async #acquireBrowser() {
    this.#browsers++;
    if (!this.#frame) {
      this.#frame = new lazy.HiddenFrame();
    }

    let frame = await this.#frame.get();
    let doc = frame.document;
    let browser = doc.createXULElement("browser");
    browser.setAttribute("remote", "true");
    browser.setAttribute("type", "content");
    browser.setAttribute(
      "style",
      `
        width: ${BACKGROUND_WIDTH}px;
        min-width: ${BACKGROUND_WIDTH}px;
        height: ${BACKGROUND_HEIGHT}px;
        min-height: ${BACKGROUND_HEIGHT}px;
      `
    );
    browser.setAttribute("maychangeremoteness", "true");
    doc.documentElement.appendChild(browser);

    return browser;
  }

  /**
   * Releases the given hidden browser.
   *
   * @param {Browser} browser
   *   The hidden browser element.
   */
  #releaseBrowser(browser) {
    browser.remove();

    this.#browsers--;
    if (this.#browsers == 0) {
      this.#frame.destroy();
      this.#frame = null;
    }
  }

  /**
   * Calls a callback function with a new hidden browser.
   * This function will return whatever the callback function returns.
   *
   * @param {Callback} callback
   *   The callback function will be called with the browser element and may
   *   be asynchronous.
   * @returns {T}
   */
  async withHiddenBrowser(callback) {
    let browser = await this.#acquireBrowser();
    try {
      return await callback(browser);
    } finally {
      this.#releaseBrowser(browser);
    }
  }
}

export class UserCharacteristicsPageService {
  classId = Components.ID("{ce3e9659-e311-49fb-b18b-7f27c6659b23}");
  QueryInterface = ChromeUtils.generateQI([
    "nsIUserCharacteristicsPageService",
  ]);

  _initialized = false;
  _isParentProcess = false;

  /**
   * A manager for hidden browsers.
   *
   * @type {HiddenBrowserManager}
   */
  _browserManager = new HiddenBrowserManager();

  /**
   * A map of hidden browsers to a resolve function that should be passed the
   * actor that was created for the browser.
   *
   * @type {WeakMap<Browser, function(PageDataParent): void>}
   */
  _backgroundBrowsers = new WeakMap();

  constructor() {
    lazy.console.debug("Init");

    if (
      Services.appinfo.processType !== Services.appinfo.PROCESS_TYPE_DEFAULT
    ) {
      throw new Error(
        "Shouldn't init UserCharacteristicsPage in content processes."
      );
    }

    // Return if we have initiated.
    if (this._initialized) {
      lazy.console.warn("preventing re-initilization...");
      return;
    }
    this._initialized = true;
  }

  shutdown() {}

  createContentPage() {
    lazy.console.debug("called createContentPage");
    return this._browserManager.withHiddenBrowser(async browser => {
      lazy.console.debug(`In withHiddenBrowser`);
      try {
        let { promise, resolve } = Promise.withResolvers();
        this._backgroundBrowsers.set(browser, resolve);

        let principal = Services.scriptSecurityManager.getSystemPrincipal();
        let loadURIOptions = {
          triggeringPrincipal: principal,
        };

        let userCharacteristicsPageURI = Services.io.newURI(
          "about:fingerprintingprotection"
        );

        browser.loadURI(userCharacteristicsPageURI, loadURIOptions);

        let data = await promise;
        if (data.debug) {
          lazy.console.debug(`Debugging Output:`);
          for (let line of data.debug) {
            lazy.console.debug(line);
          }
          lazy.console.debug(`(debugging output done)`);
        }
        lazy.console.debug(`Data:`, data.output);

        lazy.console.debug("Populating Glean metrics...");

        for (let gamepad of data.output.gamepads) {
          Glean.characteristics.gamepads.add(gamepad);
        }

        Glean.characteristics.zoomCount.set(await this.populateZoomPrefs());

        try {
          Glean.characteristics.canvasdata1.set(data.output.canvas1data);
          Glean.characteristics.canvasdata2.set(data.output.canvas2data);
          Glean.characteristics.canvasdata3.set(data.output.canvas3data);
          Glean.characteristics.canvasdata4.set(data.output.canvas4data);
          Glean.characteristics.canvasdata5.set(data.output.canvas5data);
          Glean.characteristics.canvasdata6.set(data.output.canvas6data);
          Glean.characteristics.canvasdata7.set(data.output.canvas7data);
          Glean.characteristics.canvasdata8.set(data.output.canvas8data);
          Glean.characteristics.canvasdata9.set(data.output.canvas9data);
          Glean.characteristics.canvasdata10.set(data.output.canvas10data);
          Glean.characteristics.canvasdata11Webgl.set(data.output.glcanvasdata);
          Glean.characteristics.canvasdata12Fingerprintjs1.set(
            data.output.fingerprintjscanvas1data
          );
          Glean.characteristics.canvasdata13Fingerprintjs2.set(
            data.output.fingerprintjscanvas2data
          );
          Glean.characteristics.voices.set(data.output.voices);
          Glean.characteristics.mediaCapabilities.set(
            data.output.mediaCapabilities
          );
          this.populateDisabledMediaPrefs();
        } catch (e) {
          // Grab the exception and send it to the console
          // (we don't see it otherwise)
          lazy.console.debug(e);
          // But still fail
          throw e;
        }

        lazy.console.debug("Unregistering actor");
        Services.obs.notifyObservers(
          null,
          "user-characteristics-populating-data-done"
        );
      } finally {
        this._backgroundBrowsers.delete(browser);
      }
    });
  }

  async populateZoomPrefs() {
    const zoomPrefsCount = await new Promise(resolve => {
      lazy.contentPrefs.getByName("browser.content.full-zoom", null, {
        _result: 0,
        handleResult(_) {
          this._result++;
        },
        handleCompletion() {
          resolve(this._result);
        },
      });
    });

    return zoomPrefsCount;
  }

  async pageLoaded(browsingContext, data) {
    lazy.console.debug(
      `pageLoaded browsingContext=${browsingContext} data=${data}`
    );

    let browser = browsingContext.embedderElement;

    let backgroundResolve = this._backgroundBrowsers.get(browser);
    if (backgroundResolve) {
      backgroundResolve(data);
      return;
    }
    throw new Error(`No backround resolve for ${browser} found`);
  }

  async populateDisabledMediaPrefs() {
    const PREFS = [
      "media.wave.enabled",
      "media.ogg.enabled",
      "media.opus.enabled",
      "media.mp4.enabled",
      "media.wmf.hevc.enabled",
      "media.webm.enabled",
      "media.av1.enabled",
      "media.encoder.webm.enabled",
      "media.mediasource.enabled",
      "media.mediasource.mp4.enabled",
      "media.mediasource.webm.enabled",
      "media.mediasource.vp9.enabled",
    ];

    const defaultPrefs = new lazy.Preferences({ defaultBranch: true });
    const changedPrefs = {};
    for (const pref of PREFS) {
      const value = lazy.Preferences.get(pref);
      if (lazy.Preferences.isSet(pref) && defaultPrefs.get(pref) !== value) {
        const key = pref.substring(6).substring(0, pref.length - 8 - 6);
        changedPrefs[key] = value;
      }
    }
    Glean.characteristics.changedMediaPrefs.set(JSON.stringify(changedPrefs));
  }
}
