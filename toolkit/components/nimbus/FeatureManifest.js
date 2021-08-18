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
        description:
          "Should users see about:welcome? If this is false, users will see a regular new tab instead.",
      },
      screens: {
        type: "json",
        fallbackPref: "browser.aboutwelcome.screens",
        description: "Content to show in the onboarding flow",
      },
      skipFocus: {
        type: "boolean",
        fallbackPref: "browser.aboutwelcome.skipFocus",
        description:
          "Should the urlbar should be focused when users first land on about:welcome?",
      },
      transitions: {
        type: "boolean",
        description: "Enable transition effect between screens",
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
        description: "Is the feature enabled?",
      },
    },
  },
  firefox100: {
    description: "Firefox User-Agent version",
    isEarlyStartup: true,
    variables: {
      firefoxVersion: {
        type: "int",
        description: "Firefox version to spoof (or `0` to use default version)",
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
        description: "Is the new UI enabled?",
      },
      customizationMenuEnabled: {
        type: "boolean",
        fallbackPref:
          "browser.newtabpage.activity-stream.customizationMenu.enabled",
        description: "Enable the customization panel inside of the newtab",
      },
      prefsButtonIcon: {
        type: "string",
        description: "Icon url to use for the preferences button",
      },
    },
  },
  pocketNewtab: {
    description: "The Pocket section in newtab",
    isEarlyStartup: true,
    variables: {
      spocPositions: {
        type: "string",
        fallbackPref:
          "browser.newtabpage.activity-stream.discoverystream.spoc-positions",
        description: "CSV string of spoc position indexes on newtab grid",
      },
    },
  },
  "password-autocomplete": {
    description: "A special autocomplete UI for password fields.",
    variables: {
      directMigrateSingleProfile: {
        type: "boolean",
        description: "Enable direct migration?",
      },
    },
  },
  shellService: {
    description: "Interface with OS, e.g., pinning and set default",
    isEarlyStartup: true,
    variables: {
      disablePin: {
        type: "boolean",
        description: "Disable pin to taskbar feature",
      },
      setDefaultBrowserUserChoice: {
        type: "boolean",
        fallbackPref: "browser.shell.setDefaultBrowserUserChoice",
        description: "Should it set as default browser",
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
        description: "Is the feature enabled?",
      },
    },
  },
  privatebrowsing: {
    description: "about:privatebrowsing",
    variables: {
      infoEnabled: {
        type: "boolean",
        fallbackPref: "browser.privatebrowsing.infoEnabled",
        description: "Should we show the info section.",
      },
      infoIcon: {
        type: "string",
        fallbackPref: "browser.privatebrowsing.infoIcon",
        description:
          "Icon shown in the left side of the info section. Default is the private browsing icon.",
      },
      infoTitle: {
        type: "string",
        fallbackPref: "browser.privatebrowsing.infoTitle",
        description: "Is the title in the info section enabled.",
      },
      infoTitleEnabled: {
        type: "boolean",
        fallbackPref: "browser.privatebrowsing.infoTitleEnabled",
        description: "Is the title in the info section enabled.",
      },
      infoBody: {
        type: "string",
        fallbackPref: "browser.privatebrowsing.infoBody",
        description: "Text content in the info section.",
      },
      infoLinkText: {
        type: "string",
        fallbackPref: "browser.privatebrowsing.infoLinkText",
        description: "Text for the link in the info section.",
      },
      infoLinkUrl: {
        type: "string",
        fallbackPref: "browser.privatebrowsing.infoLinkUrl",
        description: "URL for the info section link.",
      },
      promoEnabled: {
        type: "boolean",
        fallbackPref: "browser.privatebrowsing.promoEnabled",
        description: "Should we show the promo section.",
      },
      promoSectionStyle: {
        type: "string",
        description:
          "Sets the position of the promo section. Possible values are: top, bottom. Default bottom.",
      },
      promoTitle: {
        type: "string",
        fallbackPref: "browser.privatebrowsing.promoTitle",
        description: "The text content of the promo section.",
      },
      promoTitleEnabled: {
        type: "boolean",
        fallbackPref: "browser.privatebrowsing.promoTitleEnabled",
        description: "Should we show text content in the promo section.",
      },
      promoLinkText: {
        type: "string",
        fallbackPref: "browser.privatebrowsing.promoLinkText",
        description: "The text of the link in the promo box.",
      },
      promoLinkUrl: {
        type: "string",
        fallbackPref: "browser.privatebrowsing.promoLinkUrl",
        description: "URL for link in the promo box.",
      },
      promoLinkType: {
        type: "string",
        description:
          "Type of promo link type. Possible values: link, button. Default is button.",
      },
    },
  },
  readerMode: {
    description: "Firefox Reader Mode",
    isEarlyStartup: true,
    variables: {
      pocketCTAVersion: {
        type: "string",
        fallbackPref: "reader.pocket.ctaVersion",
        description:
          "What version of Pocket CTA to show in Reader Mode (Empty string is no CTA)",
      },
    },
  },
};
