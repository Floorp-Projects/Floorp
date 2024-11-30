//@ts-expect-error
import { ActorManagerParent } from "resource://gre/modules/ActorManagerParent.sys.mjs";

function localPathToResourceURI(path: string, mtsToMjs = true) {
  if (!path.startsWith("../")) {
    throw Error("localpath should starts with `../`");
  }
  const resourceURI = `resource://noraneko/${path.slice(3)}`;
  return mtsToMjs ? resourceURI.replace(/.mts$/, ".mjs") : resourceURI;
}

const JS_WINDOW_ACTORS: {
  [k: string]: WindowActorOptions;
} = {
  //https://searchfox.org/mozilla-central/rev/3a34b4616994bd8d2b6ede2644afa62eaec817d1/browser/components/BrowserGlue.sys.mjs#310
  NRAboutNewTab: {
    child: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRAboutNewTabChild.sys.mts",
      ),

      events: {
        DOMContentLoaded: {},
      },
    },
    // The wildcard on about:newtab is for the # parameter
    // that is used for the newtab devtools. The wildcard for about:home
    // is similar, and also allows for falling back to loading the
    // about:home document dynamically if an attempt is made to load
    // about:home?jscache from the AboutHomeStartupCache as a top-level
    // load.
    matches: ["about:home*", "about:welcome", "about:newtab*"],
    remoteTypes: ["privilegedabout"],
  },
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
    matches: ["*://localhost/*"],
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
    matches: ["*://localhost/*"],
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
    matches: ["*://localhost/*"],
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
    matches: ["*://localhost/*"],
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
    matches: ["*://localhost/*"],
  },
};

ActorManagerParent.addJSWindowActors(JS_WINDOW_ACTORS);
