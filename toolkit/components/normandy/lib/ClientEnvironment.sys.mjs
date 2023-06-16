/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { ClientEnvironmentBase } from "resource://gre/modules/components-utils/ClientEnvironment.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AddonRollouts: "resource://normandy/lib/AddonRollouts.sys.mjs",
  AddonStudies: "resource://normandy/lib/AddonStudies.sys.mjs",
  NormandyApi: "resource://normandy/lib/NormandyApi.sys.mjs",
  PreferenceExperiments:
    "resource://normandy/lib/PreferenceExperiments.sys.mjs",
  PreferenceRollouts: "resource://normandy/lib/PreferenceRollouts.sys.mjs",
});

// Cached API request for client attributes that are determined by the Normandy
// service.
let _classifyRequest = null;

export class ClientEnvironment extends ClientEnvironmentBase {
  /**
   * Fetches information about the client that is calculated on the server,
   * like geolocation and the current time.
   *
   * The server request is made lazily and is cached for the entire browser
   * session.
   */
  static async getClientClassification() {
    if (!_classifyRequest) {
      _classifyRequest = lazy.NormandyApi.classifyClient();
    }
    return _classifyRequest;
  }

  static clearClassifyCache() {
    _classifyRequest = null;
  }

  /**
   * Test wrapper that mocks the server request for classifying the client.
   * @param  {Object}   data          Fake server data to use
   * @param  {Function} testFunction  Test function to execute while mock data is in effect.
   */
  static withMockClassify(data, testFunction) {
    return async function inner() {
      const oldRequest = _classifyRequest;
      _classifyRequest = Promise.resolve(data);
      await testFunction();
      _classifyRequest = oldRequest;
    };
  }

  static get userId() {
    return ClientEnvironment.randomizationId;
  }

  static get country() {
    return (async () => {
      const { country } = await ClientEnvironment.getClientClassification();
      return country;
    })();
  }

  static get request_time() {
    return (async () => {
      const { request_time } =
        await ClientEnvironment.getClientClassification();
      return request_time;
    })();
  }

  static get experiments() {
    return (async () => {
      const names = { all: [], active: [], expired: [] };

      for (const {
        slug,
        expired,
      } of await lazy.PreferenceExperiments.getAll()) {
        names.all.push(slug);
        if (expired) {
          names.expired.push(slug);
        } else {
          names.active.push(slug);
        }
      }

      return names;
    })();
  }

  static get studies() {
    return (async () => {
      const rv = { pref: {}, addon: {} };
      for (const prefStudy of await lazy.PreferenceExperiments.getAll()) {
        rv.pref[prefStudy.slug] = prefStudy;
      }
      for (const addonStudy of await lazy.AddonStudies.getAll()) {
        rv.addon[addonStudy.slug] = addonStudy;
      }
      return rv;
    })();
  }

  static get rollouts() {
    return (async () => {
      const rv = { pref: {}, addon: {} };
      for (const prefRollout of await lazy.PreferenceRollouts.getAll()) {
        rv.pref[prefRollout.slug] = prefRollout;
      }
      for (const addonRollout of await lazy.AddonRollouts.getAll()) {
        rv.addon[addonRollout.slug] = addonRollout;
      }
      return rv;
    })();
  }

  static get isFirstRun() {
    return Services.prefs.getBoolPref("app.normandy.first_run", true);
  }
}
