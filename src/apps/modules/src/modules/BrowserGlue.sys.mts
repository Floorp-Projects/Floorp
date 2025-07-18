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

// init WebScraperService
ChromeUtils.importESModule(
  "resource://noraneko/modules/WebScraperService.sys.mjs",
);

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
    matches: [
      "*://localhost/*",
      "chrome://noraneko-settings/*",
      "about:*",
    ],
  },
  NRPanelSidebar: {
    parent: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRPanelSidebarParent.sys.mts",
      ),
    },
    child: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRPanelSidebarChild.sys.mts",
      ),
      events: {
        DOMDocElementInserted: {},
      },
    },
    matches: [
      "*://localhost/*",
      "chrome://noraneko-settings/*",
      "about:*",
    ],
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
    matches: [
      "*://localhost/*",
      "chrome://noraneko-settings/*",
      "about:*",
    ],
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
    matches: [
      "*://localhost/*",
      "chrome://noraneko-settings/*",
      "about:*",
    ],
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
    matches: [
      "*://localhost/*",
      "chrome://noraneko-settings/*",
      "about:*",
    ],
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
    matches: [
      "*://localhost/*",
      "chrome://noraneko-settings/*",
      "about:*",
    ],
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
    matches: [
      "*://localhost/*",
      "chrome://noraneko-settings/*",
      "about:*",
    ],
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
    matches: ["*://localhost/*", "chrome://noraneko-settings/*", "about:*"],
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
    matches: [
      "*://localhost/*",
      "chrome://noraneko-newtab/*",
      "about:*",
    ],
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
    matches: [
      "*://localhost/*",
      "chrome://noraneko-welcome/*",
      "about:*",
    ],
  },

  NRSearchEngine: {
    parent: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRSearchEngineParent.sys.mts",
      ),
    },

    child: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRSearchEngineChild.sys.mts",
      ),
      events: {
        DOMContentLoaded: {},
      },
    },

    matches: [
      "*://localhost/*",
      "chrome://noraneko-welcome/*",
      "chrome://noraneko-newtab/*",
      "about:*",
    ],
  },

  NRWebScraper: {
    parent: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRWebScraperParent.sys.mts",
      ),
    },
    child: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRWebScraperChild.sys.mts",
      ),
    },
    includeChrome: true,
    allFrames: true,
  },
};

ActorManagerParent.addJSWindowActors(JS_WINDOW_ACTORS);
