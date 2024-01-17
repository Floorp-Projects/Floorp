// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  JsonSchema: "resource://gre/modules/JsonSchema.sys.mjs",
  RemoteSettings: "resource://services-settings/remote-settings.sys.mjs",
});
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

ChromeUtils.defineLazyGetter(lazy, "logConsole", () => {
  return console.createInstance({
    prefix: "FingerprintingWebCompatService",
    maxLogLevelPref:
      "privacy.fingerprintingProtection.WebCompatService.logLevel",
  });
});

const SCHEMA = `{
  "type": "object",
  "title": "Fingerprinting Overrides",
  "$schema": "http://json-schema.org/draft-07/schema#",
  "required": [
    "firstPartyDomain",
    "overrides"
  ],
  "properties": {
    "overrides": {
      "type": "string",
      "pattern": "^[+-][A-Za-z]+(?:,[+-][A-Za-z]+)*$",
      "description": "The fingerprinting overrides. See https://searchfox.org/mozilla-central/source/toolkit/components/resistfingerprinting/RFPTargets.inc for details."
    },
    "firstPartyDomain": {
      "type": "string",
      "pattern": "^(\\*|(?!-)[A-Za-z0-9-]{1,63}(?<!-)\\.)+[A-Za-z]{2,6}$",
      "description": "The first-party domain associated with the override. Use '*' to match all domains. Only legit domains allowed."
    },
    "thirdPartyDomain": {
      "type": "string",
      "pattern": "^(\\*|(?!-)[A-Za-z0-9-]{1,63}(?<!-)\\.)+[A-Za-z]{2,6}$",
      "description": "The third-party domain associated with the override. Use '*' to match all domains. Only legit domains allowed. Leave this field empty if the override is only for the first-party context."
    }
  }
}`;

const COLLECTION_NAME = "fingerprinting-protection-overrides";
const PREF_GRANULAR_OVERRIDES =
  "privacy.fingerprintingProtection.granularOverrides";

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "granularOverridesPref",
  PREF_GRANULAR_OVERRIDES
);

/**
 * The object represents a fingerprinting override.
 */
export class FingerprintingOverride {
  classID = Components.ID("{07f45442-1806-44be-9230-12eb79de9bac}");
  QueryInterface = ChromeUtils.generateQI(["nsIFingerprintingOverride"]);

  constructor(firstPartyDomain, thirdPartyDomain, overrides) {
    this.firstPartyDomain = firstPartyDomain;
    this.thirdPartyDomain = thirdPartyDomain;
    this.overrides = overrides;
  }
}

/**
 * The singleton service that is responsible for the WebCompat of the
 * fingerprinting protection. It gets fingerprinting overrides from remote
 * settings and the local test pref.
 */
export class FingerprintingWebCompatService {
  classId = Components.ID("{e7b1da06-2594-4670-aea4-131070baca4c}");
  QueryInterface = ChromeUtils.generateQI([
    "nsIFingerprintingWebCompatService",
  ]);
  #initialized = false;
  #remoteOverrides;
  #granularOverrides;
  #rs;
  #validator;

  #isParentProcess;

  constructor() {
    this.#isParentProcess =
      Services.appinfo.processType === Services.appinfo.PROCESS_TYPE_DEFAULT;

    this.#remoteOverrides = new Set();
    this.#granularOverrides = new Set();

    this.#rs = lazy.RemoteSettings(COLLECTION_NAME);
    this.#validator = new lazy.JsonSchema.Validator(SCHEMA);
  }

  async init() {
    lazy.logConsole.debug("init");
    // We can only access remote settings from the parent process. So, never
    // init it in the content process.
    if (!this.#isParentProcess) {
      throw new Error(
        "Shouldn't init FingerprintingWebCompatService in content processes."
      );
    }

    // Return if we have initiated.
    if (this.#initialized) {
      return;
    }
    this.#initialized = true;

    // Register listener to import overrides when the overrides pref changes.
    Services.prefs.addObserver(PREF_GRANULAR_OVERRIDES, this);

    // Register the sync event for the remote settings updates.
    this.#rs.on("sync", event => {
      let {
        data: { current },
      } = event;
      this.#onRemoteUpdate(current);

      this.#populateOverrides();
    });

    // Get the remote overrides from the remote settings.
    await this.#importRemoteSettingsOverrides();

    // Get the granular overrides from the pref.
    this.#importPrefOverrides();

    // Populate the overrides to the nsRFPService.
    this.#populateOverrides();

    lazy.logConsole.debug("Init completes");
  }

  // Import fingerprinting overrides from the local granular pref.
  #importPrefOverrides() {
    lazy.logConsole.debug("importLocalGranularOverrides");

    // Clear overrides before we update.
    this.#granularOverrides.clear();

    let overrides;
    try {
      overrides = JSON.parse(lazy.granularOverridesPref || "[]");
    } catch (error) {
      lazy.logConsole.error(
        `Failed to parse granular override JSON string: Not a valid JSON.`,
        error
      );
      return;
    }

    // Ensure we have an array we can iterate over and not an object.
    if (!Array.isArray(overrides)) {
      lazy.logConsole.error(
        "Failed to parse granular overrides JSON String: Not an array."
      );
      return;
    }

    for (let override of overrides) {
      // Validate the override.
      let { valid, errors } = this.#validator.validate(override);

      if (!valid) {
        lazy.logConsole.debug("Override validation error", override, errors);
        continue;
      }

      this.#granularOverrides.add(
        this.#createFingerprintingOverrideFrom(override)
      );
    }
  }

  // Import fingerprinting overrides from the remote settings.
  async #importRemoteSettingsOverrides() {
    lazy.logConsole.debug("importRemoteSettingsOverrides");

    let entries;
    try {
      entries = await this.#rs.get();
    } catch (e) {}

    this.#onRemoteUpdate(entries || []);
  }

  #onRemoteUpdate(entries) {
    lazy.logConsole.debug("onUpdateEntries", { entries });
    // Clear all overrides before we update the overrides.
    this.#remoteOverrides.clear();

    for (let entry of entries) {
      this.#remoteOverrides.add(this.#createFingerprintingOverrideFrom(entry));
    }
  }

  #createFingerprintingOverrideFrom(entry) {
    return new FingerprintingOverride(
      entry.firstPartyDomain,
      entry.thirdPartyDomain,
      entry.overrides
    );
  }

  #populateOverrides() {
    lazy.logConsole.debug("populateOverrides");

    // Create the array that contains all overrides. We explicitly concat the
    // overrides from testing pref after the ones from remote settings to ensure
    // that the testing pref will take precedence.
    let overrides = Array.from(this.#remoteOverrides).concat(
      Array.from(this.#granularOverrides)
    );

    // Set the remote override to the RFP service.
    Services.rfp.setFingerprintingOverrides(Array.from(overrides));
  }

  observe(subject, topic, prefName) {
    if (prefName != PREF_GRANULAR_OVERRIDES) {
      return;
    }

    this.#importPrefOverrides();
    this.#populateOverrides();
  }

  shutdown() {
    lazy.logConsole.debug("shutdown");

    Services.prefs.removeObserver(PREF_GRANULAR_OVERRIDES, this);
  }
}
