//@ts-expect-error
import { ActorManagerParent } from "resource://gre/modules/ActorManagerParent.sys.mjs";

function localPathToResourceURI(path: string) {
  const re = new RegExp(/\.\.\/([a-zA-Z0-9-_\/]+)\.sys\.mts/);
  const result = re.exec(path);
  if (!result || result.length != 2) {
    throw Error(
      `[nora-browserGlue] localPathToResource URI match failed : ${path}`,
    );
  }
  const resourceURI = `resource://noraneko/${result[1]}.sys.mjs`;
  return resourceURI;
}

const JS_WINDOW_ACTORS: {
  [k: string]: WindowActorOptions;
} = {
  NRAboutPreferences: {
    child: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRAboutPreferencesChild.sys.mts",
      ),
      events: {
        DOMContentLoaded: {},
      },
    },
    matches: ["about:preferences*", "about:settings*"],
  },
  NRSettings: {
    parent: {
      esModuleURI: localPathToResourceURI("../actors/NRSettingsParent.sys.mts"),
    },
    child: {
      esModuleURI: localPathToResourceURI("../actors/NRSettingsChild.sys.mts"),
      events: {
        /**
         * actorCreated seems to require any of events for init
         */
        DOMDocElementInserted: {},
      },
    },
    //* port seems to not be supported
    //https://searchfox.org/mozilla-central/rev/3966e5534ddf922b186af4777051d579fd052bad/dom/chrome-webidl/JSWindowActor.webidl#99
    //https://searchfox.org/mozilla-central/rev/3966e5534ddf922b186af4777051d579fd052bad/dom/chrome-webidl/MatchPattern.webidl#17
    matches: ["*://localhost/*", "chrome://noraneko-settings/*"],
  },
  NRTabManager: {
    parent: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRTabManagerParent.sys.mts",
      ),
    },
    child: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRTabManagerChild.sys.mts",
      ),
      events: {
        DOMDocElementInserted: {},
      },
    },
    matches: ["*://localhost/*", "chrome://noraneko-settings/*"],
  },
  NRSyncManager: {
    parent: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRSyncManagerParent.sys.mts",
      ),
    },
    child: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRSyncManagerChild.sys.mts",
      ),
      events: {
        DOMDocElementInserted: {},
      },
    },
    matches: ["*://localhost/*", "chrome://noraneko-settings/*"],
  },
  NRAppConstants: {
    parent: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRAppConstantsParent.sys.mts",
      ),
    },
    child: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRAppConstantsChild.sys.mts",
      ),
      events: {
        DOMDocElementInserted: {},
      },
    },
    matches: ["*://localhost/*", "chrome://noraneko-settings/*"],
  },
  NRRestartBrowser: {
    parent: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRRestartBrowserParent.sys.mts",
      ),
    },
    child: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRRestartBrowserChild.sys.mts",
      ),
      events: {
        DOMDocElementInserted: {},
      },
    },
    matches: ["*://localhost/*", "chrome://noraneko-settings/*"],
  },
  NRProgressiveWebApp: {
    parent: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRProgressiveWebAppParent.sys.mts",
      ),
    },
    child: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRProgressiveWebAppChild.sys.mts",
      ),
      events: {
        pageshow: {},
      },
    },
    allFrames: true,
  },
  NRPwaManager: {
    parent: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRPwaManagerParent.sys.mts",
      ),
    },
    child: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRPwaManagerChild.sys.mts",
      ),
      events: {
        DOMDocElementInserted: {},
      },
    },
    matches: ["*://localhost/*", "chrome://noraneko-settings/*"],
  },
  // Floorp 13 test
  NRWebContentModifier: {
    parent: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRWebContentModifierParent.sys.mts",
      ),
    },
    child: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRWebContentModifierChild.sys.mts",
      ),
      events: {
        DOMContentLoaded: {},
      },
    },
    matches: ["https://*/*", "http://*/*"],
  },
  NRChromeModal: {
    child: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRChromeModalChild.sys.mts",
      ),
      events: {
        DOMContentLoaded: {},
      },
    },
    matches: ["*://localhost/*", "chrome://noraneko-modal-child/*"],
  },
  NRProfileManager: {
    parent: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRProfileManagerParent.sys.mts",
      ),
    },
    child: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRProfileManagerChild.sys.mts",
      ),
      events: {
        DOMDocElementInserted: {},
      },
    },
    matches: ["*://localhost/*", "chrome://noraneko-settings/*"],
  },

  NRStartPage: {
    parent: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRStartPageParent.sys.mts",
      ),
    },
    child: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRStartPageChild.sys.mts",
      ),
      events: {
        DOMContentLoaded: {},
      },
    },
    matches: ["*://localhost/*", "chrome://noraneko-newtab/*"],
  },

  NRWelcomePage: {
    parent: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRWelcomePageParent.sys.mts",
      ),
    },
    child: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRWelcomePageChild.sys.mts",
      ),
      events: {
        DOMContentLoaded: {},
      },
    },
    matches: ["*://localhost/*", "chrome://noraneko-welcome/*"],
  },
};

ActorManagerParent.addJSWindowActors(JS_WINDOW_ACTORS);
