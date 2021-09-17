"user strict";

AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "1.9"
);

let server = AddonTestUtils.createHttpServer({ hosts: ["example.com"] });
server.registerDirectory("/data/", do_get_file("data"));

// Use %LOCALE% which is set based on the app locale.
Services.prefs.setStringPref(
  PREF_GETADDONS_BYIDS,
  "http://example.com/addons-%LOCALE%.json"
);

const TEST_ADDON_ID = "test_AddonRepository_1@tests.mozilla.org";

const repositoryData = {
  page_size: 25,
  page_count: 1,
  count: 1,
  next: null,
  previous: null,
  results: [
    {
      name: "Repo Add-on 1",
      type: "extension",
      guid: TEST_ADDON_ID,
      current_version: {
        version: "2.1",
        files: [
          {
            platform: "all",
            size: 9,
            url: "http://example.com/repo/1/install.xpi",
          },
        ],
      },
      authors: [
        {
          name: "Repo Add-on 1 - Creator",
          url: "http://example.com/repo/1/creator.html",
        },
      ],
      summary: "This is an en-US addon data object",
      description: "Full Description en-US",
      icons: {
        "32": "http://example.com/repo/1/icon.png",
      },
      ratings: {
        count: 1234,
        text_count: 1111,
        average: 1,
      },
      homepage: "http://example.com/repo/1/homepage.html",
      support_url: "http://example.com/repo/1/support.html",
      contributions_url: "http://example.com/repo/1/meetDevelopers.html",
      ratings_url: "http://example.com/repo/1/review.html",
      weekly_downloads: 3331,
      last_updated: "1970-01-01T00:00:09Z",
    },
  ],
};

server.registerPathHandler("/addons-en-US.json", (request, response) => {
  // The request contains the IDs to retreive, but we're just handling the
  // two test addons so it's static data.
  response.setHeader("content-type", "application/json");
  response.write(JSON.stringify(repositoryData));
});

server.registerPathHandler("/addons-und.json", (request, response) => {
  // Simple modification to make it the "und" locale.  The real AMO api
  // would return the localized data if it were available.
  const undRepositoryData = JSON.parse(JSON.stringify(repositoryData));
  undRepositoryData.results[0].summary = "This is an und addon data object";
  undRepositoryData.results[0].description = "Full Description und";

  response.setHeader("content-type", "application/json");
  response.write(JSON.stringify(undRepositoryData));
});

const ADDONS = [
  {
    manifest: {
      name: "XPI Add-on 1",
      version: "1.1",

      description: "XPI Add-on 1 - Description",
      developer: {
        name: "XPI Add-on 1 - Author",
      },

      homepage_url: "http://example.com/xpi/1/homepage.html",
      icons: {
        32: "icon.png",
      },

      options_ui: {
        page: "options.html",
      },

      applications: {
        gecko: { id: TEST_ADDON_ID },
      },
    },
  },
  {
    // Necessary to provide the "und" locale
    manifest: {
      name: "und Language Pack",
      version: "1.0",
      manifest_version: 2,
      applications: {
        gecko: {
          id: "langpack-und@test.mozilla.org",
        },
      },
      sources: {
        browser: {
          base_path: "browser/",
        },
      },
      langpack_id: "und",
      languages: {
        und: {
          chrome_resources: {
            global: "chrome/und/locale/und/global/",
          },
          version: "20171001190118",
        },
      },
      author: "Mozilla Localization Task Force",
      description: "Language pack for Testy for und",
    },
  },
];
const ADDON_IDS = ADDONS.map(addon => addon.manifest.applications.gecko.id);
const ADDON_FILES = ADDONS.map(addon =>
  AddonTestUtils.createTempWebExtensionFile(addon)
);

const PREF_GETADDONS_CACHE_ENABLED = "extensions.getAddons.cache.enabled";
const PREF_METADATA_LASTUPDATE = "extensions.getAddons.cache.lastUpdate";

function promiseMetaDataUpdate(locales) {
  return new Promise(resolve => {
    let listener = args => {
      Services.prefs.removeObserver(PREF_METADATA_LASTUPDATE, listener);
      resolve();
    };

    Services.prefs.addObserver(PREF_METADATA_LASTUPDATE, listener);
    if (locales) {
      Services.locale.requestedLocales = locales;
    }
  });
}

add_task(async function setup() {
  Services.prefs.setBoolPref(PREF_GETADDONS_CACHE_ENABLED, true);
  await promiseStartupManager();

  await promiseInstallAllFiles(ADDON_FILES);
  let updated = promiseMetaDataUpdate();
  await promiseRestartManager();
  await updated;
  // This pref is a 1s resolution, set it to zero so the
  // next test can wait on it being updated again.
  Services.prefs.setIntPref(PREF_METADATA_LASTUPDATE, 0);
});

add_task(async function test_locale_change() {
  let addon = await AddonRepository.getCachedAddonByID(ADDON_IDS[0]);
  Assert.ok(addon.description.includes("en-US"), "description is en-us");
  Assert.ok(
    addon.fullDescription.includes("en-US"),
    "fullDescription is en-us"
  );

  // Wait for the last update timestamp to be updated.
  await promiseMetaDataUpdate(["und"]);

  addon = await AddonRepository.getCachedAddonByID(ADDON_IDS[0]);

  Assert.ok(addon.description.includes("und"), "description is und");
  Assert.ok(addon.fullDescription.includes("und"), "fullDescription is und");
});
