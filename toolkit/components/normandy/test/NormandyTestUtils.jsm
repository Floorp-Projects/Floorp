/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

ChromeUtils.import("resource://normandy/lib/AddonStudies.jsm", this);

const FIXTURE_ADDON_ID = "normandydriver-a@example.com";

var EXPORTED_SYMBOLS = ["NormandyTestUtils"];

// Factory IDs
let _addonStudyFactoryId = 0;
let _preferenceStudyFactoryId = 0;

let testGlobals = {};

const NormandyTestUtils = {
  init({ add_task } = {}) {
    testGlobals.add_task = add_task;
  },

  factories: {
    addonStudyFactory(attrs) {
      for (const key of ["name", "description"]) {
        if (attrs && attrs[key]) {
          throw new Error(
            `${key} is no longer a valid key for addon studies, please update to v2 study schema`
          );
        }
      }

      return Object.assign(
        {
          recipeId: _addonStudyFactoryId++,
          slug: "test-study",
          userFacingName: "Test study",
          userFacingDescription: "test description",
          branch: AddonStudies.NO_BRANCHES_MARKER,
          active: true,
          addonId: FIXTURE_ADDON_ID,
          addonUrl: "http://test/addon.xpi",
          addonVersion: "1.0.0",
          studyStartDate: new Date(),
          studyEndDate: null,
          extensionApiId: 1,
          extensionHash:
            "ade1c14196ec4fe0aa0a6ba40ac433d7c8d1ec985581a8a94d43dc58991b5171",
          extensionHashAlgorithm: "sha256",
        },
        attrs
      );
    },

    branchedAddonStudyFactory(attrs) {
      return NormandyTestUtils.factories.addonStudyFactory(
        Object.assign(
          {
            branch: "a",
          },
          attrs
        )
      );
    },

    preferenceStudyFactory(attrs) {
      const defaultPref = {
        "test.study": {},
      };
      const defaultPrefInfo = {
        preferenceValue: false,
        preferenceType: "boolean",
        previousPreferenceValue: undefined,
        preferenceBranchType: "default",
      };
      const preferences = {};
      for (const [prefName, prefInfo] of Object.entries(
        attrs.preferences || defaultPref
      )) {
        preferences[prefName] = { ...defaultPrefInfo, ...prefInfo };
      }

      // Generate a slug from userFacingName
      let {
        userFacingName = `Test study ${_preferenceStudyFactoryId++}`,
        slug,
      } = attrs;
      delete attrs.slug;
      if (userFacingName && !slug) {
        slug = userFacingName.replace(" ", "-").toLowerCase();
      }

      return Object.assign(
        {
          userFacingName,
          slug,
          branch: "control",
          expired: false,
          lastSeen: new Date().toJSON(),
          experimentType: "exp",
        },
        attrs,
        {
          preferences,
        }
      );
    },
  },

  /**
   * Combine a list of functions right to left. The rightmost function is passed
   * to the preceding function as the argument; the result of this is passed to
   * the next function until all are exhausted. For example, this:
   *
   * decorate(func1, func2, func3);
   *
   * is equivalent to this:
   *
   * func1(func2(func3));
   */
  decorate(...args) {
    const funcs = Array.from(args);
    let decorated = funcs.pop();
    const origName = decorated.name;
    funcs.reverse();
    for (const func of funcs) {
      decorated = func(decorated);
    }
    Object.defineProperty(decorated, "name", { value: origName });
    return decorated;
  },

  /**
   * Wrapper around add_task for declaring tests that use several with-style
   * wrappers. The last argument should be your test function; all other arguments
   * should be functions that accept a single test function argument.
   *
   * The arguments are combined using decorate and passed to add_task as a single
   * test function.
   *
   * @param {[Function]} args
   * @example
   *   decorate_task(
   *     withMockPreferences,
   *     withMockNormandyApi,
   *     async function myTest(mockPreferences, mockApi) {
   *       // Do a test
   *     }
   *   );
   */
  decorate_task(...args) {
    return testGlobals.add_task(NormandyTestUtils.decorate(...args));
  },
};
