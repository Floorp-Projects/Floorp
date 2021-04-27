/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * FEATURE MANIFEST
 * =================
 * Features must be added here to be accessible through the NimbusFeature API.
 */

const EXPORTED_SYMBOLS = ["FeatureManifest"];

const FeatureManifest = {
  urlbar: {
    description: "The Address Bar",
    variables: {
      quickSuggestEnabled: {
        type: "boolean",
        fallbackPref: "browser.urlbar.quicksuggest.enabled",
      },
    },
  },
  aboutwelcome: {
    description: "The about:welcome page",
    enabledFallbackPref: "browser.aboutwelcome.enabled",
    variables: {
      screens: {
        type: "json",
        fallbackPref: "browser.aboutwelcome.screens",
      },
      isProton: {
        type: "boolean",
        fallbackPref: "browser.proton.enabled",
      },
      skipFocus: {
        type: "boolean",
        fallbackPref: "browser.aboutwelcome.skipFocus",
      },
    },
  },
  newtab: {
    description: "The about:newtab page",
    variables: {
      newNewtabExperienceEnabled: {
        type: "boolean",
        fallbackPref:
          "browser.newtabpage.activity-stream.newNewtabExperience.enabled",
      },
      customizationMenuEnabled: {
        type: "boolean",
        fallbackPref:
          "browser.newtabpage.activity-stream.customizationMenu.enabled",
      },
      prefsButtonIcon: {
        type: "string",
      },
    },
  },
  "password-autocomplete": {
    description: "A special autocomplete UI for password fields.",
  },
  upgradeDialog: {
    description: "The dialog shown for major upgrades",
    enabledFallbackPref: "browser.startup.upgradeDialog.enabled",
  },
};
