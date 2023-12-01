/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This module handles JavaScript-implemented JSWindowActors, registered through DOM IPC
 * infrastructure, and are fission-compatible.
 */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

/**
 * Fission-compatible JSProcess implementations.
 * Each actor options object takes the form of a ProcessActorOptions dictionary.
 * Detailed documentation of these options is in dom/docs/ipc/jsactors.rst,
 * available at https://firefox-source-docs.mozilla.org/dom/ipc/jsactors.html
 */
let JSPROCESSACTORS = {
  AsyncPrefs: {
    parent: {
      esModuleURI: "resource://gre/modules/AsyncPrefs.sys.mjs",
    },
    child: {
      esModuleURI: "resource://gre/modules/AsyncPrefs.sys.mjs",
    },
  },

  ContentPrefs: {
    parent: {
      esModuleURI: "resource://gre/modules/ContentPrefServiceParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource://gre/modules/ContentPrefServiceChild.sys.mjs",
    },
  },

  ExtensionContent: {
    child: {
      esModuleURI: "resource://gre/modules/ExtensionContent.sys.mjs",
    },
    includeParent: true,
  },

  ProcessConduits: {
    parent: {
      esModuleURI: "resource://gre/modules/ConduitsParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource://gre/modules/ConduitsChild.sys.mjs",
    },
  },
};

/**
 * Fission-compatible JSWindowActor implementations.
 * Each actor options object takes the form of a WindowActorOptions dictionary.
 * Detailed documentation of these options is in dom/docs/ipc/jsactors.rst,
 * available at https://firefox-source-docs.mozilla.org/dom/ipc/jsactors.html
 */
let JSWINDOWACTORS = {
  AboutCertViewer: {
    parent: {
      esModuleURI: "resource://gre/modules/AboutCertViewerParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource://gre/modules/AboutCertViewerChild.sys.mjs",

      events: {
        DOMDocElementInserted: { capture: true },
      },
    },

    matches: ["about:certificate"],
  },

  AboutHttpsOnlyError: {
    parent: {
      esModuleURI: "resource://gre/actors/AboutHttpsOnlyErrorParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource://gre/actors/AboutHttpsOnlyErrorChild.sys.mjs",
      events: {
        DOMDocElementInserted: {},
      },
    },
    matches: ["about:httpsonlyerror?*"],
    allFrames: true,
  },

  AboutTranslations: {
    parent: {
      esModuleURI: "resource://gre/actors/AboutTranslationsParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource://gre/actors/AboutTranslationsChild.sys.mjs",
      events: {
        // Run the actor before any content of the page appears to inject functions.
        DOMDocElementInserted: {},
        DOMContentLoaded: {},
        // Used to show and hide the translations button.
        pageshow: { mozSystemGroup: true },
        pagehide: { mozSystemGroup: true },
      },
    },
    matches: ["about:translations"],
    remoteTypes: ["privilegedabout"],
    enablePreference: "browser.translations.enable",
  },

  AudioPlayback: {
    parent: {
      esModuleURI: "resource://gre/actors/AudioPlaybackParent.sys.mjs",
    },

    child: {
      esModuleURI: "resource://gre/actors/AudioPlaybackChild.sys.mjs",
      observers: ["audio-playback"],
    },

    allFrames: true,
  },

  AutoComplete: {
    parent: {
      esModuleURI: "resource://gre/actors/AutoCompleteParent.sys.mjs",
      // These two messages are also used, but are currently synchronous calls
      // through the per-process message manager.
      // "FormAutoComplete:GetSelectedIndex",
      // "FormAutoComplete:SelectBy"
    },

    child: {
      esModuleURI: "resource://gre/actors/AutoCompleteChild.sys.mjs",
    },

    allFrames: true,
  },

  Autoplay: {
    parent: {
      esModuleURI: "resource://gre/actors/AutoplayParent.sys.mjs",
    },

    child: {
      esModuleURI: "resource://gre/actors/AutoplayChild.sys.mjs",
      events: {
        GloballyAutoplayBlocked: {},
      },
    },

    allFrames: true,
  },

  AutoScroll: {
    parent: {
      esModuleURI: "resource://gre/actors/AutoScrollParent.sys.mjs",
    },

    child: {
      esModuleURI: "resource://gre/actors/AutoScrollChild.sys.mjs",
      events: {
        mousedown: { capture: true, mozSystemGroup: true },
      },
    },

    allFrames: true,
  },

  BackgroundThumbnails: {
    child: {
      esModuleURI: "resource://gre/actors/BackgroundThumbnailsChild.sys.mjs",
      events: {
        DOMDocElementInserted: { capture: true },
      },
    },
    messageManagerGroups: ["thumbnails"],
  },

  BrowserElement: {
    parent: {
      esModuleURI: "resource://gre/actors/BrowserElementParent.sys.mjs",
    },

    child: {
      esModuleURI: "resource://gre/actors/BrowserElementChild.sys.mjs",
      events: {
        DOMWindowClose: {},
      },
    },

    allFrames: true,
  },

  Conduits: {
    parent: {
      esModuleURI: "resource://gre/modules/ConduitsParent.sys.mjs",
    },

    child: {
      esModuleURI: "resource://gre/modules/ConduitsChild.sys.mjs",
    },

    allFrames: true,
  },

  Controllers: {
    parent: {
      esModuleURI: "resource://gre/actors/ControllersParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource://gre/actors/ControllersChild.sys.mjs",
    },

    allFrames: true,
  },

  CookieBanner: {
    parent: {
      esModuleURI: "resource://gre/actors/CookieBannerParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource://gre/actors/CookieBannerChild.sys.mjs",
      events: {
        DOMContentLoaded: {},
        load: { capture: true },
      },
    },
    // Only need handle cookie banners for HTTP/S scheme.
    matches: ["https://*/*", "http://*/*"],
    // Only handle banners for browser tabs (including sub-frames).
    messageManagerGroups: ["browsers"],
    // Cookie banners can be shown in sub-frames so we need to include them.
    allFrames: true,
    // Holds lazy pref getters.
    _prefs: {},
    // Remember current register state to avoid duplicate calls to register /
    // unregister.
    _isRegistered: false,
    onAddActor(register, unregister) {
      // Register / unregister on pref changes.
      let onPrefChange = () => {
        if (
          this._prefs["cookiebanners.bannerClicking.enabled"] &&
          (this._prefs["cookiebanners.service.mode"] != 0 ||
            this._prefs["cookiebanners.service.mode.privateBrowsing"] != 0)
        ) {
          if (!this._isRegistered) {
            register();
            this._isRegistered = true;
          }
        } else if (this._isRegistered) {
          unregister();
          this._isRegistered = false;
        }
      };

      // Add lazy pref getters with pref observers so we can dynamically enable
      // or disable the actor.
      [
        "cookiebanners.bannerClicking.enabled",
        "cookiebanners.service.mode",
        "cookiebanners.service.mode.privateBrowsing",
      ].forEach(prefName => {
        XPCOMUtils.defineLazyPreferenceGetter(
          this._prefs,
          prefName,
          prefName,
          null,
          onPrefChange
        );
      });

      // Check initial state.
      onPrefChange();
    },
  },

  ExtFind: {
    child: {
      esModuleURI: "resource://gre/actors/ExtFindChild.sys.mjs",
    },

    allFrames: true,
  },

  FindBar: {
    parent: {
      esModuleURI: "resource://gre/actors/FindBarParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource://gre/actors/FindBarChild.sys.mjs",
      events: {
        keypress: { mozSystemGroup: true },
      },
    },

    allFrames: true,
    messageManagerGroups: ["browsers", "test"],
  },

  // This is the actor that responds to requests from the find toolbar and
  // searches for matches and highlights them.
  Finder: {
    child: {
      esModuleURI: "resource://gre/actors/FinderChild.sys.mjs",
    },

    allFrames: true,
  },

  FormHistory: {
    parent: {
      esModuleURI: "resource://gre/actors/FormHistoryParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource://gre/actors/FormHistoryChild.sys.mjs",
      events: {
        DOMFormBeforeSubmit: {},
      },
    },

    allFrames: true,
  },

  InlineSpellChecker: {
    parent: {
      esModuleURI: "resource://gre/actors/InlineSpellCheckerParent.sys.mjs",
    },

    child: {
      esModuleURI: "resource://gre/actors/InlineSpellCheckerChild.sys.mjs",
    },

    allFrames: true,
  },

  KeyPressEventModelChecker: {
    child: {
      esModuleURI:
        "resource://gre/actors/KeyPressEventModelCheckerChild.sys.mjs",
      events: {
        CheckKeyPressEventModel: { capture: true, mozSystemGroup: true },
      },
    },

    allFrames: true,
  },

  LoginManager: {
    parent: {
      esModuleURI: "resource://gre/modules/LoginManagerParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource://gre/modules/LoginManagerChild.sys.mjs",
      events: {
        DOMFormBeforeSubmit: {},
        DOMFormHasPassword: {},
        DOMFormHasPossibleUsername: {},
        DOMInputPasswordAdded: {},
      },
    },

    allFrames: true,
    messageManagerGroups: ["browsers", "webext-browsers", ""],
  },

  ManifestMessages: {
    child: {
      esModuleURI: "resource://gre/modules/ManifestMessagesChild.sys.mjs",
    },
  },

  NetError: {
    parent: {
      esModuleURI: "resource://gre/actors/NetErrorParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource://gre/actors/NetErrorChild.sys.mjs",
      events: {
        DOMDocElementInserted: {},
        click: {},
      },
    },

    matches: ["about:certerror?*", "about:neterror?*"],
    allFrames: true,
  },

  PictureInPictureLauncher: {
    parent: {
      esModuleURI: "resource://gre/modules/PictureInPicture.sys.mjs",
    },
    child: {
      esModuleURI: "resource://gre/actors/PictureInPictureChild.sys.mjs",
      events: {
        MozTogglePictureInPicture: { capture: true },
      },
    },

    allFrames: true,
  },

  PictureInPicture: {
    parent: {
      esModuleURI: "resource://gre/modules/PictureInPicture.sys.mjs",
    },
    child: {
      esModuleURI: "resource://gre/actors/PictureInPictureChild.sys.mjs",
    },

    allFrames: true,
  },

  PictureInPictureToggle: {
    parent: {
      esModuleURI: "resource://gre/modules/PictureInPicture.sys.mjs",
    },
    child: {
      esModuleURI: "resource://gre/actors/PictureInPictureChild.sys.mjs",
      events: {
        UAWidgetSetupOrChange: {},
        contextmenu: { capture: true },
      },
    },

    allFrames: true,
  },

  PopupBlocking: {
    parent: {
      esModuleURI: "resource://gre/actors/PopupBlockingParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource://gre/actors/PopupBlockingChild.sys.mjs",
      events: {
        DOMPopupBlocked: { capture: true },
        // Only listen for the `pageshow` event after the actor has already been
        // created for some other reason.
        pageshow: { createActor: false },
      },
    },
    allFrames: true,
  },

  Printing: {
    parent: {
      esModuleURI: "resource://gre/actors/PrintingParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource://gre/actors/PrintingChild.sys.mjs",
      events: {
        PrintingError: { capture: true },
        printPreviewUpdate: { capture: true },
      },
    },
  },

  PrintingSelection: {
    child: {
      esModuleURI: "resource://gre/actors/PrintingSelectionChild.sys.mjs",
    },
    allFrames: true,
  },

  PurgeSessionHistory: {
    child: {
      esModuleURI: "resource://gre/actors/PurgeSessionHistoryChild.sys.mjs",
    },
    allFrames: true,
  },

  ReportBrokenSite: {
    parent: {
      esModuleURI: "resource://gre/actors/ReportBrokenSiteParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource://gre/actors/ReportBrokenSiteChild.sys.mjs",
    },
    matches: [
      "http://*/*",
      "https://*/*",
      "about:certerror?*",
      "about:neterror?*",
    ],
    messageManagerGroups: ["browsers"],
    allFrames: true,
  },

  // This actor is available for all pages that one can
  // view the source of, however it won't be created until a
  // request to view the source is made via the message
  // 'ViewSource:LoadSource' or 'ViewSource:LoadSourceWithSelection'.
  ViewSource: {
    child: {
      esModuleURI: "resource://gre/actors/ViewSourceChild.sys.mjs",
    },

    allFrames: true,
  },

  // This actor is for the view-source page itself.
  ViewSourcePage: {
    parent: {
      esModuleURI: "resource://gre/actors/ViewSourcePageParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource://gre/actors/ViewSourcePageChild.sys.mjs",
      events: {
        pageshow: { capture: true },
        click: {},
      },
    },

    matches: ["view-source:*"],
    allFrames: true,
  },

  WebChannel: {
    parent: {
      esModuleURI: "resource://gre/actors/WebChannelParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource://gre/actors/WebChannelChild.sys.mjs",
      events: {
        WebChannelMessageToChrome: { capture: true, wantUntrusted: true },
      },
    },

    allFrames: true,
  },

  Thumbnails: {
    child: {
      esModuleURI: "resource://gre/actors/ThumbnailsChild.sys.mjs",
    },
  },

  // Determines if a page can be translated, and coordinates communication with the
  // translations engine.
  Translations: {
    parent: {
      esModuleURI: "resource://gre/actors/TranslationsParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource://gre/actors/TranslationsChild.sys.mjs",
      events: {
        DOMContentLoaded: {},
      },
    },
    matches: [
      "http://*/*",
      "https://*/*",
      "file:///*",

      // The actor is explicitly loaded by this page,
      // so it needs to be allowed for it.
      "about:translations",
    ],
    enablePreference: "browser.translations.enable",
  },

  // A single process that controls all of the translations.
  TranslationsEngine: {
    parent: {
      esModuleURI: "resource://gre/actors/TranslationsEngineParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource://gre/actors/TranslationsEngineChild.sys.mjs",
      events: {
        DOMContentLoaded: { createActor: true },
      },
    },
    includeChrome: true,
    matches: ["chrome://global/content/translations/translations-engine.html"],
    enablePreference: "browser.translations.enable",
  },

  UAWidgets: {
    child: {
      esModuleURI: "resource://gre/actors/UAWidgetsChild.sys.mjs",
      events: {
        UAWidgetSetupOrChange: {},
        UAWidgetTeardown: {},
      },
    },

    includeChrome: true,
    allFrames: true,
  },

  UnselectedTabHover: {
    parent: {
      esModuleURI: "resource://gre/actors/UnselectedTabHoverParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource://gre/actors/UnselectedTabHoverChild.sys.mjs",
      events: {
        "UnselectedTabHover:Enable": {},
        "UnselectedTabHover:Disable": {},
      },
    },

    allFrames: true,
  },
};

/**
 * Note that turning on page data collection for snapshots currently disables
 * collection of generic page info for normal history entries. See bug 1740234.
 */
if (!Services.prefs.getBoolPref("browser.pagedata.enabled", false)) {
  JSWINDOWACTORS.ContentMeta = {
    parent: {
      esModuleURI: "resource://gre/actors/ContentMetaParent.sys.mjs",
    },

    child: {
      esModuleURI: "resource://gre/actors/ContentMetaChild.sys.mjs",
      events: {
        DOMContentLoaded: {},
        DOMMetaAdded: { createActor: false },
      },
    },

    messageManagerGroups: ["browsers"],
  };
}

if (AppConstants.platform != "android") {
  // Note that GeckoView has another implementation in mobile/android/actors.
  JSWINDOWACTORS.Select = {
    parent: {
      esModuleURI: "resource://gre/actors/SelectParent.sys.mjs",
    },

    child: {
      esModuleURI: "resource://gre/actors/SelectChild.sys.mjs",
      events: {
        mozshowdropdown: {},
        "mozshowdropdown-sourcetouch": {},
        mozhidedropdown: { mozSystemGroup: true },
      },
    },

    includeChrome: true,
    allFrames: true,
  };

  // Note that GeckoView handles MozOpenDateTimePicker in GeckoViewPrompt.
  JSWINDOWACTORS.DateTimePicker = {
    parent: {
      esModuleURI: "resource://gre/actors/DateTimePickerParent.sys.mjs",
    },

    child: {
      esModuleURI: "resource://gre/actors/DateTimePickerChild.sys.mjs",
      events: {
        MozOpenDateTimePicker: {},
        MozUpdateDateTimePicker: {},
        MozCloseDateTimePicker: {},
      },
    },

    includeChrome: true,
    allFrames: true,
  };
}

export var ActorManagerParent = {
  _addActors(actors, kind) {
    let register, unregister;
    switch (kind) {
      case "JSProcessActor":
        register = ChromeUtils.registerProcessActor;
        unregister = ChromeUtils.unregisterProcessActor;
        break;
      case "JSWindowActor":
        register = ChromeUtils.registerWindowActor;
        unregister = ChromeUtils.unregisterWindowActor;
        break;
      default:
        throw new Error("Invalid JSActor kind " + kind);
    }
    for (let [actorName, actor] of Object.entries(actors)) {
      // The actor defines its own register/unregister logic.
      if (actor.onAddActor) {
        actor.onAddActor(
          () => register(actorName, actor),
          () => unregister(actorName, actor)
        );
        continue;
      }

      // If enablePreference is set, only register the actor while the
      // preference is set to true.
      if (actor.enablePreference) {
        let actorNameProp = actorName + "_Preference";
        XPCOMUtils.defineLazyPreferenceGetter(
          this,
          actorNameProp,
          actor.enablePreference,
          false,
          (prefName, prevValue, isEnabled) => {
            if (isEnabled) {
              register(actorName, actor);
            } else {
              unregister(actorName, actor);
            }
            if (actor.onPreferenceChanged) {
              actor.onPreferenceChanged(prefName, prevValue, isEnabled);
            }
          }
        );
        if (!this[actorNameProp]) {
          continue;
        }
      }

      register(actorName, actor);
    }
  },

  addJSProcessActors(actors) {
    this._addActors(actors, "JSProcessActor");
  },
  addJSWindowActors(actors) {
    this._addActors(actors, "JSWindowActor");
  },
};

ActorManagerParent.addJSProcessActors(JSPROCESSACTORS);
ActorManagerParent.addJSWindowActors(JSWINDOWACTORS);
