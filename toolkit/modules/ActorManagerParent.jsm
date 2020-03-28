/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * This module handles 2 types of JavaScript-implemented actors.
 * The first is implementations of JSWindowActor, registered through DOM IPC
 * infrastructure, and are fission-compatible.
 * The second is legacy implementations of ActorChild/ActorParent, which are
 * JS classes managed through this module and ActorManagerChild.jsm, which use the
 * message manager.
 *
 * The lists of both types of actors start with documentation for that type of actor.
 */

var EXPORTED_SYMBOLS = ["ActorManagerParent"];

const { ExtensionUtils } = ChromeUtils.import(
  "resource://gre/modules/ExtensionUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const { DefaultMap } = ExtensionUtils;

/**
 * Fission-compatible JSWindowActor implementations.
 * Each actor options object takes the form of a WindowActorOptions dictionary.
 * Detailed documentation of these options is in dom/docs/Fission.rst,
 * available at https://firefox-source-docs.mozilla.org/dom/Fission.html#jswindowactor
 */
let ACTORS = {
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
      events: {
        DOMContentLoaded: {},
        pageshow: { capture: true },
        pagehide: { capture: true },
        unload: { capture: true },
        focus: { capture: true },
        blur: { capture: true },
        mousedown: { capture: true },
        input: { capture: true },
        keydown: { capture: true },
        keypress: { capture: true, mozSystemGroup: true },
        compositionstart: { capture: true },
        compositionend: { capture: true },
        contextmenu: { capture: true },
      },
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
      moduleURI: "resource://gre/actors/FormHistoryParent.jsm",
    },
    child: {
      moduleURI: "resource://gre/actors/FormHistoryChild.jsm",
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
        DOMFormBeforeSubmit: {},
        DOMFormHasPassword: {},
        DOMInputPasswordAdded: {},
      },
    },

    allFrames: true,
  },
  PictureInPicture: {
    parent: {
      moduleURI: "resource://gre/modules/PictureInPicture.jsm",
    },
    child: {
      moduleURI: "resource://gre/actors/PictureInPictureChild.jsm",
      events: {
        MozTogglePictureInPicture: { capture: true },
        MozStopPictureInPicture: { capture: true },
      },
    },

    allFrames: true,
  },

  PictureInPictureToggle: {
    parent: {
      moduleURI: "resource://gre/modules/PictureInPicture.jsm",
    },
    child: {
      moduleURI: "resource://gre/actors/PictureInPictureChild.jsm",
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
      },
    },
    allFrames: true,
  },

  PurgeSessionHistory: {
    child: {
      moduleURI: "resource://gre/actors/PurgeSessionHistoryChild.jsm",
    },
    allFrames: true,
  },

  Select: {
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

    allFrames: true,
  },

  SidebarSearch: {
    parent: {
      moduleURI: "resource://gre/actors/SidebarSearchParent.jsm",
    },

    allFrames: true,
  },

  // This actor is available for all pages that one can
  // view the source of, however it won't be created until a
  // request to view the source is made via the message
  // 'ViewSource:LoadSource' or 'ViewSource:LoadSourceWithSelection'.
  ViewSource: {
    child: {
      moduleURI: "resource://gre/actors/ViewSourceChild.jsm",
    },

    allFrames: true,
  },

  // This actor is for the view-source page itself.
  ViewSourcePage: {
    parent: {
      moduleURI: "resource://gre/actors/ViewSourcePageParent.jsm",
    },
    child: {
      moduleURI: "resource://gre/actors/ViewSourcePageChild.jsm",
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
      moduleURI: "resource://gre/actors/WebChannelParent.jsm",
    },
    child: {
      moduleURI: "resource://gre/actors/WebChannelChild.jsm",
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

  WebNavigation: {
    child: {
      moduleURI: "resource://gre/actors/WebNavigationChild.jsm",
    },
  },

  Zoom: {
    parent: {
      moduleURI: "resource://gre/actors/ZoomParent.jsm",
    },
    child: {
      moduleURI: "resource://gre/actors/ZoomChild.jsm",
      events: {
        PreFullZoomChange: {},
        FullZoomChange: {},
        TextZoomChange: {},
        DoZoomEnlargeBy10: {
          capture: true,
          mozSystemGroup: true,
        },
        DoZoomReduceBy10: {
          capture: true,
          mozSystemGroup: true,
        },
        mozupdatedremoteframedimensions: {
          capture: true,
          mozSystemGroup: true,
        },
      },
    },

    allFrames: true,
  },
};

/*
 * The following code implements logic for managing JavaScript actor instances
 * bound to message managers. It handles lazily instantiating those actors based
 * on DOM events, IPC messages, or observer notifications, and is meant to
 * entirely replace the existing concept of frame scripts.
 *
 * All actors must be registered in the parent process, before the first child
 * process starts. Once all actors have been registered, the actor data is
 * mangled into a form which can be handled efficiently during content process
 * startup, and shared with all content processes. Frame scripts in those
 * content processes attach that data to frame message managers via
 * ActorManagerChild, which handles instantiating and dispatching to those
 * actors as necessary.
 *
 *
 * Each actor is a class which lives in a JSM, and has a constructor which takes
 * a single message manager argument. Each actor may conceptually have both
 * Child and Parent variants, but only Child variants are currently implemented.
 * The parent and child variants live in separate JSMs, and have separate class
 * names, each of which have Child or Parent appended to their names, as
 * appropriate. For instance, the AudioPlayback actor has a child instance named
 * AudioPlaybackChild which lives in AudioPlaybackChild.jsm.
 *
 *
 * Actors are defined by calling ActorManagerParent.addActors, with an object
 * containing a property for each actor being defined, whose value is an object
 * describing how the actor should be loaded. That object may have the following
 * properties:
 *
 * - "child": The actor definition for the child side of the actor.
 *
 * Each "child" (or "parent", when it is implemented) actor definition may
 * contain the following properties:
 *
 * - "module": The URI from which the modules is loaded. This should be a
 *   resource: URI, ideally beginning with "resource://gre/actors/" or
 *   "resource:///actors/", with a filename matching the name of the actor for
 *   the given side. So, the child side of the AudioPlayback actor should live at
 *   "resource://gre/actors/AudioPlaybackChild.jsm".
 *
 * - "group": A group name which restricts the message managers to which this
 *   actor may be attached. This should match the "messagemanagergroup"
 *   attribute of a <browser> element. Frame scripts are responsible for
 *   attaching the appropriate actors to the appropriate browsers using
 *   ActorManagerChild.attach().
 *
 * - "events": An object containing a property for each event the actor will
 *   listen for, with an options object, as accepted by addEventListener, as its
 *   value. For each such property, an event listener will be added to the
 *   message manager[1] for the given event name, which delegates to the actor's
 *   handleEvent method.
 *
 * - "messages": An array of message manager message names. For each message
 *   name in the list, a message listener will be added to the frame message
 *   manager, and the messages it receives will be delegated to the actor's
 *   receiveMessage method.
 *
 * - "observers": An array of observer topics. A global observer will be added
 *   for each topic in the list, and observer notifications for it will be
 *   delegated to the actor's observe method. Note that observers are global in
 *   nature, and these notifications may therefore have nothing to do with the
 *   message manager the actor is bound to. The actor itself is responsible for
 *   filtering the notifications that apply to it.
 *
 *   These observers are automatically unregistered when the message manager is
 *   destroyed.
 *
 * - "matches": An array of URL match patterns (as accepted by the MatchPattern
 *   class in MatchPattern.webidl) which restrict which pages the actor may be
 *   instantiated for. If this is defined, the actor will only receive DOM
 *   events sent to windows which match this pattern, and will only receive
 *   message manager messages for frame message managers which are currently
 *   hosting a matching DOM window.
 *
 * - "allFrames": this modifies its behavior to allow it to match sub-frames
 *   as well as top-level frames. If "allFrames" is not specified, it will
 *   match only top-level frames.
 *
 * - "matchAboutBlank": If "matches" is specified, this modifies its behavior to
 *   allow it to match about:blank pages. See MozDocumentMatcher.webidl for more
 *   information.
 *
 * [1]: For actors which specify "matches" or "allFrames", the listener will be
 *      added to the DOM window rather than the frame message manager.
 *
 * If Fission is being simulated, and an actor needs to receive events from
 * sub-frames, it must use "allFrames".
 */
let LEGACY_ACTORS = {
  ManifestMessages: {
    child: {
      module: "resource://gre/modules/ManifestMessagesChild.jsm",
      messages: [
        "DOM:ManifestObtainer:Obtain",
        "DOM:WebManifest:fetchIcon",
        "DOM:WebManifest:hasManifestLink",
      ],
    },
  },

  Printing: {
    child: {
      module: "resource://gre/actors/PrintingChild.jsm",
      events: {
        PrintingError: { capture: true },
        printPreviewUpdate: { capture: true },
      },
      messages: [
        "Printing:Preview:Enter",
        "Printing:Preview:Exit",
        "Printing:Preview:Navigate",
        "Printing:Preview:ParseDocument",
        "Printing:Print",
      ],
    },
  },
};

class ActorSet {
  constructor(group, actorSide) {
    this.group = group;
    this.actorSide = actorSide;

    this.actors = new Map();
    this.events = [];
    this.messages = new DefaultMap(() => []);
    this.observers = new DefaultMap(() => []);
  }

  addActor(actorName, actor) {
    actorName += this.actorSide;
    this.actors.set(actorName, { module: actor.module });

    if (actor.events) {
      for (let [event, options] of Object.entries(actor.events)) {
        this.events.push({ actor: actorName, event, options });
      }
    }
    for (let msg of actor.messages || []) {
      this.messages.get(msg).push(actorName);
    }
    for (let topic of actor.observers || []) {
      this.observers.get(topic).push(actorName);
    }
  }
}

const { sharedData } = Services.ppmm;

var ActorManagerParent = {
  // Actor sets which should be loaded in the child side, keyed by
  // "messagemanagergroup".
  childGroups: new DefaultMap(group => new ActorSet(group, "Child")),
  // Actor sets which should be loaded in the parent side, keyed by
  // "messagemanagergroup".
  parentGroups: new DefaultMap(group => new ActorSet(group, "Parent")),

  // Singleton actor sets, which should be loaded only for documents which match
  // a specific pattern. The keys in this map are plain objects specifying
  // filter keys as understood by MozDocumentMatcher.
  singletons: new DefaultMap(() => new ActorSet(null, "Child")),

  addActors(actors) {
    for (let [actorName, actor] of Object.entries(actors)) {
      ChromeUtils.registerWindowActor(actorName, actor);
    }
  },

  addLegacyActors(actors) {
    for (let [actorName, actor] of Object.entries(actors)) {
      let { child } = actor;
      {
        let actorSet;
        if (child.matches || child.allFrames) {
          actorSet = this.singletons.get({
            matches: child.matches || ["<all_urls>"],
            allFrames: child.allFrames,
            matchAboutBlank: child.matchAboutBlank,
          });
        } else {
          actorSet = this.childGroups.get(child.group || null);
        }

        actorSet.addActor(actorName, child);
      }

      if (actor.parent) {
        let { parent } = actor;
        this.parentGroups.get(parent.group || null).addActor(actorName, parent);
      }
    }
  },

  /**
   * Serializes the current set of registered actors into ppmm.sharedData, for
   * use by ActorManagerChild. This must be called before any frame message
   * managers have been created. It will have no effect on existing message
   * managers.
   */
  flush() {
    for (let [name, data] of this.childGroups) {
      sharedData.set(`ChildActors:${name || ""}`, data);
    }
    sharedData.set("ChildSingletonActors", this.singletons);
  },
};

ActorManagerParent.addActors(ACTORS);
ActorManagerParent.addLegacyActors(LEGACY_ACTORS);
