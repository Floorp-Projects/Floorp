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
      moduleURI: "resource://gre/modules/ContentPrefServiceParent.jsm",
    },
    child: {
      moduleURI: "resource://gre/modules/ContentPrefServiceChild.jsm",
    },
  },

  ExtensionContent: {
    child: {
      moduleURI: "resource://gre/modules/ExtensionContent.jsm",
    },
    includeParent: true,
  },

  ProcessConduits: {
    parent: {
      moduleURI: "resource://gre/modules/ConduitsParent.jsm",
    },
    child: {
      moduleURI: "resource://gre/modules/ConduitsChild.jsm",
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
      moduleURI: "resource://gre/modules/AboutCertViewerParent.jsm",
    },
    child: {
      moduleURI: "resource://gre/modules/AboutCertViewerChild.jsm",

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
      },
    },
    matches: ["about:translations"],

    enablePreference: "browser.translations.enable",
  },

  AudioPlayback: {
    parent: {
      moduleURI: "resource://gre/actors/AudioPlaybackParent.jsm",
    },

    child: {
      moduleURI: "resource://gre/actors/AudioPlaybackChild.jsm",
      observers: ["audio-playback"],
    },

    allFrames: true,
  },

  AutoComplete: {
    parent: {
      moduleURI: "resource://gre/actors/AutoCompleteParent.jsm",
      // These two messages are also used, but are currently synchronous calls
      // through the per-process message manager.
      // "FormAutoComplete:GetSelectedIndex",
      // "FormAutoComplete:SelectBy"
    },

    child: {
      moduleURI: "resource://gre/actors/AutoCompleteChild.jsm",
    },

    allFrames: true,
  },

  Autoplay: {
    parent: {
      moduleURI: "resource://gre/actors/AutoplayParent.jsm",
    },

    child: {
      moduleURI: "resource://gre/actors/AutoplayChild.jsm",
      events: {
        GloballyAutoplayBlocked: {},
      },
    },

    allFrames: true,
  },

  AutoScroll: {
    parent: {
      moduleURI: "resource://gre/actors/AutoScrollParent.jsm",
    },

    child: {
      moduleURI: "resource://gre/actors/AutoScrollChild.jsm",
      events: {
        mousedown: { capture: true, mozSystemGroup: true },
      },
    },

    allFrames: true,
  },

  BackgroundThumbnails: {
    child: {
      moduleURI: "resource://gre/actors/BackgroundThumbnailsChild.jsm",
      events: {
        DOMDocElementInserted: { capture: true },
      },
    },
    messageManagerGroups: ["thumbnails"],
  },

  BrowserElement: {
    parent: {
      moduleURI: "resource://gre/actors/BrowserElementParent.jsm",
    },

    child: {
      moduleURI: "resource://gre/actors/BrowserElementChild.jsm",
      events: {
        DOMWindowClose: {},
      },
    },

    allFrames: true,
  },

  Conduits: {
    parent: {
      moduleURI: "resource://gre/modules/ConduitsParent.jsm",
    },

    child: {
      moduleURI: "resource://gre/modules/ConduitsChild.jsm",
    },

    allFrames: true,
  },

  Controllers: {
    parent: {
      moduleURI: "resource://gre/actors/ControllersParent.jsm",
    },
    child: {
      moduleURI: "resource://gre/actors/ControllersChild.jsm",
    },

    allFrames: true,
  },

  CookieBanner: {
    parent: {
      moduleURI: "resource://gre/actors/CookieBannerParent.jsm",
    },
    child: {
      moduleURI: "resource://gre/actors/CookieBannerChild.jsm",
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
    enablePreference: "cookiebanners.bannerClicking.enabled",
  },

  DateTimePicker: {
    parent: {
      moduleURI: "resource://gre/actors/DateTimePickerParent.jsm",
    },

    child: {
      moduleURI: "resource://gre/actors/DateTimePickerChild.jsm",
      events: {
        MozOpenDateTimePicker: {},
        MozUpdateDateTimePicker: {},
        MozCloseDateTimePicker: {},
      },
    },

    allFrames: true,
  },

  ExtFind: {
    child: {
      moduleURI: "resource://gre/actors/ExtFindChild.jsm",
    },

    allFrames: true,
  },

  FindBar: {
    parent: {
      moduleURI: "resource://gre/actors/FindBarParent.jsm",
    },
    child: {
      moduleURI: "resource://gre/actors/FindBarChild.jsm",
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
      moduleURI: "resource://gre/actors/FinderChild.jsm",
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
      moduleURI: "resource://gre/actors/InlineSpellCheckerParent.jsm",
    },

    child: {
      moduleURI: "resource://gre/actors/InlineSpellCheckerChild.jsm",
    },

    allFrames: true,
  },

  KeyPressEventModelChecker: {
    child: {
      moduleURI: "resource://gre/actors/KeyPressEventModelCheckerChild.jsm",
      events: {
        CheckKeyPressEventModel: { capture: true, mozSystemGroup: true },
      },
    },

    allFrames: true,
  },

  LoginManager: {
    parent: {
      moduleURI: "resource://gre/modules/LoginManagerParent.jsm",
    },
    child: {
      moduleURI: "resource://gre/modules/LoginManagerChild.jsm",
      events: {
        DOMDocFetchSuccess: {},
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
      moduleURI: "resource://gre/modules/ManifestMessagesChild.jsm",
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
      moduleURI: "resource://gre/actors/PopupBlockingParent.jsm",
    },
    child: {
      moduleURI: "resource://gre/actors/PopupBlockingChild.jsm",
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
      moduleURI: "resource://gre/actors/PrintingParent.jsm",
    },
    child: {
      moduleURI: "resource://gre/actors/PrintingChild.jsm",
      events: {
        PrintingError: { capture: true },
        printPreviewUpdate: { capture: true },
      },
    },
  },

  PrintingSelection: {
    child: {
      moduleURI: "resource://gre/actors/PrintingSelectionChild.jsm",
    },
    allFrames: true,
  },

  PurgeSessionHistory: {
    child: {
      moduleURI: "resource://gre/actors/PurgeSessionHistoryChild.jsm",
    },
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
      moduleURI: "resource://gre/actors/ThumbnailsChild.jsm",
    },
  },

  // The newer translations feature backed by local machine learning models.
  // See Bug 971044.
  Translations: {
    parent: {
      esModuleURI: "resource://gre/actors/TranslationsParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource://gre/actors/TranslationsChild.sys.mjs",
      events: {
        pageshow: {},
        DOMHeadElementParsed: {},
        DOMDocElementInserted: {},
        DOMContentLoaded: {},
      },
    },
    enablePreference: "browser.translations.enable",
  },

  UAWidgets: {
    child: {
      moduleURI: "resource://gre/actors/UAWidgetsChild.jsm",
      events: {
        UAWidgetSetupOrChange: {},
        UAWidgetTeardown: {},
      },
    },

    allFrames: true,
  },

  UnselectedTabHover: {
    parent: {
      moduleURI: "resource://gre/actors/UnselectedTabHoverParent.jsm",
    },
    child: {
      moduleURI: "resource://gre/actors/UnselectedTabHoverChild.jsm",
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
      moduleURI: "resource://gre/actors/ContentMetaParent.jsm",
    },

    child: {
      moduleURI: "resource://gre/actors/ContentMetaChild.jsm",
      events: {
        DOMContentLoaded: {},
        DOMMetaAdded: { createActor: false },
      },
    },

    messageManagerGroups: ["browsers"],
  };
}

if (AppConstants.platform != "android") {
  // For GeckoView support see bug 1776829.
  JSWINDOWACTORS.ClipboardReadPaste = {
    parent: {
      moduleURI: "resource://gre/actors/ClipboardReadPasteParent.jsm",
    },

    child: {
      moduleURI: "resource://gre/actors/ClipboardReadPasteChild.jsm",
      events: {
        MozClipboardReadPaste: {},
      },
    },

    allFrames: true,
  };

  /**
   * Note that GeckoView has another implementation in mobile/android/actors.
   */
  JSWINDOWACTORS.Select = {
    parent: {
      moduleURI: "resource://gre/actors/SelectParent.jsm",
    },

    child: {
      moduleURI: "resource://gre/actors/SelectChild.jsm",
      events: {
        mozshowdropdown: {},
        "mozshowdropdown-sourcetouch": {},
        mozhidedropdown: { mozSystemGroup: true },
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
