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
      firefoxSuggestLabelsEnabled: {
        type: "boolean",
        fallbackPref:
          "browser.urlbar.experimental.firefoxSuggestLabels.enabled",
        description:
          "Whether to show the Firefox Suggest label above the general group in the urlbar view",
      },
      quickSuggestEnabled: {
        type: "boolean",
        fallbackPref: "browser.urlbar.quicksuggest.enabled",
        description: "Global toggle for the QuickSuggest feature",
      },
      quickSuggestNonSponsoredIndex: {
        type: "int",
        fallbackPref: "browser.urlbar.quicksuggest.nonSponsoredIndex",
        description:
          "The index of non-sponsored QuickSuggest results within the general group. A negative index is relative to the end of the group",
      },
      quickSuggestShouldShowOnboardingDialog: {
        type: "boolean",
        fallbackPref: "browser.urlbar.quicksuggest.shouldShowOnboardingDialog",
        description:
          "Whether or not to show the QuickSuggest onboarding dialog",
      },
      quickSuggestShowOnboardingDialogAfterNRestarts: {
        type: "int",
        fallbackPref:
          "browser.urlbar.quicksuggest.showOnboardingDialogAfterNRestarts",
        description:
          "Show QuickSuggest onboarding dialog after N browser restarts",
      },
      quickSuggestSponsoredIndex: {
        type: "int",
        fallbackPref: "browser.urlbar.quicksuggest.sponsoredIndex",
        description:
          "The index of sponsored QuickSuggest results within the general group. A negative index is relative to the end of the group",
      },
    },
  },
  aboutwelcome: {
    description: "The about:welcome page",
    isEarlyStartup: true,
    variables: {
      enabled: {
        type: "boolean",
        fallbackPref: "browser.aboutwelcome.enabled",
      },
      screens: {
        type: "json",
        fallbackPref: "browser.aboutwelcome.screens",
      },
      isProton: {
        type: "boolean",
        fallbackPref: "browser.aboutwelcome.protonDesign",
      },
      skipFocus: {
        type: "boolean",
        fallbackPref: "browser.aboutwelcome.skipFocus",
      },
      transitions: {
        type: "boolean",
      },
    },
  },
  abouthomecache: {
    description: "The startup about:home cache.",
    isEarlyStartup: true,
    variables: {
      enabled: {
        type: "boolean",
        fallbackPref: "browser.startup.homepage.abouthome_cache.enabled",
      },
    },
  },
  newtab: {
    description: "The about:newtab page",
    isEarlyStartup: true,
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
    variables: {
      directMigrateSingleProfile: {
        type: "boolean",
      },
    },
  },
  upgradeDialog: {
    description: "The dialog shown for major upgrades",
    isEarlyStartup: true,
    variables: {
      enabled: {
        type: "boolean",
        fallbackPref: "browser.startup.upgradeDialog.enabled",
      },
    },
  },
  privatebrowsing: {
    description: "about:privatebrowsing",
    variables: {
      infoEnabled: {
        type: "boolean",
        fallbackPref: "browser.privatebrowsing.infoEnabled",
      },
      infoIcon: {
        type: "string",
        fallbackPref: "browser.privatebrowsing.infoIcon",
      },
      infoTitle: {
        type: "string",
        fallbackPref: "browser.privatebrowsing.infoTitle",
      },
      infoTitleEnabled: {
        type: "boolean",
        fallbackPref: "browser.privatebrowsing.infoTitleEnabled",
      },
      infoBody: {
        type: "string",
        fallbackPref: "browser.privatebrowsing.infoBody",
      },
      infoLinkText: {
        type: "string",
        fallbackPref: "browser.privatebrowsing.infoLinkText",
      },
      infoLinkUrl: {
        type: "string",
        fallbackPref: "browser.privatebrowsing.infoLinkUrl",
      },
      promoEnabled: {
        type: "boolean",
        fallbackPref: "browser.privatebrowsing.promoEnabled",
      },
      promoSectionStyle: {
        type: "string",
      },
      promoTitle: {
        type: "string",
        fallbackPref: "browser.privatebrowsing.promoTitle",
      },
      promoTitleEnabled: {
        type: "boolean",
        fallbackPref: "browser.privatebrowsing.promoTitleEnabled",
      },
      promoLinkText: {
        type: "string",
        fallbackPref: "browser.privatebrowsing.promoLinkText",
      },
      promoHeader: {
        type: "string",
      },
      promoLinkUrl: {
        type: "string",
        fallbackPref: "browser.privatebrowsing.promoLinkUrl",
      },
      promoLinkType: {
        type: "string",
      },
      promoImageLarge: {
        type: "string",
      },
      promoImageSmall: {
        type: "string",
      },
    },
  },
};
