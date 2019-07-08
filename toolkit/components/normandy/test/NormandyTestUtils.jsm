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

const NormandyTestUtils = {
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

      return Object.assign(
        {
          name: `Test study ${_preferenceStudyFactoryId++}`,
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
};
